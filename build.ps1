param(
    [string]$BuildRoot = (Join-Path $PSScriptRoot '.build'),
    [switch]$DebugLog
)
$ErrorActionPreference = 'Stop'
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$libraryVersion = '0.2.26'
$library = Join-Path $BuildRoot "deps/M5GFX-$libraryVersion"
$properties = Join-Path $library 'library.properties'
if (!(Test-Path -LiteralPath $properties)) {
    $dependencies = Join-Path $BuildRoot 'deps'
    New-Item -ItemType Directory -Force -Path $dependencies | Out-Null
    $archive = Join-Path $dependencies "M5GFX-$libraryVersion.zip"
    Invoke-WebRequest "https://codeload.github.com/m5stack/M5GFX/zip/refs/tags/$libraryVersion" -OutFile $archive -UseBasicParsing -TimeoutSec 180
    Expand-Archive -LiteralPath $archive -DestinationPath $dependencies -Force
}
if (!(Select-String -LiteralPath $properties -Pattern '^version=0\.2\.26$' -Quiet)) {
    throw "Expected M5GFX $libraryVersion at $library"
}
$cores = (& arduino-cli core list --format json | ConvertFrom-Json).platforms
if ($LASTEXITCODE -ne 0) { throw 'Could not list installed Arduino cores' }
$core = $cores | Where-Object { $_.id -eq 'm5stack:esp32' }
if (!$core -or $core.installed_version -ne '3.3.7') {
    throw 'Install m5stack:esp32@3.3.7 using the commands in README.md.'
}
$variant = if ($DebugLog) { 'debug' } else { 'release' }
$buildPath = Join-Path $BuildRoot $variant
$compileArgs = @('compile', '-b', 'm5stack:esp32:m5stack_stopwatch', '--library', $library,
                 '--build-path', $buildPath, '--warnings', 'all')
if ($DebugLog) { $compileArgs += @('--build-property', 'compiler.cpp.extra_flags=-DSTOPWATCH_MOUSE_DEBUG=1') }
$compileArgs += (Join-Path $PSScriptRoot 'stopwatch_mouse')
$previousErrorPreference = $ErrorActionPreference
try {
    # Windows PowerShell can turn native stderr warnings into terminating errors
    # when a caller redirects output. Use the compiler exit status instead.
    $ErrorActionPreference = 'Continue'
    & arduino-cli @compileArgs
    $compileExit = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $previousErrorPreference
}
if ($compileExit -ne 0) { throw "Firmware build failed (exit $compileExit)" }
Write-Output "Built firmware in $buildPath"
