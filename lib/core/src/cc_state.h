// Durum makineleri — STATE_MACHINE.md §1 (sistem), §2 (ısıtma zinciri), §5 (controller_state).
// Havalandırma durumu Ventilation::reportState ile türetilir (§3).
#pragma once
#include "cc_types.h"

namespace cc {

// ---------------- Sistem (ana) makine ----------------
struct SysInput {
  uint32_t dt_ms = 100;
  bool safety_lockout = false;    // Safety trip (FAILSAFE nedeni var)
  FailsafeReason reason = FailsafeReason::NONE;
  bool heating_idle = true;       // ısıtma zinciri IDLE (talep 0 + post-cool bitti)
  uint32_t service_timeout_ms = 30u * 60000u;
  uint32_t ota_prep_timeout_ms = 5u * 60000u;
};

class SystemSM {
 public:
  SysState state() const { return st_; }
  SysState previous() const { return prev_; }
  // Boot akışı
  void bootDone(bool restart_storm);                 // BOOT → SELF_TEST / RECOVERY
  void selfTestResult(bool pass);                    // SELF_TEST → RUN / FAILSAFE
  bool recoveryAck();                                // RECOVERY → SELF_TEST (yalnız yerel)
  // Servis
  bool serviceEnter();                               // RUN → SERVICE (ısıtma boştaysa)
  void serviceExit();                                // SERVICE → RUN
  void serviceActivity() { svc_t_.reset(); }
  // OTA
  bool otaBegin();                                   // RUN → OTA_PREP
  void otaAbort();                                   // OTA_PREP → RUN
  void otaFailed();                                  // OTA → RUN (imaj reddedildi)
  bool step(const SysInput& in);                     // true: durum değişti
  uint32_t serviceRemainingMs(uint32_t timeout_ms) const {
    return st_ == SysState::SERVICE && svc_t_.ms < timeout_ms ? timeout_ms - svc_t_.ms : 0;
  }
  bool ev_service_timeout = false, ev_ota_timeout = false;

 private:
  void go(SysState s) { prev_ = st_; st_ = s; t_.reset(); svc_t_.reset(); }
  SysState st_ = SysState::BOOT, prev_ = SysState::BOOT;
  Timer t_, svc_t_;
};

// Restart fırtınası (S14): pencere içindeki hatalı boot sayısı
bool isRestartStorm(uint32_t fault_boots_in_window, int32_t limit);

// ---------------- Isıtma zinciri ----------------
struct HeatChainInput {
  bool lockout = false;           // Safety kilidi / ARM=0
  bool chain_request = false;     // talep > 0 ∧ izin
  bool any_r = false;             // R effective
  bool prepurge = false;          // interlock: fan prestart sürüyor
  bool post_cool = false;         // interlock: post-cool sürüyor
};

class HeatingChain {
 public:
  HeatPhase step(const HeatChainInput& in);
  HeatPhase phase() const { return ph_; }
  bool heating() const { return ph_ == HeatPhase::PREPURGE || ph_ == HeatPhase::ACTIVE; }

 private:
  HeatPhase ph_ = HeatPhase::IDLE;
};

// ---------------- controller_state (§5) ----------------
CtrlState deriveControllerState(SysState sys, HeatPhase heat, VentState vent, OpMode mode);

}  // namespace cc
