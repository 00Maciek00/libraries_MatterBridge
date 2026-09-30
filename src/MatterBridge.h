// =============================================================================
//  MatterBridge — MatterBridge.h: Public API
//  MatterBridge — MatterBridge.h: Publiczne API
// =============================================================================
//
//  Version / Wersja: 1.0.2
//  Author / Autor:   Maciej Sikorski
//  Date / Data:      2026-09-30
//  Project / Projekt: S.M. DIY Home
//
//  Matter bridge (aggregator) for ESP32-C6 / ESP32-S3: virtual or GPIO-backed
//  devices, runtime add/remove without restart.
//  Most Matter (agregator) dla ESP32-C6 / ESP32-S3: urządzenia wirtualne lub
//  na GPIO, dodawanie/usuwanie w locie bez restartu.
//
// =============================================================================
//  Changelog
// =============================================================================
//
//  v1.0.2 (2026-09-30)
//    - ESP32-C6 and ESP32-S3 support, any board (detected via CONFIG_IDF_TARGET_*)
//      Obsługa ESP32-C6 i ESP32-S3, dowolna płytka (wykrywanie przez CONFIG_IDF_TARGET_*)
//    - S3: slot/descriptor tables in PSRAM (DRAM fallback)
//      S3: tablice slotów/deskryptorów w PSRAM (fallback na DRAM)
//    - addRuntimeAt(): fixed slot = stable endpoint id
//      addRuntimeAt(): stały slot = stały endpoint id
//    - addTempHumidSensorRuntime(), MBTempHumidSensor, MBElectricalPlug
//    - Library is application-independent (no UART/LED pins)
//      Biblioteka niezależna od aplikacji (brak pinów UART/LED)
//    - Bilingual documentation and examples 01-06
//      Dwujęzyczna dokumentacja i przykłady 01-06
//
//  v1.0.1 ALFA (2026-04)
//    - Runtime add/remove without restart / Dodawanie/usuwanie w locie bez restartu
//    - chip_stack_lock around Matter calls / chip_stack_lock wokół wywołań Matter
//
//  v1.0.0
//    - Initial release / Pierwsza wersja
//
// =============================================================================
//  License / Licencja
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

#pragma once
#include <Arduino.h>
#include "MBConfig.h"
#include "MBTypes.h"
#include "MBNode.h"
#include "MBDevice.h"
#include "MBSlotManager.h"
#include "MBStorage.h"

class MatterBridgeClass {
public:
  MatterBridgeClass();

  // ============================================================
  //  INITIALISATION / INICJALIZACJA
  //
  //  Order in setup() / Kolejność w setup():
  //    1. MatterBridge.setRuntimeNVS(false)  (optional / opcjonalnie)
  //    2. MatterBridge.begin()
  //    3. add devices / dodaj urządzenia: addXxxRuntime(...)
  //    4. MatterBridge.onChange(callback)
  //    5. MatterBridge.start()
  //    6. WiFi.begin()  (optional / opcjonalnie)
  // ============================================================
  bool begin();   // Matter node + aggregator / węzeł Matter + agregator
  bool start();   // start Matter stack / uruchom stos Matter

  // ============================================================
  //  NVS MODE FOR RUNTIME DEVICES / TRYB NVS DLA URZĄDZEŃ RUNTIME
  //
  //  true (default / domyślnie): addRuntime/removeRuntime save to / remove from
  //    NVS and begin() restores devices after a reboot.
  //    addRuntime/removeRuntime zapisują / usuwają z NVS,
  //    a begin() odtwarza urządzenia po restarcie.
  //  false: use when you create devices in every setup() (or an external
  //    host is the source of truth) – avoids duplicates after reboot.
  //    Użyj, gdy urządzenia tworzysz w każdym setup() (lub źródłem prawdy
  //    jest zewnętrzny host) – unika duplikatów po restarcie.
  //
  //  Call BEFORE begin() / Wywołaj PRZED begin().
  // ============================================================
  void setRuntimeNVS(bool use);

  // ============================================================
  //  GLOBAL CALLBACK / GLOBALNY CALLBACK
  //  Called when a device state changes (from the hub or from code).
  //  Wywoływany przy zmianie stanu urządzenia (z huba lub z kodu).
  // ============================================================
  void onChange(MBCallback cb);

  // ============================================================
  //  STATIC MODE – saves to NVS and RESTARTS the ESP
  //  TRYB STATYCZNY – zapisuje do NVS i RESTARTUJE ESP
  // ============================================================
  uint8_t add(const MBDescriptor& desc);

