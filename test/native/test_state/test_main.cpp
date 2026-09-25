// Durum makineleri — STATE_MACHINE §1 (sistem), §2 (ısıtma zinciri), §5 (controller_state).
#include <unity.h>
#include "cc_state.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static void stepN(SystemSM& s, SysInput in, int n) { for (int i = 0; i < n; ++i) s.step(in); }

void test_boot_paths() {
  SystemSM a;
  TEST_ASSERT_EQUAL(SysState::BOOT, a.state());
  a.bootDone(false);
  TEST_ASSERT_EQUAL(SysState::SELF_TEST, a.state());
  a.selfTestResult(true);
  TEST_ASSERT_EQUAL(SysState::RUN, a.state());
  SystemSM b;
  b.bootDone(false);
  b.selfTestResult(false);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, b.state());
  SystemSM c;
  c.bootDone(true);  // restart fırtınası
  TEST_ASSERT_EQUAL(SysState::RECOVERY, c.state());
  TEST_ASSERT_TRUE(c.recoveryAck());
  TEST_ASSERT_EQUAL(SysState::SELF_TEST, c.state());
  TEST_ASSERT_TRUE(isRestartStorm(5, 5));
  TEST_ASSERT_FALSE(isRestartStorm(4, 5));
}

static SystemSM running() {
  SystemSM s;
  s.bootDone(false);
  s.selfTestResult(true);
  return s;
}

void test_run_failsafe_run() {
  SystemSM s = running();
  SysInput in;
  in.safety_lockout = true;
  s.step(in);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, s.state());
  in.safety_lockout = false;
  s.step(in);
  TEST_ASSERT_EQUAL(SysState::RUN, s.state());
}

void test_service_paths() {
  SystemSM s = running();
  TEST_ASSERT_TRUE(s.serviceEnter());
  TEST_ASSERT_EQUAL(SysState::SERVICE, s.state());
  SysInput in;
  in.service_timeout_ms = 1000;
  stepN(s, in, 9);
  TEST_ASSERT_EQUAL(SysState::SERVICE, s.state());
  s.serviceActivity();
  stepN(s, in, 9);
  TEST_ASSERT_EQUAL(SysState::SERVICE, s.state());
  stepN(s, in, 2);
  TEST_ASSERT_EQUAL(SysState::RUN, s.state());  // zaman aşımı
  TEST_ASSERT_TRUE(s.serviceEnter());
  in.safety_lockout = true;
  s.step(in);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, s.state());  // SERVICE → FAILSAFE
  SystemSM f;
  f.bootDone(false);
  f.selfTestResult(false);
  TEST_ASSERT_FALSE(f.serviceEnter());  // yalnız RUN'dan
}

void test_ota_paths() {
  SystemSM s = running();
  TEST_ASSERT_TRUE(s.otaBegin());
  SysInput in;
  in.heating_idle = false;
  s.step(in);
  TEST_ASSERT_EQUAL(SysState::OTA_PREP, s.state());
  in.heating_idle = true;
  s.step(in);
  TEST_ASSERT_EQUAL(SysState::OTA, s.state());
  s.otaFailed();
  TEST_ASSERT_EQUAL(SysState::RUN, s.state());
  // hazırlık zaman aşımı → RUN
  s.otaBegin();
  in.heating_idle = false;
  in.ota_prep_timeout_ms = 500;
  stepN(s, in, 6);
  TEST_ASSERT_EQUAL(SysState::RUN, s.state());
  // iptal
  s.otaBegin();
  s.otaAbort();
  TEST_ASSERT_EQUAL(SysState::RUN, s.state());
  // OTA_PREP sırasında safety trip → FAILSAFE
  s.otaBegin();
  in.safety_lockout = true;
  s.step(in);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, s.state());
}

