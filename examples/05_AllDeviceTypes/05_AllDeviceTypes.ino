// =============================================================================
//  MatterBridge — Example 05: All Device Types
//  MatterBridge — Przykład 05: Wszystkie typy urządzeń
// =============================================================================
//
//  Version / Wersja: 1.0.2
//  Author / Autor:   Maciej Sikorski
//  Date / Data:      2026-09-30
//  Project / Projekt: S.M. DIY Home
//  Repository / Repozytorium: https://github.com/00Maciek00/libraries_MatterBridge
//
//  One of every device type, all virtual (no GPIO). Shows how to read what the
//  hub wrote (dimmer level, fan mode, blind position) and how to push state
//  from code (presence, button events, energy meter).
//  Po jednym urządzeniu każdego typu, wszystkie wirtualne (bez GPIO). Pokazuje
//  odczyt tego, co zapisał hub (poziom dimmera, tryb wentylatora, pozycja
//  rolety) oraz wysyłanie stanu z kodu (obecność, zdarzenia przycisku, licznik
//  energii).
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

static uint8_t sLight, sDimmer, sCt, sPlug, sFan, sCover, sPresence, sButton, sEnergy;

// The callback gets (slot, bool); richer values live in device->attrs().
// Callback dostaje (slot, bool); bogatsze wartości są w device->attrs().
static void onChange(uint8_t slot, bool state) {
  MBDevice* d = MatterBridge.device(slot);
  if (!d) return;
  if (slot == sDimmer)     Serial.printf("Dimmer level=%u\n", d->attrs().level);
  else if (slot == sFan)   Serial.printf("Fan mode=%u\n",  d->attrs().fanMode);
  else if (slot == sCover) Serial.printf("Blind position=%u%%\n", d->attrs().coveringPos);
  else Serial.printf("%s (slot %u) -> %d\n", d->getName(), slot, (int)state);
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

  sLight    = MatterBridge.addOnOffLightRuntime("Light");
  sDimmer   = MatterBridge.addDimmableLightRuntime("Dimmer");
  sCt       = MatterBridge.addColorTempLightRuntime("CT Light");
  sPlug     = MatterBridge.addOnOffPlugRuntime("Plug");
  sFan      = MatterBridge.addFanRuntime("Fan");
  sCover    = MatterBridge.addWindowCoveringRuntime("Blind");
  sPresence = MatterBridge.addPresenceSensorRuntime("Presence");
  sButton   = MatterBridge.addSwitchRuntime("Button");
  sEnergy   = MatterBridge.addElectricalSensorRuntime("Energy meter");
  MatterBridge.onChange(onChange);

  if (!MatterBridge.start()) { while (true) delay(1000); }
  printPairingInfo();
  MatterBridge.printStatus();
}

void loop() {
  MatterBridge.update();
  MatterBridge.checkBoot();

  static unsigned long last = 0;
  static bool present = false;
  if (millis() - last >= 15000UL) {
    last = millis();

    present = !present;
    MatterBridge.setState(sPresence, present);     // occupancy / obecność

    if (auto* btn = static_cast<MBSwitch*>(MatterBridge.device(sButton)))
      btn->sendMultiPress(2);                      // double press / podwójne kliknięcie

    if (auto* em = static_cast<MBElectricalPlug*>(MatterBridge.device(sEnergy))) {
      em->setVoltage(23050);      // 230.50 V  (unit / jednostka 0.01 V)
      em->setCurrent(500);        // 0.500 A   (0.001 A)
      em->setActivePower(1150);   // 115.0 W   (0.1 W)
      em->setEnergy(1000);        // 1000 Wh
    }
  }
}
