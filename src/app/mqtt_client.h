// MQTT katmanı (F5) — MQTT_INTEGRATION.md, mqtt-studio-dugum sözleşmesi.
// İstemci ESP-IDF esp-mqtt'tir (Arduino-ESP32 çekirdeğinde hazır; QoS 1, retain bayrağı, 2048 B tampon; OPEN
// ISSUE QoS kararı). esp-mqtt kendi ağ görevinde çalışır; bu modülün "mqtt" görevi istemcinin tek kullanıcısıdır:
// bağlantı yaşam döngüsü, keşif, yayın zamanlayıcıları ve komut dağıtımı burada. Olay geri çağrısı yalnız
// bayrak/kuyruk yazar. Komutlar çekirdeğe web ile aynı yoldan (ClimateCore::command, kaynak MQTT) gider;
// MQTT/Wi-Fi kaybı kontrolü değiştirmez.
#pragma once
#include <cstdint>

namespace mq {

// Adlar Arduino makrolarıyla (DISABLED …) çakışmasın diye CamelCase; tel adları stateName()
enum class State : uint8_t { Disabled, Connecting, Connected, Backoff, AuthFail };

struct Status {
  State state = State::Disabled;
  uint32_t reconnects = 0;
  uint32_t commands = 0, rejected = 0, ignored_retained = 0, events_dropped = 0;
  char note[72] = "";
};

void begin();                  // görevi başlatır (net::begin'den sonra)
Status status();
const char* stateName(State s);

}  // namespace mq
