// =============================================================================
//  MatterBridge — Example 02: Contact Sensor on a GPIO
//  MatterBridge — Przykład 02: Czujnik kontaktronowy na GPIO
// =============================================================================
//
//  Version / Wersja: 1.0.2
//  Author / Autor:   Maciej Sikorski
//  Date / Data:      2026-09-30
//  Project / Projekt: S.M. DIY Home
//  Repository / Repozytorium: https://github.com/00Maciek00/libraries_MatterBridge
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
// =============================================================================
//  Copyright 2026 Maciej Sikorski — S.M. DIY Home
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
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
