// SPDX-License-Identifier: MIT
#include "StopWatchHardware.h"
#include "TrackpadUI.h"
#include "MouseGestures.h"
#include "ClickSequence.h"
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <esp_mac.h>
#include <atomic>

using namespace stopwatch;

// Three button bits, signed relative X/Y and vertical wheel; report ID 1.
static uint8_t reportMap[] = {0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xA1,
                              0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01,
                              0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81,
                              0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81,
                              0x25, 0x7F, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0};
static constexpr char kVersion[] = "1.0.1";
static constexpr uint16_t kAdvertisingInterval = 0x20;  // 20 ms in 0.625 ms units.
static std::atomic<bool> connected{false}, resetSession{false};
static std::atomic<bool> encrypted{false}, subscribed{false}, retryReport{false};
static std::atomic<uint32_t> sent{0}, notifyErrors{0};
static uint32_t reportAttempts = 0;
static char deviceName[sizeof("StopWatch FFFF")];
static BLEServer* mouseServer = nullptr;
static BLEHIDDevice* hid = nullptr;
static BLECharacteristic* mouseReport = nullptr;
static StopWatchHardware hardware;
static TrackpadUI trackpad(hardware);
static MouseGestures gestures;
static ClickSequence clicks;
static uint8_t previousScroll = 0, lastButtons = 0, clickPulse = 0;
static uint32_t nextScroll = 0, lastUi = 0, lastBattery = 0;
static bool requireRelease = true, touchTrace = false;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        connected = true;
        resetSession = true;
        MOUSE_DEBUG("BLE CONNECT t=%lu\n", (unsigned long)millis());
    }
    void onDisconnect(BLEServer*) override {
        connected = false;
        encrypted = false;
        subscribed = false;
        resetSession = true;
    }
};

class ReportCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* characteristic) override {
        (void)characteristic;
        MOUSE_DEBUG("HID READ handle=%u\n", characteristic->getHandle());
    }
    void onWrite(BLECharacteristic* characteristic) override {
        (void)characteristic;
        MOUSE_DEBUG("HID WRITE handle=%u bytes=%u\n", characteristic->getHandle(),
                    (unsigned)characteristic->getValue().length());
    }
    void onStatus(BLECharacteristic*, Status status, uint32_t code) override {
        if (status == SUCCESS_NOTIFY) {
            ++sent;
        } else {
            ++notifyErrors;
            retryReport = true;
            MOUSE_ERROR("HID NOTIFY ERROR status=%d code=%lu\n", int(status), (unsigned long)code);
        }
    }
};
static ReportCallbacks reportCallbacks;

class SecurityCallbacks : public BLESecurityCallbacks {
    bool onSecurityRequest() override { return true; }
#if defined(CONFIG_NIMBLE_ENABLED)
    void onAuthenticationComplete(ble_gap_conn_desc* peer) override {
        encrypted = peer && peer->sec_state.encrypted;
        if (!encrypted)
            MOUSE_ERROR("BLE AUTH ERROR encryption failed\n");
        MOUSE_DEBUG("BLE AUTH encrypted=%d bonded=%d\n", int(encrypted.load()),
                    peer ? peer->sec_state.bonded : 0);
    }
#endif
#if defined(CONFIG_BLUEDROID_ENABLED)
    void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
        encrypted = result.success;
        if (!result.success)
            MOUSE_ERROR("BLE AUTH ERROR reason=0x%02x\n", result.fail_reason);
    }
#endif
};

#if defined(CONFIG_NIMBLE_ENABLED)
static ble_gap_event_listener gapListener;
static int handleGap(ble_gap_event* event, void*) {
    if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        MOUSE_DEBUG("BLE DISCONNECT reason=0x%x\n", event->disconnect.reason);
    }
    if (event->type == BLE_GAP_EVENT_ENC_CHANGE && event->enc_change.status != 0) {
        MOUSE_ERROR("BLE ENCRYPT ERROR status=0x%x\n", event->enc_change.status);
    }
    if (event->type == BLE_GAP_EVENT_SUBSCRIBE) {
        if (mouseReport && event->subscribe.attr_handle == mouseReport->getHandle()) {
            subscribed = event->subscribe.cur_notify;
        }
        MOUSE_DEBUG("BLE SUBSCRIBE handle=%u notify=%d\n", event->subscribe.attr_handle,
                    event->subscribe.cur_notify);
    }
    return 0;
}
#endif

