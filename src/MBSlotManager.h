// =============================================================================
//  MatterBridge — MBSlotManager.h: Slot table (DRAM on C6, PSRAM on S3)
//  MatterBridge — MBSlotManager.h: Tablica slotów (DRAM na C6, PSRAM na S3)
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
#include "MBDevice.h"
#include "MBDeviceFactory.h"
#include "MBConfig.h"

#if MB_USE_PSRAM
  #include <esp_heap_caps.h>
#endif

// ============================================================
//  MBSlotManager.h – tablica slotów
//
//  Każdy slot:
//    - MBDeviceBuffer  – bufor na obiekt urządzenia (placement new)
//    - MBDevice*       – wskaźnik do skonstruowanego obiektu
//
//  Zero alokacji na stercie dla urządzeń (placement new w buforze).
//  Lookup po epId w O(n) – wystarczające dla MB_SLOTS ≤ 100.
//
//  Podział przestrzeni slotów (patrz MBConfig.h):
//    kanał 0: sloty 0 .. MB_MAX_DEVICES-1
//    kanał 1: sloty MB_MAX_DEVICES .. MB_SLOTS-1
//  Gwarantuje stały, deterministyczny epId dla każdego urządzenia.
//
//  Układ pamięci:
//    C6: _slots[MB_SLOTS] – statyczny member w DRAM (40 × ~144B = ~5760B)
//    S3: _slots* w PSRAM  – 100 × ~144B = ~14400B poza DRAM
// ============================================================

struct MBSlot {
  MBDeviceBuffer buf;
  MBDevice*      device;

  MBSlot() : device(nullptr) {}
  bool isOccupied() const { return device != nullptr; }
};

class MBSlotManager {
public:

// ── Warunkowy layout pamięci ───────────────────────────────────────────────────
// C6: statyczna tablica jako member klasy – zero alokacji, zero ryzyka
//     związanego z inicjalizacją statycznych zmiennych lokalnych.
// S3: wskaźnik do bloku PSRAM – alokowany w konstruktorze.
//     Fallback na DRAM gdy PSRAM niedostępna (np. niepoprawna konfiguracja).
#if MB_USE_PSRAM
  MBSlot*  _slots;
#else
  MBSlot   _slots[MB_SLOTS];
#endif

  uint8_t  _occupied;

