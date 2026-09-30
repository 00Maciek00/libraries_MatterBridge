// =============================================================================
//  MatterBridge — MBConfig.h: Central configuration
//  MatterBridge — MBConfig.h: Centralna konfiguracja
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

// ============================================================
//  Board detection / Detekcja płytki
//
//  Chip detected via CONFIG_IDF_TARGET_* (any ESP32-C6 / ESP32-S3 board)
//  or Seeed Xiao macros. Other chips → compile error.
//  Chip rozpoznawany po CONFIG_IDF_TARGET_* (dowolna płytka z C6 / S3)
//  lub po makrach Xiao. Inne chipy → błąd kompilacji.
// ============================================================
#if __has_include(<sdkconfig.h>)
  #include <sdkconfig.h>
#endif
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(ARDUINO_XIAO_ESP32C6)
  #define MB_TARGET_C6  1
  #define MB_TARGET_S3  0
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_XIAO_ESP32S3)
  #define MB_TARGET_C6  0
  #define MB_TARGET_S3  1
#else
  #error "MatterBridge: supported chips are ESP32-C6 and ESP32-S3 (select such a board)."
#endif

// ============================================================
//  Device count and Matter slots / Liczba urządzeń i sloty Matter
//
//  Slot layout (same on C6 and S3, only the scale differs):
//  Schemat slotów (taki sam na C6 i S3, różni się tylko skala):
//
//    channel 0 / kanał 0:  slots / sloty  0 .. MB_MAX_DEVICES-1
//    channel 1 / kanał 1:  slots / sloty  MB_MAX_DEVICES .. MB_SLOTS-1
//
//  Example, 2-channel device with baseIdx=7 / Przykład, urządzenie 2-kanałowe baseIdx=7:
//    C6: channel 0 slot = 7,  channel 1 slot = 27  (7 + 20)
//    S3: channel 0 slot = 7,  channel 1 slot = 57  (7 + 50)
//
//  Channel-1 slot = baseIdx + MB_MAX_DEVICES (fixed formula).
//  Slot kanału 1 = baseIdx + MB_MAX_DEVICES (stała formuła).
//
//  C6: 20 devices → 40 slots / 20 urządzeń → 40 slotów
//  S3: 50 devices → 100 slots, table in PSRAM / 50 urządzeń → 100 slotów, tablica w PSRAM
//
//  NOTE: the Matter stack (esp-matter) has an internal limit of dynamic
//  endpoints. On S3 with PSRAM, 50 devices is safe.
//  UWAGA: stos Matter (esp-matter) ma wewnętrzny limit dynamicznych
//  endpointów. Na S3 z PSRAM 50 urządzeń jest bezpieczne.
//
//  Override with / Nadpisz przez: -DMB_MAX_DEVICES=n
// ============================================================
#if MB_TARGET_S3
  #ifndef MB_MAX_DEVICES
    #define MB_MAX_DEVICES  50
  #endif
  // MB_USE_PSRAM = 1: slot and descriptor tables are allocated in PSRAM
  // (falls back to DRAM if PSRAM is unavailable).
  // MB_USE_PSRAM = 1: tablice slotów i deskryptorów w PSRAM
  // (fallback na DRAM, gdy PSRAM niedostępna).
  #ifndef MB_USE_PSRAM
    #define MB_USE_PSRAM  1
  #endif
#else
  #ifndef MB_MAX_DEVICES
    #define MB_MAX_DEVICES  20
  #endif
  #ifndef MB_USE_PSRAM
    #define MB_USE_PSRAM  0
  #endif
#endif

// MB_SLOTS = MB_MAX_DEVICES * 2 – always / zawsze
// C6: 20 × 2 = 40 Matter slots / slotów Matter
// S3: 50 × 2 = 100 Matter slots / slotów Matter
#ifndef MB_SLOTS
  #define MB_SLOTS        (MB_MAX_DEVICES * 2)
#endif

// ============================================================
//  BOOT button pin (factory reset – MatterBridge.checkBoot())
//  Pin przycisku BOOT (factory reset – MatterBridge.checkBoot())
//
//  C6: GPIO9, S3: GPIO0. Override with -DMB_BOOT_PIN=x if needed.
//  C6: GPIO9, S3: GPIO0. Nadpisz przez -DMB_BOOT_PIN=x jeśli trzeba.
// ============================================================
#ifndef MB_BOOT_PIN
  #if MB_TARGET_C6
    #define MB_BOOT_PIN  9
  #else
    #define MB_BOOT_PIN  0
  #endif
#endif

// ============================================================
//  Other parameters (same on both chips)
//  Pozostałe parametry (takie same na obu układach)
// ============================================================

// Max device name length (without null terminator)
// Maksymalna długość nazwy urządzenia (bez null-terminatora)
#ifndef MB_MAX_NAME_LEN
  #define MB_MAX_NAME_LEN   31
#endif

// GPIO debounce time in ms / Czas debouncingu GPIO w ms
#ifndef MB_DEBOUNCE_MS
  #define MB_DEBOUNCE_MS    50
#endif

// Sentinels / Sentinele
#define MB_INVALID_SLOT   255
#define MB_NO_PIN         255

// Preferences (NVS) namespace / Namespace dla Preferences (NVS)
#define MB_NVS_NAMESPACE  "MatterBridge"

// Aggregator endpoint ID (EP1 right after Root EP0)
// Endpoint ID agregatora (EP1 zaraz po Root EP0)
#define MB_AGGREGATOR_EP_ID  1

// Vendor / Product ID – test values, change to production IDs before release
// Vendor / Product ID – wartości testowe, zmień na produkcyjne przed wdrożeniem
#ifndef MB_VENDOR_ID
  #define MB_VENDOR_ID    0xFFF1
#endif
#ifndef MB_PRODUCT_ID
  #define MB_PRODUCT_ID   0x8001
#endif
