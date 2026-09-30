// =============================================================================
//  MatterBridge — Example 02: Contact Sensor on a GPIO
//  MatterBridge — Przykład 02: Czujnik kontaktronowy na GPIO
// =============================================================================
//
//  Door/window sensor wired to a GPIO. pinInverted=true enables INPUT_PULLUP
//  and treats LOW as "contact closed". update() debounces (MB_DEBOUNCE_MS).
//  Czujnik drzwi/okna na GPIO. pinInverted=true włącza INPUT_PULLUP i traktuje
//  LOW jako "kontakt zamknięty". update() robi debounce (MB_DEBOUNCE_MS).
//
//  Wiring / Podłączenie:
//    Reed switch / Kontaktron: SENSOR_PIN → GND
//
//  Board / Płytka: ESP32-C6 or/lub ESP32-S3, Arduino-ESP32 3.x
//  Partition scheme / Schemat partycji: e.g./np. "Huge APP"
//  Commission over Bluetooth (see Serial Monitor).
//  Komisjonuj przez Bluetooth (patrz Serial Monitor).
//
//  S.M. DIY Home | https://github.com/00Maciek00/libraries_MatterBridge
// =============================================================================

#include <MatterBridge.h>

#define SENSOR_PIN 4   // free GPIO / wolny GPIO

static void onChange(uint8_t slot, bool state) {
  Serial.printf("Door (slot %u): %s\n", slot, state ? "closed / zamkniete" : "open / otwarte");
}

static void printPairingInfo() {
  if (MatterBridge.isCommissioned()) {
    Serial.println("Already commissioned / Juz skomisjonowany");
    return;
  }
  char code[16] = {0};
  char qr[128]  = {0};
  if (MatterBridge.getPairingCode(code, sizeof(code))) Serial.printf("Pairing code / Kod parowania: %s\n", code);
  if (MatterBridge.getQRCode(qr, sizeof(qr)))          Serial.printf("QR payload: %s\n", qr);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  MatterBridge.setRuntimeNVS(false);
  if (!MatterBridge.begin()) { while (true) delay(1000); }

  MatterBridge.addContactSensorRuntime("Door", SENSOR_PIN, /*pinInverted=*/true);
  MatterBridge.onChange(onChange);

  if (!MatterBridge.start()) { while (true) delay(1000); }
  printPairingInfo();
}

void loop() {
  MatterBridge.update();
  MatterBridge.checkBoot();
}
