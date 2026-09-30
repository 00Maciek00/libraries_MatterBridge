// =============================================================================
//  MatterBridge — MBDevices.h: All device types (placement new, no heap)
//  MatterBridge — MBDevices.h: Wszystkie typy urządzeń (placement new, bez sterty)
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

// ============================================================
//  MBDevices.h – all Matter device types
//
//  RULES (apply to every class):
//
//  1. _buildEndpoint() checks the result of EVERY create().
//     If any create() returns nullptr → return nullptr.
//     No check = a "dead" device in the hub with no error message.
//
//  2. _addBridgedBasicInfo(ep) returns bool.
//     Call: if (!_addBridgedBasicInfo(ep)) return nullptr;
//
//  3. feature::add() is called AFTER the cluster's create() (as in MBSwitch).
//     Do NOT set features before create() – unstable in esp-matter 3.3.x.
//
//  4. applyState() → ONLY updates _attrs + reportAttribute().
//     It does not call esp_matter::attribute::set_val() directly.
//
//  5. onAttrRead() → returns the value from _attrs.
//     The stack calls this callback on every read.
//
//  6. [FIX] Every _buildEndpoint() finishes with
//     esp_matter::endpoint::add_device_type(ep, DEVICE_TYPE_ID, 1)
//     using the proper DeviceType ID per the Matter spec.
//     Without it the hub sees a "Bridged Node" (0x0013) instead of the right type.
//
//  7. [FIX] onAttrRead() handles FeatureMap (0xFFFC) and
//     ClusterRevision (0xFFFD) for every cluster – required by the spec.
//
//  MBDevices.h – wszystkie typy urządzeń Matter
//
//  ZASADY (obowiązują każdą klasę):
//
//  1. _buildEndpoint() sprawdza wynik KAŻDEGO create().
//     Jeśli którekolwiek create() zwróci nullptr → return nullptr.
//     Brak sprawdzenia = "trup" w hubie bez żadnego komunikatu błędu.
//
//  2. _addBridgedBasicInfo(ep) zwraca bool.
//     Wywołuj: if (!_addBridgedBasicInfo(ep)) return nullptr;
//
//  3. feature::add() wywoływujemy PO create() klastra (jak w MBSwitch).
//     NIE ustawiamy features przed create() – niestabilne w esp-matter 3.3.x.
//
//  4. applyState() → TYLKO aktualizuje _attrs + reportAttribute().
//     Nie wywołuje esp_matter::attribute::set_val() bezpośrednio.
//
//  5. onAttrRead() → zwraca wartość z _attrs.
//     Stos woła ten callback przy każdym odczycie.
//
//  6. [POPRAWKA] Każdy _buildEndpoint() wywołuje na końcu
//     esp_matter::endpoint::add_device_type(ep, DEVICE_TYPE_ID, 1)
//     z właściwym DeviceType ID wg spec Matter.
//     Bez tego hub widzi "Bridged Node" (0x0013) zamiast właściwego typu.
//
//  7. [POPRAWKA] onAttrRead() obsługuje FeatureMap (0xFFFC) i
//     ClusterRevision (0xFFFD) dla każdego klastra – wymagane przez spec.
// ============================================================

// ============================================================
//  ── PASSIVE SENSORS / CZUJNIKI PASYWNE ──────────────────────────────────────
// ============================================================

// ============================================================
//  MBContactSensor
//  Matter: Contact Sensor (0x0015)
//  Cluster: BooleanState – StateValue (bool)
//  Usage / Użycie:
//    MatterBridge.setState(slot, true);   // contact closed / kontakt zamknięty
//    MatterBridge.setState(slot, false);  // contact open / kontakt otwarty
// ============================================================
class MBContactSensor final : public MBDevice {
public:
  explicit MBContactSensor(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::ContactSensor; }

