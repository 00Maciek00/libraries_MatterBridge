// =============================================================================
//  MatterBridge — MatterBridge.cpp: Implementation of the public API
//  MatterBridge — MatterBridge.cpp: Implementacja publicznego API
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

#include "MatterBridge.h"

// ============================================================
//  MatterBridge.cpp – implementacja
// ============================================================

static esp_err_t _globalAttrRead(uint16_t epId, uint32_t cId, uint32_t aId,
                                   esp_matter_attr_val_t* val, void* /*priv*/) {
  return MatterBridge.onAttrRead(epId, cId, aId, val);
}
static esp_err_t _globalAttrWrite(uint16_t epId, uint32_t cId, uint32_t aId,
                                    esp_matter_attr_val_t* val, void* /*priv*/) {
  return MatterBridge.onAttrWrite(epId, cId, aId, val);
}

// ============================================================
//  Konstruktor
// ============================================================
MatterBridgeClass::MatterBridgeClass()
  : _bootHeldSince(0), _globalCallback(nullptr),
    _started(false), _useNVS(true) {}

// ============================================================
//  setRuntimeNVS – wywołaj PRZED begin()
// ============================================================
void MatterBridgeClass::setRuntimeNVS(bool use) {
  _useNVS = use;
}

// ============================================================
//  begin() – węzeł + agregator + NVS + endpointy
// ============================================================
bool MatterBridgeClass::begin() {
  if (!MBNode.init(_globalAttrRead, _globalAttrWrite)) {
    ESP_LOGE("MatterBridge", "begin: init węzła FAILED");
    return false;
  }
  ESP_LOGI("MatterBridge", "begin: węzeł OK");

  if (_useNVS) {
    uint8_t n = _storage.load();
    ESP_LOGI("MatterBridge", "begin: wczytano %d urządzeń z NVS", n);
    _buildFromStorage();
  } else {
    ESP_LOGI("MatterBridge", "begin: NVS runtime pominięty (_useNVS=false)");
  }

  return true;
}

// ============================================================
//  start() – uruchom stos Matter
// ============================================================
bool MatterBridgeClass::start() {
  esp_err_t err = MBNode.start();
  if (err != ESP_OK) {
    ESP_LOGE("MatterBridge", "start FAILED: %s", esp_err_to_name(err));
    return false;
  }
  _started = true;
  ESP_LOGI("MatterBridge", "start: stos Matter uruchomiony");
  return true;
}

// ============================================================
//  onChange
// ============================================================
void MatterBridgeClass::onChange(MBCallback cb) {
  _globalCallback = cb;
  // Iteruj przez MB_SLOTS (nie MB_MAX_DEVICES) – sloty kanału 1 urządzeń
  // 2-kanałowych siedzą w przedziale MB_MAX_DEVICES..MB_SLOTS-1 i też
  // muszą dostać callback (np. OnOffPlug kanał 1 musi raportować zmiany).
  for (uint8_t i = 0; i < MB_SLOTS; i++) {
    if (_slots.isOccupied(i)) {
      _slots.get(i)->setCallback(cb);
    }
  }
}

// ============================================================
//  _buildFromStorage
// ============================================================
void MatterBridgeClass::_buildFromStorage() {
  for (uint8_t i = 0; i < _storage.count(); i++) {
    const MBDescriptor& desc = _storage.get(i);
    if (desc.isEmpty()) continue;
    uint8_t slot = _constructAndRegister(desc, /*runtime=*/false);
    if (slot == MB_INVALID_SLOT) {
      ESP_LOGE("MatterBridge", "_buildFromStorage: failed for '%s'", desc.name);
    }
  }
}

