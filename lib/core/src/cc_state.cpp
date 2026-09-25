#include "cc_state.h"

namespace cc {

void SystemSM::bootDone(bool storm) {
  if (st_ == SysState::BOOT) go(storm ? SysState::RECOVERY : SysState::SELF_TEST);
}
void SystemSM::selfTestResult(bool pass) {
  if (st_ == SysState::SELF_TEST) go(pass ? SysState::RUN : SysState::FAILSAFE);
}
bool SystemSM::recoveryAck() {
  if (st_ != SysState::RECOVERY) return false;
  go(SysState::SELF_TEST);
  return true;
}
bool SystemSM::serviceEnter() {
  if (st_ != SysState::RUN) return false;
  go(SysState::SERVICE);
  return true;
}
void SystemSM::serviceExit() {
  if (st_ == SysState::SERVICE) go(SysState::RUN);
}
bool SystemSM::otaBegin() {
  if (st_ != SysState::RUN) return false;
  go(SysState::OTA_PREP);
  return true;
}
void SystemSM::otaAbort() {
  if (st_ == SysState::OTA_PREP) go(SysState::RUN);
}
void SystemSM::otaFailed() {
  if (st_ == SysState::OTA) go(SysState::RUN);
}

bool SystemSM::step(const SysInput& in) {
  ev_service_timeout = ev_ota_timeout = false;
  const SysState before = st_;
  t_.add(in.dt_ms);
  svc_t_.add(in.dt_ms);
  switch (st_) {
    case SysState::RUN:
      if (in.safety_lockout) go(SysState::FAILSAFE);
      break;
    case SysState::SERVICE:
      if (in.safety_lockout) go(SysState::FAILSAFE);
      else if (svc_t_.atLeast(in.service_timeout_ms)) { go(SysState::RUN); ev_service_timeout = true; }
      break;
    case SysState::FAILSAFE:
      if (!in.safety_lockout) go(SysState::RUN);
      break;
    case SysState::OTA_PREP:
      if (in.safety_lockout) go(SysState::FAILSAFE);  // OTA iptal, güvenli yön
      else if (in.heating_idle) go(SysState::OTA);
      else if (t_.atLeast(in.ota_prep_timeout_ms)) { go(SysState::RUN); ev_ota_timeout = true; }
      break;
    default:
      break;  // BOOT/SELF_TEST/RECOVERY/OTA dış olaylarla ilerler
  }
  return st_ != before;
}

bool isRestartStorm(uint32_t fault_boots_in_window, int32_t limit) {
  return limit > 0 && fault_boots_in_window >= (uint32_t)limit;
}

HeatPhase HeatingChain::step(const HeatChainInput& in) {
  switch (ph_) {
    case HeatPhase::IDLE:
      if (in.lockout) ph_ = HeatPhase::LOCKOUT;
      else if (in.chain_request) ph_ = in.any_r ? HeatPhase::ACTIVE : HeatPhase::PREPURGE;
      else if (in.post_cool) ph_ = HeatPhase::POST_COOL;  // boot post-cool
      break;
    case HeatPhase::PREPURGE:
      if (in.lockout) ph_ = HeatPhase::LOCKOUT;
      else if (!in.chain_request) ph_ = in.post_cool ? HeatPhase::POST_COOL : HeatPhase::IDLE;
      else if (!in.prepurge) ph_ = HeatPhase::ACTIVE;
      break;
    case HeatPhase::ACTIVE:
      if (in.lockout) ph_ = HeatPhase::LOCKOUT;  // R önce kapanır (interlock I-1)
      else if (!in.chain_request && !in.any_r) ph_ = in.post_cool ? HeatPhase::POST_COOL : HeatPhase::IDLE;
      break;
    case HeatPhase::POST_COOL:
      if (in.lockout) ph_ = HeatPhase::LOCKOUT;
      else if (in.chain_request) ph_ = HeatPhase::ACTIVE;  // fan zaten açık, prepurge yok
      else if (!in.post_cool) ph_ = HeatPhase::IDLE;
      break;
    case HeatPhase::LOCKOUT:
      if (!in.lockout && !in.post_cool && !in.any_r) ph_ = HeatPhase::IDLE;
      break;
  }
  return ph_;
}

CtrlState deriveControllerState(SysState sys, HeatPhase heat, VentState vent, OpMode mode) {
  switch (sys) {
    case SysState::BOOT: return CtrlState::BOOT;
    case SysState::SELF_TEST: return CtrlState::SELF_TEST;
    case SysState::RECOVERY: return CtrlState::RECOVERY;
    case SysState::OTA_PREP:
    case SysState::OTA: return CtrlState::OTA;
    case SysState::FAILSAFE: return CtrlState::FAILSAFE;
    case SysState::SERVICE: return CtrlState::SERVICE;
    case SysState::RUN: break;
  }
  if (heat == HeatPhase::PREPURGE || heat == HeatPhase::ACTIVE) return CtrlState::HEATING;
  if (heat == HeatPhase::POST_COOL) return CtrlState::POST_COOL;
  if (vent == VentState::AUTO || vent == VentState::MANUAL || vent == VentState::FORCED) return CtrlState::VENTILATING;
  if (mode == OpMode::MANUAL) return CtrlState::MANUAL;
  if (mode == OpMode::OFF) return CtrlState::OFF;
  return CtrlState::IDLE;
}

}  // namespace cc
