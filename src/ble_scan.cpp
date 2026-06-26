// ---------------------------------------------------------------------------
// BLE scanner — investigation tool (NOT the PSU controller firmware).
//
// Purpose: find out whether the 8BitDo controller emits a BLE advertisement
// when it powers on, and capture its fingerprint (address + raw payload) so the
// real firmware can watch for it.
//
// Build/flash this instead of main.cpp:
//     pio run -e scanner -t upload && pio device monitor
// Then go back to the controller firmware with:
//     pio run -e esp32-c3-devkitm-1 -t upload
//
// NOTE: the ESP32-C3 radio is BLE-only. Controllers paired in a Bluetooth
// Classic mode (Switch/Android/X-input on many 8BitDo pads) will NOT appear
// here at all. If nothing new shows up when you power the controller, try its
// macOS / BLE pairing mode, or it's using Classic BT and the C3 can't see it.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <NimBLEDevice.h>

// Each distinct device is reported once (duplicate filter on), so a controller
// that you power on mid-scan pops out as a fresh line. Active scan so we also
// pull the scan-response packet (where names usually live).
static const bool ACTIVE_SCAN = true;

static std::string toHex(const std::vector<uint8_t> &data) {
  static const char *hex = "0123456789abcdef";
  std::string out;
  out.reserve(data.size() * 3);
  for (size_t i = 0; i < data.size(); i++) {
    if (i) out += ' ';
    out += hex[data[i] >> 4];
    out += hex[data[i] & 0x0F];
  }
  return out;
}

class ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *dev) override {
    std::string name = dev->getName();

    Serial.println("------------------------------------------------------------");
    Serial.printf("addr   : %s (type %u)\n",
                  dev->getAddress().toString().c_str(),
                  dev->getAddressType());
    Serial.printf("rssi   : %d dBm\n", dev->getRSSI());
    Serial.printf("name   : %s\n", name.empty() ? "<none>" : name.c_str());

    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      std::vector<uint8_t> bytes(md.begin(), md.end());
      Serial.printf("mfg    : %s\n", toHex(bytes).c_str());
    }

    uint8_t uuidCount = dev->getServiceUUIDCount();
    for (uint8_t i = 0; i < uuidCount; i++) {
      Serial.printf("svc[%u] : %s\n", i, dev->getServiceUUID(i).toString().c_str());
    }

    Serial.printf("payload: %s\n", toHex(dev->getPayload()).c_str());
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override {
    Serial.printf("[SCAN] cycle ended (reason %d, %d devices) — restarting\n",
                  reason, results.getCount());
    NimBLEDevice::getScan()->start(0, false);
  }
};

static ScanCallbacks scanCallbacks;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 2000) {
    delay(10);
  }

  Serial.println();
  Serial.println("=== BLE scanner (8BitDo investigation) ===");
  Serial.println("Power your controller on now and watch for a new 'addr' line.");

  NimBLEDevice::init("");

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&scanCallbacks, false);  // false = filter duplicates
  scan->setActiveScan(ACTIVE_SCAN);
  scan->setInterval(100);  // ms
  scan->setWindow(99);     // ms (must be <= interval)
  scan->start(0, false);   // 0 = scan continuously
}

void loop() {
  delay(1000);
}
