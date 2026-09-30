// =============================================================================
//  MatterBridge — MBDeviceFactory.h: Device factory (placement new, no heap)
//  MatterBridge — MBDeviceFactory.h: Fabryka urządzeń (placement new, bez sterty)
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
#include "MBDevices.h"

// ============================================================
//  MBDeviceFactory.h – device factory (placement new, zero heap)
//
//  MBDeviceBuffer – an aligned buffer for the largest possible object.
//  MBDeviceFactory::create() – constructs in-place, returns MBDevice*.
//  MBDeviceFactory::destroy() – calls the destructor, does not free memory.
//
//  To add a new type:
//    1. Add a field to MBDeviceUnion
//    2. Add a case in MBDeviceFactory::create()
//
//  MBDeviceFactory.h – fabryka urządzeń (placement new, zero heap)
//
//  MBDeviceBuffer – wyrównany bufor na największy możliwy obiekt.
//  MBDeviceFactory::create() – konstruuje in-place, zwraca MBDevice*.
//  MBDeviceFactory::destroy() – wywołuje destruktor, nie zwalnia pamięci.
//
//  Aby dodać nowy typ:
//    1. Dodaj pole do MBDeviceUnion
//    2. Dodaj case w MBDeviceFactory::create()
// ============================================================

// Union used to determine the buffer size and alignment
// Unia do wyznaczenia rozmiaru i wyrównania bufora
union MBDeviceUnion {
  MBContactSensor   _contact;
  MBPresenceSensor  _presence;
  MBSwitch          _switch;
  MBTemperatureSensor _temp;
  MBHumiditySensor  _humidity;
  MBOnOffLight      _light;
  MBOnOffPlug       _plug;
  MBDimmableLight   _dimmer;
  MBColorTempLight  _colorTemp;
  MBFan             _fan;
  MBWindowCovering  _covering;
  MBElectricalPlug  _electrical;  // NEW CLASS / NOWA KLASA
  MBTempHumidSensor _tempHumid;
};

struct MBDeviceBuffer {
  alignas(MBDeviceUnion) char data[sizeof(MBDeviceUnion)];
};

class MBDeviceFactory {
public:
  static MBDevice* create(MBDeviceBuffer* buf, const MBDescriptor& desc) {
    void* p = buf->data;
    switch (desc.type) {
      case MBDeviceType::ContactSensor:    return new (p) MBContactSensor(desc);
      case MBDeviceType::PresenceSensor:   return new (p) MBPresenceSensor(desc);
      case MBDeviceType::Switch:           return new (p) MBSwitch(desc);
      case MBDeviceType::TemperatureSensor:return new (p) MBTemperatureSensor(desc);
      case MBDeviceType::HumiditySensor:   return new (p) MBHumiditySensor(desc);
      case MBDeviceType::OnOffLight:       return new (p) MBOnOffLight(desc);
      case MBDeviceType::OnOffPlug:        return new (p) MBOnOffPlug(desc);
      case MBDeviceType::DimmableLight:    return new (p) MBDimmableLight(desc);
      case MBDeviceType::ColorTempLight:   return new (p) MBColorTempLight(desc);
      case MBDeviceType::Fan:              return new (p) MBFan(desc);
      case MBDeviceType::WindowCovering:   return new (p) MBWindowCovering(desc);
      case MBDeviceType::ElectricalSensor: return new (p) MBElectricalPlug(desc);  // NEW CLASS / NOWA KLASA
      case MBDeviceType::TempHumidSensor:  return new (p) MBTempHumidSensor(desc);
      default: return nullptr;
    }
  }

  static void destroy(MBDevice* dev) {
    if (dev) dev->~MBDevice();
  }
};