  void applyState(bool state) override {
    _attrs.boolState = state;
    esp_matter_attr_val_t val = esp_matter_bool(state);
    reportAttribute(chip::app::Clusters::BooleanState::Id,
                    chip::app::Clusters::BooleanState::Attributes::StateValue::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == BooleanState::Id) {
      if (aId == BooleanState::Attributes::StateValue::Id) {
        *val = esp_matter_bool(_attrs.boolState); return ESP_OK;
      }
      // FeatureMap: BooleanState has no defined features → 0 / nie ma zdefiniowanych feature'ów
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0); return ESP_OK; }
      // ClusterRevision: BooleanState rev 1
      if (aId == 0xFFFD) { *val = esp_matter_uint16(1); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("ContactSensor", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("ContactSensor", "identify create failed"); return nullptr; }

    esp_matter::cluster::boolean_state::config_t bCfg = {};
    bCfg.state_value = false;
    // boolean_state: tylko CLUSTER_FLAG_SERVER – czujnik nie obsługuje zapisów z huba
    auto* bs = esp_matter::cluster::boolean_state::create(
      ep, &bCfg, CLUSTER_FLAG_SERVER);
    if (!bs) { ESP_LOGE("ContactSensor", "boolean_state create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Contact Sensor (0x0015)
    // POPRAWKA: ustaw właściwy DeviceType ID – Contact Sensor (0x0015)
    // Without it the hub sees "Bridged Node" instead of "Contact Sensor"
    // Bez tego hub widzi "Bridged Node" zamiast "Contact Sensor"
    esp_matter::endpoint::add_device_type(ep, 0x0015, 1);

    return ep;
  }
};

// ============================================================
//  MBPresenceSensor
//  Matter: Occupancy Sensor (0x0107)
//  Cluster: OccupancySensing – Occupancy (bitmap8)
//  Feature: PassiveInfrared (PIR) – added AFTER the cluster's create()
//  Feature: PassiveInfrared (PIR) – dodawana PO create() klastra
// ============================================================
class MBPresenceSensor final : public MBDevice {
public:
  explicit MBPresenceSensor(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::PresenceSensor; }

  void applyState(bool state) override {
    _attrs.occupancy = state ? 1 : 0;
    esp_matter_attr_val_t val = esp_matter_bitmap8(_attrs.occupancy);
    reportAttribute(chip::app::Clusters::OccupancySensing::Id,
                    chip::app::Clusters::OccupancySensing::Attributes::Occupancy::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OccupancySensing::Id) {
      if (aId == OccupancySensing::Attributes::Occupancy::Id) {
        *val = esp_matter_bitmap8(_attrs.occupancy); return ESP_OK;
      }
      if (aId == OccupancySensing::Attributes::OccupancySensorType::Id) {
        *val = esp_matter_enum8(0); return ESP_OK;  // 0 = PIR
      }
      if (aId == OccupancySensing::Attributes::OccupancySensorTypeBitmap::Id) {
        *val = esp_matter_bitmap8(1); return ESP_OK;  // bit0 = PIR
      }
      // FeatureMap: bit0 = PIR (PassiveInfrared) = 1
      if (aId == 0xFFFC) { *val = esp_matter_uint32(1); return ESP_OK; }
      // ClusterRevision: OccupancySensing rev 3 (Matter 1.x)
      if (aId == 0xFFFD) { *val = esp_matter_uint16(3); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("PresenceSensor", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("PresenceSensor", "identify create failed"); return nullptr; }

    esp_matter::cluster::occupancy_sensing::config_t oCfg = {};
    oCfg.occupancy             = (uint8_t)0;
    oCfg.occupancy_sensor_type = (uint8_t)0;   // 0 = PIR

    auto* oc = esp_matter::cluster::occupancy_sensing::create(
      ep, &oCfg, CLUSTER_FLAG_SERVER);
    if (!oc) { ESP_LOGE("PresenceSensor", "occupancy_sensing create failed"); return nullptr; }

    // NOTE: in esp32c6-libs 3.3.6 passive_infrared has no config_t –
    // add() takes only a pointer to the cluster (no configuration struct).
    // The PIR delay attributes (PIROccupiedToUnoccupiedDelay etc.) are handled
    // by onAttrRead() above, where FeatureMap returns bit0=1 (PIR).
    // UWAGA: W esp32c6-libs 3.3.6 passive_infrared nie ma config_t –
    // add() przyjmuje wyłącznie wskaźnik na klaster (bez struktury konfiguracji).
    // Atrybuty opóźnień PIR (PIROccupiedToUnoccupiedDelay itp.) obsługiwane są
    // przez onAttrRead() wyżej, gdzie FeatureMap zwraca bit0=1 (PIR).
    esp_matter::cluster::occupancy_sensing::feature::passive_infrared::add(oc);

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // DeviceType ID: Occupancy Sensor (0x0107)
    esp_matter::endpoint::add_device_type(ep, 0x0107, 1);

    return ep;
  }
};

// ============================================================
//  MBSwitch
//  Matter: Generic Switch (0x000F) – momentary
//  Cluster: Switch with features MS + MSR + MSLP + MSMP
//  Cluster: Switch z feature MS + MSR + MSLP + MSMP
//
//  Interactions:
//  Interakcje:
//    applyState(true/false)  → InitialPress / ShortRelease
//    sendMultiPress(n)       → MultiPressComplete (n=1/2/3 clicks / kliknięć)
//    sendLongPress()         → LongPress
//    sendLongRelease()       → LongRelease
//
//  Access to type-specific methods:
//  Dostęp do metod specyficznych:
//    auto* sw = static_cast<MBSwitch*>(MatterBridge.device(slot));
//    sw->sendMultiPress(2);   // double click / dwuklik
// ============================================================
class MBSwitch final : public MBDevice {
public:
  explicit MBSwitch(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::Switch; }

  void applyState(bool state) override {
    _attrs.switchPosition = state ? 1 : 0;
    if (_endpointId == 0) return;
    // FIX: chip_stack_lock is required when sending switch events
    // from loop() context – without the lock a race with the Matter thread
    // corrupts the stack's internal structures (heap corruption / reboots).
    // POPRAWKA: chip_stack_lock wymagany przy wysyłaniu zdarzeń switch
    // z kontekstu loop() – bez locka wyścig z wątkiem Matter powoduje
    // korupcję wewnętrznych struktur stosu (heap corruption / restarty).
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    if (state)
      esp_matter::cluster::switch_cluster::event::send_initial_press(
        _endpointId, _attrs.switchPosition);
    else
      esp_matter::cluster::switch_cluster::event::send_short_release(
        _endpointId, _attrs.switchPosition);
    esp_matter::lock::chip_stack_unlock();
  }

  bool sendMultiPress(uint8_t count) {
    if (_endpointId == 0 || count == 0 || count > 3) return false;
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_matter::cluster::switch_cluster::event::send_multi_press_complete(
      _endpointId, /*previousPosition=*/0, count);
    esp_matter::lock::chip_stack_unlock();
    return true;
  }

  bool sendLongPress() {
    if (_endpointId == 0) return false;
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_matter::cluster::switch_cluster::event::send_long_press(
      _endpointId, _attrs.switchPosition);
    esp_matter::lock::chip_stack_unlock();
    return true;
  }

  bool sendLongRelease() {
    if (_endpointId == 0) return false;
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_matter::cluster::switch_cluster::event::send_long_release(
      _endpointId, _attrs.switchPosition);
    esp_matter::lock::chip_stack_unlock();
    return true;
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters::Switch;
    if (cId == Id) {
      if (aId == Attributes::NumberOfPositions::Id) {
        *val = esp_matter_uint8(2); return ESP_OK;
      }
      if (aId == Attributes::CurrentPosition::Id) {
        *val = esp_matter_uint8(_attrs.switchPosition); return ESP_OK;
      }
      if (aId == Attributes::MultiPressMax::Id) {
        *val = esp_matter_uint8(3); return ESP_OK;
      }
      // FeatureMap: the stack manages this automatically via feature::add(),
      // but we return it explicitly to be safe.
      // FeatureMap: stos zarządza tym automatycznie przez feature::add(),
      // ale zwracamy jawnie dla pewności.
      // MS=bit0, MSR=bit1, MSLP=bit2, MSMP=bit3 → 0x0F
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0x0F); return ESP_OK; }
      // ClusterRevision: Switch rev 1
      if (aId == 0xFFFD) { *val = esp_matter_uint16(1); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("Switch", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("Switch", "identify create failed"); return nullptr; }

    esp_matter::cluster::switch_cluster::config_t sCfg = {};
    sCfg.number_of_positions = 2;
    sCfg.current_position    = 0;
    auto* swCl = esp_matter::cluster::switch_cluster::create(
      ep, &sCfg, CLUSTER_FLAG_SERVER);
    if (!swCl) { ESP_LOGE("Switch", "switch_cluster create failed"); return nullptr; }

    // Features in the required order (Matter spec §1.11.4 dependencies):
    // Features w wymaganej kolejności (zależności spec Matter §1.11.4):
    //   MS → MSR → MSLP → MSMP
    esp_matter::cluster::switch_cluster::feature::momentary_switch::add(swCl);
    esp_matter::cluster::switch_cluster::feature::momentary_switch_release::add(swCl);
    esp_matter::cluster::switch_cluster::feature::momentary_switch_long_press::add(swCl);
    esp_matter::cluster::switch_cluster::feature::momentary_switch_multi_press::config_t msmpCfg = {};
    msmpCfg.multi_press_max = 3;
    esp_matter::cluster::switch_cluster::feature::momentary_switch_multi_press::add(swCl, &msmpCfg);

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Generic Switch (0x000F)
    // POPRAWKA: ustaw właściwy DeviceType ID – Generic Switch (0x000F)
    esp_matter::endpoint::add_device_type(ep, 0x000F, 1);

    return ep;
  }
};

// ============================================================
//  MBTemperatureSensor
//  Matter: Temperature Sensor (0x0302)
//  Cluster: TemperatureMeasurement – MeasuredValue (int16 ×100)
//  Usage / Użycie:
//    auto* t = static_cast<MBTemperatureSensor*>(MatterBridge.device(slot));
//    t->setTemperature(21.5f);   // 21.5°C
// ============================================================
class MBTemperatureSensor final : public MBDevice {
public:
  explicit MBTemperatureSensor(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::TemperatureSensor; }

  void applyState(bool /*unused*/) override {}

  void setTemperature(float celsius) {
    _attrs.temperature = (int16_t)(celsius * 100.0f);
    esp_matter_attr_val_t val = esp_matter_int16(_attrs.temperature);
    reportAttribute(
      chip::app::Clusters::TemperatureMeasurement::Id,
      chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Id,
      &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == TemperatureMeasurement::Id) {
      if (aId == TemperatureMeasurement::Attributes::MeasuredValue::Id) {
        *val = esp_matter_int16(_attrs.temperature); return ESP_OK;
      }
      if (aId == TemperatureMeasurement::Attributes::MinMeasuredValue::Id) {
        *val = esp_matter_int16(-10000); return ESP_OK;
      }
      if (aId == TemperatureMeasurement::Attributes::MaxMeasuredValue::Id) {
        *val = esp_matter_int16(10000); return ESP_OK;
      }
      // FeatureMap: TemperatureMeasurement has no features → 0 / nie ma feature'ów
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0); return ESP_OK; }
      // ClusterRevision: TemperatureMeasurement rev 4
      if (aId == 0xFFFD) { *val = esp_matter_uint16(4); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("TempSensor", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("TempSensor", "identify create failed"); return nullptr; }

    esp_matter::cluster::temperature_measurement::config_t tCfg = {};
    tCfg.measured_value     = (int16_t)0;
    tCfg.min_measured_value = (int16_t)-10000;
    tCfg.max_measured_value = (int16_t)10000;
    auto* tc = esp_matter::cluster::temperature_measurement::create(
      ep, &tCfg, CLUSTER_FLAG_SERVER);
    if (!tc) { ESP_LOGE("TempSensor", "temperature_measurement create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Temperature Sensor (0x0302)
    // POPRAWKA: ustaw właściwy DeviceType ID – Temperature Sensor (0x0302)
    esp_matter::endpoint::add_device_type(ep, 0x0302, 1);

    return ep;
  }
};

// ============================================================
//  MBHumiditySensor
//  Matter: Humidity Sensor (0x0307)
//  Cluster: RelativeHumidityMeasurement – MeasuredValue (uint16 ×100)
//  Usage / Użycie:
//    auto* h = static_cast<MBHumiditySensor*>(MatterBridge.device(slot));
//    h->setHumidity(55.0f);   // 55%RH
// ============================================================
class MBHumiditySensor final : public MBDevice {
public:
  explicit MBHumiditySensor(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::HumiditySensor; }

  void applyState(bool /*unused*/) override {}

  void setHumidity(float percent) {
    _attrs.humidity = (uint16_t)(percent * 100.0f);
    esp_matter_attr_val_t val = esp_matter_uint16(_attrs.humidity);
    reportAttribute(
      chip::app::Clusters::RelativeHumidityMeasurement::Id,
      chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id,
      &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == RelativeHumidityMeasurement::Id) {
      if (aId == RelativeHumidityMeasurement::Attributes::MeasuredValue::Id) {
        *val = esp_matter_uint16(_attrs.humidity); return ESP_OK;
      }
      if (aId == RelativeHumidityMeasurement::Attributes::MinMeasuredValue::Id) {
        *val = esp_matter_uint16(0); return ESP_OK;
      }
      if (aId == RelativeHumidityMeasurement::Attributes::MaxMeasuredValue::Id) {
        *val = esp_matter_uint16(10000); return ESP_OK;
      }
      // FeatureMap: RelativeHumidityMeasurement has no features → 0 / nie ma feature'ów
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0); return ESP_OK; }
      // ClusterRevision: RelativeHumidityMeasurement rev 3
      if (aId == 0xFFFD) { *val = esp_matter_uint16(3); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("HumidSensor", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("HumidSensor", "identify create failed"); return nullptr; }

    esp_matter::cluster::relative_humidity_measurement::config_t hCfg = {};
    hCfg.measured_value     = (uint16_t)0;
    hCfg.min_measured_value = (uint16_t)0;
    hCfg.max_measured_value = (uint16_t)10000;
    auto* hc = esp_matter::cluster::relative_humidity_measurement::create(
      ep, &hCfg, CLUSTER_FLAG_SERVER);
    if (!hc) { ESP_LOGE("HumidSensor", "humidity_measurement create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Humidity Sensor (0x0307)
    // POPRAWKA: ustaw właściwy DeviceType ID – Humidity Sensor (0x0307)
    esp_matter::endpoint::add_device_type(ep, 0x0307, 1);

    return ep;
  }
};

// ============================================================
//  ── CONTROL (hub sends commands) / STEROWANIE (hub wysyła komendy) ───────────────────────
// ============================================================

// ============================================================
//  MBOnOffLight
//  Matter: On/Off Light (0x0100)
//  Cluster: OnOff z feature Lighting (LT)
//  Callback invoked when the hub changes the state.
//  Callback wywołany gdy hub zmienia stan.
//
//  NOTE: The class is marked final – do NOT inherit from it.
//  For plugs use MBOnOffPlug (a separate class, different OnOff
//  cluster configuration without the Lighting feature).
//  UWAGA: Klasa jest oznaczona jako final – NIE dziedzicz po niej.
//  Do gniazdek używaj MBOnOffPlug (osobna klasa, inna konfiguracja
//  klastra OnOff bez feature Lighting).
// ============================================================
class MBOnOffLight final : public MBDevice {
public:
  explicit MBOnOffLight(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::OnOffLight; }

  void applyState(bool state) override {
    _attrs.onOff = state;
    esp_matter_attr_val_t val = esp_matter_bool(state);
    reportAttribute(chip::app::Clusters::OnOff::Id,
                    chip::app::Clusters::OnOff::Attributes::OnOff::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id) {
      if (aId == OnOff::Attributes::OnOff::Id) {
        *val = esp_matter_bool(_attrs.onOff); return ESP_OK;
      }
      // FeatureMap: OnOff with the Lighting (LT) feature = bit0 → 1 / z feature Lighting (LT)
      if (aId == 0xFFFC) { *val = esp_matter_uint32(1); return ESP_OK; }
      // ClusterRevision: OnOff rev 4
      if (aId == 0xFFFD) { *val = esp_matter_uint16(4); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

  esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                         esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id && aId == OnOff::Attributes::OnOff::Id) {
      _attrs.onOff = val->val.b;
      _lastState   = _attrs.onOff;
      if (_callback) _callback(_slotIndex, _attrs.onOff);
      return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("OnOffLight", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("OnOffLight", "identify create failed"); return nullptr; }

    esp_matter::cluster::on_off::config_t oCfg = {};
    oCfg.on_off = false;
    // SDK 3.3.6: on_off for lighting requires the LT (Lighting) feature
    // SDK 3.3.6: on_off dla oświetlenia wymaga feature LT (Lighting)
    auto* oo = esp_matter::cluster::on_off::create(
      ep, &oCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
      esp_matter::cluster::on_off::feature::lighting::get_id());
    if (!oo) { ESP_LOGE("OnOffLight", "on_off create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – On/Off Light (0x0100)
    // POPRAWKA: ustaw właściwy DeviceType ID – On/Off Light (0x0100)
    esp_matter::endpoint::add_device_type(ep, 0x0100, 1);

    return ep;
  }
};

// ============================================================
//  MBOnOffPlug
//  Matter: On/Off Plug-in Unit (0x010A)
//  The hub shows a plug icon instead of a light bulb.
//  Hub wyświetla ikonę gniazdka zamiast żarówki.
//
//  FIX v2: A standalone class inheriting directly from MBDevice,
//  NOT from MBOnOffLight. The previous version inherited from MBOnOffLight,
//  which caused:
//    - MBOnOffLight's _buildEndpoint() created the OnOff cluster with the
//      Lighting feature (LT, FeatureMap bit0=1) and CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
//      even though _buildEndpoint() was overridden in MBOnOffPlug – in edge cases
//      the compiler could call the base version through the vtable.
//    - Inconsistency: onAttrRead() returned FeatureMap=0, but the Matter stack had
//      registered a cluster with FeatureMap=1 → corrupt heap when queried.
//    - The SmartThings hub saw the device as a dimmable light bulb.
//
//  Now the OnOff cluster is created WITHOUT the Lighting feature and WITHOUT
//  CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION – only CLUSTER_FLAG_SERVER.
//
//  POPRAWKA v2: Samodzielna klasa dziedzicząca bezpośrednio po MBDevice,
//  NIE po MBOnOffLight. Poprzednia wersja dziedziczyła po MBOnOffLight,
//  co powodowało:
//    - _buildEndpoint() z MBOnOffLight tworzył klaster OnOff z feature
//      Lighting (LT, bit0 FeatureMap=1) i CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
//      nawet mimo nadpisania _buildEndpoint() w MBOnOffPlug – kompilator mógł
//      wywołać wersję bazową przez vtable w przypadkach edge.
//    - Niespójność: onAttrRead() zwracał FeatureMap=0, ale stos Matter miał
//      zarejestrowany klaster z FeatureMap=1 → corrupt heap przy odpytywaniu.
//    - Hub SmartThings widział urządzenie jako żarówkę z dimmerem.
//
//  Teraz klaster OnOff jest tworzony BEZ feature Lighting i BEZ
//  CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION – wyłącznie CLUSTER_FLAG_SERVER.
// ============================================================
class MBOnOffPlug final : public MBDevice {
public:
  explicit MBOnOffPlug(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::OnOffPlug; }

  void applyState(bool state) override {
    _attrs.onOff = state;
    esp_matter_attr_val_t val = esp_matter_bool(state);
    reportAttribute(chip::app::Clusters::OnOff::Id,
                    chip::app::Clusters::OnOff::Attributes::OnOff::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id) {
      if (aId == OnOff::Attributes::OnOff::Id) {
        *val = esp_matter_bool(_attrs.onOff); return ESP_OK;
      }
      // OnOffPlug has NO Lighting feature → FeatureMap = 0 / NIE ma feature Lighting
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0); return ESP_OK; }
      // ClusterRevision: OnOff rev 4
      if (aId == 0xFFFD) { *val = esp_matter_uint16(4); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

  esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                         esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id && aId == OnOff::Attributes::OnOff::Id) {
      _attrs.onOff = val->val.b;
      _lastState   = _attrs.onOff;
      if (_callback) _callback(_slotIndex, _attrs.onOff);
      return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("OnOffPlug", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("OnOffPlug", "identify create failed"); return nullptr; }

    esp_matter::cluster::on_off::config_t oCfg = {};
    oCfg.on_off = false;
    // KEY: feature = 0 (no Lighting), CLUSTER_FLAG_SERVER only.
    // We do not use CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION – state changes
    // arrive through the global attribute callback in MBNode.
    // KLUCZOWE: feature = 0 (brak Lighting), tylko CLUSTER_FLAG_SERVER.
    // Nie używamy CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION – zmiany stanu
    // trafiają przez globalny callback atrybutów w MBNode.
    auto* oo = esp_matter::cluster::on_off::create(
      ep, &oCfg, CLUSTER_FLAG_SERVER, 0);
    if (!oo) { ESP_LOGE("OnOffPlug", "on_off create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // DeviceType ID: On/Off Plug-in Unit (0x010A)
    esp_matter::endpoint::add_device_type(ep, 0x010A, 1);

    return ep;
  }
};

// ============================================================
//  MBDimmableLight
//  Matter: Dimmable Light (0x0101)
//  Clusters / Klastry: OnOff + LevelControl
//  Usage / Użycie:
//    auto* d = static_cast<MBDimmableLight*>(MatterBridge.device(slot));
//    d->setLevel(128);   // 50% brightness / jasności
// ============================================================
class MBDimmableLight final : public MBDevice {
public:
  explicit MBDimmableLight(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::DimmableLight; }

  void applyState(bool state) override {
    _attrs.onOff = state;
    esp_matter_attr_val_t val = esp_matter_bool(state);
    reportAttribute(chip::app::Clusters::OnOff::Id,
                    chip::app::Clusters::OnOff::Attributes::OnOff::Id,
                    &val);
  }

  void setLevel(uint8_t level) {
    _attrs.level = level;
    bool newOnOff = (level > 0);
    if (_attrs.onOff != newOnOff) {
      _attrs.onOff = newOnOff;
      esp_matter_attr_val_t onOffVal = esp_matter_bool(newOnOff);
      reportAttribute(chip::app::Clusters::OnOff::Id,
                      chip::app::Clusters::OnOff::Attributes::OnOff::Id,
                      &onOffVal);
    }
    esp_matter_attr_val_t val = esp_matter_nullable_uint8(level);
    reportAttribute(chip::app::Clusters::LevelControl::Id,
                    chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id) {
      if (aId == OnOff::Attributes::OnOff::Id) {
        *val = esp_matter_bool(_attrs.onOff); return ESP_OK;
      }
      // OnOff with the Lighting feature → FeatureMap bit0 = 1 / z feature Lighting
      if (aId == 0xFFFC) { *val = esp_matter_uint32(1); return ESP_OK; }
      if (aId == 0xFFFD) { *val = esp_matter_uint16(4); return ESP_OK; }
    }
    if (cId == LevelControl::Id) {
      if (aId == LevelControl::Attributes::CurrentLevel::Id) {
        *val = esp_matter_nullable_uint8(_attrs.level); return ESP_OK;
      }
      if (aId == LevelControl::Attributes::MinLevel::Id) {
        *val = esp_matter_uint8(1); return ESP_OK;
      }
      if (aId == LevelControl::Attributes::MaxLevel::Id) {
        *val = esp_matter_uint8(254); return ESP_OK;
      }
      // LevelControl with the Lighting (LT) feature = bit2 → 4 / z feature Lighting (LT)
      if (aId == 0xFFFC) { *val = esp_matter_uint32(4); return ESP_OK; }
      if (aId == 0xFFFD) { *val = esp_matter_uint16(5); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

  esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                         esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id && aId == OnOff::Attributes::OnOff::Id) {
      _attrs.onOff = val->val.b;
      _lastState   = _attrs.onOff;
      if (_callback) _callback(_slotIndex, _attrs.onOff);
      return ESP_OK;
    }
    if (cId == LevelControl::Id &&
        aId == LevelControl::Attributes::CurrentLevel::Id) {
      _attrs.level = val->val.u8;
      _attrs.onOff = (_attrs.level > 0);
      if (_callback) _callback(_slotIndex, _attrs.onOff);
      return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("DimmLight", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("DimmLight", "identify create failed"); return nullptr; }

    esp_matter::cluster::on_off::config_t oCfg = {};
    auto* oo = esp_matter::cluster::on_off::create(
      ep, &oCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
      esp_matter::cluster::on_off::feature::lighting::get_id());
    if (!oo) { ESP_LOGE("DimmLight", "on_off create failed"); return nullptr; }

    esp_matter::cluster::level_control::config_t lCfg = {};
    lCfg.current_level         = 254;
    lCfg.lighting.min_level    = 1;
    lCfg.lighting.max_level    = 254;
    auto* lc = esp_matter::cluster::level_control::create(
      ep, &lCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
      esp_matter::cluster::level_control::feature::lighting::get_id());
    if (!lc) { ESP_LOGE("DimmLight", "level_control create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Dimmable Light (0x0101)
    // POPRAWKA: ustaw właściwy DeviceType ID – Dimmable Light (0x0101)
    esp_matter::endpoint::add_device_type(ep, 0x0101, 1);

    return ep;
  }
};

// ============================================================
//  MBColorTempLight
//  Matter: Color Temperature Light (0x010C)
//  Clusters / Klastry: OnOff + LevelControl + ColorControl (CT mode)
//  Usage / Użycie:
//    auto* c = static_cast<MBColorTempLight*>(MatterBridge.device(slot));
//    c->setColorTemp(370);   // ~2700K (warm white / ciepła biel)
// ============================================================
class MBColorTempLight final : public MBDevice {
public:
  explicit MBColorTempLight(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::ColorTempLight; }

  void applyState(bool state) override {
    _attrs.onOff = state;
    esp_matter_attr_val_t val = esp_matter_bool(state);
    reportAttribute(chip::app::Clusters::OnOff::Id,
                    chip::app::Clusters::OnOff::Attributes::OnOff::Id,
                    &val);
  }

  void setLevel(uint8_t level) {
    _attrs.level = level;
    bool newOnOff = (level > 0);
    if (_attrs.onOff != newOnOff) {
      _attrs.onOff = newOnOff;
      esp_matter_attr_val_t onOffVal = esp_matter_bool(newOnOff);
      reportAttribute(chip::app::Clusters::OnOff::Id,
                      chip::app::Clusters::OnOff::Attributes::OnOff::Id,
                      &onOffVal);
    }
    esp_matter_attr_val_t val = esp_matter_nullable_uint8(level);
    reportAttribute(chip::app::Clusters::LevelControl::Id,
                    chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id,
                    &val);
  }

  void setColorTemp(uint16_t mireds) {
    _attrs.colorTemp = mireds;
    esp_matter_attr_val_t val = esp_matter_uint16(mireds);
    reportAttribute(chip::app::Clusters::ColorControl::Id,
                    chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id) {
      if (aId == OnOff::Attributes::OnOff::Id) {
        *val = esp_matter_bool(_attrs.onOff); return ESP_OK;
      }
      if (aId == 0xFFFC) { *val = esp_matter_uint32(1); return ESP_OK; }  // LT feature
      if (aId == 0xFFFD) { *val = esp_matter_uint16(4); return ESP_OK; }
    }
    if (cId == LevelControl::Id) {
      if (aId == LevelControl::Attributes::CurrentLevel::Id) {
        *val = esp_matter_nullable_uint8(_attrs.level); return ESP_OK;
      }
      if (aId == 0xFFFC) { *val = esp_matter_uint32(4); return ESP_OK; }  // LT feature
      if (aId == 0xFFFD) { *val = esp_matter_uint16(5); return ESP_OK; }
    }
    if (cId == ColorControl::Id) {
      if (aId == ColorControl::Attributes::ColorTemperatureMireds::Id) {
        *val = esp_matter_uint16(_attrs.colorTemp); return ESP_OK;
      }
      if (aId == ColorControl::Attributes::ColorMode::Id) {
        *val = esp_matter_enum8(2); return ESP_OK;  // 2 = ColorTemperatureMireds
      }
      if (aId == ColorControl::Attributes::ColorTempPhysicalMinMireds::Id) {
        *val = esp_matter_uint16(153); return ESP_OK;  // ~6500K
      }
      if (aId == ColorControl::Attributes::ColorTempPhysicalMaxMireds::Id) {
        *val = esp_matter_uint16(500); return ESP_OK;  // ~2000K
      }
      // FeatureMap: ColorControl with the CT (ColorTemperature) feature = bit4 → 0x10 / z feature CT
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0x10); return ESP_OK; }
      // ClusterRevision: ColorControl rev 5
      if (aId == 0xFFFD) { *val = esp_matter_uint16(5); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

  esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                         esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == OnOff::Id && aId == OnOff::Attributes::OnOff::Id) {
      _attrs.onOff = val->val.b; _lastState = _attrs.onOff;
      if (_callback) _callback(_slotIndex, _attrs.onOff); return ESP_OK;
    }
    if (cId == LevelControl::Id &&
        aId == LevelControl::Attributes::CurrentLevel::Id) {
      _attrs.level = val->val.u8;
      if (_callback) _callback(_slotIndex, _attrs.onOff); return ESP_OK;
    }
    if (cId == ColorControl::Id &&
        aId == ColorControl::Attributes::ColorTemperatureMireds::Id) {
      _attrs.colorTemp = val->val.u16;
      if (_callback) _callback(_slotIndex, _attrs.onOff); return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("CTLight", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("CTLight", "identify create failed"); return nullptr; }

    esp_matter::cluster::on_off::config_t oCfg = {};
    auto* oo = esp_matter::cluster::on_off::create(
      ep, &oCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
      esp_matter::cluster::on_off::feature::lighting::get_id());
    if (!oo) { ESP_LOGE("CTLight", "on_off create failed"); return nullptr; }

    esp_matter::cluster::level_control::config_t lCfg = {};
    lCfg.current_level      = 254;
    lCfg.lighting.min_level = 1;
    auto* lc = esp_matter::cluster::level_control::create(
      ep, &lCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
      esp_matter::cluster::level_control::feature::lighting::get_id());
    if (!lc) { ESP_LOGE("CTLight", "level_control create failed"); return nullptr; }

    esp_matter::cluster::color_control::config_t cCfg = {};
    cCfg.color_temperature.color_temperature_mireds         = 370;
    cCfg.color_temperature.color_temp_physical_min_mireds   = 153;
    cCfg.color_temperature.color_temp_physical_max_mireds   = 500;
    auto* cc = esp_matter::cluster::color_control::create(
      ep, &cCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION,
      esp_matter::cluster::color_control::feature::color_temperature::get_id());
    if (!cc) { ESP_LOGE("CTLight", "color_control create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Color Temperature Light (0x010C)
    // POPRAWKA: ustaw właściwy DeviceType ID – Color Temperature Light (0x010C)
    esp_matter::endpoint::add_device_type(ep, 0x010C, 1);

    return ep;
  }
};

// ============================================================
//  MBFan
//  Matter: Fan (0x002B)
//  Cluster: FanControl – FanMode (0=off,1=low,2=med,3=high,4=on,5=auto)
//  Usage / Użycie:
//    auto* f = static_cast<MBFan*>(MatterBridge.device(slot));
//    f->setFanMode(2);   // medium
// ============================================================
class MBFan final : public MBDevice {
public:
  explicit MBFan(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::Fan; }

  void applyState(bool state) override {
    _attrs.fanMode = state ? 4 : 0;
    esp_matter_attr_val_t val = esp_matter_enum8(_attrs.fanMode);
    reportAttribute(chip::app::Clusters::FanControl::Id,
                    chip::app::Clusters::FanControl::Attributes::FanMode::Id,
                    &val);
  }

  void setFanMode(uint8_t mode) {
    _attrs.fanMode = mode;
    esp_matter_attr_val_t val = esp_matter_enum8(mode);
    reportAttribute(chip::app::Clusters::FanControl::Id,
                    chip::app::Clusters::FanControl::Attributes::FanMode::Id,
                    &val);
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == FanControl::Id) {
      if (aId == FanControl::Attributes::FanMode::Id) {
        *val = esp_matter_enum8(_attrs.fanMode); return ESP_OK;
      }
      if (aId == FanControl::Attributes::FanModeSequence::Id) {
        *val = esp_matter_enum8(2); return ESP_OK;  // 2 = OffLowMedHighAuto
      }
      // FeatureMap: FanControl without extra features → 0 / bez dodatkowych feature'ów
      if (aId == 0xFFFC) { *val = esp_matter_uint32(0); return ESP_OK; }
      // ClusterRevision: FanControl rev 2
      if (aId == 0xFFFD) { *val = esp_matter_uint16(2); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

  esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                         esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == FanControl::Id && aId == FanControl::Attributes::FanMode::Id) {
      _attrs.fanMode = val->val.u8;
      _lastState = (_attrs.fanMode != 0);
      if (_callback) _callback(_slotIndex, _lastState);
      return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("Fan", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("Fan", "identify create failed"); return nullptr; }

    esp_matter::cluster::fan_control::config_t fCfg = {};
    fCfg.fan_mode          = 0;
    fCfg.fan_mode_sequence = 2;
    auto* fc = esp_matter::cluster::fan_control::create(
      ep, &fCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION);
    if (!fc) { ESP_LOGE("Fan", "fan_control create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Fan (0x002B)
    // POPRAWKA: ustaw właściwy DeviceType ID – Fan (0x002B)
    esp_matter::endpoint::add_device_type(ep, 0x002B, 1);

    return ep;
  }
};

// ============================================================
//  MBWindowCovering
//  Matter: Window Covering (0x0202) – blind / shutter (roleta / żaluzja)
//  Cluster: WindowCovering – CurrentPositionLiftPercent100ths
//
//  NOTE: esp-matter SDK 3.3.6 does not provide set_command_callback
//  for the WindowCovering cluster. The stack translates the commands
//  (GoToLiftPercentage, UpOrOpen, DownOrClose) into a write of the
//  TargetPositionLiftPercent100ths attribute via an internal handler – so
//  onAttrWrite on this attribute WORKS correctly even though the hub sends a command.
//  No separate command callback is needed.
//  UWAGA: SDK esp-matter 3.3.6 nie udostępnia set_command_callback
//  dla klastra WindowCovering. Komendy (GoToLiftPercentage, UpOrOpen,
//  DownOrClose) stos tłumaczy na zapis atrybutu
//  TargetPositionLiftPercent100ths przez wewnętrzny handler – dlatego
//  onAttrWrite na tym atrybucie DZIAŁA poprawnie mimo że hub wysyła komendę.
//  Nie trzeba osobnego callbacku komend.
//
//  Usage / Użycie:
//    auto* r = static_cast<MBWindowCovering*>(MatterBridge.device(slot));
//    r->setPosition(50);   // 50% open / otwarta
// ============================================================
class MBWindowCovering final : public MBDevice {
public:
  explicit MBWindowCovering(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::WindowCovering; }

  void applyState(bool state) override {
    _attrs.coveringPos = state ? 0 : 100;
    _reportPosition();
  }

  void setPosition(uint8_t percent) {
    _attrs.coveringPos = percent > 100 ? 100 : percent;
    _reportPosition();
  }

  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == WindowCovering::Id) {
      uint16_t p100 = (uint16_t)_attrs.coveringPos * 100;
      if (aId == WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id) {
        *val = esp_matter_nullable_uint16(p100); return ESP_OK;
      }
      if (aId == WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id) {
        *val = esp_matter_nullable_uint16(p100); return ESP_OK;
      }
      if (aId == WindowCovering::Attributes::Type::Id) {
        *val = esp_matter_enum8(0); return ESP_OK;  // 0=Rollershade
      }
      if (aId == WindowCovering::Attributes::Mode::Id) {
        *val = esp_matter_bitmap8(0); return ESP_OK;
      }
      // FeatureMap: WindowCovering with the LF (Lift) feature = bit0 → 1 / z feature LF (Lift)
      if (aId == 0xFFFC) { *val = esp_matter_uint32(1); return ESP_OK; }
      // ClusterRevision: WindowCovering rev 5
      if (aId == 0xFFFD) { *val = esp_matter_uint16(5); return ESP_OK; }
    }
    return MBDevice::onAttrRead(cId, aId, val);
  }

  // SDK 3.3.6: the stack translates the GoToLiftPercentage/UpOrOpen/DownOrClose
  // commands into a write of TargetPositionLiftPercent100ths – this callback is invoked
  // SDK 3.3.6: stos tłumaczy komendy GoToLiftPercentage/UpOrOpen/DownOrClose
  // na zapis TargetPositionLiftPercent100ths – ten callback jest wywoływany
  esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                         esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;
    if (cId == WindowCovering::Id &&
        aId == WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id) {
      _attrs.coveringPos = (uint8_t)(val->val.u16 / 100);
      _lastState = (_attrs.coveringPos < 50);
      _reportPosition();
      if (_callback) _callback(_slotIndex, _lastState);
      return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("Covering", "bridged_node create failed"); return nullptr; }

    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("Covering", "identify create failed"); return nullptr; }

    esp_matter::cluster::window_covering::config_t wCfg = {};
    wCfg.type = 0;  // Rollershade
    auto* wc = esp_matter::cluster::window_covering::create(
      ep, &wCfg, CLUSTER_FLAG_SERVER | CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION, 0);
    if (!wc) { ESP_LOGE("Covering", "window_covering create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // FIX: set the correct DeviceType ID – Window Covering (0x0202)
    // POPRAWKA: ustaw właściwy DeviceType ID – Window Covering (0x0202)
    esp_matter::endpoint::add_device_type(ep, 0x0202, 1);

    return ep;
  }

private:
  void _reportPosition() {
    uint16_t p100 = (uint16_t)_attrs.coveringPos * 100;
    esp_matter_attr_val_t cur = esp_matter_nullable_uint16(p100);
    // Report Current – visible to the hub as the current state
    // Raportuj Current – widoczne dla huba jako aktualny stan
    reportAttribute(
      chip::app::Clusters::WindowCovering::Id,
      chip::app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id,
      &cur);
    // Report Target = Current – the hub knows the movement has finished (Current==Target)
    // Raportuj Target = Current – hub wie że ruch się zakończył (Current==Target)
    reportAttribute(
      chip::app::Clusters::WindowCovering::Id,
      chip::app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id,
      &cur);
  }
};

// ============================================================
//  MBElectricalPlug
//
//  Matter: On/Off Plug-in Unit (0x010A) with electrical energy measurement / z pomiarem energii elektrycznej.
//
//  ════════════════════════════════════════════════════════════
//  BUG HISTORY – read before changing anything / HISTORIA BŁĘDÓW – czytaj zanim cokolwiek zmienisz
//  ════════════════════════════════════════════════════════════
//
//  BUG #1 – Wrong current attribute (fixed in the 2025-05-12 session)
//    Symptom: compile error
//      'chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Current'
//      has not been declared
//    Cause: the AC current attribute in the EPM cluster (0x0090) is called
//      ActiveCurrent, not Current.
//    Fix: changed to Attributes::ActiveCurrent::Id.
//
//  BŁĄD #1 – Zły atrybut prądu (naprawiony w sesji 2025-05-12)
//    Symptom: błąd kompilacji
//      'chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Current'
//      has not been declared
//    Przyczyna: atrybut prądu AC w klastrze EPM (0x0090) nazywa się
//      ActiveCurrent, nie Current.
//    Naprawa: zmiana na Attributes::ActiveCurrent::Id.
//
//  BUG #2 – Missing 4th argument of create() for the EPM/EEM clusters (fixed)
//    Symptom: compile error "too few arguments"
//    Cause: in esp-matter 3.3.8 the Matter 1.3 clusters (EPM/EEM) require an
//      explicit features bitmap as the 4th argument of create().
//      Without it the optional attributes (Voltage, ActiveCurrent, ActivePower, etc.)
//      are not registered in the Matter stack.
//    Fix:
//      EPM: features = 1        (bit 0 = AlternatingCurrent / AC)
//      EEM: features = 0x05     (bit 0 = ImportedEnergy, bit 2 = CumulativeEnergy)
//
//  BŁĄD #2 – Brakujący 4. argument create() klastrów EPM/EEM (naprawiony)
//    Symptom: błąd kompilacji "too few arguments"
//    Przyczyna: w esp-matter 3.3.8 klastry Matter 1.3 (EPM/EEM) wymagają
//      jawnej mapy bitowej features jako 4. argumentu create().
//      Bez niej opcjonalne atrybuty (Voltage, ActiveCurrent, ActivePower itp.) nie są
//      rejestrowane w stosie Matter.
//    Naprawa:
//      EPM: features = 1        (bit 0 = AlternatingCurrent / AC)
//      EEM: features = 0x05     (bit 0 = ImportedEnergy, bit 2 = CumulativeEnergy)
//
//  BUG #3 – Missing onAttrRead() (fixed)
//    Symptom: the plug is visible in the hub, but all measurements are empty / missing
//    Cause: the base class MBDevice::onAttrRead() handles only the clusters
//      BooleanState, OccupancySensing, BridgedDeviceBasicInformation.
//      Hub queries for EPM / EEM returned ESP_ERR_NOT_FOUND → the hub showed nothing.
//    Fix: override onAttrRead() in this class, handling EPM and EEM.
//
//  BŁĄD #3 – Brak onAttrRead() (naprawiony)
//    Symptom: gniazdko widoczne w hubie, ale wszystkie pomiary puste / brak
//    Przyczyna: klasa bazowa MBDevice::onAttrRead() obsługuje tylko klastry
//      BooleanState, OccupancySensing, BridgedDeviceBasicInformation.
//      Zapytania huba o EPM / EEM zwracały ESP_ERR_NOT_FOUND → hub nic nie pokazywał.
//    Naprawa: override onAttrRead() w tej klasie obsługujący EPM i EEM.
//
//  BUG #4 – No explicit attribute::create() for Voltage/ActiveCurrent/ActivePower/
//            CumulativeEnergyImported (fixed)
//    Symptom: empty measurements despite a correct onAttrRead() and feature flags
//    Cause: esp-matter registers the cluster via create(), but optional
//      attributes exist in the stack ONLY if they are explicitly created with
//      attribute::create(). Without it attribute::update() in the setters returns
//      0x586 UNSUPPORTED_ATTRIBUTE and updates nothing.
//      The same problem occurred earlier with UniqueID and SoftwareVersion
//      in _addBridgedBasicInfo() – analogous solution.
//    Fix: explicit attribute::create() for each measurement attribute
//      right after the cluster's create().
//
//  BŁĄD #4 – Brak jawnego attribute::create() dla Voltage/ActiveCurrent/ActivePower/
//            CumulativeEnergyImported (naprawiony)
//    Symptom: pomiary puste mimo poprawnego onAttrRead() i feature flags
//    Przyczyna: esp-matter rejestruje klaster przez create(), ale opcjonalne
//      atrybuty istnieją w stosie TYLKO jeśli są jawnie stworzone przez
//      attribute::create(). Bez tego attribute::update() w setterach zwraca
//      0x586 UNSUPPORTED_ATTRIBUTE i nic nie aktualizuje.
//      Identyczny problem był wcześniej z UniqueID i SoftwareVersion
//      w _addBridgedBasicInfo() – analogiczne rozwiązanie.
//    Naprawa: jawne attribute::create() dla każdego atrybutu pomiarowego
//      bezpośrednio po create() klastra.
//
//  BUG #5 – Inconsistent value type: esp_matter_int32 vs esp_matter_nullable_int32
//    Symptom: reportAttribute() silently returns an error; measurements do not update
//    Cause: the EPM attributes (Voltage, ActiveCurrent, ActivePower) are
//      NULLABLE per the Matter 1.4 spec (nullable int64, units mV/mA/mW).
//      The setter used esp_matter_int32 (non-nullable), onAttrRead returned
//      esp_matter_nullable_int32. The Matter stack checks type compatibility in
//      attribute::update() – a mismatch = rejection without a log.
//    Fix: esp_matter_nullable_int64 everywhere (the spec says int64, not int32).
//      Scale: Voltage in mV, ActiveCurrent in mA, ActivePower in mW.
//
//  BŁĄD #5 – Niespójny typ wartości: esp_matter_int32 vs esp_matter_nullable_int32
//    Symptom: reportAttribute() cicho zwraca błąd; pomiary nie aktualizują się
//    Przyczyna: atrybuty EPM (Voltage, ActiveCurrent, ActivePower) są
//      NULLABLE wg spec Matter 1.4 (typ int64 nullable, jednostki mV/mA/mW).
//      Setter używał esp_matter_int32 (non-nullable), onAttrRead zwracał
//      esp_matter_nullable_int32. Stos Matter sprawdza zgodność typów przy
//      attribute::update() – niezgodność = odrzucenie bez logu.
//    Naprawa: wszędzie esp_matter_nullable_int64 (spec mówi int64, nie int32).
//      Skala: Voltage w mV, ActiveCurrent w mA, ActivePower w mW.
//
//  BUG #6 – CumulativeEnergyImported as a bare int64 instead of a struct
//    Symptom: energy counter always 0 or missing in the hub
//    Cause: per the Matter 1.4 spec (doc chapter 5.1) CumulativeEnergyImported
//      is EnergyMeasurementStruct { Energy: int64 [mWh], Timestamp: uint32 [UTC] }.
//      esp-matter stores it as nullable int64 (SDK simplification) –
//      reporting a bare esp_matter_int64 is OK for esp-matter,
//      but the type must be CONSISTENT across attribute::create(), onAttrRead()
//      and reportAttribute(). The earlier code mixed int64 and nullable_int64.
//    Fix: esp_matter_nullable_int64(mWh) everywhere. The timestamp is omitted
//      (esp-matter has no native EnergyMeasurementStruct support in SDK 3.3.x).
//
//  BŁĄD #6 – CumulativeEnergyImported jako gołe int64 zamiast struct
//    Symptom: licznik energii zawsze 0 lub brak w hubie
//    Przyczyna: wg Matter 1.4 spec (rozdz. 5.1 doc) CumulativeEnergyImported
//      to EnergyMeasurementStruct { Energy: int64 [mWh], Timestamp: uint32 [UTC] }.
//      esp-matter przechowuje go jako nullable int64 (uproszczenie SDK) –
//      raportowanie gołym esp_matter_int64 jest OK dla esp-matter,
//      ale typ musi być SPÓJNY między attribute::create(), onAttrRead()
//      i reportAttribute(). Poprzedni kod mieszał int64 i nullable_int64.
//    Naprawa: wszędzie esp_matter_nullable_int64(mWh). Timestamp pomijamy
//      (esp-matter nie ma natywnego wsparcia EnergyMeasurementStruct w SDK 3.3.x).
//
//  BUG #7 – Wrong cluster IDs in comments (documentation only, not code)
//    EPM was marked in comments as 0x0091 → the correct ID is 0x0090
//    EEM was marked in comments as 0x0092 → the correct ID is 0x0091
//    The code uses C++ symbols (ElectricalPowerMeasurement::Id), so it compiles
//    correctly, but the comments are misleading when debugging logs.
//
//  BŁĄD #7 – Błędne ID klastrów w komentarzach (tylko dokumentacja, nie kod)
//    EPM w komentarzach oznaczone jako 0x0091 → prawidłowe ID to 0x0090
//    EEM w komentarzach oznaczone jako 0x0092 → prawidłowe ID to 0x0091
//    Kod używa symboli C++ (ElectricalPowerMeasurement::Id) więc kompiluje się
//    poprawnie, ale komentarze wprowadzają w błąd przy debugowaniu logów.
//
//  BUG #8 – Missing PowerTopology cluster (0x009C)
//    Symptom: some hubs (Google Home, Apple Home) may not show measurements
//    Cause: Matter spec 1.3+ requires the PowerTopology cluster on an
//      ElectricalSensor (0x0510) endpoint. It contains the AvailableEndpoints
//      and ActiveEndpoints attributes pointing to the measured endpoints.
//    Fix (2025-05-12): added the PowerTopology cluster created via
//      esp_matter::cluster::create() with the raw ID 0x009C.
//      esp_matter_array is used with a (uint8_t*) cast to pass
//      the endpoint list as uint16_t.
//
//  BŁĄD #8 – Brak klastra PowerTopology (0x009C)
//    Symptom: część hubów (Google Home, Apple Home) może nie wyświetlać pomiarów
//    Przyczyna: spec Matter 1.3+ wymaga klastra PowerTopology na endpoincie
//      typu ElectricalSensor (0x0510). Zawiera atrybuty AvailableEndpoints
//      i ActiveEndpoints wskazujące które endpointy są mierzone.
//    Naprawa (2025-05-12): dodano klaster PowerTopology tworzony przez
//      esp_matter::cluster::create() z raw ID 0x009C.
//      Użyto esp_matter_array z rzutowaniem (uint8_t*), aby przekazać
//      listę endpointów jako uint16_t.
//
//  BUG #9 – Compilation: on_off::config_t initialisation (fixed 2025-05-12)
//    Symptom: could not convert '{false}' ... to config_t
//    Cause: the form `config_t ooCfg = { false };` is invalid.
//      The config_t struct has an `on_off` field and is not an aggregate.
//    Fix: restored the original form:
//      config_t ooCfg = {};
//      ooCfg.on_off = false;
//
//  BŁĄD #9 – Kompilacja: inicjalizacja on_off::config_t (naprawiony 2025-05-12)
//    Symptom: could not convert '{false}' ... to config_t
//    Przyczyna: zapis `config_t ooCfg = { false };` jest niepoprawny.
//      Struktura config_t ma pole `on_off`, nie jest agregatem.
//    Naprawa: przywrócono pierwotny zapis:
//      config_t ooCfg = {};
//      ooCfg.on_off = false;
//
//  BUG #10 – Compilation: `Attributes` ambiguity (fixed 2025-05-12)
//    Symptom: reference to 'Attributes' is ambiguous
//    Cause: using `using namespace chip::app::Clusters::ElectricalEnergyMeasurement;`
//      inside _buildEndpoint() makes both EPM::Attributes and
//      EEM::Attributes visible, which causes a conflict.
//    Fix: removed all `using namespace` from _buildEndpoint().
//      All attributes are now fully qualified (e.g.
//      chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Voltage::Id).
//      In onAttrRead() `using namespace chip::app::Clusters;` is safe,
//      because EPM::Attributes and EEM::Attributes are distinguished there by the full path.
//
//  BŁĄD #10 – Kompilacja: niejednoznaczność `Attributes` (naprawiony 2025-05-12)
//    Symptom: reference to 'Attributes' is ambiguous
//    Przyczyna: użycie `using namespace chip::app::Clusters::ElectricalEnergyMeasurement;`
//      wewnątrz _buildEndpoint() powoduje, że zarówno EPM::Attributes jak i
//      EEM::Attributes są widoczne, co wywołuje konflikt.
//    Naprawa: usunięto wszystkie `using namespace` z _buildEndpoint().
//      Wszystkie atrybuty są teraz w pełni kwalifikowane (np.
//      chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Voltage::Id).
//      W onAttrRead() `using namespace chip::app::Clusters;` jest bezpieczne,
//      bo EPM::Attributes i EEM::Attributes są tam rozróżniane przez pełną ścieżkę.
//
//  BUG #11 – Feedback loop in onAttrWrite (fixed 2026-05-29)
//    Symptom: the callback (_callback) is not invoked, or the stack deadlocks
//    Cause: calling reportAttribute() inside onAttrWrite() notifies the
//      Matter stack again about the attribute change, which can lead
//      to recursion or to the stack rejecting the whole write callback.
//    Fix: removed the redundant reportAttribute() – the hub already knows the new
//      value because it set it itself. reportAttribute is only needed for changes
//      that originate from the device (e.g. a physical button).
//
//  BŁĄD #11 – Pętla zwrotna w onAttrWrite (naprawiony 2026-05-29)
//    Symptom: brak wywołania callbacka (_callback) lub zakleszczenie stosu
//    Przyczyna: wywołanie reportAttribute() wewnątrz onAttrWrite() powoduje
//      ponowne powiadomienie stosu Matter o zmianie atrybutu, co może prowadzić
//      do rekurencji lub odrzucenia całego write callbacka przez stos.
//    Naprawa: usunięto zbędne reportAttribute() – hub już zna nową wartość,
//      bo sam ją ustawił. reportAttribute jest potrzebne tylko przy zmianach
//      pochodzących z urządzenia (np. przycisk fizyczny).
//
//  ════════════════════════════════════════════════════════════
//  ARCHITECTURE – important note for future changes / ARCHITEKTURA – ważna uwaga do przyszłych zmian
//  ════════════════════════════════════════════════════════════
//
//  The Matter 1.4 spec (section 2 of the document) recommends two separate endpoints:
//    EP1: On/Off Plug-in Unit (0x010A) – OnOff only
//    EP2: Electrical Sensor  (0x0510) – PowerTopology + EPM + EEM
//  The current implementation combines both in ONE bridged_node endpoint.
//  It works on SmartThings (tested). If other hubs have problems,
//  it has to be split into two endpoints – this requires refactoring
//  MBElectricalPlug into two classes (or one object managing two endpoints).
//
//  Spec Matter 1.4 (sekcja 2 dokumentu) zaleca dwa osobne endpointy:
//    EP1: On/Off Plug-in Unit (0x010A) – tylko OnOff
//    EP2: Electrical Sensor  (0x0510) – PowerTopology + EPM + EEM
//  Obecna implementacja łączy oba w JEDNYM endpoincie bridged_node.
//  Działa na SmartThings (testowane). Jeśli inne huby będą miały problem,
//  trzeba rozbić na dwa endpointy – to wymaga refaktoru MBElectricalPlug
//  na dwie klasy (lub jeden obiekt zarządzający dwoma endpointami).
//
//  ════════════════════════════════════════════════════════════
//  UNITS / JEDNOSTKI (Matter 1.4 Application Cluster Spec)
//  ════════════════════════════════════════════════════════════
//  Voltage:      mV  (int64 nullable)  e.g./np. 230000 = 230.000 V
//  ActiveCurrent:mA  (int64 nullable)  e.g./np. 500    = 0.500 A
//  ActivePower:  mW  (int64 nullable)  e.g./np. 115000 = 115.0 W
//  Energy:       mWh (int64 nullable)  e.g./np. 1000000= 1.000 kWh
//
//  MBAttrStore stores / przechowuje:
//    voltage     uint16  [0.01 V]    → ×10    → mV
//    current     uint16  [0.001 A]   → ×1     → mA (current is already in 0.001 A steps = mA / current już jest w mA ×0.001 co jest mA)
//    activePower int16   [0.1 W]     → ×100   → mW
//    energy      uint32  [Wh]        → ×1000  → mWh
// ============================================================
// ============================================================
//  MBElectricalPlug
//
//  Matter: On/Off Plug-in Unit (0x010A) with electrical energy measurement / z pomiarem energii elektrycznej.
//
//  ════════════════════════════════════════════════════════════
//  BUG HISTORY – read before changing anything / HISTORIA BŁĘDÓW – czytaj zanim cokolwiek zmienisz
//  ...
//  (the full bug history was kept from your code / pełną historię błędów zachowałem z Twojego kodu)
//  ...
//  ════════════════════════════════════════════════════════════
//  BUG #12 – Duplicate CumulativeEnergyImported attribute (fixed 2026-05-29)
//    Symptom: runtime error "Insufficient space for reading Endpoint
//      0x0002's Cluster 0x00000091's Attribute 0x00000001: required: 4, max: 2"
//    Cause: the ElectricalEnergyMeasurement cluster (0x0091) with the ImportedEnergy
//      feature enabled creates attribute 0x0001 by itself. A manual attribute::create()
//      caused a metadata conflict (2-byte buffer vs a callback returning 9 bytes).
//    Fix: removed the redundant attribute::create() for CumulativeEnergyImported.
//      The attribute is created automatically with the nullable_int64 type.
//
//  BŁĄD #12 – Duplikacja atrybutu CumulativeEnergyImported (naprawiony 2026-05-29)
//    Symptom: błąd uruchomieniowy "Insufficient space for reading Endpoint
//      0x0002's Cluster 0x00000091's Attribute 0x00000001: required: 4, max: 2"
//    Przyczyna: klaster ElectricalEnergyMeasurement (0x0091) przy włączonym
//      ficzerze ImportedEnergy sam tworzy atrybut 0x0001. Ręczne attribute::create()
//      powodowało konflikt metadanych (bufor na 2 bajty vs callback zwracający 9 bajtów).
//    Naprawa: usunięto zbędne attribute::create() dla CumulativeEnergyImported.
//      Atrybut jest poprawnie tworzony automatycznie z typem nullable_int64.
//
//  ════════════════════════════════════════════════════════════
//  UNITS / JEDNOSTKI (Matter 1.4 Application Cluster Spec)
//  ════════════════════════════════════════════════════════════
//  Voltage:      mV  (int64 nullable)  e.g./np. 230000 = 230.000 V
//  ActiveCurrent:mA  (int64 nullable)  e.g./np. 500    = 0.500 A
//  ActivePower:  mW  (int64 nullable)  e.g./np. 115000 = 115.0 W
//  Energy:       mWh (int64 nullable)  e.g./np. 1000000= 1.000 kWh
//
//  MBAttrStore stores / przechowuje:
//    voltage     uint16  [0.01 V]    → ×10    → mV
//    current     uint16  [0.001 A]   → ×1     → mA
//    activePower int16   [0.1 W]     → ×100   → mW
//    energy      uint32  [Wh]        → ×1000  → mWh
// ============================================================
class MBElectricalPlug final : public MBDevice {
public:
    explicit MBElectricalPlug(const MBDescriptor& d) : MBDevice(d) {}
    MBDeviceType deviceType() const override { return MBDeviceType::ElectricalSensor; }

    void applyState(bool state) override {
        _attrs.onOff = state;
        esp_matter_attr_val_t val = esp_matter_bool(state);
        reportAttribute(chip::app::Clusters::OnOff::Id,
                        chip::app::Clusters::OnOff::Attributes::OnOff::Id, &val);
    }

    // ── Setters – conversion to Matter base units / Settery – konwersja na jednostki bazowe Matter ──────────────────────
    void setVoltage(uint16_t voltage_0_01V) {
        _attrs.voltage = voltage_0_01V;
        esp_matter_attr_val_t val = esp_matter_nullable_int64((int64_t)voltage_0_01V * 10);
        reportAttribute(chip::app::Clusters::ElectricalPowerMeasurement::Id,
                        chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Voltage::Id, &val);
    }

    void setCurrent(uint16_t current_mA) {
        _attrs.current = current_mA;
        esp_matter_attr_val_t val = esp_matter_nullable_int64((int64_t)current_mA);
        reportAttribute(chip::app::Clusters::ElectricalPowerMeasurement::Id,
                        chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActiveCurrent::Id, &val);
    }

    void setActivePower(int16_t power_0_1W) {
        _attrs.activePower = power_0_1W;
        esp_matter_attr_val_t val = esp_matter_nullable_int64((int64_t)power_0_1W * 100);
        reportAttribute(chip::app::Clusters::ElectricalPowerMeasurement::Id,
                        chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActivePower::Id, &val);
    }

    void setEnergy(uint32_t energy_Wh) {
        _attrs.energy = energy_Wh;
        esp_matter_attr_val_t val = esp_matter_nullable_int64((int64_t)energy_Wh * 1000);
        reportAttribute(chip::app::Clusters::ElectricalEnergyMeasurement::Id,
                        chip::app::Clusters::ElectricalEnergyMeasurement::Attributes::CumulativeEnergyImported::Id, &val);
    }

    // ── onAttrWrite ──────────────────────────────────────────────────────────
    esp_err_t onAttrWrite(uint32_t cId, uint32_t aId,
                           esp_matter_attr_val_t* val) override {
        using namespace chip::app::Clusters;
        if (cId == OnOff::Id && aId == OnOff::Attributes::OnOff::Id) {
            _attrs.onOff = val->val.b;
            _lastState   = _attrs.onOff;
            if (_callback) _callback(_slotIndex, _lastState);
            return ESP_OK;
        }
        return ESP_ERR_NOT_SUPPORTED;
    }

    // ── onAttrRead ───────────────────────────────────────────────────────────
    esp_err_t onAttrRead(uint32_t clusterId, uint32_t attrId,
                         esp_matter_attr_val_t* val) override {
        using namespace chip::app::Clusters;

        // ElectricalPowerMeasurement (0x0090)
        if (clusterId == ElectricalPowerMeasurement::Id) {
            if (attrId == 0x0000) { *val = esp_matter_enum8(1); return ESP_OK; }
            if (attrId == ElectricalPowerMeasurement::Attributes::Voltage::Id) {
                *val = esp_matter_nullable_int64((int64_t)_attrs.voltage * 10);
                return ESP_OK;
            }
            if (attrId == ElectricalPowerMeasurement::Attributes::ActiveCurrent::Id) {
                *val = esp_matter_nullable_int64((int64_t)_attrs.current);
                return ESP_OK;
            }
            if (attrId == ElectricalPowerMeasurement::Attributes::ActivePower::Id) {
                *val = esp_matter_nullable_int64((int64_t)_attrs.activePower * 100);
                return ESP_OK;
            }
            if (attrId == 0xFFFC) { *val = esp_matter_uint32(1); return ESP_OK; }
            if (attrId == 0xFFFD) { *val = esp_matter_uint16(1); return ESP_OK; }
        }

        // ElectricalEnergyMeasurement (0x0091)
        if (clusterId == ElectricalEnergyMeasurement::Id) {
            if (attrId == ElectricalEnergyMeasurement::Attributes::CumulativeEnergyImported::Id) {
                *val = esp_matter_nullable_int64((int64_t)_attrs.energy * 1000);
                return ESP_OK;
            }
            if (attrId == 0xFFFC) { *val = esp_matter_uint32(0x05); return ESP_OK; }
            if (attrId == 0xFFFD) { *val = esp_matter_uint16(1);    return ESP_OK; }
        }

        // OnOff (0x0006)
        if (clusterId == OnOff::Id && attrId == OnOff::Attributes::OnOff::Id) {
            *val = esp_matter_bool(_attrs.onOff);
            return ESP_OK;
        }

        // PowerTopology (0x009C)
        if (clusterId == 0x009C) {
            if (attrId == 0x0000) {
                static uint16_t available[1] = {0};
                available[0] = _endpoint ? esp_matter::endpoint::get_id(_endpoint) : 2;
                *val = esp_matter_array((uint8_t*)available, sizeof(uint16_t), 1);
                return ESP_OK;
            }
            if (attrId == 0x0001) {
                static uint16_t active[1] = {0};
                active[0] = _endpoint ? esp_matter::endpoint::get_id(_endpoint) : 2;
                *val = esp_matter_array((uint8_t*)active, sizeof(uint16_t), 1);
                return ESP_OK;
            }
        }

        return MBDevice::onAttrRead(clusterId, attrId, val);
    }

protected:
    esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
        esp_matter::endpoint::bridged_node::config_t cfg = {};
        esp_matter::endpoint_t* ep =
            esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
        if (!ep) { ESP_LOGE("ElecPlug", "bridged_node create failed"); return nullptr; }
        _endpoint = ep;

        // Identify
        esp_matter::cluster::identify::config_t iCfg = {};
        auto* ic = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
        if (!ic) { ESP_LOGE("ElecPlug", "identify create failed"); return nullptr; }

        // OnOff
        esp_matter::cluster::on_off::config_t ooCfg = {};
        ooCfg.on_off = false;
        auto* oc = esp_matter::cluster::on_off::create(ep, &ooCfg, CLUSTER_FLAG_SERVER, 0);
        if (!oc) { ESP_LOGE("ElecPlug", "on_off create failed"); return nullptr; }

        // ElectricalPowerMeasurement (0x0090)
        esp_matter::cluster::electrical_power_measurement::config_t epmCfg = {};
        auto* epm = esp_matter::cluster::electrical_power_measurement::create(
            ep, &epmCfg, CLUSTER_FLAG_SERVER, 1);
        if (!epm) { ESP_LOGE("ElecPlug", "elec_power_meas create failed"); return nullptr; }

        {
            esp_matter_attr_val_t pmVal  = esp_matter_enum8(1);
            esp_matter_attr_val_t nullI64 = esp_matter_nullable_int64(0);
            esp_matter::attribute::create(epm, 0x0000, 0x01, pmVal);
            esp_matter::attribute::create(epm,
                chip::app::Clusters::ElectricalPowerMeasurement::Attributes::Voltage::Id,
                0x01, nullI64);
            esp_matter::attribute::create(epm,
                chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActiveCurrent::Id,
                0x01, nullI64);
            esp_matter::attribute::create(epm,
                chip::app::Clusters::ElectricalPowerMeasurement::Attributes::ActivePower::Id,
                0x01, nullI64);
        }

        // ElectricalEnergyMeasurement (0x0091)
        // ⚠️ IMPORTANT: CumulativeEnergyImported is created automatically
        // by the cluster. It must not be created a second time – remove the block
        // that existed earlier!
        // ⚠️ WAŻNE: CumulativeEnergyImported jest tworzony automatycznie
        // przez klaster. Nie wolno go tworzyć drugi raz – usuń ten blok,
        // który był wcześniej!
        esp_matter::cluster::electrical_energy_measurement::config_t eemCfg = {};
        auto* eem = esp_matter::cluster::electrical_energy_measurement::create(
            ep, &eemCfg, CLUSTER_FLAG_SERVER, 0x05);
        if (!eem) { ESP_LOGE("ElecPlug", "elec_energy_meas create failed"); return nullptr; }

        // PowerTopology (0x009C)
        {
            esp_matter::cluster_t* pt = esp_matter::cluster::create(ep, 0x009C, CLUSTER_FLAG_SERVER);
            if (pt) {
                uint16_t epId = esp_matter::endpoint::get_id(ep);
                esp_matter_attr_val_t availVal = esp_matter_array((uint8_t*)&epId, sizeof(uint16_t), 1);
                esp_matter::attribute::create(pt, 0x0000, 0x01, availVal);
                esp_matter_attr_val_t activeVal = esp_matter_array((uint8_t*)&epId, sizeof(uint16_t), 1);
                esp_matter::attribute::create(pt, 0x0001, 0x01, activeVal);
            } else {
                ESP_LOGW("ElecPlug", "PowerTopology create failed");
            }
        }

        // BridgedDeviceBasicInformation
        if (!_addBridgedBasicInfo(ep)) return nullptr;

        // Device types
        esp_matter::endpoint::add_device_type(ep, 0x010A, 1); // On/Off Plug-in Unit
        esp_matter::endpoint::add_device_type(ep, 0x0510, 1); // Electrical Sensor
        return ep;
    }

private:
    esp_matter::endpoint_t* _endpoint = nullptr;
};


// ============================================================
//  MBTempHumidSensor
//  Matter: Temperature & Humidity Sensor – one endpoint,
//          two measurement clusters.
//
//  DeviceType:
//    0x0302  Temperature Sensor  – required, ST recognises the sensor
//    0x0307  Humidity Sensor     – added alongside, ST sees one
//                                  device with two attributes
//
//  Clusters:
//    TemperatureMeasurement   (0x0402)  – MeasuredValue int16 ×100
//    RelativeHumidityMeasurement (0x0405) – MeasuredValue uint16 ×100
//
//  Scaling:
//    setTemperature(21.5f)  → _attrs.temperature = 2150  → ST: 21.5 °C
//    setHumidity(55.3f)     → _attrs.humidity    = 5530  → ST: 55.3 %
//
//  Matter: Temperature & Humidity Sensor – jeden endpoint,
//          dwa klastry pomiarowe.
//
//  DeviceType:
//    0x0302  Temperature Sensor  – wymagany, ST rozpoznaje czujnik
//    0x0307  Humidity Sensor     – dodany obok, ST widzi jedno
//                                  urządzenie z dwoma atrybutami
//
//  Clusters / Klastry:
//    TemperatureMeasurement   (0x0402)  – MeasuredValue int16 ×100
//    RelativeHumidityMeasurement (0x0405) – MeasuredValue uint16 ×100
//
//  Skalowanie:
//    setTemperature(21.5f)  → _attrs.temperature = 2150  → ST: 21.5 °C
//    setHumidity(55.3f)     → _attrs.humidity    = 5530  → ST: 55.3 %
//
//  Usage (from an external data source, e.g. UART/MQTT) / Użycie (z zewnętrznego źródła danych, np. UART/MQTT):
//    auto* th = static_cast<MBTempHumidSensor*>(MatterBridge.device(slot));
//    th->setTemperature(21.5f);
//    th->setHumidity(55.3f);
//
//  NOTE: MBDeviceType::TempHumidSensor must be added to the
//        MBDeviceType enum in MBDevice.h before using this class.
//  UWAGA: MBDeviceType::TempHumidSensor musi być dodany do enum
//         MBDeviceType w MBDevice.h przed użyciem tej klasy.
// ============================================================
class MBTempHumidSensor final : public MBDevice {
public:
  explicit MBTempHumidSensor(const MBDescriptor& d) : MBDevice(d) {}
  MBDeviceType deviceType() const override { return MBDeviceType::TempHumidSensor; }

  // applyState() – passive sensor, no boolean state / czujnik pasywny, brak stanu boolowskiego.
  void applyState(bool /*unused*/) override {}

  // ── Setters – called from an external data source / Settery – wywoływane z zewnętrznego źródła danych ──────────

  void setTemperature(float celsius) {
    _attrs.temperature = (int16_t)(celsius * 100.0f);
    ESP_LOGD("TempHumid", "setTemp: %.2f°C → raw=%" PRId16 " epId=%" PRIu16,
             celsius, _attrs.temperature, _endpointId);
    esp_matter_attr_val_t val = esp_matter_int16(_attrs.temperature);
    esp_err_t err = reportAttribute(
      chip::app::Clusters::TemperatureMeasurement::Id,
      chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Id,
      &val);
    if (err != ESP_OK) {
      ESP_LOGE("TempHumid", "setTemperature reportAttribute FAILED: 0x%x", err);
    }
  }

  void setHumidity(float percent) {
    _attrs.humidity = (uint16_t)(percent * 100.0f);
    ESP_LOGD("TempHumid", "setHumid: %.2f%% → raw=%" PRIu16 " epId=%" PRIu16,
             percent, _attrs.humidity, _endpointId);
    esp_matter_attr_val_t val = esp_matter_uint16(_attrs.humidity);
    esp_err_t err = reportAttribute(
      chip::app::Clusters::RelativeHumidityMeasurement::Id,
      chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Id,
      &val);
    if (err != ESP_OK) {
      ESP_LOGE("TempHumid", "setHumidity reportAttribute FAILED: 0x%x", err);
    }
  }

  // ── onAttrRead ────────────────────────────────────────────────────────────
  esp_err_t onAttrRead(uint32_t cId, uint32_t aId,
                        esp_matter_attr_val_t* val) override {
    using namespace chip::app::Clusters;

    if (cId == TemperatureMeasurement::Id) {
      switch (aId) {
        case TemperatureMeasurement::Attributes::MeasuredValue::Id:
          *val = esp_matter_int16(_attrs.temperature);   return ESP_OK;
        case TemperatureMeasurement::Attributes::MinMeasuredValue::Id:
          *val = esp_matter_int16(-4000);                return ESP_OK; // -40.00°C
        case TemperatureMeasurement::Attributes::MaxMeasuredValue::Id:
          *val = esp_matter_int16(8500);                 return ESP_OK; // +85.00°C
        case 0xFFFC: *val = esp_matter_uint32(0);        return ESP_OK; // FeatureMap
        case 0xFFFD: *val = esp_matter_uint16(4);        return ESP_OK; // ClusterRevision
        default: break;
      }
    }

    if (cId == RelativeHumidityMeasurement::Id) {
      switch (aId) {
        case RelativeHumidityMeasurement::Attributes::MeasuredValue::Id:
          *val = esp_matter_uint16(_attrs.humidity);     return ESP_OK;
        case RelativeHumidityMeasurement::Attributes::MinMeasuredValue::Id:
          *val = esp_matter_uint16(0);                   return ESP_OK;
        case RelativeHumidityMeasurement::Attributes::MaxMeasuredValue::Id:
          *val = esp_matter_uint16(10000);               return ESP_OK; // 100.00%
        case 0xFFFC: *val = esp_matter_uint32(0);        return ESP_OK; // FeatureMap
        case 0xFFFD: *val = esp_matter_uint16(3);        return ESP_OK; // ClusterRevision
        default: break;
      }
    }

    return MBDevice::onAttrRead(cId, aId, val);
  }

protected:
  esp_matter::endpoint_t* _buildEndpoint(esp_matter::node_t* node) override {
    // ── Endpoint: bridged_node ─────────────────────────────────────────────
    esp_matter::endpoint::bridged_node::config_t cfg = {};
    esp_matter::endpoint_t* ep =
      esp_matter::endpoint::bridged_node::create(node, &cfg, ENDPOINT_FLAG_BRIDGE, this);
    if (!ep) { ESP_LOGE("TempHumid", "bridged_node create failed"); return nullptr; }

    // Identify – required by the spec for every endpoint / wymagany przez spec dla każdego endpointu
    esp_matter::cluster::identify::config_t iCfg = {};
    auto* id = esp_matter::cluster::identify::create(ep, &iCfg, CLUSTER_FLAG_SERVER);
    if (!id) { ESP_LOGE("TempHumid", "identify create failed"); return nullptr; }

    // ── TemperatureMeasurement ─────────────────────────────────────────────
    esp_matter::cluster::temperature_measurement::config_t tCfg = {};
    tCfg.measured_value     = (int16_t)2000;   // placeholder 20.00°C (non-zero! / niezerowy!)
    tCfg.min_measured_value = (int16_t)-4000;
    tCfg.max_measured_value = (int16_t)8500;
    auto* tc = esp_matter::cluster::temperature_measurement::create(
      ep, &tCfg, CLUSTER_FLAG_SERVER);
    if (!tc) { ESP_LOGE("TempHumid", "temperature_measurement create failed"); return nullptr; }

    // ── RelativeHumidityMeasurement ────────────────────────────────────────
    esp_matter::cluster::relative_humidity_measurement::config_t hCfg = {};
    hCfg.measured_value     = (uint16_t)5000;  // placeholder 50.00% (non-zero! / niezerowy!)
    hCfg.min_measured_value = (uint16_t)0;
    hCfg.max_measured_value = (uint16_t)10000;
    auto* hc = esp_matter::cluster::relative_humidity_measurement::create(
      ep, &hCfg, CLUSTER_FLAG_SERVER);
    if (!hc) { ESP_LOGE("TempHumid", "relative_humidity_measurement create failed"); return nullptr; }

    if (!_addBridgedBasicInfo(ep)) return nullptr;

    // ── DeviceType IDs ─────────────────────────────────────────────────────
    // 0x0302 Temperature Sensor – primary, ST recognises the device type / główny, ST rozpoznaje typ urządzenia
    // 0x0307 Humidity Sensor    – secondary, ST shows both measurements in one tile / dodatkowy, ST widzi oba pomiary w jednym kafelku
    esp_matter::endpoint::add_device_type(ep, 0x0302, 1);
    esp_matter::endpoint::add_device_type(ep, 0x0307, 1);

    ESP_LOGI("TempHumid", "_buildEndpoint OK: epId=%" PRIu16
             " devType=0x0302+0x0307",
             esp_matter::endpoint::get_id(ep));
    return ep;
  }
};