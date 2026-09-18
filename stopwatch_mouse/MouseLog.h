// SPDX-License-Identifier: MIT
#pragma once

#include <Arduino.h>

#if defined(STOPWATCH_MOUSE_DEBUG) && STOPWATCH_MOUSE_DEBUG
#define MOUSE_DEBUG(...) Serial.printf(__VA_ARGS__)
#else
#define MOUSE_DEBUG(...) ((void)0)
#endif

// Errors and explicit diagnostic commands never depend on the debug flag.
#define MOUSE_ERROR(...) Serial.printf(__VA_ARGS__)
