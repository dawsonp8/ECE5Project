// ESP32 heart-rate bridge (ESP32 DevKit V1): pio run -e esp32_hr -t upload
//
// Connects to a Garmin watch (Forerunner 265S) that is broadcasting heart rate
// over Bluetooth LE, using the standard Heart Rate service (0x180D) and its
// Heart Rate Measurement characteristic (0x2A37), and forwards each reading
// to the Uno as a text line:
//   "HR 87\n"   one per watch update (about 1 per second)
//   "HR 0\n"    once per second while not connected (Uno uses its fallback)
//
// Wiring: ESP32 GPIO17 (TX2) -> Uno D0 (RX), ESP32 GND -> Uno GND. Power the
// ESP32 from its own USB cable. Only ESP32 -> Uno is used, and the ESP32's
// 3.3 V high is above the Uno's 3.0 V input threshold, so no level shifter.
// Unplug the GPIO17 wire from D0 while uploading to the Uno.
//
// Watch: Settings > Sensors & Accessories > Wrist Heart Rate > Broadcast
// Heart Rate. The USB serial monitor (115200) lists every heart-rate device
// found; set WATCH_NAME if other watches nearby are broadcasting too.
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

#include "config.h"  // HR_SERIAL_BAUD

static const uint8_t PIN_UNO_TX = 17;  // TX2 on the DevKit V1
static const uint8_t PIN_UNO_RX = 16;  // unused, but Serial2 wants a pin

// Connect only to a device whose advertised name contains this text.
// Empty = the first heart-rate broadcaster found.
static const char *const WATCH_NAME = "";

static const NimBLEUUID HR_SERVICE((uint16_t)0x180D);
static const NimBLEUUID HR_MEASUREMENT((uint16_t)0x2A37);

static NimBLEClient *client;
static NimBLEAddress targetAddr;
static volatile bool haveTarget;
static uint32_t lastLostMsg;
static bool wasConnected;

// Heart Rate Measurement: byte 0 = flags; bit 0 set means a 16-bit BPM.
static void onMeasurement(NimBLERemoteCharacteristic *, uint8_t *data, size_t len, bool) {
  if (len < 2) return;
  uint16_t bpm = data[1];
  if ((data[0] & 0x01) && len >= 3) bpm = data[1] | (uint16_t)(data[2] << 8);
  Serial2.printf("HR %u\n", bpm);
  Serial.printf("HR %u\n", bpm);
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *dev) override {
    if (haveTarget || !dev->isAdvertisingService(HR_SERVICE)) return;
    std::string name = dev->getName();
    Serial.printf("Found heart-rate device: \"%s\" %s\n", name.c_str(),
                  dev->getAddress().toString().c_str());
    if (WATCH_NAME[0] && strstr(name.c_str(), WATCH_NAME) == nullptr) return;
    targetAddr = dev->getAddress();
    haveTarget = true;
    NimBLEDevice::getScan()->stop();
  }
};

static ScanCallbacks scanCallbacks;

static bool connectAndSubscribe() {
  Serial.printf("Connecting to %s\n", targetAddr.toString().c_str());
  if (!client->connect(targetAddr)) return false;
  NimBLERemoteService *svc = client->getService(HR_SERVICE);
  NimBLERemoteCharacteristic *chr = svc ? svc->getCharacteristic(HR_MEASUREMENT) : nullptr;
  if (!chr || !chr->canNotify() || !chr->subscribe(true, onMeasurement)) {
    client->disconnect();
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(HR_SERIAL_BAUD, SERIAL_8N1, PIN_UNO_RX, PIN_UNO_TX);
  Serial.println("ESP32 heart-rate bridge");

  NimBLEDevice::init("");
  client = NimBLEDevice::createClient();
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);
  scan->setActiveScan(true);  // ask for scan responses, which carry the name
}

void loop() {
  if (client->isConnected()) {
    if (!wasConnected) Serial.println("Connected, streaming heart rate");
    wasConnected = true;
    delay(50);
    return;
  }

  if (wasConnected) {
    Serial.println("Watch disconnected, scanning again");
    wasConnected = false;
    haveTarget = false;
  }

  uint32_t now = millis();
  if (now - lastLostMsg >= 1000) {
    lastLostMsg = now;
    Serial2.print("HR 0\n");
  }

  NimBLEScan *scan = NimBLEDevice::getScan();
  if (haveTarget) {
    if (scan->isScanning()) return;  // wait for stop() to take effect
    if (connectAndSubscribe()) return;
    Serial.println("Connect failed, scanning again");
    haveTarget = false;
  }
  if (!scan->isScanning()) {
    Serial.println("Scanning for heart-rate broadcast...");
    scan->start(5000, false, true);  // 5 s, non-blocking; results via callback
  }
  delay(20);
}
