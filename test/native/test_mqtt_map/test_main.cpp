// MQTT entity tablosu ve keşif üreticisi (MQTT_INTEGRATION §5, mqtt-studio-dugum §2–§3, §7).
#include <unity.h>
#include <cstdio>
#include <cstring>
#include "cc_config.h"
#include "cc_mqtt_map.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static const MqIdentity kId = {"mqttsuite/climate/kulube_iklim_3c71bf", "kulube_iklim_3c71bf", "Kulübe İklim 3c71bf", "0.3.0"};

void test_ids_unique_and_wire_safe() {
  for (size_t i = 0; i < mqEntityCount(); ++i) {
    const MqEntity& a = mqEntity(i);
    for (const char* p = a.id; *p; ++p)
      TEST_ASSERT_TRUE_MESSAGE((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_', a.id);
    for (size_t j = i + 1; j < mqEntityCount(); ++j) {
      TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strcmp(a.id, mqEntity(j).id), a.id);
      TEST_ASSERT_NOT_EQUAL_MESSAGE(0, strcmp(a.name, mqEntity(j).name), a.name);
    }
  }
}

void test_numbers_and_selects_backed_by_config() {
  for (size_t i = 0; i < mqEntityCount(); ++i) {
    const MqEntity& e = mqEntity(i);
    if (e.comp != MqComp::NUMBER && e.comp != MqComp::SELECT) continue;
    const FieldInfo* f = findField(e.id);
    TEST_ASSERT_NOT_NULL_MESSAGE(f, e.id);   // sınırlar/seçenekler tek kaynaktan
    if (e.comp == MqComp::SELECT) TEST_ASSERT_TRUE_MESSAGE(f->kind == FieldKind::ENUM && f->enumCount > 0, e.id);
  }
}

void test_payloads_fit_and_follow_value_template_rule() {
  char topic[160], buf[kMqBuffer];
  for (size_t i = 0; i < mqEntityCount(); ++i) {
    const MqEntity& e = mqEntity(i);
    TEST_ASSERT_TRUE(mqDiscoveryTopic(kId, e, topic, sizeof topic));
    const size_t n = mqDiscoveryPayload(kId, e, buf, sizeof buf);
    TEST_ASSERT_TRUE_MESSAGE(n > 0 && n < 1024, e.id);   // tampon 2048; tek kayıt ≤ 1 KB
    if (e.comp != MqComp::BUTTON) {
      char want[96];
      snprintf(want, sizeof want, "\"val_tpl\":\"{{ value_json.%s }}\"", e.id);
      TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, want), e.id);
    }
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"avty_t\":\"~/avail\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ids\":[\"kulube_iklim_3c71bf\"]"));
    TEST_ASSERT_NULL(strstr(buf, "bridge"));
    TEST_ASSERT_EQUAL(mqWritable(e), strstr(buf, "\"cmd_t\"") != nullptr);
  }
}

void test_examples() {
  char buf[kMqBuffer], topic[160];
  const MqEntity* sp = mqFind("temperature_setpoint");
  TEST_ASSERT_NOT_NULL(sp);
  mqDiscoveryTopic(kId, *sp, topic, sizeof topic);
  TEST_ASSERT_EQUAL_STRING("homeassistant/number/kulube_iklim_3c71bf_temperature_setpoint/config", topic);
  mqDiscoveryPayload(kId, *sp, buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"cmd_t\":\"~/temperature_setpoint/set\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"min\":5,\"max\":30,\"step\":0.5"));
  const MqEntity* om = mqFind("operating_mode");
  mqDiscoveryPayload(kId, *om, buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"options\":[\"OFF\",\"AUTO\",\"MANUAL\",\"VENT_ONLY\"]"));
  const MqEntity* ack = mqFind("alarm_ack");
  mqDiscoveryPayload(kId, *ack, buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"json_attributes_template\":\"{{ value_json.ack_count }}\""));
  TEST_ASSERT_NULL(strstr(buf, "stat_t"));
  const MqEntity* vs = mqFind("ventilation_start_temperature");
  mqDiscoveryPayload(kId, *vs, buf, sizeof buf);
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"stat_t\":\"~/config/reported\""));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"ent_cat\":\"config\""));
  TEST_ASSERT_EQUAL(0, mqDiscoveryPayload(kId, *vs, buf, 64));   // taşma → 0, yarım yük yok
}

void test_parse_set_topic() {
  char id[48];
  const char* B = "mqttsuite/climate/kulube_iklim_3c71bf";
  TEST_ASSERT_TRUE(mqParseSetTopic(B, "mqttsuite/climate/kulube_iklim_3c71bf/temperature_setpoint/set", id, sizeof id));
  TEST_ASSERT_EQUAL_STRING("temperature_setpoint", id);
  TEST_ASSERT_TRUE(mqParseSetTopic(B, "mqttsuite/climate/kulube_iklim_3c71bf/alarm_ack/set", id, sizeof id));
  TEST_ASSERT_FALSE(mqParseSetTopic(B, "mqttsuite/climate/kulube_iklim_3c71bf/temperature/set", id, sizeof id));   // salt okunur
  TEST_ASSERT_FALSE(mqParseSetTopic(B, "mqttsuite/climate/kulube_iklim_3c71bf/antifreeze_enabled/set", id, sizeof id));
  TEST_ASSERT_FALSE(mqParseSetTopic(B, "mqttsuite/climate/baska/boost/set", id, sizeof id));
  TEST_ASSERT_FALSE(mqParseSetTopic(B, "mqttsuite/climate/kulube_iklim_3c71bf/boost/set/x", id, sizeof id));
  TEST_ASSERT_FALSE(mqParseSetTopic(B, "mqttsuite/climate/kulube_iklim_3c71bf/state", id, sizeof id));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ids_unique_and_wire_safe);
  RUN_TEST(test_numbers_and_selects_backed_by_config);
  RUN_TEST(test_payloads_fit_and_follow_value_template_rule);
  RUN_TEST(test_examples);
  RUN_TEST(test_parse_set_topic);
  return UNITY_END();
}