// ============================================================
//  _constructAndRegister
//
//  POPRAWKA: notifyTopologyChange() jest wołane TYLKO gdy stos
//  Matter jest już uruchomiony (_started=true).
//
//  Przed start() (np. addRuntime w setup()) attribute::update() wewnątrz
//  notifyTopologyChange() próbuje postować do kolejki zadań Matter,
//  która nie istnieje → assert(pxQueue != NULL) → crash.
//
//  Po start() wywołanie jest bezpieczne i potrzebne żeby hub
//  zobaczył dynamicznie dodane endpointy.
// ============================================================
uint8_t MatterBridgeClass::_constructAndRegister(const MBDescriptor& desc,
                                                   bool runtime) {
  uint8_t slot = _slots.construct(desc);
  if (slot == MB_INVALID_SLOT) {
    ESP_LOGE("MatterBridge", "_constructAndRegister: brak wolnych slotów dla '%s'", desc.name);
    return MB_INVALID_SLOT;
  }

  MBDevice* dev = _slots.get(slot);

  if (_globalCallback) {
    dev->setCallback(_globalCallback);
  }

  if (!dev->registerOn(MBNode.node())) {
    ESP_LOGE("MatterBridge", "_constructAndRegister: registerOn FAILED dla '%s'", desc.name);
    _slots.destroy(slot);
    return MB_INVALID_SLOT;
  }

  ESP_LOGI("MatterBridge", "%s: slot=%d epId=%u '%s'",
           runtime ? "Runtime" : "Static",
           slot, dev->getEndpointId(), desc.name);

  // Powiadom hub o zmianie topologii – tylko gdy stos Matter już działa.
  if (_started && runtime) {
    MBNode.notifyTopologyChange();
  }

  return slot;
}

// DODANE: wersja _constructAndRegister z ustalonym slotem
uint8_t MatterBridgeClass::_constructAndRegisterAt(uint8_t slot, const MBDescriptor& desc, bool runtime) {
    // Sprawdzenie zakresu względem MB_SLOTS (nie MB_MAX_DEVICES) –
    // sloty kanału 1 urządzeń 2-kanałowych leżą w przedziale
    // MB_MAX_DEVICES..MB_SLOTS-1, np. slot=21 dla baseIdx=1 ch1.
    // MBSlotManager.constructAt() też sprawdza MB_SLOTS – zgodność gwarantowana.
    if (slot >= MB_SLOTS || _slots.isOccupied(slot)) {
        ESP_LOGE("MatterBridge", "_constructAndRegisterAt: slot %u niedostępny", slot);
        return MB_INVALID_SLOT;
    }

    // Konstruujemy bezpośrednio w podanym slocie
    if (_slots.constructAt(slot, desc) != slot) {
        ESP_LOGE("MatterBridge", "_constructAndRegisterAt: constructAt nie powiódł się dla slotu %u", slot);
        return MB_INVALID_SLOT;
    }

    MBDevice* dev = _slots.get(slot);
    if (_globalCallback) dev->setCallback(_globalCallback);

    if (!dev->registerOn(MBNode.node())) {
        ESP_LOGE("MatterBridge", "_constructAndRegisterAt: registerOn FAILED dla '%s'", desc.name);
        _slots.destroy(slot);
        return MB_INVALID_SLOT;
    }

    ESP_LOGI("MatterBridge", "%s: slot=%d epId=%u '%s'",
             runtime ? "Runtime" : "Static",
             slot, dev->getEndpointId(), desc.name);

    if (_started && runtime) {
        MBNode.notifyTopologyChange();
    }
    return slot;
}

// ============================================================
//  TRYB STATYCZNY – NVS + restart
// ============================================================
uint8_t MatterBridgeClass::add(const MBDescriptor& desc) {
  if (!_storage.append(desc)) {
    ESP_LOGE("MatterBridge", "add: NVS pełny!");
    return MB_INVALID_SLOT;
  }
  _storage.save();
  ESP_LOGI("MatterBridge", "add: '%s' zapisane → restart...", desc.name);
  delay(200);
  ESP.restart();
  return MB_INVALID_SLOT;
}

uint8_t MatterBridgeClass::addContactSensor    (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::ContactSensor,     n, p, inv)); }
uint8_t MatterBridgeClass::addPresenceSensor   (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::PresenceSensor,    n, p, inv)); }
uint8_t MatterBridgeClass::addSwitch           (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::Switch,            n, p, inv)); }
uint8_t MatterBridgeClass::addTemperatureSensor(const char* n, uint8_t p)           { return add(MBDescriptor(MBDeviceType::TemperatureSensor, n, p, false)); }
uint8_t MatterBridgeClass::addHumiditySensor   (const char* n, uint8_t p)           { return add(MBDescriptor(MBDeviceType::HumiditySensor,    n, p, false)); }
uint8_t MatterBridgeClass::addOnOffLight       (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::OnOffLight,        n, p, inv)); }
uint8_t MatterBridgeClass::addOnOffPlug        (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::OnOffPlug,         n, p, inv)); }
uint8_t MatterBridgeClass::addDimmableLight    (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::DimmableLight,     n, p, inv)); }
uint8_t MatterBridgeClass::addColorTempLight   (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::ColorTempLight,    n, p, inv)); }
uint8_t MatterBridgeClass::addFan              (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::Fan,               n, p, inv)); }
uint8_t MatterBridgeClass::addWindowCovering   (const char* n, uint8_t p, bool inv) { return add(MBDescriptor(MBDeviceType::WindowCovering,    n, p, inv)); }
uint8_t MatterBridgeClass::addElectricalSensor (const char* n, uint8_t p)           { return add(MBDescriptor(MBDeviceType::ElectricalSensor,  n, p, false)); }

