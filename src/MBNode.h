// =============================================================================
//  MatterBridge — MBNode.h: Matter node init, aggregator, attribute callbacks
//  MatterBridge — MBNode.h: Inicjalizacja węzła Matter, agregator, callbacki atrybutów
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
#include "MBConfig.h"

#include <esp_matter.h>
#include <esp_matter_endpoint.h>
#include <esp_matter_cluster.h>
#include <esp_matter_attribute_utils.h>
#include <esp_matter_identify.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <credentials/FabricTable.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#include <platform/ConfigurationManager.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/PlatformManager.h>

// CHIP reporting – needed for MatterReportingAttributeChangeCallback
// CHIP reporting – potrzebne do MatterReportingAttributeChangeCallback
#include <app/reporting/reporting.h>

// ── OpenThread – required ONLY on ESP32-C6 ─────────────────────────────────
// On ESP32-S3 OpenThread does not exist in the SDK – including the headers
// would cause a compile error. On C6 arduino-esp32 3.x always links
// OpenThread and requires set_openthread_platform_config() to be called before start().
// ── OpenThread – wymagany TYLKO na ESP32-C6 ───────────────────────────────────
// Na ESP32-S3 OpenThread nie istnieje w SDK – dołączenie nagłówków
// spowodowałoby błąd kompilacji. Na C6 arduino-esp32 3.x zawsze linkuje
// OpenThread i wymaga wywołania set_openthread_platform_config() przed start().
#if MB_TARGET_C6
  #include <platform/ESP32/OpenthreadLauncher.h>
  #include <esp_openthread_types.h>
#endif

#include "MBTypes.h"

using esp_matter::ENDPOINT_FLAG_NONE;
using esp_matter::ENDPOINT_FLAG_BRIDGE;
using esp_matter::CLUSTER_FLAG_SERVER;
using esp_matter::CLUSTER_FLAG_ATTRIBUTE_CHANGED_FUNCTION;

// ============================================================
//  MBNode.h – Matter node initialisation via esp-matter
//  MBNode.h – inicjalizacja węzła Matter przez esp-matter
// ============================================================

typedef esp_err_t (*MBAttrReadCb) (uint16_t epId, uint32_t clusterId,
                                    uint32_t attrId,
                                    esp_matter_attr_val_t* val,
                                    void* privData);
typedef esp_err_t (*MBAttrWriteCb)(uint16_t epId, uint32_t clusterId,
                                    uint32_t attrId,
                                    esp_matter_attr_val_t* val,
                                    void* privData);

class MBNodeClass {
public:
  MBNodeClass()
    : _node(nullptr), _aggregator(nullptr),
      _aggregatorEpId(0),
      _attrReadCb(nullptr), _attrWriteCb(nullptr)
  {
    esp_log_level_set("esp_matter_cluster", ESP_LOG_WARN);
  }

  // --------------------------------------------------------
  //  init() – NVS + Matter node + aggregator EP1
  //  init() – NVS + węzeł Matter + agregator EP1
  // --------------------------------------------------------
  bool init(MBAttrReadCb readCb, MBAttrWriteCb writeCb) {
    _attrReadCb  = readCb;
    _attrWriteCb = writeCb;

    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_LOGW("MBNode", "NVS uszkodzony – kasowanie i reinicjalizacja");
      nvs_flash_erase();
      nvs_flash_init();
    }

    esp_matter::node::config_t nodeCfg = {};
    _node = esp_matter::node::create(&nodeCfg, _staticAttrCb, _staticIdentifyCb);
    if (!_node) {
      ESP_LOGE("MBNode", "Nie można utworzyć węzła Matter!");
      return false;
    }

    if (!_createAggregator()) {
      ESP_LOGE("MBNode", "Nie można utworzyć agregatora EP1!");
      return false;
    }