static void halt(const char* reason) {
    while (true) {
        MOUSE_ERROR("INIT FAILED: %s\n", reason);
        delay(1000);
    }
}

static void sendReport(uint8_t buttons, int8_t dx = 0, int8_t dy = 0, int8_t wheel = 0) {
    const uint8_t report[] = {buttons, uint8_t(dx), uint8_t(dy), uint8_t(wheel)};
    ++reportAttempts;
    mouseReport->setValue(report, sizeof(report));
    mouseReport->notify();
    lastButtons = buttons;
}

static void printDiagnostics(const TouchFrame& touch) {
    auto* advertising = BLEDevice::getAdvertising();
    Serial.printf("BLE AD name=%s advertising=%d\n", deviceName, advertising->isAdvertising());
    Serial.printf(
        "BLE STATUS peers=%lu encrypted=%d subscribed=%d attempts=%lu sent=%lu errors=%lu\n",
        (unsigned long)mouseServer->getConnectedCount(), int(encrypted.load()),
        int(subscribed.load()), (unsigned long)reportAttempts, (unsigned long)sent.load(),
        (unsigned long)notifyErrors.load());
    Serial.printf("READY v%s screen=%dx%d touch_valid=%d contacts=%u touch_errors=%lu uptime=%lu\n",
                  kVersion, kScreenWidth, kScreenHeight, touch.valid, touch.count,
                  (unsigned long)hardware.touchErrorCount(), (unsigned long)millis());
    for (uint8_t i = 0; i < touch.count; ++i) {
        Serial.printf("TOUCH id=%u x=%d y=%d\n", touch.points[i].id, touch.points[i].x,
                      touch.points[i].y);
    }
}

void setup() {
    Serial.begin(115200);
    MOUSE_DEBUG("M5StopWatch BLE Mouse %s\n", kVersion);
    if (!hardware.begin())
        halt(hardware.error);
    if (!trackpad.begin())
        halt("UI framebuffer allocation failed");
    trackpad.status(false);

    uint8_t mac[6];
    if (esp_efuse_mac_get_default(mac) != ESP_OK)
        halt("Factory MAC read failed");
    snprintf(deviceName, sizeof(deviceName), "StopWatch %02X%02X", mac[4], mac[5]);
    if (!BLEDevice::init(deviceName))
        halt("BLE initialization failed");
#if defined(CONFIG_NIMBLE_ENABLED)
    if (ble_gap_event_listener_register(&gapListener, handleGap, nullptr) != 0)
        halt("BLE GAP listener failed");
#endif
    mouseServer = BLEDevice::createServer();
    if (!mouseServer)
        halt("BLE server allocation failed");
    mouseServer->setCallbacks(new ServerCallbacks());
    mouseServer->advertiseOnDisconnect(true);
    hid = new BLEHIDDevice(mouseServer);
    mouseReport = hid->inputReport(1);
    if (!mouseReport)
        halt("HID input report allocation failed");
    mouseReport->setCallbacks(&reportCallbacks);
    hid->protocolMode()->setCallbacks(&reportCallbacks);
    hid->hidControl()->setCallbacks(&reportCallbacks);
    for (uint16_t id : {uint16_t(0x2a4b), uint16_t(0x2a4a)}) {
        hid->hidService()->getCharacteristic(BLEUUID(id))->setCallbacks(&reportCallbacks);
    }
    auto* pnp = hid->deviceInfo()->getCharacteristic(BLEUUID(uint16_t(0x2a50)));
    // 0xffff is a reserved test vendor ID: this project has no assigned vendor ID.
    // Encode the fields explicitly in little-endian order.
    uint8_t pnpValue[] = {0x01, 0xff, 0xff, 0x01, 0x00, 0x00, 0x01};
    pnp->setValue(pnpValue, sizeof(pnpValue));
    pnp->setCallbacks(&reportCallbacks);
    const uint8_t released[4] = {};
    mouseReport->setValue(released, sizeof(released));
    // The accessor creates the optional manufacturer characteristic before assigning its value.
    hid->manufacturer()->setValue("M5StopWatch BLE Mouse");
    hid->hidInfo(0, 0x02);
    hid->reportMap(reportMap, sizeof(reportMap));
    BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
    BLESecurity::setAuthenticationMode(true, false, true);
    BLESecurity::setCapability(ESP_IO_CAP_NONE);
    hid->startServices();
    if (hardware.batteryLevel() >= 0)
        hid->setBatteryLevel(hardware.batteryLevel());

    auto* advertising = mouseServer->getAdvertising();
    advertising->setAppearance(HID_MOUSE);
    advertising->setMinInterval(kAdvertisingInterval);
    advertising->setMaxInterval(kAdvertisingInterval);
    // Keep the full name and HID UUID in the primary advertisement for passive scanners.
    BLEAdvertisementData advertisement;
    advertisement.setFlags(0x06);
    advertisement.setCompleteServices(BLEUUID(uint16_t(0x1812)));
    advertisement.setAppearance(HID_MOUSE);
    advertisement.setName(deviceName);
    if (!advertising->setAdvertisementData(advertisement))
        halt("BLE advertisement configuration failed");
    advertising->setScanResponse(false);
    if (!advertising->start())
        halt("BLE advertising start failed");
    MOUSE_DEBUG("BLE AD name=%s started=1\n", deviceName);
}