bool MatterBridgeClass::remove(uint8_t slot) {
  if (!_slots.isOccupied(slot)) return false;
  const char* name = _slots.get(slot)->getName();
  uint8_t si = _findStorageIndex(name);
  if (si == MB_INVALID_SLOT) return false;
  _storage.remove(si);
  _storage.save();
  ESP_LOGI("MatterBridge", "remove: '%s' → restart...", name);
  delay(200);
  ESP.restart();
  return true;
}

// ============================================================
//  TRYB RUNTIME – bez restartu
//
//  POPRAWKA NVS: gdy _useNVS=false, nie zapisujemy
//  do NVS. Urządzenia są tworzone przy każdym starcie w setup(). Zapis do NVS powodowałby duplikaty po czystym restarcie:
//    boot 1: NVS puste → setup() dodaje 2 urządzenia → zapisuje do NVS
//    boot 2: _buildFromStorage tworzy 2 z NVS → setup() dodaje 2 więcej = 4
//
//  POPRAWKA notifyTopologyChange: _constructAndRegister nie woła
//  notifyTopologyChange() przed _started (przed start()).
// ============================================================
uint8_t MatterBridgeClass::addRuntime(const MBDescriptor& desc) {
  if (_useNVS) {
    if (!_storage.append(desc)) {
      ESP_LOGE("MatterBridge", "addRuntime: NVS pełny!");
      return MB_INVALID_SLOT;
    }
    _storage.save();
  }

  uint8_t slot = _constructAndRegister(desc, /*runtime=*/true);
  if (slot == MB_INVALID_SLOT) {
    if (_useNVS) {
      // Cofnij NVS przy błędzie rejestracji
      _storage.remove(_storage.count() - 1);
      _storage.save();
    }
    ESP_LOGE("MatterBridge", "addRuntime: rejestracja '%s' FAILED", desc.name);
  }
  return slot;
}

// DODANE: addRuntimeAt – wersja z wymuszonym slotem
uint8_t MatterBridgeClass::addRuntimeAt(uint8_t preferredSlot, const MBDescriptor& desc) {
    // Przy setRuntimeNVS(false) NVS jest wyłączone – nie zapisujemy.
    // Jeśli jednak ktoś włączył NVS, to zapisujemy deskryptor (ale bez gwarancji slotu)
    if (_useNVS) {
        if (!_storage.append(desc)) {
            ESP_LOGE("MatterBridge", "addRuntimeAt: NVS pełny!");
            return MB_INVALID_SLOT;
        }
        _storage.save();
    }

    uint8_t slot = _constructAndRegisterAt(preferredSlot, desc, /*runtime=*/true);
    if (slot == MB_INVALID_SLOT) {
        if (_useNVS) {
            // Cofnij dodanie do NVS
            _storage.remove(_storage.count() - 1);
            _storage.save();
        }
        ESP_LOGE("MatterBridge", "addRuntimeAt: rejestracja w slocie %u FAILED dla '%s'", preferredSlot, desc.name);
    }
    return slot;
}