    ESP_LOGI("MBNode", "Węzeł Matter + Aggregator OK (aggrEpId=%u)", _aggregatorEpId);
    return true;
  }

  // --------------------------------------------------------
  //  start()
  //
  //  KEY DIFFERENCE C6 vs S3:
  //    C6: arduino-esp32 3.x always links OpenThread – we must call
  //        set_openthread_platform_config() even when using WiFi.
  //        Skipping it causes an assert/crash in start().
  //    S3: OpenThread does not exist in the SDK – we call nothing.
  //        esp_matter::start() handles WiFi without OpenThread.
  //
  //  KLUCZOWA RÓŻNICA C6 vs S3:
  //    C6: arduino-esp32 3.x zawsze linkuje OpenThread – musimy wywołać
  //        set_openthread_platform_config() nawet gdy używamy WiFi.
  //        Pominięcie powoduje assert/crash przy start().
  //    S3: OpenThread nie istnieje w SDK – nie wywołujemy niczego.
  //        esp_matter::start() obsługuje WiFi bez OpenThread.
  // --------------------------------------------------------
  esp_err_t start() {

#if MB_TARGET_C6
    // OpenThread configuration – required by arduino-esp32 3.x on C6.
    // Even in WiFi mode it must be called before start().
    // Konfiguracja OpenThread – wymagana przez arduino-esp32 3.x na C6.
    // Nawet w trybie WiFi musi być wywołana przed start().
    esp_openthread_platform_config_t otCfg;
    memset(&otCfg, 0, sizeof(otCfg));
    otCfg.radio_config.radio_mode            = RADIO_MODE_NATIVE;
    otCfg.host_config.host_connection_mode   = HOST_CONNECTION_MODE_NONE;
    otCfg.port_config.storage_partition_name = "nvs";
    otCfg.port_config.netif_queue_size       = 10;
    otCfg.port_config.task_queue_size        = 10;

    esp_err_t err = set_openthread_platform_config(&otCfg);
    if (err != ESP_OK) {
      ESP_LOGE("MBNode", "set_openthread_platform_config błąd: %s",
               esp_err_to_name(err));
      return err;
    }
    ESP_LOGI("MBNode", "OpenThread platform config OK (C6, tryb WiFi)");
#else
    // S3: OpenThread is unavailable, esp_matter::start() works over WiFi
    // S3: OpenThread niedostępny, esp_matter::start() działa przez WiFi
    ESP_LOGI("MBNode", "Pominięto OpenThread (S3 – brak radia Thread)");
    esp_err_t err = ESP_OK;
#endif  // MB_TARGET_C6

    err = esp_matter::start(_staticEventCb);
    if (err != ESP_OK) {
      ESP_LOGE("MBNode", "esp_matter::start() błąd: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI("MBNode", "esp_matter::start() OK (platforma: %s)",
               MB_TARGET_C6 ? "ESP32-C6" : "ESP32-S3");
    }
    return err;
  }

  esp_matter::node_t*     node()           { return _node; }
  esp_matter::endpoint_t* aggregator()     { return _aggregator; }
  uint16_t                aggregatorEpId() const { return _aggregatorEpId; }

  bool isCommissioned() const {
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
  }

  bool getPairingCode(char* buf, size_t len) const {
    chip::SetupPayload payload;
    if (!_getSetupPayload(payload)) return false;
    chip::ManualSetupPayloadGenerator gen(payload);
    std::string code;
    if (gen.payloadDecimalStringRepresentation(code) != CHIP_NO_ERROR) return false;
    snprintf(buf, len, "%s", code.c_str());
    return true;
  }

  bool getQRCode(char* buf, size_t len) const {
    chip::SetupPayload payload;
    if (!_getSetupPayload(payload)) return false;
    chip::QRCodeSetupPayloadGenerator gen(payload);
    std::string code;
    if (gen.payloadBase38RepresentationWithAutoTLVBuffer(code) != CHIP_NO_ERROR)
      return false;
    snprintf(buf, len, "%s%s",
             (code.rfind("MT:", 0) != 0) ? "MT:" : "", code.c_str());
    return true;
  }

  void decommission() {
    chip::Server::GetInstance().ScheduleFactoryReset();
  }

  // --------------------------------------------------------
  //  notifyTopologyChange – informs the hub about a change of the endpoint list.
  //
  //  CRITICAL FIX: the old code used the pattern:
  //    attribute::get_val(attr, &val);   // val points to an internal buffer
  //    attribute::update(epId, ...&val); // esp-matter tries to free the old
  //                                        buffer – which we have just returned!
  //  Result: double-free → CORRUPT HEAP.
  //
  //  Correct solution: MatterReportingAttributeChangeCallback(epId, cId, aId)
  //  marks the attribute as dirty directly in the CHIP reporting engine.
  //
  //  notifyTopologyChange – informuje hub o zmianie listy endpointów.
  //
  //  POPRAWKA KRYTYCZNA: stary kod używał wzorca:
  //    attribute::get_val(attr, &val);   // val wskazuje na wewnętrzny bufor
  //    attribute::update(epId, ...&val); // esp-matter próbuje zwolnić stary
  //                                        bufor – który właśnie zwróciliśmy!
  //  Skutek: double-free → CORRUPT HEAP.
  //
  //  Poprawne rozwiązanie: MatterReportingAttributeChangeCallback(epId, cId, aId)
  //  bezpośrednio oznacza atrybut jako brudny w silniku raportowania CHIP.
  // --------------------------------------------------------
  void notifyTopologyChange() {
    using namespace chip::app::Clusters;

    esp_matter::lock::chip_stack_lock(portMAX_DELAY);

    MatterReportingAttributeChangeCallback(
        chip::EndpointId(0),
        Descriptor::Id,
        Descriptor::Attributes::PartsList::Id);

    if (_aggregatorEpId != 0) {
      MatterReportingAttributeChangeCallback(
          chip::EndpointId(_aggregatorEpId),
          Descriptor::Id,
          Descriptor::Attributes::PartsList::Id);
    }

    esp_matter::lock::chip_stack_unlock();

    ESP_LOGI("MBNode", "notifyTopologyChange: PartsList dirty on EP0 + EP%u",
             _aggregatorEpId);
  }

  // --------------------------------------------------------
  //  setReachable – sets the Reachable attribute on an endpoint.
  //  setReachable – ustawia atrybut Reachable na endpoincie.
  // --------------------------------------------------------
  bool setReachable(uint16_t epId, bool reachable) {
    esp_matter::node_t* nd = esp_matter::node::get();
    if (!nd) return false;

    esp_matter::lock::chip_stack_lock(portMAX_DELAY);

    bool result = false;
    esp_matter::endpoint_t* ep = esp_matter::endpoint::get(nd, epId);
    if (ep) {
      using namespace chip::app::Clusters::BridgedDeviceBasicInformation;
      esp_matter::cluster_t* cl = esp_matter::cluster::get(ep, Id);
      if (cl) {
        esp_matter::attribute_t* attr =
          esp_matter::attribute::get(cl, Attributes::Reachable::Id);
        if (attr) {
          esp_matter_attr_val_t val = esp_matter_bool(reachable);
          result = (esp_matter::attribute::set_val(attr, &val) == ESP_OK);
        }
      }
    }

    esp_matter::lock::chip_stack_unlock();
    return result;
  }

  void dispatchAttrCb(uint16_t epId, uint32_t clusterId,
                       uint32_t attrId, esp_matter_attr_val_t* val,
                       bool isWrite, void* privData) {
    esp_err_t result = ESP_ERR_NOT_FOUND;

    if (isWrite && _attrWriteCb)
      result = _attrWriteCb(epId, clusterId, attrId, val, privData);
    else if (!isWrite && _attrReadCb)
      result = _attrReadCb(epId, clusterId, attrId, val, privData);

    if (result == ESP_ERR_NOT_FOUND) {
      ESP_LOGV("MBNode", "Unhandled attr %s ep=%u cl=0x%04lx at=0x%04lx",
               isWrite ? "write" : "read", epId,
               (unsigned long)clusterId, (unsigned long)attrId);
    }
  }

