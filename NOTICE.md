# Third-party notices

The project's MIT license covers its own source. Dependencies retain their respective licenses and are downloaded separately; their source trees and firmware images are not distributed in this repository.

## M5Stack board initialization

IO-expander startup, display power/reset sequencing, and the CO5300 register initialization are based on the following MIT-licensed sources in [M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/6b4aa125288b6fe9dca661f10159f6e1e5ee785c):

- [`main/hal/hal_ioe.cpp`](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/hal_ioe.cpp)
- [`main/hal/hal_display.cpp`](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/main/hal/hal_display.cpp)

Source revision: `6b4aa125288b6fe9dca661f10159f6e1e5ee785c`. The upstream [license](https://github.com/m5stack/M5StopWatch-UserDemo/blob/6b4aa125288b6fe9dca661f10159f6e1e5ee785c/LICENSE) is reproduced below for the derived initialization work.

```text
MIT License

Copyright (c) 2026 M5Stack Technology CO LTD

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Graphics

[M5GFX 0.2.26](https://github.com/m5stack/M5GFX/tree/0.2.26) is used for software canvases. Its top-level [MIT license](https://github.com/m5stack/M5GFX/blob/0.2.26/LICENSE) is copyright (c) 2021 M5Stack. The library also contains separately licensed components, including LovyanGFX (FreeBSD/2-clause BSD) and the Font2 bitmap font used by this application (FreeBSD/2-clause BSD, Bodmer). See its [complete license inventory](https://github.com/m5stack/M5GFX/blob/0.2.26/README.md#license) and retain the applicable notices if redistributing compiled firmware with those components.

[M5Unified 0.2.19](https://github.com/m5stack/M5Unified/tree/0.2.19) was also reviewed: its [license](https://github.com/m5stack/M5Unified/blob/0.2.19/LICENSE) is MIT, copyright (c) 2021 M5Stack. M5Unified is not a dependency of this firmware.

## Touch protocol

The CST8xx register layout is described by [Adafruit_CST8XX](https://github.com/adafruit/Adafruit_CST8XX_Library/blob/2f43c19d376f5f2800fd63ca277f1d1466c3503d/Adafruit_CST8XX.cpp), written by Melissa LeBlanc-Williams for Adafruit Industries and distributed under MIT. This project implements its own packet decoder and gesture state machine; it does not include the Adafruit driver.

The chip's two-point capability is documented in the [CST820 datasheet supplied by M5Stack](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1242/CST820B_datasheet.pdf). Panel electrode geometry and controller firmware determine which pairs of contacts can be distinguished.

## Arduino and ESP-IDF

The build uses [M5Stack's Arduino ESP32 core](https://github.com/m5stack/arduino-esp32) and the ESP-IDF libraries supplied with it. Those dependencies retain their upstream licenses. No board package or third-party binary is included here.