// Isıtma zinciri geçiş tablosu (§2)
void test_heating_chain_transitions() {
  HeatingChain h;
  HeatChainInput in;
  TEST_ASSERT_EQUAL(HeatPhase::IDLE, h.step(in));
  in.chain_request = true; in.prepurge = true;
  TEST_ASSERT_EQUAL(HeatPhase::PREPURGE, h.step(in));
  in.chain_request = false; in.prepurge = false;
  TEST_ASSERT_EQUAL(HeatPhase::IDLE, h.step(in));  // talep kalktı
  in.chain_request = true; in.prepurge = true;
  h.step(in);
  in.prepurge = false; in.any_r = true;
  TEST_ASSERT_EQUAL(HeatPhase::ACTIVE, h.step(in));
  in.chain_request = false; in.any_r = true;  // min ON tutuyor
  TEST_ASSERT_EQUAL(HeatPhase::ACTIVE, h.step(in));
  in.any_r = false; in.post_cool = true;
  TEST_ASSERT_EQUAL(HeatPhase::POST_COOL, h.step(in));
  in.chain_request = true;  // post-cool'dan prepurge'süz aktife
  TEST_ASSERT_EQUAL(HeatPhase::ACTIVE, h.step(in));
  in.chain_request = false;
  h.step(in);
  in.post_cool = false;
  TEST_ASSERT_EQUAL(HeatPhase::IDLE, h.step(in));
  // Kilit: ACTIVE → LOCKOUT; post-cool bitene kadar LOCKOUT
  in.chain_request = true; in.any_r = true;
  h.step(in);
  in.lockout = true; in.any_r = false; in.post_cool = true;
  TEST_ASSERT_EQUAL(HeatPhase::LOCKOUT, h.step(in));
  in.lockout = false;
  TEST_ASSERT_EQUAL(HeatPhase::LOCKOUT, h.step(in));
  in.post_cool = false; in.chain_request = false;
  TEST_ASSERT_EQUAL(HeatPhase::IDLE, h.step(in));
  in.lockout = true;
  TEST_ASSERT_EQUAL(HeatPhase::LOCKOUT, h.step(in));  // IDLE → LOCKOUT
}

// controller_state öncelik tablosunun 12 satırı
void test_controller_state_table() {
  struct Row { SysState s; HeatPhase h; VentState v; OpMode m; CtrlState e; };
  const Row rows[] = {
      {SysState::BOOT, HeatPhase::ACTIVE, VentState::AUTO, OpMode::AUTO, CtrlState::BOOT},
      {SysState::SELF_TEST, HeatPhase::IDLE, VentState::OFF, OpMode::AUTO, CtrlState::SELF_TEST},
      {SysState::RECOVERY, HeatPhase::POST_COOL, VentState::OFF, OpMode::AUTO, CtrlState::RECOVERY},
      {SysState::OTA_PREP, HeatPhase::POST_COOL, VentState::OFF, OpMode::AUTO, CtrlState::OTA},
      {SysState::OTA, HeatPhase::IDLE, VentState::OFF, OpMode::AUTO, CtrlState::OTA},
      {SysState::FAILSAFE, HeatPhase::LOCKOUT, VentState::FORCED, OpMode::AUTO, CtrlState::FAILSAFE},
      {SysState::SERVICE, HeatPhase::ACTIVE, VentState::OFF, OpMode::AUTO, CtrlState::SERVICE},
      {SysState::RUN, HeatPhase::PREPURGE, VentState::AUTO, OpMode::MANUAL, CtrlState::HEATING},
      {SysState::RUN, HeatPhase::ACTIVE, VentState::OFF, OpMode::AUTO, CtrlState::HEATING},
      {SysState::RUN, HeatPhase::POST_COOL, VentState::AUTO, OpMode::AUTO, CtrlState::POST_COOL},
      {SysState::RUN, HeatPhase::IDLE, VentState::MANUAL, OpMode::OFF, CtrlState::VENTILATING},
      {SysState::RUN, HeatPhase::IDLE, VentState::FORCED, OpMode::AUTO, CtrlState::VENTILATING},
      {SysState::RUN, HeatPhase::IDLE, VentState::INHIBITED, OpMode::MANUAL, CtrlState::MANUAL},
      {SysState::RUN, HeatPhase::IDLE, VentState::OFF, OpMode::OFF, CtrlState::OFF},
      {SysState::RUN, HeatPhase::IDLE, VentState::OFF, OpMode::AUTO, CtrlState::IDLE},
      {SysState::RUN, HeatPhase::IDLE, VentState::OFF, OpMode::VENT_ONLY, CtrlState::IDLE},
  };
  for (const Row& r : rows) TEST_ASSERT_EQUAL((int)r.e, (int)deriveControllerState(r.s, r.h, r.v, r.m));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_paths);
  RUN_TEST(test_run_failsafe_run);
  RUN_TEST(test_service_paths);
  RUN_TEST(test_ota_paths);
  RUN_TEST(test_heating_chain_transitions);
  RUN_TEST(test_controller_state_table);
  return UNITY_END();
}
