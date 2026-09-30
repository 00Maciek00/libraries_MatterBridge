// =============================================================================
//  MatterBridge — Example 06: Runtime Add/Remove Cycle Test
//  MatterBridge — Przykład 06: Test cyklu dodawania/usuwania w locie
// =============================================================================
//
//  Stress/compatibility test: adds and removes devices while the bridge runs,
//  WITH persistence in NVS (setRuntimeNVS(true), the default) – after a reboot
//  begin() restores the devices and the test continues.
//  Test obciążeniowy/zgodności: dodaje i usuwa urządzenia podczas pracy mostka,
//  Z zapisem w NVS (setRuntimeNVS(true), domyślnie) – po restarcie begin()
//  odtwarza urządzenia i test jest kontynuowany.
//
//  IMPORTANT / WAŻNE (SmartThings): add at least ONE device BEFORE start().
//  SmartThings reads the Descriptor cluster during commissioning; an empty
//  bridge may be stored without devices and later additions ignored
//  ("2 bridges / no devices").
//  Dodaj co najmniej JEDNO urządzenie PRZED start(). SmartThings czyta klaster
//  Descriptor podczas komisjonowania; pusty mostek może zostać zapisany bez
//  urządzeń, a późniejsze dodania są ignorowane ("2 mostki / brak urządzeń").
//
//  Scenario / Scenariusz:
//    boot / start   1 device before start() / 1 urządzenie przed start()
//    every minute   next device at runtime / kolejne urządzenie w locie
//    all 11 added   wait 2 min – check your hub / czekaj 2 min – sprawdź hub
//    then / potem   remove one per minute / usuń po jednym na minutę
//    when empty     factoryReset() → new cycle / nowy cykl
//
//  Board / Płytka: ESP32-C6 or/lub ESP32-S3, Arduino-ESP32 3.x
//
//  S.M. DIY Home | https://github.com/00Maciek00/libraries_MatterBridge
// =============================================================================

#include <WiFi.h>
#include <MatterBridge.h>

// Optional: leave as is to commission over Bluetooth.
// Opcjonalnie: zostaw bez zmian, by komisjonować przez Bluetooth.
const char* WIFI_SSID = "YourWiFi";
const char* WIFI_PASS = "YourPassword";

struct DeviceDef { const char* name; MBDeviceType type; };
static const DeviceDef DEVICES[] = {
  { "Window",        MBDeviceType::ContactSensor     },
  { "Hall PIR",      MBDeviceType::PresenceSensor    },
  { "Button",        MBDeviceType::Switch            },
  { "Thermometer",   MBDeviceType::TemperatureSensor },
  { "Humidity",      MBDeviceType::HumiditySensor    },
  { "Lamp On/Off",   MBDeviceType::OnOffLight        },
  { "Plug",          MBDeviceType::OnOffPlug         },
  { "Lamp Dimmer",   MBDeviceType::DimmableLight     },
  { "Lamp CT",       MBDeviceType::ColorTempLight    },
  { "Fan",           MBDeviceType::Fan               },
  { "Blind",         MBDeviceType::WindowCovering    },
};
static const uint8_t DEVICE_COUNT = sizeof(DEVICES) / sizeof(DEVICES[0]);

static const uint32_t ADD_INTERVAL_MS = 60000;    // also the remove interval / także odstęp usuwania
static const uint32_t FULL_WAIT_MS    = 120000;
static const uint32_t EMPTY_WAIT_MS   = 10000;

enum class Phase { ADDING, FULL_WAIT, REMOVING, EMPTY_WAIT };
static Phase    phase        = Phase::ADDING;
static uint32_t lastActionMs = 0;
static uint8_t  nextIdx      = 0;
static uint8_t  slots[DEVICE_COUNT];
static uint8_t  slotCount    = 0;

static void onDeviceChange(uint8_t slot, bool state) {
  Serial.printf("[CB] slot %u -> %s\n", slot, state ? "ON" : "OFF");
}

