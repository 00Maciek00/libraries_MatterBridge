// =============================================================================
//  MatterBridge — Example 04: Add / Remove at Runtime
//  MatterBridge — Przykład 04: Dodawanie / usuwanie w locie
// =============================================================================
//
//  Version / Wersja: 1.0.2
//  Author / Autor:   Maciej Sikorski
//  Date / Data:      2026-09-30
//  Project / Projekt: S.M. DIY Home
//  Repository / Repozytorium: https://github.com/00Maciek00/libraries_MatterBridge
//
//  Change the device list while the bridge runs. After 20 s a smart plug
//  appears in your app, after 60 s it is removed – no restart. addRuntimeAt()
//  pins a device to a fixed slot = stable endpoint id between reboots
//  (create devices in the same order/slots every time).
//  Zmieniaj listę urządzeń, gdy mostek działa. Po 20 s w aplikacji pojawia się
//  gniazdko, po 60 s znika – bez restartu. addRuntimeAt() przypina urządzenie
//  do stałego slotu = stały endpoint id między restartami (twórz urządzenia
//  zawsze w tej samej kolejności / slotach).
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

static uint8_t plugSlot = MB_INVALID_SLOT;

static void onChange(uint8_t slot, bool state) {
  Serial.printf("slot %u -> %s\n", slot, state ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  MatterBridge.setRuntimeNVS(false);
  if (!MatterBridge.begin()) { while (true) delay(1000); }

  // Fixed slot 0 for the light (valid: 0 .. MB_MAX_DEVICES-1).
  // Stały slot 0 dla lampy (dozwolone: 0 .. MB_MAX_DEVICES-1).
  MatterBridge.addRuntimeAt(0, MBDescriptor(MBDeviceType::OnOffLight, "Lamp", MB_NO_PIN, false));
  MatterBridge.onChange(onChange);

  if (!MatterBridge.start()) { while (true) delay(1000); }
}

void loop() {
  MatterBridge.update();
  MatterBridge.checkBoot();

  static bool added = false, removed = false;
  unsigned long s = millis() / 1000;

  if (!added && s >= 20) {
    added = true;
    plugSlot = MatterBridge.addOnOffPlugRuntime("Plug");
    Serial.printf("Plug added / dodano, slot=%u\n", plugSlot);
  }
  if (added && !removed && s >= 60) {
    removed = true;
    MatterBridge.removeRuntime(plugSlot);
    Serial.println("Plug removed / usunieto");
  }
}
