// =============================================================================
//  MatterBridge — MBDevice.h: Base class of a Matter device
//  MatterBridge — MBDevice.h: Bazowa klasa urządzenia Matter
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
#include <Arduino.h>
#include "MBTypes.h"
#include "MBNode.h"   // extern MBNode – używane w registerOn()

// ESP-IDF headers – dostępne przez arduino-esp32 3.x bez Matter.h
#include <esp_matter.h>
#include <esp_matter_cluster.h>
#include <esp_matter_endpoint.h>
#include <esp_matter_attribute_utils.h>
#include <esp_log.h>

// IDs klastrów i atrybutów z CHIP SDK
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Attributes.h>

using esp_matter::ENDPOINT_FLAG_NONE;
using esp_matter::ENDPOINT_FLAG_BRIDGE;
using esp_matter::CLUSTER_FLAG_SERVER;
using esp_matter::CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION;

// ============================================================
//  MBDevice – natywne urządzenie Matter (bez Arduino wrapperów)
// ============================================================
class MBDevice {
public:
  explicit MBDevice(const MBDescriptor& desc)
    : _desc(desc),
      _endpoint(nullptr),
      _endpointId(0),
      _lastState(false),
      _rawState(false),
      _lastDebounce(0),
      _active(true),
      _slotIndex(MB_INVALID_SLOT),
      _callback(nullptr) {
    if (_desc.pin != MB_NO_PIN) {
      pinMode(_desc.pin, _desc.pinInverted ? INPUT_PULLUP : INPUT);
    }
  }

  virtual ~MBDevice() = default;

  // --------------------------------------------------------
  //  registerOn – rejestruje endpoint w węźle Matter.
  //  Wywołaj PRZED esp_matter::start() (statyczna) lub PO (runtime).
  // --------------------------------------------------------
  bool registerOn(esp_matter::node_t* node, bool runtime = false) {
    ESP_LOGI("MBDevice", "registerOn: start '%s' runtime=%d", _desc.name, (int)runtime);

    // endpoint::create/enable modyfikuje wewnetrzne struktury stosu Matter.
    // Gdy wywolujemy po start() (runtime), wymagany chip_stack_lock –
    // bez locka wysc z watkiem Matter moze skutkowac heap corruption lub
    // brakiem rejestracji endpointu (efekt: brak slotu po DEV_ADD w locie).
    if (runtime) esp_matter::lock::chip_stack_lock(portMAX_DELAY);

    _endpoint = _buildEndpoint(node);

    if (runtime) esp_matter::lock::chip_stack_unlock();

    if (!_endpoint) {
      ESP_LOGE("MBDevice", "registerOn: _buildEndpoint FAILED for '%s'", _desc.name);
      return false;
    }

    _endpointId = esp_matter::endpoint::get_id(_endpoint);
    ESP_LOGI("MBDevice", "registerOn: epId=%u for '%s'", _endpointId, _desc.name);

    // Przypnij endpoint pod Agregator (EP1)
    extern MBNodeClass MBNode;
    uint16_t aggrEpId = MBNode.aggregatorEpId();
    if (aggrEpId != 0) {
      esp_matter::node_t* nd = esp_matter::node::get();
      // Odczyt struktury agregatora – rowniez pod lockiem przy runtime
      if (runtime) esp_matter::lock::chip_stack_lock(portMAX_DELAY);
      esp_matter::endpoint_t* aggrEp = nd
        ? esp_matter::endpoint::get(nd, aggrEpId) : nullptr;
      if (aggrEp) {
        esp_matter::endpoint::set_parent_endpoint(_endpoint, aggrEp);
        ESP_LOGI("MBDevice", "registerOn: parent set to aggrEp=%u", aggrEpId);
      } else {
        ESP_LOGW("MBDevice", "registerOn: aggregator ep not found (aggrEpId=%u)", aggrEpId);
      }
      if (runtime) esp_matter::lock::chip_stack_unlock();
    }

    _setNodeLabel(_desc.name);
    ESP_LOGI("MBDevice", "registerOn: OK '%s' epId=%u", _desc.name, _endpointId);
    return true;
  }

  // Usuń endpoint ze stosu Matter (runtime)
  bool unregisterFrom(esp_matter::node_t* node) {
    if (!_endpoint || _endpointId == 0) return false;

    // endpoint::destroy() modyfikuje wewnętrzne struktury stosu Matter –
    // musi być wywołane pod chip_stack_lock gdy wywołujemy z Arduino loop().
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_err_t err = esp_matter::endpoint::destroy(node, _endpoint);
    esp_matter::lock::chip_stack_unlock();

    if (err != ESP_OK) {
      ESP_LOGE("MBDevice", "destroy ep=%u: %s", _endpointId, esp_err_to_name(err));
      return false;
    }
    _endpoint   = nullptr;
    _endpointId = 0;
    return true;
  }