private:
  esp_matter::node_t*     _node;
  esp_matter::endpoint_t* _aggregator;
  uint16_t                _aggregatorEpId;
  MBAttrReadCb            _attrReadCb;
  MBAttrWriteCb           _attrWriteCb;

  bool _createAggregator() {
    esp_matter::endpoint::aggregator::config_t cfg = {};
    _aggregator = esp_matter::endpoint::aggregator::create(
      _node, &cfg, ENDPOINT_FLAG_NONE, nullptr);
    if (!_aggregator) return false;
    esp_matter::cluster::descriptor::config_t descCfg = {};
    auto* desc = esp_matter::cluster::descriptor::create(
      _aggregator, &descCfg, CLUSTER_FLAG_SERVER);
    if (!desc) {
      ESP_LOGE("MBNode", "_createAggregator: descriptor create FAILED");
      return false;
    }
    _aggregatorEpId = esp_matter::endpoint::get_id(_aggregator);
    return true;
  }

  static esp_err_t _staticAttrCb(
      const esp_matter::attribute::callback_type_t type,
      uint16_t epId, uint32_t clusterId,
      uint32_t attrId, esp_matter_attr_val_t* val, void* privData) {

    bool isWrite = (type == esp_matter::attribute::PRE_UPDATE ||
                    type == esp_matter::attribute::POST_UPDATE);

    extern MBNodeClass MBNode;
    MBNode.dispatchAttrCb(epId, clusterId, attrId, val, isWrite, privData);
    return ESP_OK;
  }

  static esp_err_t _staticIdentifyCb(
      esp_matter::identification::callback_type_t type,
      uint16_t epId, uint8_t effectId, uint8_t effectVariant, void* privData) {
    ESP_LOGI("MBNode", "Identify ep=%u effect=%u", epId, effectId);
    return ESP_OK;
  }

  static void _staticEventCb(
      const chip::DeviceLayer::ChipDeviceEvent* event, intptr_t) {
    switch (event->Type) {
      case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI("MBNode", "Komisjonowanie zakończone!"); break;
      case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI("MBNode", "Fabric usunięty."); break;
      case chip::DeviceLayer::DeviceEventType::kWiFiConnectivityChange:
        if (event->WiFiConnectivityChange.Result ==
            chip::DeviceLayer::kConnectivity_Established)
          ESP_LOGI("MBNode", "WiFi połączone!");
        else
          ESP_LOGW("MBNode", "WiFi rozłączone.");
        break;
      default: break;
    }
  }

  bool _getSetupPayload(chip::SetupPayload& payload) const {
    auto* provider = chip::DeviceLayer::GetCommissionableDataProvider();
    if (!provider) return false;
    uint16_t discriminator = 0;
    uint32_t passcode      = 0;
    provider->GetSetupDiscriminator(discriminator);
    provider->GetSetupPasscode(passcode);
    payload.discriminator.SetLongValue(discriminator);
    payload.setUpPINCode = passcode;
    payload.version      = 0;
    payload.rendezvousInformation.SetValue(
      chip::RendezvousInformationFlag::kBLE);
    return true;
  }
};

extern MBNodeClass MBNode;
