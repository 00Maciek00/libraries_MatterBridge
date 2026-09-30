// =============================================================================
//  MatterBridge — MBStorage.h: NVS persistence layer
//  MatterBridge — MBStorage.h: Warstwa trwałości danych NVS
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
#include <Preferences.h>
#include "MBTypes.h"
#include "MBConfig.h"

#if MB_USE_PSRAM
  #include <esp_heap_caps.h>
#endif

// ============================================================
//  MBStorage.h – data persistence layer (NVS / Preferences)
//
//  Responsible only for saving and loading the device configuration.
//  Zero use of String – only char arrays and primitive types.
//
//  NVS format:
//    "cnt"     → uint8_t  number of saved devices
//    "t{i}"    → uint8_t  MBDeviceType
//    "n{i}"    → char[]   name (max MB_MAX_NAME_LEN+1 bytes)
//    "p{i}"    → uint8_t  pin
//    "v{i}"    → uint8_t  pinInverted (0/1)
//
//  NVS keys are at most 15 characters long (an ESP-IDF limitation).
//  Format: a letter + a decimal number (max 2 digits → max 99 devices).
//
//  Memory layout of the descriptor table:
//    C6: _descs[MB_MAX_DEVICES] – static member in DRAM (20 × 36B = 720B)
//    S3: _descs* in PSRAM       – 50 × 36B = 1800B outside DRAM
//
//  MBStorage.h – warstwa trwałości danych (NVS / Preferences)
//
//  Odpowiada wyłącznie za zapis i odczyt konfiguracji urządzeń.
//  Zero użycia String – wyłącznie tablice char i typy prymitywne.
//
//  Format NVS:
//    "cnt"     → uint8_t  liczba zapisanych urządzeń
//    "t{i}"    → uint8_t  MBDeviceType
//    "n{i}"    → char[]   nazwa (max MB_MAX_NAME_LEN+1 bajtów)
//    "p{i}"    → uint8_t  pin
//    "v{i}"    → uint8_t  pinInverted (0/1)
//
//  Klucze NVS mają max 15 znaków (ograniczenie ESP-IDF).
//  Format: litera + liczba dziesiętna (max 2 cyfry → max 99 urządzeń).
//
//  Układ pamięci tablicy deskryptorów:
//    C6: _descs[MB_MAX_DEVICES] – statyczny member w DRAM (20 × 36B = 720B)
//    S3: _descs* w PSRAM        – 50 × 36B = 1800B poza DRAM
// ============================================================