static uint8_t addDevice(const DeviceDef& d) {
  return MatterBridge.addRuntime(MBDescriptor(d.type, d.name, MB_NO_PIN, false));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== MatterBridge - runtime add/remove cycle test ===");

  if (!MatterBridge.begin()) {
    Serial.println("[ERR] begin() failed");
    while (true) delay(1000);
  }
  MatterBridge.onChange(onDeviceChange);

  if (MatterBridge.registeredCount() > 0) {
    // begin() restored devices from NVS - continue the test.
    // begin() odtworzył urządzenia z NVS - kontynuuj test.
    slotCount = MatterBridge.registeredCount();
    nextIdx   = slotCount;
    for (uint8_t i = 0; i < slotCount; i++) slots[i] = i;
    Serial.printf("[APP] Restored %u device(s) from NVS\n", slotCount);
    phase = (slotCount >= DEVICE_COUNT) ? Phase::FULL_WAIT : Phase::ADDING;
  } else {
    // KEY STEP: first device BEFORE start() and commissioning.
    // KLUCZOWE: pierwsze urządzenie PRZED start() i komisjonowaniem.
    uint8_t slot = addDevice(DEVICES[0]);
    if (slot != MB_INVALID_SLOT) {
      slots[slotCount++] = slot;
      nextIdx = 1;
      Serial.printf("[APP] '%s' -> slot %u\n", DEVICES[0].name, slot);
    } else {
      Serial.println("[ERR] first device failed");
    }
    phase = Phase::ADDING;
  }

  if (strcmp(WIFI_SSID, "YourWiFi") != 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (uint8_t t = 0; WiFi.status() != WL_CONNECTED && t < 20; t++) {
      delay(500);
      Serial.print('.');
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? "\n[APP] WiFi OK" : "\n[WARN] no WiFi");
  } else {
    Serial.println("[APP] No WiFi configured - commission over Bluetooth");
  }

  if (!MatterBridge.start()) {
    Serial.println("[ERR] start() failed");
    while (true) delay(1000);
  }

  if (!MatterBridge.isCommissioned()) {
    char code[32] = {0}, qr[128] = {0};
    if (MatterBridge.getPairingCode(code, sizeof(code))) Serial.printf("[APP] Pairing code: %s\n", code);
    if (MatterBridge.getQRCode(qr, sizeof(qr)))          Serial.printf("[APP] QR payload: %s\n", qr);
  } else {
    Serial.println("[APP] Already commissioned - resuming test");
  }

  MatterBridge.printStatus();
  lastActionMs = millis();
}

void loop() {
  MatterBridge.update();
  MatterBridge.checkBoot();

  uint32_t now = millis();

  switch (phase) {

    case Phase::ADDING:
      if (now - lastActionMs >= ADD_INTERVAL_MS) {
        lastActionMs = now;
        if (nextIdx < DEVICE_COUNT) {
          const DeviceDef& d = DEVICES[nextIdx];
          uint8_t slot = addDevice(d);
          if (slot != MB_INVALID_SLOT) {
            slots[slotCount++] = slot;
            Serial.printf("[ADD] %u/%u '%s' -> slot %u (total: %u)\n",
                          nextIdx + 1, DEVICE_COUNT, d.name, slot,
                          MatterBridge.registeredCount());
          } else {
            Serial.printf("[ERR] adding '%s' failed\n", d.name);
          }
          nextIdx++;
        }
        if (nextIdx >= DEVICE_COUNT) {
          Serial.printf("[APP] All devices added. Check your hub; removal starts in %lus\n",
                        (unsigned long)(FULL_WAIT_MS / 1000));
          phase        = Phase::FULL_WAIT;
          lastActionMs = now;
        }
      }
      break;

    case Phase::FULL_WAIT:
      if (now - lastActionMs >= FULL_WAIT_MS) {
        Serial.println("[APP] Removing one device per minute...");
        phase        = Phase::REMOVING;
        nextIdx      = 0;
        lastActionMs = now;
      }
      break;

    case Phase::REMOVING:
      if (now - lastActionMs >= ADD_INTERVAL_MS) {
        lastActionMs = now;
        if (nextIdx < slotCount) {
          uint8_t slot = slots[nextIdx];
          if (MatterBridge.removeRuntime(slot)) {
            Serial.printf("[REM] slot %u removed (left: %u)\n",
                          slot, MatterBridge.registeredCount());
          } else {
            Serial.printf("[ERR] removing slot %u failed\n", slot);
          }
          nextIdx++;
        }
        if (nextIdx >= slotCount) {
          Serial.printf("[APP] All removed. New cycle in %lus\n",
                        (unsigned long)(EMPTY_WAIT_MS / 1000));
          phase        = Phase::EMPTY_WAIT;
          lastActionMs = now;
        }
      }
      break;

    case Phase::EMPTY_WAIT:
      if (now - lastActionMs >= EMPTY_WAIT_MS) {
        Serial.println("[APP] Factory reset - starting a new cycle");
        MatterBridge.factoryReset();   // clears NVS and restarts / czyści NVS i restartuje
      }
      break;
  }

  static uint32_t lastStatusMs = 0;
  if (now - lastStatusMs >= 30000) {
    lastStatusMs = now;
    const char* p = phase == Phase::ADDING    ? "ADDING"
                  : phase == Phase::FULL_WAIT ? "FULL_WAIT"
                  : phase == Phase::REMOVING  ? "REMOVING" : "EMPTY_WAIT";
    Serial.printf("[STATUS] %s devices=%u commissioned=%d\n",
                  p, MatterBridge.registeredCount(), (int)MatterBridge.isCommissioned());
  }
}