  MBSlotManager() : _occupied(0) {
#if MB_USE_PSRAM
    // MALLOC_CAP_SPIRAM  – alokuj w PSRAM (SPI RAM)
    // MALLOC_CAP_8BIT    – wymagany przez placement new (dostęp byte-po-byte)
    _slots = static_cast<MBSlot*>(
      heap_caps_calloc(MB_SLOTS, sizeof(MBSlot),
                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!_slots) {
      // PSRAM niedostępna lub za mała – fallback na DRAM
      ESP_LOGE("MBSlot", "PSRAM alloc FAILED (%u × %u B) – fallback DRAM",
               MB_SLOTS, (unsigned)sizeof(MBSlot));
      _slots = static_cast<MBSlot*>(
        calloc(MB_SLOTS, sizeof(MBSlot)));
    } else {
      ESP_LOGI("MBSlot", "Tablica slotów w PSRAM: %u × %u B = %u B łącznie",
               MB_SLOTS, (unsigned)sizeof(MBSlot),
               (unsigned)(MB_SLOTS * sizeof(MBSlot)));
    }

    // calloc() zeruje pamięć, ale nie wywołuje konstruktorów C++.
    // Jawna inicjalizacja przez placement new w każdym slocie.
    for (uint8_t i = 0; i < MB_SLOTS; i++) {
      new (&_slots[i]) MBSlot();
    }
#else
    // C6: statyczna tablica – konstruktory wywołane automatycznie
    for (uint8_t i = 0; i < MB_SLOTS; i++) {
      _slots[i].device = nullptr;
    }
#endif
  }

#if MB_USE_PSRAM
  ~MBSlotManager() {
    if (!_slots) return;
    // Wywołaj destruktory urządzeń i slotów przed zwolnieniem bloku PSRAM
    for (uint8_t i = 0; i < MB_SLOTS; i++) {
      if (_slots[i].isOccupied()) {
        MBDeviceFactory::destroy(_slots[i].device);
        _slots[i].device = nullptr;
      }
      _slots[i].~MBSlot();
    }
    heap_caps_free(_slots);
    _slots = nullptr;
  }
#endif

  // ── Konstruuje urządzenie w pierwszym wolnym slocie ───────────────────────
  // Używane przez tryb statyczny (add/remove z restartem).
  // Przy zewnętrznym źródle prawdy używaj constructAt() dla deterministycznego slotu.
  uint8_t construct(const MBDescriptor& desc) {
    int8_t idx = _findFree();
    if (idx < 0) {
      Serial.println("[MBSlotMgr] Brak wolnych slotów!");
      return MB_INVALID_SLOT;
    }
    MBDevice* dev = MBDeviceFactory::create(&_slots[idx].buf, desc);
    if (!dev) {
      Serial.printf("[MBSlotMgr] Nieznany typ %d\n", (int)desc.type);
      return MB_INVALID_SLOT;
    }
    _slots[idx].device = dev;
    dev->setSlotIndex((uint8_t)idx);
    _occupied++;
    return (uint8_t)idx;
  }

  // ── Konstruuje urządzenie w konkretnym slocie ─────────────────────────────
  // Pozwala na deterministyczny slot, żeby slot był zawsze równy
  // baseIdx (kanał 0) lub baseIdx + MB_MAX_DEVICES (kanał 1).
  // Stały slot = stały epId = SmartThings/Apple Home nie gubią powiązań.
  // Zwraca slot jeśli OK, MB_INVALID_SLOT jeśli slot zajęty lub poza zakresem.
  uint8_t constructAt(uint8_t slot, const MBDescriptor& desc) {
    if (slot >= MB_SLOTS || _slots[slot].isOccupied()) return MB_INVALID_SLOT;
    MBDevice* dev = MBDeviceFactory::create(&_slots[slot].buf, desc);
    if (!dev) return MB_INVALID_SLOT;
    _slots[slot].device = dev;
    dev->setSlotIndex(slot);
    _occupied++;
    return slot;
  }

  // ── Niszczy obiekt urządzenia ─────────────────────────────────────────────
  // Destruktor + nulluje wskaźnik. Bufor pozostaje, slot wolny do reużycia.
  bool destroy(uint8_t idx) {
    if (idx >= MB_SLOTS || !_slots[idx].isOccupied()) return false;
    MBDeviceFactory::destroy(_slots[idx].device);
    _slots[idx].device = nullptr;
    _occupied--;
    return true;
  }

  // ── Dostęp do urządzenia ──────────────────────────────────────────────────
  MBDevice*       get(uint8_t idx) {
    return (idx < MB_SLOTS) ? _slots[idx].device : nullptr;
  }
  const MBDevice* get(uint8_t idx) const {
    return (idx < MB_SLOTS) ? _slots[idx].device : nullptr;
  }

  // ── Lookup po endpoint ID – potrzebny w callbackach atrybutów ─────────────
  MBDevice* findByEpId(uint16_t epId) {
    for (uint8_t i = 0; i < MB_SLOTS; i++) {
      if (_slots[i].isOccupied() &&
          _slots[i].device->getEndpointId() == epId)
        return _slots[i].device;
    }
    return nullptr;
  }

  // ── Lookup po nazwie ──────────────────────────────────────────────────────
  uint8_t findByName(const char* name) const {
    for (uint8_t i = 0; i < MB_SLOTS; i++) {
      if (!_slots[i].isOccupied()) continue;
      if (strncmp(_slots[i].device->getName(), name, MB_MAX_NAME_LEN) == 0)
        return i;
    }
    return MB_INVALID_SLOT;
  }

  bool    isOccupied(uint8_t idx) const {
    return idx < MB_SLOTS && _slots[idx].isOccupied();
  }
  uint8_t occupiedCount() const { return _occupied; }
  // capacity() zwraca MB_SLOTS (fizyczny rozmiar tablicy).
  // C6: 40, S3: 100
  uint8_t capacity()      const { return MB_SLOTS; }

private:
  int8_t _findFree() const {
    for (uint8_t i = 0; i < MB_SLOTS; i++) {
      if (!_slots[i].isOccupied()) return (int8_t)i;
    }
    return -1;
  }
};