class MBStorage {
public:
  MBStorage() : _count(0) {
#if MB_USE_PSRAM
    // Allocate the descriptor table in PSRAM
    // MALLOC_CAP_8BIT is required by struct assignments (byte-wise)
    // Alokuj tablicę deskryptorów w PSRAM
    // MALLOC_CAP_8BIT wymagany przez przypisania struct (byte-po-byte)
    _descs = static_cast<MBDescriptor*>(
      heap_caps_calloc(MB_MAX_DEVICES, sizeof(MBDescriptor),
                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!_descs) {
      ESP_LOGE("MBStorage", "PSRAM alloc FAILED (%u × %u B) – fallback DRAM",
               MB_MAX_DEVICES, (unsigned)sizeof(MBDescriptor));
      _descs = static_cast<MBDescriptor*>(
        calloc(MB_MAX_DEVICES, sizeof(MBDescriptor)));
    } else {
      ESP_LOGI("MBStorage", "Tablica deskryptorów w PSRAM: %u × %u B = %u B",
               MB_MAX_DEVICES, (unsigned)sizeof(MBDescriptor),
               (unsigned)(MB_MAX_DEVICES * sizeof(MBDescriptor)));
    }

    // Explicit initialisation via placement new – calloc zeroes the memory but does not
    // call constructors. MBDescriptor has a constructor that sets the fields.
    // Jawna inicjalizacja przez placement new – calloc zeruje, ale nie
    // wywołuje konstruktorów. MBDescriptor ma konstruktor ustawiający pola.
    for (uint8_t i = 0; i < MB_MAX_DEVICES; i++) {
      new (&_descs[i]) MBDescriptor();
    }
#endif
  }

#if MB_USE_PSRAM
  ~MBStorage() {
    if (!_descs) return;
    for (uint8_t i = 0; i < MB_MAX_DEVICES; i++) {
      _descs[i].~MBDescriptor();
    }
    heap_caps_free(_descs);
    _descs = nullptr;
  }
#endif

  // Load the configuration from NVS into the internal descriptor buffer.
  // Returns the number of loaded devices.
  // Wczytaj konfigurację z NVS do wewnętrznego bufora deskryptorów.
  // Zwraca liczbę wczytanych urządzeń.
  uint8_t load() {
    Preferences prefs;
    prefs.begin(MB_NVS_NAMESPACE, /*readOnly=*/true);
    _count = prefs.getUChar("cnt", 0);
    if (_count > MB_MAX_DEVICES) _count = MB_MAX_DEVICES;

    for (uint8_t i = 0; i < _count; i++) {
      char key[8];

      _buildKey(key, 't', i);
      _descs[i].type = static_cast<MBDeviceType>(prefs.getUChar(key, 0));

      _buildKey(key, 'n', i);
      prefs.getBytes(key, _descs[i].name, MB_MAX_NAME_LEN + 1);
      _descs[i].name[MB_MAX_NAME_LEN] = '\0';

      _buildKey(key, 'p', i);
      _descs[i].pin = prefs.getUChar(key, MB_NO_PIN);

      _buildKey(key, 'v', i);
      _descs[i].pinInverted = (bool)prefs.getUChar(key, 0);
    }

    prefs.end();
    return _count;
  }

  // Save the full configuration (from the internal buffer) to NVS.
  // Overwrites all keys.
  // Zapisz pełną konfigurację (z wewnętrznego bufora) do NVS.
  // Nadpisuje wszystkie klucze.
  void save() {
    Preferences prefs;
    prefs.begin(MB_NVS_NAMESPACE, /*readOnly=*/false);
    prefs.putUChar("cnt", _count);

    for (uint8_t i = 0; i < _count; i++) {
      char key[8];

      _buildKey(key, 't', i);
      prefs.putUChar(key, static_cast<uint8_t>(_descs[i].type));

      _buildKey(key, 'n', i);
      prefs.putBytes(key, _descs[i].name, MB_MAX_NAME_LEN + 1);

      _buildKey(key, 'p', i);
      prefs.putUChar(key, _descs[i].pin);

      _buildKey(key, 'v', i);
      prefs.putUChar(key, (uint8_t)_descs[i].pinInverted);
    }

    prefs.end();
  }

  // Append a descriptor to the buffer. Returns false when the buffer is full.
  // Dodaj deskryptor do bufora. Zwraca false gdy bufor pełny.
  bool append(const MBDescriptor& desc) {
    if (_count >= MB_MAX_DEVICES) return false;
    _descs[_count++] = desc;
    return true;
  }

  // Remove the descriptor at the given index (shifts the rest to the left).
  // Returns false when the index is out of range.
  // Usuń deskryptor o podanym indeksie (przesuwa pozostałe w lewo).
  // Zwraca false gdy indeks poza zakresem.
  bool remove(uint8_t index) {
    if (index >= _count) return false;
    for (uint8_t i = index; i < _count - 1; i++) {
      _descs[i] = _descs[i + 1];
    }
    _count--;
    return true;
  }

  // Clear the whole NVS namespace.
  // Wyczyść cały namespace NVS.
  void clear() {
    Preferences prefs;
    prefs.begin(MB_NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    _count = 0;
  }

  // Descriptor access
  // Dostęp do deskryptorów
  uint8_t             count()          const { return _count; }
  const MBDescriptor& get(uint8_t i)   const { return _descs[i]; }
  MBDescriptor&       get(uint8_t i)         { return _descs[i]; }

private:
// ── Conditional layout of the descriptor table / Warunkowy layout tablicy deskryptorów ─────────────────────────────────────
#if MB_USE_PSRAM
  MBDescriptor* _descs;         // pointer to the PSRAM block / wskaźnik do bloku PSRAM
#else
  MBDescriptor  _descs[MB_MAX_DEVICES];  // static member in DRAM / statyczny member w DRAM
#endif
  uint8_t       _count;

  // Builds an NVS key: a letter + a number, e.g. 'n' + 3 → "n3"
  // The result goes into the key buffer (min 8 bytes).
  // Buduje klucz NVS: litera + liczba, np. 'n' + 3 → "n3"
  // Wynik w buforze key (min 8 bajtów).
  static void _buildKey(char* key, char prefix, uint8_t index) {
    key[0] = prefix;
    if (index < 10) {
      key[1] = '0' + index;
      key[2] = '\0';
    } else {
      key[1] = '0' + (index / 10);
      key[2] = '0' + (index % 10);
      key[3] = '\0';
    }
  }
};
