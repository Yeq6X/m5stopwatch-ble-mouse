// SPDX-License-Identifier: MIT
// Board initialization references: see NOTICE.md.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <M5GFX.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include "MouseLog.h"
#include "TouchFrame.h"

namespace stopwatch {

class StopWatchHardware {
    spi_device_handle_t panel = nullptr;
    uint8_t ioe = 0, stableButtons = 0, candidateButtons = 0;
    uint32_t buttonChanged = 0;
    uint8_t* dma = nullptr;
    uint8_t rgb[kScreenWidth * 3];
    int battery = -1;
    uint32_t lastTouchError = 0;
    uint32_t touchErrors = 0;
    static constexpr int kSdaPin = 47, kSclPin = 48;
    static constexpr int kScrollUpPin = 1, kScrollDownPin = 2;
    static constexpr uint32_t kI2cClockHz = 100000, kSpiClockHz = 20000000;
    static constexpr uint8_t kPmicAddress = 0x6e, kTouchAddress = 0x15;
    static constexpr int kBatteryEmptyMv = 3300, kBatteryFullMv = 4200;
    bool read(uint8_t address, uint8_t reg, uint8_t* data, size_t size) {
        Wire.beginTransmission(address);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, (uint8_t)size) != size)
            return false;
        for (size_t i = 0; i < size; i++)
            data[i] = Wire.read();
        return true;
    }
    bool write(uint8_t address, uint8_t reg, uint8_t value) {
        for (int i = 0; i < 5; i++) {
            Wire.beginTransmission(address);
            Wire.write(reg);
            Wire.write(value);
            if (Wire.endTransmission() == 0)
                return true;
            delay(20);
        }
        return false;
    }
    bool bits(uint8_t address, uint8_t reg, uint8_t mask, bool on) {
        uint8_t value;
        if (!read(address, reg, &value, 1))
            return false;
        return write(address, reg, on ? value | mask : value & ~mask);
    }
    void cmd(uint8_t reg, const uint8_t* data = nullptr, size_t n = 0) {
        spi_transaction_t t = {};
        t.cmd = 0x02;
        t.addr = uint32_t(reg) << 8;
        t.length = n * 8;
        t.tx_buffer = data;
        ESP_ERROR_CHECK(spi_device_polling_transmit(panel, &t));
    }
    void arg(uint8_t reg, uint8_t value) { cmd(reg, &value, 1); }

   public:
    const char* error = "unknown";
    bool fail(const char* reason) {
        error = reason;
        Serial.println(reason);
        return false;
    }
    bool begin() {
        MOUSE_DEBUG("INIT begin\n");
        // Match official M5IOE1 startup: reset the host bus, then let it settle.
        Wire.end();
        delay(50);
        if (!Wire.begin(kSdaPin, kSclPin, kI2cClockHz))
            return fail("INIT FAIL I2C begin");
        Wire.setTimeOut(20);
        delay(100);
        // Disable the PMIC watchdog and sleep; leave charging-current configuration unchanged.
        const bool watchdogFirst = write(kPmicAddress, 0x09, 0);
        const bool watchdogSecond = write(kPmicAddress, 0x09, 0);
        if (!watchdogFirst || !watchdogSecond || !write(kPmicAddress, 0x0a, 0) ||
            !bits(kPmicAddress, 0x06, 0x06, true))
            return fail("PMIC configuration failed");
        // Wake with START/STOP, then verify an actual register read. An address
        // ACK alone does not establish IO-expander readiness at startup.
        uint8_t cfg = 0;
        ioe = 0;
        for (int attempt = 0; attempt < 6 && !ioe; attempt++) {
            for (uint8_t a : {uint8_t(0x4f), uint8_t(0x6f)}) {
                Wire.beginTransmission(a);
                Wire.endTransmission();
                delay(10);
                if (read(a, 0x23, &cfg, 1)) {
                    ioe = a;
                    break;
                }
            }
            if (!ioe) {
                Serial.printf("INIT IOE wake retry=%d SDA=%d SCL=%d\n", attempt + 1,
                              digitalRead(kSdaPin), digitalRead(kSclPin));
                delay(800);
            }
        }
        if (!ioe)
            return fail("INIT FAIL IOE wake/readiness");
        MOUSE_DEBUG("INIT IOE address=%02x cfg=%02x\n", ioe, cfg);
        if (!write(ioe, 0x23, cfg & 0xf0)) {
            return fail("INIT FAIL IOE cfg write");
        }
        if (!bits(ioe, 0x13, 0x99, false)) {
            return fail("INIT FAIL IOE drive");
        }
        if (!bits(ioe, 0x03, 0x99, true)) {
            return fail("INIT FAIL IOE mode");
        }
        if (!bits(ioe, 0x05, 0x99, true)) {
            return fail("INIT FAIL IOE output");
        }
        bool power = false;
        for (int n = 0; n < 10; n++) {
            delay(80);
            uint8_t value = 0;
            if (read(ioe, 0x07, &value, 1) && (value & 0x80)) {
                power = true;
                break;
            }
            if (!bits(ioe, 0x05, 0x80, true))
                return fail("IOE power retry failed");
        }
        if (!power) {
            return fail("INIT FAIL IOE power");
        }
        MOUSE_DEBUG("INIT display power OK\n");
        // Reset display and touch after the supply is confirmed high.
        if (!bits(ioe, 0x05, 0x18, false))
            return fail("Display/touch reset assertion failed");
        delay(10);
        if (!bits(ioe, 0x05, 0x18, true))
            return fail("Display/touch reset release failed");
        delay(150);
        spi_bus_config_t bus = {};
        bus.sclk_io_num = 40;
        bus.mosi_io_num = 41;
        bus.miso_io_num = 42;
        bus.quadwp_io_num = 46;
        bus.quadhd_io_num = 45;
        bus.max_transfer_sz = kScreenWidth * kDmaRows * 2;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
        spi_device_interface_config_t dev = {};
        dev.command_bits = 8;
        dev.address_bits = 24;
        dev.clock_speed_hz = kSpiClockHz;
        dev.spics_io_num = 39;
        dev.queue_size = 1;
        dev.flags = SPI_DEVICE_HALFDUPLEX;
        ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &panel));
        dma = (uint8_t*)heap_caps_malloc(kScreenWidth * kDmaRows * 2, MALLOC_CAP_DMA);
        if (!dma) {
            return fail("INIT FAIL DMA");
        }
        cmd(0x11);
        delay(150);
        arg(0xc4, 0x80);
        arg(0x35, 0x80);
        uint8_t tear[] = {uint8_t(kScreenHeight >> 8), uint8_t(kScreenHeight)};
        cmd(0x44, tear, 2);
        arg(0x53, 0x20);
        cmd(0x20);
        arg(0x36, 0);
        arg(0x3a, 0x55);
        arg(0x51, 0x60);
        cmd(0x29);
        delay(20);
        pinMode(kScrollUpPin, INPUT_PULLUP);
        pinMode(kScrollDownPin, INPUT_PULLUP);
        refreshBattery();
        return true;
    }
    TouchFrame touch() {
        uint8_t bytes[13]{};
        const bool ok = read(kTouchAddress, 0, bytes, sizeof(bytes));
        TouchFrame frame = decodeTouchFrame(bytes, sizeof(bytes));
        if (!ok)
            frame.valid = false;
        if (!frame.valid) {
            ++touchErrors;
            if (touchErrors == 1 || millis() - lastTouchError >= 1000) {
                MOUSE_ERROR("TOUCH ERROR read=%d count=%u total=%lu\n", ok, bytes[2],
                            (unsigned long)touchErrors);
                lastTouchError = millis();
            }
        }
        return frame;
    }
    uint32_t touchErrorCount() const { return touchErrors; }
    uint8_t buttons() {
        uint8_t raw = (!digitalRead(kScrollDownPin) ? 1 : 0) | (!digitalRead(kScrollUpPin) ? 2 : 0);
        if (raw != candidateButtons) {
            candidateButtons = raw;
            buttonChanged = millis();
        }
        if (millis() - buttonChanged >= kButtonDebounceMs)
            stableButtons = candidateButtons;
        return stableButtons;
    }
    void refreshBattery() {
        uint8_t bytes[2];
        if (!read(kPmicAddress, 0x22, bytes, sizeof(bytes))) {
            battery = -1;
            MOUSE_ERROR("BATTERY ERROR read failed\n");
            return;
        }
        const int mv = bytes[0] | (bytes[1] << 8);
        if (mv < 2500 || mv > 4500) {
            battery = -1;
            MOUSE_ERROR("BATTERY ERROR invalid voltage=%d\n", mv);
            return;
        }
        battery =
            constrain((mv - kBatteryEmptyMv) * 100 / (kBatteryFullMv - kBatteryEmptyMv), 0, 100);
    }
    int batteryLevel() const { return battery; }
    void flush(M5Canvas& frame, int x = 0, int y = 0, int w = kScreenWidth, int h = kScreenHeight) {
        if (!panel || !dma)
            return;
        int right = min(kScreenWidth, (x + w + 1) & ~1),
            bottom = min(kScreenHeight, (y + h + 1) & ~1);
        x = max(0, x & ~1);
        y = max(0, y & ~1);
        w = right - x;
        h = bottom - y;
        if (w <= 0 || h <= 0)
            return;
        for (int top = y; top < bottom; top += kDmaRows) {
            int rows = min(kDmaRows, bottom - top);
            for (int j = 0; j < rows; j++) {
                frame.readRectRGB(x, top + j, w, 1, rgb);
                for (int i = 0; i < w; i++) {
                    uint16_t c = ((rgb[i * 3] & 0xf8) << 8) | ((rgb[i * 3 + 1] & 0xfc) << 3) |
                                 (rgb[i * 3 + 2] >> 3);
                    int pos = (j * w + i) * 2;
                    dma[pos] = c >> 8;
                    dma[pos + 1] = c;
                }
            }
            int left = x + kPanelOffsetX, end = right + kPanelOffsetX - 1;
            uint8_t col[] = {uint8_t(left >> 8), uint8_t(left), uint8_t(end >> 8), uint8_t(end)};
            uint8_t row[] = {uint8_t(top >> 8), uint8_t(top), uint8_t((top + rows - 1) >> 8),
                             uint8_t(top + rows - 1)};
            cmd(0x2a, col, 4);
            cmd(0x2b, row, 4);
            spi_transaction_t t = {};
            t.cmd = 0x32;
            t.addr = 0x2c00;
            t.flags = SPI_TRANS_MODE_QIO;
            t.length = w * rows * 16;
            t.tx_buffer = dma;
            ESP_ERROR_CHECK(spi_device_polling_transmit(panel, &t));
        }
    }
};

}  // namespace stopwatch