uint8_t MatterBridgeClass::addContactSensorRuntime    (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::ContactSensor,     n, p, inv)); }
uint8_t MatterBridgeClass::addPresenceSensorRuntime   (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::PresenceSensor,    n, p, inv)); }
uint8_t MatterBridgeClass::addSwitchRuntime           (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::Switch,            n, p, inv)); }
uint8_t MatterBridgeClass::addTemperatureSensorRuntime(const char* n, uint8_t p)           { return addRuntime(MBDescriptor(MBDeviceType::TemperatureSensor, n, p, false)); }
uint8_t MatterBridgeClass::addHumiditySensorRuntime   (const char* n, uint8_t p)           { return addRuntime(MBDescriptor(MBDeviceType::HumiditySensor,    n, p, false)); }
uint8_t MatterBridgeClass::addOnOffLightRuntime       (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::OnOffLight,        n, p, inv)); }
uint8_t MatterBridgeClass::addOnOffPlugRuntime        (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::OnOffPlug,         n, p, inv)); }
uint8_t MatterBridgeClass::addDimmableLightRuntime    (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::DimmableLight,     n, p, inv)); }
uint8_t MatterBridgeClass::addColorTempLightRuntime   (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::ColorTempLight,    n, p, inv)); }
uint8_t MatterBridgeClass::addFanRuntime              (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::Fan,               n, p, inv)); }
uint8_t MatterBridgeClass::addWindowCoveringRuntime   (const char* n, uint8_t p, bool inv) { return addRuntime(MBDescriptor(MBDeviceType::WindowCovering,    n, p, inv)); }
uint8_t MatterBridgeClass::addElectricalSensorRuntime (const char* n, uint8_t p)           { return addRuntime(MBDescriptor(MBDeviceType::ElectricalSensor,  n, p, false)); }
uint8_t MatterBridgeClass::addTempHumidSensorRuntime  (const char* n, uint8_t p)           { return addRuntime(MBDescriptor(MBDeviceType::TempHumidSensor,   n, p, false)); }

bool MatterBridgeClass::removeRuntime(uint8_t slot) {
  if (!_slots.isOccupied(slot)) return false;

  MBDevice* dev = _slots.get(slot);

  char nameBuf[MB_MAX_NAME_LEN + 1];
  strncpy(nameBuf, dev->getName(), MB_MAX_NAME_LEN);
  nameBuf[MB_MAX_NAME_LEN] = '\0';

  uint16_t epId = dev->getEndpointId();

  // Krok 1: oznacz jako nieosiągalny (chip_stack_lock wewnątrz setReachable)
  MBNode.setReachable(epId, false);
  // Nie blokujemy loop delay() – hub i tak dowie się o usunięciu przez
  // notifyTopologyChange() poniżej.

  // Krok 2: zniszcz endpoint w stosie (chip_stack_lock jest w unregisterFrom)
  if (!dev->unregisterFrom(MBNode.node())) {
    ESP_LOGW("MatterBridge", "removeRuntime: destroy ep=%u FAILED", epId);
  }

  // Krok 3: usuń z NVS (tylko gdy _useNVS)
  if (_useNVS) {
    uint8_t si = _findStorageIndex(nameBuf);
    if (si != MB_INVALID_SLOT) { _storage.remove(si); _storage.save(); }
  }

  // Krok 4: zniszcz obiekt
  _slots.destroy(slot);
  ESP_LOGI("MatterBridge", "removeRuntime: slot=%d '%s' OK", slot, nameBuf);

  // Powiadom hub o zmianie topologii – tylko gdy stos Matter już działa.
  if (_started) {
    MBNode.notifyTopologyChange();
  }

  return true;
}

bool MatterBridgeClass::setVisible(uint8_t slot, bool visible) {
  if (!_slots.isOccupied(slot)) return false;
  uint16_t epId = _slots.get(slot)->getEndpointId();
  if (epId == 0) return false;
  _slots.get(slot)->attrs().reachable = visible;
  return MBNode.setReachable(epId, visible);
}

// ============================================================
//  Sterowanie i odczyt
// ============================================================
void MatterBridgeClass::setState(uint8_t slot, bool state) {
  MBDevice* dev = _slots.get(slot);
  if (dev) dev->setState(state);
}
bool MatterBridgeClass::getState(uint8_t slot) const {
  const MBDevice* dev = _slots.get(slot);
  return dev ? dev->getState() : false;
}
void MatterBridgeClass::setCallback(uint8_t slot, MBCallback cb) {
  MBDevice* dev = _slots.get(slot);
  if (dev) dev->setCallback(cb);
}
void MatterBridgeClass::setActive(uint8_t slot, bool active) {
  MBDevice* dev = _slots.get(slot);
  if (dev) dev->setActive(active);
}
bool MatterBridgeClass::isActive(uint8_t slot) const {
  const MBDevice* dev = _slots.get(slot);
  return dev ? dev->isActive() : false;
}
MBDevice* MatterBridgeClass::device(uint8_t slot) {
  return _slots.get(slot);
}

void MatterBridgeClass::update() {
  // MB_SLOTS obejmuje sloty kanału 1 (MB_MAX_DEVICES..MB_SLOTS-1) –
  // ich tick() musi być wywołany żeby GPIO i timery działały poprawnie.
  for (uint8_t i = 0; i < MB_SLOTS; i++) {
    if (_slots.isOccupied(i)) _slots.get(i)->tick();
  }
}

