// =============================================================================
//  MatterBridge — MBTypes.h: Types, enums, descriptors
//  MatterBridge — MBTypes.h: Typy, enumy, deskryptory
// =============================================================================
//
//  Version / Wersja: 1.0.2
//  Author / Autor:   Maciej Sikorski
//  Date / Data:      2026-09-30
//  Project / Projekt: S.M. DIY Home
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

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "MBConfig.h"

// ============================================================
//  MBTypes.h – types, enums, descriptors / typy, enumy, deskryptory
//  No dependency on Arduino / Matter.h
//  Zero zależności od Arduino / Matter.h
// ============================================================

enum class MBDeviceType : uint8_t {
  None                  = 0,
  // Sensors (passive, read-only) / Czujniki (pasywne – tylko odczyt)
  ContactSensor         = 1,   // BooleanState
  PresenceSensor        = 2,   // OccupancySensing
  Switch                = 3,   // Switch (momentary)
  TemperatureSensor     = 4,   // TemperatureMeasurement
  HumiditySensor        = 5,   // RelativeHumidityMeasurement
  // Controllable (hub can send commands) / Sterowanie (hub może wysyłać komendy)
  OnOffLight            = 6,   // OnOff
  DimmableLight         = 7,   // OnOff + LevelControl
  ColorTempLight        = 8,   // OnOff + LevelControl + ColorControl (CT)
  OnOffPlug             = 9,   // OnOff (plug / gniazdko)
  Fan                   = 10,  // FanControl
  WindowCovering        = 11,  // WindowCovering (blind / roleta, żaluzja)
  ElectricalSensor      = 12,  // ElectricalMeasurement (energy meter / licznik energii)
  TempHumidSensor       = 13,  // TemperatureMeasurement + RelativeHumidityMeasurement
};

// Callback: device state change / zmiana stanu urządzenia
// slot  – slot index (constant for the device's lifetime)
//         indeks slotu (stały przez życie urządzenia)
// state – new logical state / nowy stan logiczny
typedef void (*MBCallback)(uint8_t slot, bool state);

// ============================================================
//  MBDescriptor – immutable device data / dane niezmienne urządzenia
//  Stored in NVS and in the slot table.
//  Przechowywany w NVS i w tablicy slotów.
//  POD (plain old data) – copied by value / kopiowany przez value.
//  No String, no dynamic allocation / Zero String, zero dynamicznej alokacji.
// ============================================================
struct MBDescriptor {
  MBDeviceType type;
  char         name[MB_MAX_NAME_LEN + 1];
  uint8_t      pin;
  bool         pinInverted;

  MBDescriptor()
    : type(MBDeviceType::None), pin(MB_NO_PIN), pinInverted(false) {
    name[0] = '\0';
  }

  MBDescriptor(MBDeviceType t, const char* n, uint8_t p, bool inv)
    : type(t), pin(p), pinInverted(inv) {
    uint8_t i = 0;
    while (i < MB_MAX_NAME_LEN && n[i] != '\0') { name[i] = n[i]; i++; }
    name[i] = '\0';
  }

  bool isEmpty() const { return type == MBDeviceType::None; }
};

// ============================================================
//  MBAttrStore – attribute values (external storage)
//  wartości atrybutów (external storage)
//  The Matter stack reads these via the onAttrRead callback.
//  Stos Matter odczytuje te wartości przez callback onAttrRead.
// ============================================================
struct MBAttrStore {
  // Binary sensors / Czujniki binarne
  bool    boolState;       // ContactSensor / BooleanState
  uint8_t occupancy;       // PresenceSensor / OccupancySensing bitmap8

  // Button / Przycisk
  uint8_t switchPosition;  // Switch: 0=released/zwolniony, 1=pressed/wciśnięty

  // Measurement sensors (x100 per Matter spec) / Sensory pomiarowe (x100 wg specyfikacji Matter)
  int16_t temperature;     // °C × 100  (e.g./np. 2150 = 21.50°C)
  uint16_t humidity;       // %RH × 100 (e.g./np. 5500 = 55.00%)

  // Control (OnOff / Dimmer / Color / Fan / Covering) / Sterowanie
  bool    onOff;           // true = on / włączone
  uint8_t level;           // 0-254 (LevelControl)
  uint16_t colorTemp;      // mireds (ColorControl, e.g./np. 370 = 2700K)
  uint8_t  fanMode;        // FanControl: 0=off,1=low,2=med,3=high,4=on,5=auto
  uint8_t  coveringPos;    // WindowCovering: 0-100% (percentage)

  // Energy meter (ElectricalSensor) / Licznik energii
  uint16_t voltage;      // voltage / napięcie [0.01 V]   e.g./np. 24290 = 242.90 V
  uint16_t current;      // current / prąd [0.001 A]      e.g./np. 500 = 0.500 A
  int16_t  activePower;  // active power / moc czynna [0.1 W]  e.g./np. 1234 = 123.4 W (may be negative / może być ujemna)
  uint32_t energy;       // total energy / energia całkowita [Wh]

  // Common / Wspólne
  bool    reachable;

  MBAttrStore()
    : boolState(false), occupancy(0), switchPosition(0),
      temperature(0), humidity(0),
      onOff(false), level(0), colorTemp(370),
      fanMode(0), coveringPos(0),
      voltage(0), current(0), activePower(0), energy(0),
      reachable(true) {}
};

// ============================================================
//  MBDataSource – data source interface (RESERVED, not used yet)
//  interfejs źródła danych (ZAREZERWOWANY, jeszcze nieużywany)
//
//  Meant to separate "where a value comes from" from "how it is reported
//  to Matter". Planned: GPIO, virtual, HTTP, MQTT, I2C sources.
//  Ma oddzielać "skąd pochodzi wartość" od "jak jest raportowana do
//  Matter". Planowane: źródła GPIO, wirtualne, HTTP, MQTT, I2C.
//
//  Today MBDevice uses GPIO when pin != MB_NO_PIN, otherwise it is virtual
//  (state pushed from your code).
//  Obecnie MBDevice używa GPIO gdy pin != MB_NO_PIN, w przeciwnym razie
//  jest wirtualne (stan wysyłasz z własnego kodu).
// ============================================================
class MBDataSource {
public:
  virtual ~MBDataSource() = default;
  // Returns the current value. Call cyclically in tick().
  // Zwraca aktualną wartość. Wywołuj cyklicznie w tick().
  virtual bool read(bool& out) = 0;
  // Returns true if the source has a new value (edge-triggered)
  // Zwraca true jeśli źródło ma nową wartość (edge-triggered)
  virtual bool hasChanged() = 0;
};