  // --------------------------------------------------------
  //  applyState / deviceType – implementuj w klasach pochodnych
  // --------------------------------------------------------
  virtual void applyState(bool state) = 0;
  virtual MBDeviceType deviceType() const = 0;

  // --------------------------------------------------------
  //  reportAttribute – informuje stos Matter o zmianie wartości.
  // --------------------------------------------------------
  bool reportAttribute(uint32_t clusterId, uint32_t attrId,
                       esp_matter_attr_val_t* val) {
    if (_endpointId == 0) {
      ESP_LOGW("MBDevice", "reportAttribute: epId=0 for '%s' – pomijam", _desc.name);
      return false;
    }

    // POPRAWKA: chip_stack_lock wymagany gdy wywołujemy attribute::update()
    // z kontekstu loop() (nie z wątku Matter). Bez locka wyścig z wątkiem
    // chip prowadzi do korupcji sterty i losowych zawieszeń/restartów.
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_err_t err = esp_matter::attribute::update(
      _endpointId, clusterId, attrId, val);
    esp_matter::lock::chip_stack_unlock();

    if (err != ESP_OK) {
      ESP_LOGW("MBDevice", "report ep=%u cl=0x%04lx at=0x%04lx err=%s",
               _endpointId, (unsigned long)clusterId,
               (unsigned long)attrId, esp_err_to_name(err));
      return false;
    }
    return true;
  }

  // --------------------------------------------------------
  //  onAttrRead / onAttrWrite – callbacki external storage
  // --------------------------------------------------------
  virtual esp_err_t onAttrRead(uint32_t clusterId,
                                uint32_t attrId,
                                esp_matter_attr_val_t* val) {
    using namespace chip::app::Clusters;

    if (clusterId == BooleanState::Id &&
        attrId    == BooleanState::Attributes::StateValue::Id) {
      *val = esp_matter_bool(_attrs.boolState);
      return ESP_OK;
    }
    if (clusterId == OccupancySensing::Id &&
        attrId    == OccupancySensing::Attributes::Occupancy::Id) {
      *val = esp_matter_uint8(_attrs.occupancy);
      return ESP_OK;
    }
    if (clusterId == BridgedDeviceBasicInformation::Id) {
      if (attrId == BridgedDeviceBasicInformation::Attributes::Reachable::Id) {
        *val = esp_matter_bool(_attrs.reachable);
        return ESP_OK;
      }
      if (attrId == BridgedDeviceBasicInformation::Attributes::NodeLabel::Id) {
        uint8_t len = 0;
        while (len < MB_MAX_NAME_LEN && _desc.name[len]) len++;
        *val = esp_matter_char_str(const_cast<char*>(_desc.name), len);
        return ESP_OK;
      }
      // SoftwareVersion (0x0009) – odpytywany przez SmartThings przy komisjonowaniu.
      // Brak odpowiedzi → błąd 586 (UNSUPPORTED_ATTRIBUTE) → hub blokuje odczyt
      // całego endpointu i wyświetla NaN / brak danych.
      if (attrId == BridgedDeviceBasicInformation::Attributes::SoftwareVersion::Id) {
        *val = esp_matter_uint32(1);
        return ESP_OK;
      }
      // UniqueID (0x0010) – jednoznaczna identyfikacja urządzenia między restartami mostka.
      if (attrId == BridgedDeviceBasicInformation::Attributes::UniqueID::Id) {
        uint8_t len = 0;
        while (len < MB_MAX_NAME_LEN && _desc.name[len]) len++;
        *val = esp_matter_char_str(const_cast<char*>(_desc.name), len);
        return ESP_OK;
      }
      // FeatureMap i ClusterRevision dla BridgedDeviceBasicInformation
      if (attrId == 0xFFFC) { *val = esp_matter_uint32(0); return ESP_OK; }
      if (attrId == 0xFFFD) { *val = esp_matter_uint16(2); return ESP_OK; }
    }
    return ESP_ERR_NOT_FOUND;
  }

  virtual esp_err_t onAttrWrite(uint32_t clusterId,
                                 uint32_t attrId,
                                 esp_matter_attr_val_t* val) {
    return ESP_ERR_NOT_FOUND;
  }

  // --------------------------------------------------------
  //  tick – GPIO debouncing + aktualizacja stanu
  // --------------------------------------------------------
  bool tick() {
    if (!_active || _desc.pin == MB_NO_PIN) return false;

    bool raw     = (bool)digitalRead(_desc.pin);
    bool logical = _desc.pinInverted ? !raw : raw;

    if (logical != _rawState) {
      _rawState     = logical;
      _lastDebounce = millis();
    }

    bool stable = (millis() - _lastDebounce) >= MB_DEBOUNCE_MS
                  ? logical : _lastState;

    if (stable == _lastState) return false;

    _lastState = stable;
    applyState(_lastState);
    if (_callback) _callback(_slotIndex, _lastState);
    return true;
  }