void MatterBridgeClass::checkBoot(uint8_t pin, uint32_t holdMs) {
  if (digitalRead(pin) == LOW) {
    if (_bootHeldSince == 0) _bootHeldSince = millis();
    if (millis() - _bootHeldSince >= holdMs) {
      ESP_LOGI("MatterBridge", "BOOT przytrzymany → factory reset!");
      factoryReset();
    }
  } else {
    _bootHeldSince = 0;
  }
}

// ============================================================
//  Diagnostyka
// ============================================================
uint8_t MatterBridgeClass::activeCount() const {
  uint8_t n = 0;
  // MB_SLOTS – liczymy aktywne urządzenia we wszystkich slotach łącznie
  // z kanałem 1 urządzeń 2-kanałowych (sloty MB_MAX_DEVICES..MB_SLOTS-1).
  for (uint8_t i = 0; i < MB_SLOTS; i++) {
    const MBDevice* dev = _slots.get(i);
    if (dev && dev->isActive()) n++;
  }
  return n;
}
uint8_t MatterBridgeClass::registeredCount() const {
  return _slots.occupiedCount();
}
void MatterBridgeClass::printStatus() const {
  // MB_MAX_DEVICES = liczba urządzeń fizycznych (nie slotów Matter).
  // MB_SLOTS = fizyczny rozmiar tablicy (MB_MAX_DEVICES * 2).
  Serial.printf("[MatterBridge] %d/%d urządzeń (sloty Matter: %d/%d), komisjonowany=%d\n",
                _slots.occupiedCount(), (int)MB_MAX_DEVICES,
                _slots.occupiedCount(), (int)MB_SLOTS,
                (int)isCommissioned());
  for (uint8_t i = 0; i < MB_SLOTS; i++) {
    const MBDevice* d = _slots.get(i);
    if (!d) continue;
    Serial.printf("  [%2d] epId=%-5u %-31s typ=%d pin=%-3d active=%d state=%d\n",
                  i, d->getEndpointId(), d->getName(),
                  (int)d->deviceType(), d->getPin(),
                  (int)d->isActive(), (int)d->getState());
  }
}

// ============================================================
//  Komisjonowanie
// ============================================================
bool MatterBridgeClass::isCommissioned() const { return MBNode.isCommissioned(); }
bool MatterBridgeClass::getPairingCode(char* buf, size_t len) const { return MBNode.getPairingCode(buf, len); }
bool MatterBridgeClass::getQRCode(char* buf, size_t len) const      { return MBNode.getQRCode(buf, len); }

void MatterBridgeClass::decommission() {
  MBNode.decommission();
  delay(300);
  ESP.restart();
}
void MatterBridgeClass::factoryReset() {
  _storage.clear();
  MBNode.decommission();
  delay(300);
  ESP.restart();
}

// ============================================================
//  Callbacki atrybutów
// ============================================================
esp_err_t MatterBridgeClass::onAttrRead(uint16_t epId, uint32_t cId,
                                         uint32_t aId,
                                         esp_matter_attr_val_t* val) {
  MBDevice* dev = _slots.findByEpId(epId);
  if (!dev) return ESP_ERR_NOT_FOUND;
  return dev->onAttrRead(cId, aId, val);
}

esp_err_t MatterBridgeClass::onAttrWrite(uint16_t epId, uint32_t cId,
                                          uint32_t aId,
                                          esp_matter_attr_val_t* val) {
  MBDevice* dev = _slots.findByEpId(epId);
  if (!dev) return ESP_ERR_NOT_FOUND;
  return dev->onAttrWrite(cId, aId, val);
}

// ============================================================
//  Prywatne
// ============================================================
uint8_t MatterBridgeClass::_findStorageIndex(const char* name) const {
  for (uint8_t i = 0; i < _storage.count(); i++) {
    const char* sn = _storage.get(i).name;
    bool match = true;
    for (uint8_t j = 0; j <= MB_MAX_NAME_LEN; j++) {
      if (sn[j] != name[j]) { match = false; break; }
      if (sn[j] == '\0')    { break; }
    }
    if (match) return i;
  }
  return MB_INVALID_SLOT;
}

// Singletony
MatterBridgeClass MatterBridge;
MBNodeClass       MBNode;