  uint8_t addContactSensor    (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = true);
  uint8_t addPresenceSensor   (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addSwitch           (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = true);
  uint8_t addTemperatureSensor(const char* name, uint8_t pin = MB_NO_PIN);
  uint8_t addHumiditySensor   (const char* name, uint8_t pin = MB_NO_PIN);
  uint8_t addOnOffLight       (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addOnOffPlug        (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addDimmableLight    (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addColorTempLight   (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addFan              (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addWindowCovering   (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addElectricalSensor (const char* name, uint8_t pin = MB_NO_PIN);

  bool remove(uint8_t slot);

  // ============================================================
  //  RUNTIME MODE – no restart (recommended)
  //  TRYB RUNTIME – bez restartu (zalecany)
  //
  //  Call after begin(): before start() or after start().
  //  Wywołuj po begin(): przed start() lub po start().
  //  Returns the slot number or MB_INVALID_SLOT.
  //  Zwraca numer slotu lub MB_INVALID_SLOT.
  // ============================================================
  uint8_t addRuntime(const MBDescriptor& desc);

  // Same as addRuntime() but with a fixed slot (stable endpoint id).
  // To samo co addRuntime(), ale ze stałym slotem (stały endpoint id).
  uint8_t addRuntimeAt(uint8_t preferredSlot, const MBDescriptor& desc);

  uint8_t addContactSensorRuntime    (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = true);
  uint8_t addPresenceSensorRuntime   (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addSwitchRuntime           (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = true);
  uint8_t addTemperatureSensorRuntime(const char* name, uint8_t pin = MB_NO_PIN);
  uint8_t addHumiditySensorRuntime   (const char* name, uint8_t pin = MB_NO_PIN);
  uint8_t addOnOffLightRuntime       (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addOnOffPlugRuntime        (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addDimmableLightRuntime    (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addColorTempLightRuntime   (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addFanRuntime              (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addWindowCoveringRuntime   (const char* name, uint8_t pin = MB_NO_PIN, bool pinInverted = false);
  uint8_t addElectricalSensorRuntime (const char* name, uint8_t pin = MB_NO_PIN);
  uint8_t addTempHumidSensorRuntime  (const char* name, uint8_t pin = MB_NO_PIN);

  bool removeRuntime(uint8_t slot);

  // Show / hide a device in the hub (Reachable attribute)
  // Pokaż / ukryj urządzenie w hubie (atrybut Reachable)
  bool setVisible(uint8_t slot, bool visible);

  // ============================================================
  //  CONTROL AND STATE READ-OUT / STEROWANIE I ODCZYT STANU
  // ============================================================
  void setState   (uint8_t slot, bool state);
  bool getState   (uint8_t slot)        const;
  void setCallback(uint8_t slot, MBCallback cb);
  void setActive  (uint8_t slot, bool active);
  bool isActive   (uint8_t slot)        const;

  // Device object for type-specific setters (cast with static_cast)
  // Obiekt urządzenia do settera specyficznych dla typu (rzutuj static_cast)
  MBDevice* device(uint8_t slot);

  // ============================================================
  //  LOOP
  // ============================================================
  void update();   // call in every loop() / wywołuj w każdym loop()
  // Hold BOOT for holdMs → factory reset / Przytrzymaj BOOT przez holdMs → factory reset
  void checkBoot(uint8_t pin = MB_BOOT_PIN, uint32_t holdMs = 5000);

  // ============================================================
  //  DIAGNOSTICS / DIAGNOSTYKA
  // ============================================================
  uint8_t activeCount()     const;
  uint8_t registeredCount() const;
  void    printStatus()     const;

  // ============================================================
  //  COMMISSIONING / KOMISJONOWANIE
  // ============================================================
  bool  isCommissioned()                    const;
  bool  getPairingCode(char* buf, size_t len) const;   // manual code / kod ręczny
  bool  getQRCode     (char* buf, size_t len) const;   // QR payload "MT:..."

  void  decommission();
  void  factoryReset();

  // Attribute callbacks – called internally by MBNode
  // Callbacki atrybutów – wywoływane wewnętrznie przez MBNode
  esp_err_t onAttrRead (uint16_t epId, uint32_t clusterId,
                         uint32_t attrId, esp_matter_attr_val_t* val);
  esp_err_t onAttrWrite(uint16_t epId, uint32_t clusterId,
                         uint32_t attrId, esp_matter_attr_val_t* val);

private:
  MBSlotManager  _slots;
  MBStorage      _storage;
  unsigned long  _bootHeldSince;
  MBCallback     _globalCallback;

  // _started: true after a successful start(). Guards notifyTopologyChange()
  //   before the Matter stack runs (the Matter queue does not exist yet →
  //   assert(pxQueue != NULL) → crash).
  // _started: true po udanym start(). Chroni przed wywołaniem
  //   notifyTopologyChange() przed startem stosu Matter (kolejka Matter
  //   jeszcze nie istnieje → assert(pxQueue != NULL) → crash).
  bool _started;

  // _useNVS: true = addRuntime/removeRuntime persist in NVS and begin()
  //   restores devices; false = NVS skipped for runtime devices.
  // _useNVS: true = addRuntime/removeRuntime zapisują w NVS, a begin()
  //   odtwarza urządzenia; false = NVS pominięty dla urządzeń runtime.
  bool _useNVS;

  void    _buildFromStorage();
  uint8_t _constructAndRegister(const MBDescriptor& desc, bool runtime);
  uint8_t _constructAndRegisterAt(uint8_t slot, const MBDescriptor& desc, bool runtime);
  uint8_t _findStorageIndex(const char* name) const;
};

extern MatterBridgeClass MatterBridge;
