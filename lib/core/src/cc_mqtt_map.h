// MQTT entity tablosu — tek kaynak (MQTT_INTEGRATION §5.3, ENTITY_MODEL §2–§4, mqtt-studio-dugum §7).
// Keşif kaydı, abonelik ve komut dağıtımı bu tablodan türetilir: `id` hem state JSON anahtarı hem
// `B/<id>/set` son parçasıdır. number/select sınırları ve seçenekleri konfigürasyon alan tablosundan
// (cc_config) okunur; iki yerde yazılmaz. Saf C++: Arduino/IDF başlığı yok, dinamik bellek yok.
#pragma once
#include <cstddef>
#include <cstdint>

namespace cc {

enum class MqComp : uint8_t { SENSOR, BINARY, SWITCH, NUMBER, SELECT, BUTTON };
enum class MqSrc : uint8_t { STATE, CONFIG, DIAG };   // ~/state, ~/config/reported, ~/diag/state

struct MqEntity {
  MqComp comp;
  const char* id;
  const char* name;       // SABİT (Suite entity kimliği ad tabanlıdır; değişirse bağlamalar kopar)
  const char* unit;
  const char* dev_cla;
  MqSrc src;
  const char* icon;
};

size_t mqEntityCount();
const MqEntity& mqEntity(size_t i);
const MqEntity* mqFind(const char* id);
const char* mqCompName(MqComp c);   // homeassistant/<bu>/…
bool mqWritable(const MqEntity& e);  // switch/number/select/button

struct MqIdentity {
  const char* base;       // B = <kök>/<SLUG>
  const char* slug;
  const char* dev_name;   // kurulumda sabit: "Kulübe İklim <mac3>"
  const char* sw;         // firmware sürümü
};

// homeassistant/<comp>/<slug>_<id>/config
bool mqDiscoveryTopic(const MqIdentity& idn, const MqEntity& e, char* out, size_t cap);
// Keşif yükü (düz JSON, kısa anahtarlar, `~` kısayolu). 0 = sığmadı.
size_t mqDiscoveryPayload(const MqIdentity& idn, const MqEntity& e, char* out, size_t cap);
// "<B>/<id>/set" → id. Yalnız tablodaki yazılabilir entity'ler için true.
bool mqParseSetTopic(const char* base, const char* topic, char* id, size_t cap);

constexpr size_t kMqBuffer = 3072;   // MQTT_INTEGRATION §3 (M6): B/state kötü durumda ≈ 1.7 KB, pay bırakılır

}  // namespace cc
