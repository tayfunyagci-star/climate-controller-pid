// MQTT ayarları (Ayarlar › MQTT bölümü) — NVS "mqtt" alanında kalıcı. MQTT istemcisi F5'tedir: bu modül
// yalnız ayarları saklar ve boot'ta çekirdek alanlarını (yayın aralıkları, keşif, uzak yetkiler) konfigürasyona
// uygular. Parola yalnız yazılır; GET'e çıkmaz (passSet). Web ve konsol aynı apply() yolunu kullanır.
#pragma once
#include <cstdint>
#include "core_api.h"

namespace mqttcfg {

struct Settings {
  char host[64] = "";                     // boş = MQTT kapalı
  uint16_t port = 1883;
  char user[65] = "";
  char base[97] = "mqttsuite/climate";    // kök topic; tam taban <kök>/<SLUG>
};

// Bölümün çekirdek konfigürasyonunda yaşayan alanları (CONFIGURATION_MODEL §2.10)
constexpr const char* kCoreKeys[] = {"state_active_s", "state_idle_s", "diag_interval_s", "discovery_enabled",
                                     "history_discovery_enabled", "remote_config_enabled", "pid_remote_tuning",
                                     "remote_manual_allowed", "service_channel_enabled"};
constexpr size_t kCoreKeyCount = sizeof kCoreKeys / sizeof kCoreKeys[0];

void load();                              // NVS → bellek (setup, çekirdekten önce)
void overlay(cc::Config& c);              // kayıtlı çekirdek alanlarını boot konfigürasyonuna uygula
Settings settings();
bool passSet();
bool isStringKey(const char* key);        // mqtt_host, mqtt_port, mqtt_user, mqtt_password, mqtt_base
bool isCoreKey(const char* key);

// Aday bütünüyle doğrulanmış olmalı (web katmanı); burada yalnız biçim + NVS yazımı.
// pass == nullptr: parola korunur; "" = sil. core: bölümün çekirdek alanlarını taşıyan aday konfigürasyon.
bool apply(const Settings& s, const char* pass, const cc::Config& core, const char** err, const char** field);
bool validate(const Settings& s, const char* pass, const char** err, const char** field);

}  // namespace mqttcfg
