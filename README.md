# MatterBridge

[Polski](#polski) | [English](#english)

---

## Polski

Biblioteka mostka Matter dla **ESP32-C6** i **ESP32-S3** (Arduino-ESP32 3.x, natywne esp-matter). Jedno ESP pojawia się w Apple Home, Google Home, SmartThings lub Home Assistant jako most z wieloma urządzeniami. Urządzenia mogą być wirtualne (stan wysyłasz z własnego kodu) albo oparte na GPIO, a ich listę można zmieniać **w locie, bez restartu**.

---

## Funkcje

- Mostek Matter (agregator) na ESP32-C6 i ESP32-S3
- 13 typów urządzeń: czujniki, przycisk, światła, gniazdko, wentylator, roleta, licznik energii
- Dodawanie i usuwanie urządzeń w locie (tryb runtime), bez restartu
- Stałe sloty (`addRuntimeAt`) = stały endpoint id między restartami
- Opcjonalny zapis konfiguracji w NVS
- Debounce GPIO, callback zmiany stanu z huba
- Kod parowania i QR do komisjonowania przez Bluetooth
- S3: tablice w PSRAM (fallback na DRAM), do 50 urządzeń
- Licencja Apache 2.0

---

## Obsługiwane urządzenia

| Typ | Urządzenie Matter | Dodawanie (runtime) |
|---|---|---|
| Czujnik kontaktowy | Contact Sensor | `addContactSensorRuntime` |
| Czujnik obecności | Occupancy Sensor | `addPresenceSensorRuntime` |
| Przycisk | Generic Switch (klik / wielokrotny / długi) | `addSwitchRuntime` |
| Temperatura | Temperature Sensor | `addTemperatureSensorRuntime` |
| Wilgotność | Humidity Sensor | `addHumiditySensorRuntime` |
| Temperatura + wilgotność | jeden endpoint, dwa klastry | `addTempHumidSensorRuntime` |
| Światło On/Off | On/Off Light | `addOnOffLightRuntime` |
| Światło ściemnialne | Dimmable Light | `addDimmableLightRuntime` |
| Światło CT | Color Temperature Light | `addColorTempLightRuntime` |
| Gniazdko | On/Off Plug-in Unit | `addOnOffPlugRuntime` |
| Wentylator | Fan | `addFanRuntime` |
| Roleta | Window Covering | `addWindowCoveringRuntime` |
| Licznik energii | Electrical Sensor | `addElectricalSensorRuntime` |

---

## Struktura repozytorium

```
MatterBridge/
├── README.md
├── LICENSE
├── keywords.txt
├── library.properties
├── src/
│   ├── MatterBridge.h / MatterBridge.cpp
│   ├── MBConfig.h  MBTypes.h  MBNode.h
│   ├── MBDevice.h  MBDevices.h  MBDeviceFactory.h
│   └── MBSlotManager.h  MBStorage.h
└── examples/
    ├── 01_OnOffLight/
    ├── 02_ContactSensorGPIO/
    ├── 03_TempHumiditySensor/
    ├── 04_AddRemoveAtRuntime/
    ├── 05_AllDeviceTypes/
    └── 06_RuntimeCycleTest/
```

---

## Instalacja

**Arduino IDE:**
1. Pobierz repozytorium jako ZIP
2. Sketch → Include Library → Add .ZIP Library
3. `#include <MatterBridge.h>`

**Wymagania:**
- Płytka z ESP32-C6 lub ESP32-S3, Arduino-ESP32 **3.x** (z obsługą Matter)
- **Obecnie biblioteka działa poprawnie tylko z Arduino-ESP32 SDK 3.3.10** (esp-matter)
- Schemat partycji z dużą ilością flasha (np. *Huge APP*)
- Testowano na: Arduino-ESP32 3.3.10 (ESP32-C6, ESP32-S3)

---

## Szybki start

```cpp
#include <MatterBridge.h>

void onChange(uint8_t slot, bool state) { /* hub zmienił stan */ }

void setup() {
    MatterBridge.setRuntimeNVS(false);        // przed begin()
    MatterBridge.begin();
    MatterBridge.addOnOffLightRuntime("Lampa");
    MatterBridge.onChange(onChange);
    MatterBridge.start();
}

void loop() {
    MatterBridge.update();                    // wymagane w każdym loop()
    MatterBridge.checkBoot();                 // BOOT 5 s = factory reset
}
```

Komisjonowanie: kod z `getPairingCode()` / QR z `getQRCode()` przez Bluetooth (wypisują je przykłady).

---

## API (skrót)

| Funkcja | Opis |
|---|---|
| `begin()` / `start()` | Węzeł + agregator / start stosu Matter |
| `setRuntimeNVS(bool)` | Zapis urządzeń runtime w NVS (przed `begin()`) |
| `addXxxRuntime(name, pin, inv)` | Dodaje urządzenie w locie, zwraca slot |
| `addRuntime(desc)` / `addRuntimeAt(slot, desc)` | Ogólne dodanie / ze stałym slotem |
| `removeRuntime(slot)` | Usuwa urządzenie w locie |
| `add*()` / `remove()` | Tryb statyczny: zapis do NVS **i restart ESP** |
| `setState(slot, state)` / `getState(slot)` | Stan logiczny urządzenia |
| `device(slot)` | Obiekt urządzenia (settery specyficzne dla typu) |
| `onChange(cb)` | Callback `(slot, state)` przy zmianie z huba lub z kodu |
| `setVisible(slot, bool)` | Atrybut Reachable |
| `update()` / `checkBoot()` | Wywoływane w `loop()` |
| `isCommissioned()`, `getPairingCode()`, `getQRCode()` | Komisjonowanie |
| `decommission()` / `factoryReset()` | Usunięcie z fabryki / pełny reset |
| `printStatus()` | Diagnostyka na Serial |

---

## Konfiguracja

Nadpisz przez `-D` lub `#define` przed dołączeniem biblioteki:

| Stała | Domyślnie | Opis |
|---|---|---|
| `MB_MAX_DEVICES` | 20 (C6) / 50 (S3) | Maks. liczba urządzeń (sloty = 2×) |
| `MB_USE_PSRAM` | 0 (C6) / 1 (S3) | Tablice w PSRAM |
| `MB_BOOT_PIN` | 9 (C6) / 0 (S3) | Pin przycisku factory reset |
| `MB_DEBOUNCE_MS` | 50 | Debounce GPIO |
| `MB_MAX_NAME_LEN` | 31 | Maks. długość nazwy |
| `MB_VENDOR_ID` / `MB_PRODUCT_ID` | 0xFFF1 / 0x8001 | Wartości testowe – zmień dla produktu |

---

## Uwagi

- **Wymagane SDK 3.3.10**: biblioteka jest obecnie przetestowana i działa poprawnie wyłącznie z Arduino-ESP32 SDK w wersji **3.3.10**. Inne wersje mogą powodować błędy kompilacji lub nieprawidłowe działanie stosu Matter.
- **`setRuntimeNVS(false)`**: gdy urządzenia tworzysz w każdym `setup()`. Przy domyślnym `true` są zapisywane w NVS i odtwarzane przez `begin()`, więc ponowne dodanie w `setup()` tworzy duplikaty.
- **SmartThings**: dodaj co najmniej jedno urządzenie **przed** `start()`. Pusty mostek może zostać zapisany bez urządzeń, a późniejsze dodania są ignorowane.
## Status: alfa. API może się zmienić przed wersją 1.1.

---

## Wsparcie projektu

Jeśli biblioteka MatterBridge jest dla Ciebie przydatna i chcesz wesprzeć jej dalszy rozwój, możesz postawić mi wirtualną kawę:

<a href="https://suppi.pl/00maciek00" target="_blank"><img width="165" src="https://suppi.pl/api/widget/button.svg?fill=6457FD&textColor=ffffff"/></a>

Dziękuję! ☕

---

## Licencja

Copyright 2026 Maciej Sikorski — S.M. DIY Home
Licencja Apache 2.0. Szczegóły w pliku [LICENSE](LICENSE).

---

**Projekt:** S.M. DIY Home

**Wersja:** 1.0.2  
**Autor:** Maciej Sikorski  
**Data:** 2026-09-30  
**Licencja:** Apache 2.0

---
---

## English

Matter bridge library for **ESP32-C6** and **ESP32-S3** (Arduino-ESP32 3.x, native esp-matter). One ESP shows up in Apple Home, Google Home, SmartThings or Home Assistant as a bridge with many devices behind it. Devices can be virtual (you push state from your code) or GPIO-backed, and the list can be changed **at runtime without a restart**.

---

## Features

- Matter bridge (aggregator) on ESP32-C6 and ESP32-S3
- 13 device types: sensors, button, lights, plug, fan, blind, energy meter
- Add and remove devices at runtime, no restart
- Fixed slots (`addRuntimeAt`) = stable endpoint id across reboots
- Optional configuration storage in NVS
- GPIO debounce, state-change callback from the hub
- Pairing code and QR for Bluetooth commissioning
- S3: tables in PSRAM (DRAM fallback), up to 50 devices
- Apache 2.0 license

---

## Supported Devices

| Type | Matter device | Add (runtime) |
|---|---|---|
| Contact sensor | Contact Sensor | `addContactSensorRuntime` |
| Presence sensor | Occupancy Sensor | `addPresenceSensorRuntime` |
| Button | Generic Switch (press / multi / long) | `addSwitchRuntime` |
| Temperature | Temperature Sensor | `addTemperatureSensorRuntime` |
| Humidity | Humidity Sensor | `addHumiditySensorRuntime` |
| Temp + humidity | one endpoint, two clusters | `addTempHumidSensorRuntime` |
| On/Off light | On/Off Light | `addOnOffLightRuntime` |
| Dimmable light | Dimmable Light | `addDimmableLightRuntime` |
| Color-temperature light | Color Temperature Light | `addColorTempLightRuntime` |
| Plug | On/Off Plug-in Unit | `addOnOffPlugRuntime` |
| Fan | Fan | `addFanRuntime` |
| Blind | Window Covering | `addWindowCoveringRuntime` |
| Energy meter | Electrical Sensor | `addElectricalSensorRuntime` |

---

## Repository Structure

```
MatterBridge/
├── README.md
├── LICENSE
├── keywords.txt
├── library.properties
├── src/
│   ├── MatterBridge.h / MatterBridge.cpp
│   ├── MBConfig.h  MBTypes.h  MBNode.h
│   ├── MBDevice.h  MBDevices.h  MBDeviceFactory.h
│   └── MBSlotManager.h  MBStorage.h
└── examples/
    ├── 01_OnOffLight/
    ├── 02_ContactSensorGPIO/
    ├── 03_TempHumiditySensor/
    ├── 04_AddRemoveAtRuntime/
    ├── 05_AllDeviceTypes/
    └── 06_RuntimeCycleTest/
```

---

## Installation

**Arduino IDE:**
1. Download the repository as ZIP
2. Sketch → Include Library → Add .ZIP Library
3. `#include <MatterBridge.h>`

**Requirements:**
- ESP32-C6 or ESP32-S3 board, Arduino-ESP32 **3.x** (with Matter support)
- **Currently the library works correctly only with Arduino-ESP32 SDK 3.3.10** (esp-matter)
- Partition scheme with plenty of flash (e.g. *Huge APP*)
- Tested with: Arduino-ESP32 3.3.10 (ESP32-C6, ESP32-S3)

---

## Quick Start

```cpp
#include <MatterBridge.h>

void onChange(uint8_t slot, bool state) { /* hub changed the state */ }

void setup() {
    MatterBridge.setRuntimeNVS(false);        // before begin()
    MatterBridge.begin();
    MatterBridge.addOnOffLightRuntime("Lamp");
    MatterBridge.onChange(onChange);
    MatterBridge.start();
}

void loop() {
    MatterBridge.update();                    // must be called in every loop()
    MatterBridge.checkBoot();                 // BOOT 5 s = factory reset
}
```

Commissioning: use the code from `getPairingCode()` / the QR from `getQRCode()` over Bluetooth (the examples print both).

---

## API (summary)

| Function | Description |
|---|---|
| `begin()` / `start()` | Node + aggregator / start the Matter stack |
| `setRuntimeNVS(bool)` | Persist runtime devices in NVS (before `begin()`) |
| `addXxxRuntime(name, pin, inv)` | Add a device at runtime, returns the slot |
| `addRuntime(desc)` / `addRuntimeAt(slot, desc)` | Generic add / with a fixed slot |
| `removeRuntime(slot)` | Remove a device at runtime |
| `add*()` / `remove()` | Static mode: saves to NVS **and restarts the ESP** |
| `setState(slot, state)` / `getState(slot)` | Logical device state |
| `device(slot)` | Device object (type-specific setters) |
| `onChange(cb)` | Callback `(slot, state)` on change from the hub or from code |
| `setVisible(slot, bool)` | Reachable attribute |
| `update()` / `checkBoot()` | Call in `loop()` |
| `isCommissioned()`, `getPairingCode()`, `getQRCode()` | Commissioning |
| `decommission()` / `factoryReset()` | Remove from fabric / full reset |
| `printStatus()` | Diagnostics on Serial |

---

## Configuration

Override with `-D` or `#define` before including the library:

| Constant | Default | Description |
|---|---|---|
| `MB_MAX_DEVICES` | 20 (C6) / 50 (S3) | Max devices (slots = 2×) |
| `MB_USE_PSRAM` | 0 (C6) / 1 (S3) | Tables in PSRAM |
| `MB_BOOT_PIN` | 9 (C6) / 0 (S3) | Factory-reset button pin |
| `MB_DEBOUNCE_MS` | 50 | GPIO debounce |
| `MB_MAX_NAME_LEN` | 31 | Max name length |
| `MB_VENDOR_ID` / `MB_PRODUCT_ID` | 0xFFF1 / 0x8001 | Test values – change for a product |

---

## Notes

- **Required SDK 3.3.10**: the library is currently tested and works correctly only with Arduino-ESP32 SDK version **3.3.10**. Other versions may cause compilation errors or incorrect Matter stack behavior.
- **`setRuntimeNVS(false)`**: use it when you create devices in every `setup()`. With the default `true` they are stored in NVS and restored by `begin()`, so adding them again in `setup()` creates duplicates.
- **SmartThings:** add at least one device **before** `start()`. An empty bridge may be stored without devices and later additions ignored.
- **Status:** alpha. The API may change before 1.1.

---

## Support the project

If you find MatterBridge useful and would like to support its further development, you can buy me a virtual coffee:

<a href="https://suppi.pl/00maciek00" target="_blank"><img width="165" src="https://suppi.pl/api/widget/button.svg?fill=6457FD&textColor=ffffff"/></a>

Thank you! ☕

---

## License

Copyright 2026 Maciej Sikorski — S.M. DIY Home  
Licensed under Apache 2.0. See [LICENSE](LICENSE) for details.

---

**Project:** S.M. DIY Home

**Version:** 1.0.2  
**Author:** Maciej Sikorski  
**Date:** 2026-09-30  
**License:** Apache 2.0