void loop() {
    const auto touch = hardware.touch();
    const uint8_t physical = hardware.buttons();
    const uint32_t now = millis();
    const int command = Serial.available() ? Serial.read() : -1;
    if (command == 'P')
        trackpad.dump();
    if (command == '?')
        printDiagnostics(touch);
    if (command == 'T') {
        touchTrace = !touchTrace;
        Serial.printf("TOUCH TRACE enabled=%d\n", touchTrace);
    }

    if (resetSession.exchange(false)) {
        gestures.reset();
        clicks.reset();
        lastButtons = clickPulse = previousScroll = 0;
        requireRelease = true;
        retryReport = false;
        const uint8_t released[4] = {};
        mouseReport->setValue(released, sizeof(released));
        trackpad.status(connected.load());
    }
    bool ready = connected && encrypted;
#if defined(CONFIG_NIMBLE_ENABLED)
    ready = ready && subscribed;
#endif
    MouseInput input;
    if (!ready) {
        gestures.reset();
        clicks.reset();
        clickPulse = previousScroll = 0;
        requireRelease = true;
    } else if (requireRelease) {
        // Input held during pairing/reconnection must not become an unintended click.
        if (touch.valid && !touch.count && !physical) {
            requireRelease = false;
            sendReport(0);
        }
    } else {
        input = gestures.update(touch, now);
        if (!touch.valid || input.buttons)
            clicks.reset();
        else if (input.click)
            clicks.start(input.click, input.clickCount, now);
        clickPulse = clicks.update(now);
        int8_t wheel = 0;
        if (physical != previousScroll) {
            if (physical == 1)
                wheel = -1;
            else if (physical == 2)
                wheel = 1;
            nextScroll = now + kScrollDelayMs;
            previousScroll = physical;
        } else if ((physical == 1 || physical == 2) && int32_t(now - nextScroll) >= 0) {
            wheel = physical == 1 ? -1 : 1;
            nextScroll = now + kScrollRepeatMs;
        }
        const uint8_t buttons = input.buttons | clickPulse;
        if (retryReport.exchange(false) || input.dx || input.dy || wheel ||
            buttons != lastButtons) {
            sendReport(buttons, input.dx, input.dy, wheel);
        }
    }
    if (touchTrace) {
        static uint32_t lastTrace = 0;
        if (uint32_t(now - lastTrace) >= 50) {
            lastTrace = now;
            Serial.printf("TOUCH valid=%d n=%u", touch.valid, touch.count);
            for (uint8_t i = 0; i < touch.count; ++i) {
                Serial.printf(" id=%u x=%d y=%d", touch.points[i].id, touch.points[i].x,
                              touch.points[i].y);
            }
            Serial.printf(" buttons=%u click=%u dx=%d dy=%d\n", input.buttons, input.click,
                          input.dx, input.dy);
        }
    }
    if (uint32_t(now - lastBattery) >= kBatteryIntervalMs) {
        lastBattery = now;
        hardware.refreshBattery();
        if (hardware.batteryLevel() >= 0)
            hid->setBatteryLevel(hardware.batteryLevel());
    }
    if (uint32_t(now - lastUi) >= kStatusIntervalMs && !touch.count && !physical) {
        lastUi = now;
        trackpad.status(connected.load());
    }
    trackpad.update(input.pad, input.x, input.y, input.buttons | clickPulse);
    delay(kPollIntervalMs);
}