  // Ręczna aktualizacja bez GPIO
  bool setState(bool state) {
    if (!_active || state == _lastState) return false;
    _lastState = state;
    applyState(_lastState);
    if (_callback) _callback(_slotIndex, _lastState);
    return true;
  }

  // --------------------------------------------------------
  //  Gettery / settery
  // --------------------------------------------------------
  bool            getState()       const { return _lastState; }
  bool            isActive()       const { return _active; }
  void            setActive(bool a)      { _active = a; }
  void            setCallback(MBCallback cb) { _callback = cb; }
  void            setSlotIndex(uint8_t i)    { _slotIndex = i; }
  uint8_t         getSlotIndex()   const { return _slotIndex; }
  uint16_t        getEndpointId()  const { return _endpointId; }
  bool            isRegistered()   const { return _endpointId != 0; }
  const char*     getName()        const { return _desc.name; }
  uint8_t         getPin()         const { return _desc.pin; }
  bool            isPinInverted()  const { return _desc.pinInverted; }
  const MBDescriptor& getDescriptor() const { return _desc; }
  MBAttrStore&    attrs()                { return _attrs; }

protected:
  MBDescriptor         _desc;
  MBAttrStore          _attrs;
  esp_matter::endpoint_t* _endpoint;
  uint16_t             _endpointId;
  bool                 _lastState;
  bool                 _rawState;
  unsigned long        _lastDebounce;
  bool                 _active;
  uint8_t              _slotIndex;
  MBCallback           _callback;

  virtual esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) = 0;

  // --------------------------------------------------------
  //  _addBridgedBasicInfo – dodaje klaster BridgedDeviceBasicInformation
  //
  //  POPRAWKA: po create() klastra dodajemy atrybut UniqueID (0x0009).
  //  Jest wymagany przez spec Matter dla bridged devices – pozwala hubowi
  //  jednoznacznie identyfikować urządzenie między restartami mostka.
  //  Bez niego hub loguje błąd 0x586 (EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE).
  // --------------------------------------------------------
  bool _addBridgedBasicInfo(esp_matter::endpoint_t* ep) {
    esp_matter::cluster::bridged_device_basic_information::config_t cfg = {};
    cfg.reachable = true;
    auto* cl = esp_matter::cluster::bridged_device_basic_information::create(
      ep, &cfg, CLUSTER_FLAG_SERVER);
    if (!cl) {
      ESP_LOGE("MBDevice", "_addBridgedBasicInfo: create FAILED for '%s'", _desc.name);
      return false;
    }

    // POPRAWKA: Stos Matter sprawdza czy atrybut istnieje w endpoincie ZANIM
    // wywoła onAttrRead(). Bez jawnego attribute::create() zapytanie o SoftwareVersion
    // (0x0009) lub UniqueID (0x0010) jest odrzucane z err=586
    // (EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE) – stąd masa błędów w logu przy komisjonowaniu.
    // Flaga 0x01 = ta sama co używa reszta kodu w projekcie (esp_matter attribute flags).

    // SoftwareVersion (0x0009)
    {
      using namespace chip::app::Clusters::BridgedDeviceBasicInformation;
      esp_matter_attr_val_t val = esp_matter_uint32(1);
      auto* a = esp_matter::attribute::create(
        cl, Attributes::SoftwareVersion::Id, 0x01, val);
      if (!a) {
        ESP_LOGW("MBDevice", "_addBridgedBasicInfo: SoftwareVersion attr FAILED for '%s'", _desc.name);
      }
    }

    // UniqueID (0x0010) – jednoznaczna identyfikacja urządzenia między restartami mostka
    {
      using namespace chip::app::Clusters::BridgedDeviceBasicInformation;
      uint8_t len = 0;
      while (len < MB_MAX_NAME_LEN && _desc.name[len]) len++;
      esp_matter_attr_val_t val = esp_matter_char_str(
        const_cast<char*>(_desc.name), len);
      auto* a = esp_matter::attribute::create(
        cl, Attributes::UniqueID::Id, 0x01, val);
      if (!a) {
        ESP_LOGW("MBDevice", "_addBridgedBasicInfo: UniqueID attr FAILED for '%s'", _desc.name);
      }
    }

    return true;
  }

  // Ustaw NodeLabel widoczną w hubie
  void _setNodeLabel(const char* name) {
    if (!_endpoint) return;
    using namespace chip::app::Clusters::BridgedDeviceBasicInformation;
    esp_matter::cluster_t* cl = esp_matter::cluster::get(_endpoint, Id);
    if (!cl) return;
    esp_matter::attribute_t* attr =
      esp_matter::attribute::get(cl, Attributes::NodeLabel::Id);
    if (!attr) return;
    uint8_t len = 0;
    while (len < MB_MAX_NAME_LEN && name[len]) len++;
    esp_matter_attr_val_t val = esp_matter_char_str(
      const_cast<char*>(name), len);
    esp_matter::attribute::set_val(attr, &val);
  }
};
