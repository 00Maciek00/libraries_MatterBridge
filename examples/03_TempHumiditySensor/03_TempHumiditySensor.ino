// =============================================================================
//  MatterBridge — Example 03: Temperature + Humidity Sensor
//  MatterBridge — Przykład 03: Czujnik temperatury i wilgotności
// =============================================================================
//
//  Version / Wersja: 1.0.2
//  Author / Autor:   Maciej Sikorski
//  Date / Data:      2026-09-30
//  Project / Projekt: S.M. DIY Home
//  Repository / Repozytorium: https://github.com/00Maciek00/libraries_MatterBridge
//
//  Push measurements from your own sensor code. Sensors have no GPIO: call
//  setTemperature()/setHumidity() when you have a new reading. This sketch
//  generates fake values – replace readSensor() with your driver
//  (DHT, BME280, SHT3x...). Call the setters only AFTER start().
//  Wysyłaj pomiary z własnego kodu czujnika. Czujniki nie mają GPIO: wołaj
//  setTemperature()/setHumidity() przy nowym odczycie. Szkic generuje fałszywe
//  wartości – zastąp readSensor() swoim sterownikiem (DHT, BME280, SHT3x...).
//  Settery wołaj dopiero PO start().
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

static uint8_t climateSlot = MB_INVALID_SLOT;

// Replace with a real driver / Zastąp prawdziwym sterownikiem
static void readSensor(float& tC, float& rh) {
  tC = 21.0f + 2.0f * sinf(millis() / 60000.0f);
  rh = 50.0f + 5.0f * cosf(millis() / 90000.0f);
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

  climateSlot = MatterBridge.addTempHumidSensorRuntime("Climate");

  if (!MatterBridge.start()) { while (true) delay(1000); }
  printPairingInfo();
}

void loop() {
  MatterBridge.update();
  MatterBridge.checkBoot();

  static unsigned long last = 0;
  if (millis() - last >= 10000UL) {
    last = millis();
    float t, h;
    readSensor(t, h);
    auto* dev = static_cast<MBTempHumidSensor*>(MatterBridge.device(climateSlot));
    if (dev) {
      dev->setTemperature(t);   // °C
      dev->setHumidity(h);      // %RH
      Serial.printf("Reported / Wyslano: %.2f C  %.2f %%RH\n", t, h);
    }
  }
}
