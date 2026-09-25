#include "cc_core.h"
#include <cmath>
#include <cstring>

namespace cc {

namespace {
bool usable(Quality q) { return q == Quality::GOOD || q == Quality::UNCERTAIN; }
PidParams pidParamsFrom(const Config& c) {
  PidParams p;
  p.mode = c.pid_mode;
  p.kp = c.pid_kp;
  p.ki = c.pid_ki;
  p.kd = c.pid_kd;
  p.b = c.pid_setpoint_weight;
  p.deadband = c.pid_deadband;
  p.onoff_hyst = c.onoff_hysteresis;
  return p;
}
DemandParams demandParamsFrom(const Config& c) {
  DemandParams d;
  d.min_demand = c.min_heat_demand;
  d.slew_pct_per_min = c.demand_slew_pct_per_min;
  d.max_demand = c.max_heat_demand;
  return d;
}
bool parseOnOff(const char* p, bool& v) {
  if (!p) return false;
  if (std::strcmp(p, "ON") == 0) { v = true; return true; }
  if (std::strcmp(p, "OFF") == 0) { v = false; return true; }
  return false;
}
bool isLocal(CmdSource s) {
  return s == CmdSource::LOCAL_WEB || s == CmdSource::LOCAL_SERVICE || s == CmdSource::SYSTEM;
}
}  // namespace

ClimateCore::ClimateCore() { commitConfig(Config(), false); }

void ClimateCore::commitConfig(const Config& c, bool bumpless) {
  const float sp = prof_.last().effective;
  const float pv = t1f_.reading().value;
  cfg_ = c;
  if (bumpless) pid_.setParams(pidParamsFrom(c), sp, pv);
  else pid_ = Pid(pidParamsFrom(c));
  dem_.setParams(demandParamsFrom(c));
  pm_.setParams(PowerParams::fromConfig(c));
  il_.setParams(InterlockParams::fromConfig(c));
  saf_.setParams(SafetyParams::fromConfig(c));  // Safety kendi kopyasını yalnız doğrulanmış commit'te alır
  hpm_.setParams(HpmParams::fromConfig(c.output_driver_r == DriverKind::RELAY, c.max_continuous_heating_min));
  const uint32_t iv = sToMs((float)c.sensor_interval_s), st = sToMs((float)c.sensor_stale_s);
  t1f_.setParams(RoleParams::t1(c.t1_offset, iv, st, (float)c.sensor_filter_tau_s, sToMs((float)c.sensor_stuck_s)));
  rhf_.setParams(RoleParams::rh1(c.rh1_offset, iv, st, (float)c.sensor_filter_tau_s, sToMs((float)c.sensor_stuck_s)));
  t2f_.setParams(RoleParams::t2(c.t2_offset, iv, st));
  t2f_.setDisabled(!c.t2_enabled);
}

void ClimateCore::begin(const Config& cfg, const BootInfo& boot) {
  ValidationResult vr = validate(cfg);
  config_error_ = boot.config_error || !vr.ok();
  commitConfig(vr.ok() ? cfg : Config(), false);  // geçersizse güvenli varsayılan + ısıtma kilidi
  if (boot.restore_alarms) alm_.importPersist(boot.alarms, false);
  saf_.restoreLatched(boot.safety_latched);
  log(Severity::INFO, EvSrc::SYSTEM, EvCode::DEVICE_BOOT, boot.fault_reset ? 1.0f : 0.0f);
  if (boot.fault_reset) alm_.setCondition(AlarmId::WATCHDOG_RESET, true);
  if (boot.fault_reset && boot.heater_was_on) {
    il_.startBootPostCool();
    log(Severity::INFO, EvSrc::OUTPUT, EvCode::BOOT_POST_COOL);
  }
  const bool storm = isRestartStorm(boot.fault_boots_in_window, cfg_.restart_storm_limit);
  sys_.bootDone(storm);
  if (storm) alm_.setCondition(AlarmId::RESTART_STORM, true);
  selftest_t_.reset();
  updateSnapshot();
}

void ClimateCore::log(Severity s, EvSrc src, EvCode code, float val, CmdSource actor, uint16_t aux) {
  Event e;
  e.up_s = up_ms_ / 1000u;
  e.sev = s;
  e.src = src;
  e.code = code;
  e.actor = actor;
  e.val = val;
  e.aux = aux;
  ev_.push(e);
}

// ---------------- Sensör ----------------
void ClimateCore::feedT1(DrvStatus st, float raw) {
  t1f_.sample(st, raw, 0, chain_.heating());
  sens_hb_.reset();
}
void ClimateCore::feedRh(DrvStatus st, float raw) { rhf_.sample(st, raw, 0, chain_.heating()); }
void ClimateCore::feedT2(DrvStatus st, float raw) { t2f_.sample(st, raw, 0, chain_.heating()); }

// ---------------- Zaman ----------------
void ClimateCore::tick(uint32_t dt) {
  up_ms_ += dt;
  t1f_.tick(dt);
  rhf_.tick(dt);
  t2f_.tick(dt);
  ctrl_hb_.add(dt);
  out_hb_.add(dt);
  sens_hb_.add(dt);

  // Yerel kilit süresi
  if (local_lock_) {
    local_lock_t_.add(dt);
    if (local_lock_t_.atLeast(local_lock_ms_)) local_lock_ = false;
  }
  // Lider rotasyonu: saat geçerliyse gün devri, değilse her 24 sa çalışma
  rot_t_.add(dt);
  if (cfg_.lead_rotation == LeadRotation::DAILY) {
    if ((time_valid_ && day_rollover_) || (!time_valid_ && rot_t_.atLeast(24u * 3600u * 1000u))) {
      rotate_request_ = true;
      rot_t_.reset();
    }
  }
  // SELF_TEST
  if (sys_.state() == SysState::SELF_TEST) {
    selftest_t_.add(dt);
    runSelfTest();
  }

  acc_saf_ += dt;
  while (acc_saf_ >= kSafetyPeriodMs) { acc_saf_ -= kSafetyPeriodMs; safetyStep(kSafetyPeriodMs); }
  const uint32_t ci = sToMs((float)cfg_.control_interval_s);
  acc_ctrl_ += dt;
  while (acc_ctrl_ >= ci) {
    acc_ctrl_ -= ci;
    if (!freeze_control_) controlStep(ci);
  }
  acc_out_ += dt;
  while (acc_out_ >= kOutputPeriodMs) {
    acc_out_ -= kOutputPeriodMs;
    if (!freeze_output_) outputStep(kOutputPeriodMs);
  }
  updateSnapshot();
}

void ClimateCore::runSelfTest() {
  const bool sensorOk = t1f_.reading().quality == Quality::GOOD;
  if (sensorOk && !config_error_) {
    sys_.selfTestResult(true);
    if (alm_.rec(AlarmId::INTERNAL_FAULT).st != AlarmState::NORMAL) {
      alm_.setCondition(AlarmId::INTERNAL_FAULT, false);
      alm_.reset(AlarmId::INTERNAL_FAULT);  // başarılı self-test INTERNAL kilidini kaldırır
    }
  } else if (selftest_t_.atLeast(kSelfTestTimeoutMs) || (sensorOk && config_error_)) {
    sys_.selfTestResult(false);
  }
}

bool ClimateCore::heatingPermittedBase() const {
  return sys_.state() == SysState::RUN && controller_enable_ && !saf_.last().lockout &&
         usable(t1f_.reading().quality) && isValid(t1f_.reading().value);
}

void ClimateCore::controlStep(uint32_t dt) {
  ctrl_hb_.reset();
  const RoleReading& t1 = t1f_.reading();
  const RoleReading& rh = rhf_.reading();
  const SysState s = sys_.state();
  const OpMode mode = cfg_.operating_mode;

  // Profil / setpoint / antifreeze
  ProfileInput pin;
  pin.mode = mode;
  pin.service = s == SysState::SERVICE;
  pin.controller_enable = controller_enable_;
  pin.local_lock = local_lock_;
  pin.t1 = t1.value;
  pin.t1_quality = t1.quality;
  pin.dt_ms = dt;
  const ProfileOutput po = prof_.step(cfg_, pin);
  if (po.ev_boost_ended) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::BOOST_END);
  if (po.ev_sched_night_expired) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::SCHEDULE_REQUEST_EXPIRED, 0);
  if (po.ev_sched_away_expired) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::SCHEDULE_REQUEST_EXPIRED, 1);
  if (po.ev_antifreeze_on) log(Severity::WARNING, EvSrc::CONTROLLER, EvCode::ANTIFREEZE_ON, t1.value);
  if (po.ev_antifreeze_off) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::ANTIFREEZE_OFF, t1.value);
  antifreeze_ = po.antifreeze;

  // MANUAL zaman aşımı → AUTO (bumpless)
  if (mode == OpMode::MANUAL) {
    manual_t_.add(dt);
    if (cfg_.manual_timeout_h > 0 && manual_t_.atLeast((uint32_t)cfg_.manual_timeout_h * 3600000u)) {
      setMode(OpMode::AUTO, CmdSource::SYSTEM);
      log(Severity::INFO, EvSrc::CONTROLLER, EvCode::MANUAL_TIMEOUT);
    }
  } else {
    manual_t_.reset();
  }

  // Talep kaynağı
  const bool base = heatingPermittedBase();
  DemandSource src = DemandSource::NONE;
  HeatingReason hr = HeatingReason::NONE;
  if (base) {
    if (cfg_.operating_mode == OpMode::AUTO) {
      src = DemandSource::PID;
      hr = po.source == SetpointSource::ANTIFREEZE ? HeatingReason::ANTIFREEZE
           : po.active == ProfileActive::BOOST     ? HeatingReason::BOOST
                                                   : HeatingReason::PID;
    } else if (cfg_.operating_mode == OpMode::MANUAL) {
      src = DemandSource::MANUAL;
      hr = HeatingReason::MANUAL;
    } else if (antifreeze_) {
      src = DemandSource::PID;  // OFF / VENT_ONLY: antifreeze bekçisi
      hr = HeatingReason::ANTIFREEZE;
    }
  }

  // PID
  const bool locked = saf_.last().lockout || s != SysState::RUN;
  if (locked) was_locked_ = true;
  else if (was_locked_) {
    pid_.resetAfterFault();  // arıza sonrası temkinli başlangıç: I = 0, slew ile yükselir
    dem_.reset();
    was_locked_ = false;
  }
  const VentOutput& vprev = vent_.last();
  const bool startBlocked = vprev.heat_inhibit && !chain_.heating();
  const bool manualAf = src == DemandSource::MANUAL && antifreeze_;
  PidInput pi;
  pi.sp = po.effective;
  pi.pv = t1.value;
  pi.dt_s = dt / 1000.0f;
  pi.hold = il_.last().prepurge || startBlocked;
  pi.tracking = locked || src == DemandSource::NONE || (src == DemandSource::MANUAL && !manualAf);
  pi.track_value = src == DemandSource::MANUAL ? cfg_.manual_heat_demand : 0.0f;
  pi.applied = (!pi.tracking && chain_.heating()) ? pm_.last().applied : kNaN;
  float outMax = cfg_.max_heat_demand;
  if (isValid(vprev.heat_cap) && vprev.heat_cap < outMax) outMax = vprev.heat_cap;
  pi.out_max = outMax;
  const PidOutput pid = pid_.step(pi);

  // Talep koşullandırma
  DemandInput di;
  di.permitted = base && src != DemandSource::NONE && !startBlocked;
  di.source = src;
  di.pid_output = pid.output;
  di.manual_demand = cfg_.manual_heat_demand;
  di.antifreeze_demand = manualAf ? pid.output : kNaN;
  di.cap = vprev.heat_cap;
  di.dt_s = dt / 1000.0f;
  const DemandOutput dout = dem_.step(di);
  heat_demand_ = dout.heat_demand;
  chain_request_ = heat_demand_ > 0;
  if (manualAf && isValid(di.antifreeze_demand) && di.antifreeze_demand > cfg_.manual_heat_demand)
    hr = HeatingReason::ANTIFREEZE;
  heating_reason_ = chain_request_ ? hr : HeatingReason::NONE;

  // Havalandırma
  const SafetyOutput& so = saf_.last();
  VentInput vi;
  vi.dt_ms = dt;
  vi.mode = mode;
  vi.controller_enable = controller_enable_;
  vi.service = s == SysState::SERVICE;
  vi.ota = s == SysState::OTA_PREP || s == SysState::OTA;
  vi.t1 = t1.value;
  vi.t1_q = t1.quality;
  vi.rh = rh.value;
  vi.rh_q = rh.quality;
  vi.sp_effective = po.effective;
  vi.heating_active = chain_.heating();
  vi.heat_request = dout.target > 0 || (src != DemandSource::NONE && pid.output > 0 && base);
  vi.antifreeze = antifreeze_;
  vi.overtemp = so.vf_force;
  vi.sensor_fault = so.lockout && (so.reason == FailsafeReason::SENSOR_FAULT || so.reason == FailsafeReason::CONFIG_ERROR ||
                                   so.reason == FailsafeReason::INTERNAL_FAULT);
  vi.uptime_s = up_ms_ / 1000u;
  vi.vf_effective = il_.last().eff[VF];
  const VentOutput vo = vent_.step(cfg_, vi);
  if (vo.ev_manual_timeout) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::MANUAL_VENT_TIMEOUT);

  // HPM
  HpmInput hi;
  hi.dt_ms = dt;
  hi.t1 = t1.quality == Quality::GOOD ? t1.value : kNaN;
  hi.sp = po.effective;
  hi.demand = heat_demand_;
  hi.heating = chain_.heating();
  hi.stage = pm_.stage();
  hi.day_rollover = day_rollover_;
  hi.time_valid = time_valid_;
  hpm_.step(hi);
  day_rollover_ = false;
}

void ClimateCore::outputStep(uint32_t dt) {
  out_hb_.reset();
  const SysState s = sys_.state();
  const bool service = s == SysState::SERVICE;
  const SafetyOutput& so = saf_.last();

  // Servis testi süre sınırı
  for (uint8_t k = 0; k < OUT_COUNT; ++k) {
    if (!svc_test_[k]) continue;
    svc_test_t_[k].add(dt);
    if (!service || svc_test_t_[k].atLeast(sToMs((float)cfg_.service_test_max_s))) {
      svc_test_[k] = false;
      log(Severity::INFO, EvSrc::SERVICE, EvCode::SERVICE_TEST, (float)k, CmdSource::SYSTEM, 0);
    }
  }

  // PowerManager
  PowerInput pin;
  pin.demand = heat_demand_;
  pin.enabled = chain_request_ && s == SysState::RUN;
  pin.rotate = rotate_request_;
  pin.dt_ms = dt;
  const PowerOutput po = pm_.step(pin);
  if (po.ev_rotated) {
    rotate_request_ = false;
    log(Severity::INFO, EvSrc::CONTROLLER, EvCode::LEAD_ROTATED, (float)po.lead);
  }
  if (po.ev_stage_up && po.stage == 2) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::STAGE2_ON, heat_demand_);
  if (po.ev_stage_down && po.stage == 1) log(Severity::INFO, EvSrc::CONTROLLER, EvCode::STAGE2_OFF, heat_demand_);

  // Interlock girdileri
  const bool runOrSvc = s == SysState::RUN || s == SysState::SERVICE;
  arm_ = runOrSvc && !so.lockout && so.arm_allowed;
  InterlockInput ii;
  ii.dt_ms = dt;
  ii.heat_chain = service ? (svc_test_[R1] || svc_test_[R2]) : (chain_request_ && s == SysState::RUN);
  ii.r_req[0] = service ? svc_test_[R1] : po.req[0];
  ii.r_req[1] = service ? svc_test_[R2] : po.req[1];
  const bool ota = s == SysState::OTA_PREP || s == SysState::OTA;
  ii.hf_req = service ? svc_test_[HF] : (ota ? false : heater_fan_manual_);
  const VentOutput& vo = vent_.last();
  ii.vf_req = service ? svc_test_[VF] : vo.vf_request;
  ii.vf_inhibit = service ? Reason::NONE : vo.inhibit;
  ii.heater_lockout = so.lockout || !runOrSvc;
  ii.arm = arm_;
  ii.lockout_reason = so.lockout ? so.lockout_reason : (ota ? Reason::OTA : Reason::SAFETY_LOCKOUT);
  ii.vf_force_on = so.vf_force;
  ii.hf_force_on = so.hf_force;
  ii.antifreeze = antifreeze_ && !service;
  ii.t2 = t2f_.reading().value;
  ii.t2_q = t2f_.reading().quality;
  const InterlockOutput io = il_.step(ii);

  // OutputManager ikinci kontrol + sıralı uygulama
  const OutputGuard::Result g = guard_.apply(io.eff, dt);
  if (g.violation) guard_violation_ = true;

  if (io.ev_post_cool_start) log(Severity::INFO, EvSrc::STATE, EvCode::POST_COOL_START, (float)cfg_.post_cool_seconds);
  if (io.ev_post_cool_end) log(Severity::INFO, EvSrc::OUTPUT, EvCode::POST_COOL_END);
  for (uint8_t k = HF; k <= VF; ++k) {
    if (io.ev_on[k]) log(Severity::INFO, EvSrc::OUTPUT, EvCode::OUTPUT_ON, (float)k, CmdSource::CONTROLLER, (uint16_t)io.reason[k]);
    if (io.ev_off[k]) log(Severity::INFO, EvSrc::OUTPUT, EvCode::OUTPUT_OFF, (float)k, CmdSource::CONTROLLER, (uint16_t)io.reason[k]);
  }

  // Isıtma zinciri
  HeatChainInput hc;
  hc.lockout = ii.heater_lockout;
  hc.chain_request = ii.heat_chain;
  hc.any_r = io.eff[R1] || io.eff[R2];
  hc.prepurge = io.prepurge;
  hc.post_cool = io.post_cool;
  const HeatPhase ph = chain_.step(hc);
  if ((ph == HeatPhase::PREPURGE || ph == HeatPhase::ACTIVE) &&
      !(last_phase_ == HeatPhase::PREPURGE || last_phase_ == HeatPhase::ACTIVE))
    log(Severity::INFO, EvSrc::STATE, EvCode::HEATING_START, t1f_.reading().value);
  last_phase_ = ph;

  // Sistem makinesi
  SysInput si;
  si.dt_ms = dt;
  si.safety_lockout = so.lockout;
  si.reason = so.reason;
  si.heating_idle = !chain_request_ && !io.post_cool && !io.eff[R1] && !io.eff[R2];
  si.service_timeout_ms = minToMs((float)cfg_.service_timeout_min);
  if (sys_.step(si)) {
    if (sys_.ev_service_timeout) log(Severity::INFO, EvSrc::SERVICE, EvCode::SERVICE_TIMEOUT);
    if (sys_.ev_ota_timeout) log(Severity::WARNING, EvSrc::SYSTEM, EvCode::OTA_TIMEOUT);
    if (sys_.previous() == SysState::SERVICE)
      for (uint8_t k = 0; k < OUT_COUNT; ++k) svc_test_[k] = false;
  }
  if (sys_.state() != last_sys_) {
    log(sys_.state() == SysState::FAILSAFE ? Severity::CRITICAL : Severity::INFO, EvSrc::STATE, EvCode::STATE_CHANGE,
        (float)(uint8_t)sys_.state());
    last_sys_ = sys_.state();
  }
}

void ClimateCore::safetyStep(uint32_t dt) {
  SafetyInput si;
  si.dt_ms = dt;
  si.t1 = t1f_.reading().safety;
  si.t1_q = t1f_.reading().quality;
  si.t2 = t2f_.reading().safety;
  si.t2_q = t2f_.reading().quality;
  si.any_r_on = guard_.applied()[R1] || guard_.applied()[R2];
  si.hf_on = guard_.applied()[HF];
  si.heating_active = chain_.heating();
  si.heat_demand = heat_demand_;
  si.max_demand = cfg_.max_heat_demand;
  si.config_error = config_error_;
  si.control_hb_age_ms = ctrl_hb_.ms;
  si.sensor_hb_age_ms = sens_hb_.ms;
  si.output_hb_age_ms = out_hb_.ms;
  si.guard_violation = guard_violation_;
  si.reset_request = reset_request_;
  reset_request_ = false;
  const SafetyOutput so = saf_.step(si);
  if (so.ev_trip) log(Severity::CRITICAL, EvSrc::SAFETY, EvCode::SAFETY_TRIP, (float)(uint8_t)so.reason);
  if (so.reset_done) log(Severity::WARNING, EvSrc::SAFETY, EvCode::SAFETY_RESET, (float)so.latched);
  if (so.reason != last_fs_) last_fs_ = so.reason;

  // Alarm koşulları
  const RoleReading& rh = rhf_.reading();
  const RoleReading& t2 = t2f_.reading();
  const bool rhBad = rh.quality == Quality::BAD || rh.quality == Quality::MISSING || rh.quality == Quality::STALE;
  const bool t2Bad = cfg_.t2_enabled && (t2.quality == Quality::BAD || t2.quality == Quality::MISSING ||
                                         t2.quality == Quality::STALE);
  const bool sensorAny = so.sensor_fault || rhBad || t2Bad || t1f_.reading().intermittent;
  alm_.setCondition(AlarmId::SENSOR_FAULT, sensorAny, so.sensor_fault ? Severity::CRITICAL : Severity::WARNING);
  alm_.setCondition(AlarmId::SENSOR_STALE, so.sensor_stale);
  alm_.setCondition(AlarmId::OVERTEMPERATURE, so.overtemp_condition || so.vf_force || so.hf_force);
  alm_.setCondition(AlarmId::HEATER_FAN_FAULT, (so.latched & SL_FAN_FAULT) != 0);
  alm_.setCondition(AlarmId::HEATING_TIMEOUT, (so.latched & SL_HEATING_TIMEOUT) != 0);
  const bool riseCrit = (so.latched & SL_TEMP_RISE) != 0;
  alm_.setCondition(AlarmId::UNEXPECTED_TEMPERATURE_RISE, riseCrit || so.rise_warning,
                    riseCrit ? Severity::CRITICAL : Severity::WARNING);
  alm_.setCondition(AlarmId::OUTPUT_FAULT, (so.latched & SL_OUTPUT) != 0);
  alm_.setCondition(AlarmId::INTERNAL_FAULT, (so.latched & SL_INTERNAL) != 0);
  alm_.setCondition(AlarmId::CONFIGURATION_ERROR, config_error_);
  alm_.setCondition(AlarmId::FROST_RISK_NO_SENSOR, so.frost_risk);
  alm_.setCondition(AlarmId::RESTART_STORM, sys_.state() == SysState::RECOVERY);
  alm_.setCondition(AlarmId::HEATING_PERFORMANCE_LOW, hpm_.last().performance_low);
  alm_.setCondition(AlarmId::HUMIDITY_HIGH, vent_.last().humidity_high);
  alm_.setCondition(AlarmId::HUMIDITY_LOW, rh.quality == Quality::GOOD && rh.value < cfg_.humidity_low_limit);
  alm_.setCondition(AlarmId::TIME_INVALID, !time_valid_ && cfg_.time_source != TimeSource::NONE);
  const bool relayLife = cfg_.output_driver_r == DriverKind::RELAY &&
                         (guard_.switchCount(R1) >= (uint32_t)(0.8 * cfg_.relay_life_cycles) ||
                          guard_.switchCount(R2) >= (uint32_t)(0.8 * cfg_.relay_life_cycles));
  alm_.setCondition(AlarmId::RELAY_LIFE_WARNING, relayLife);
  alm_.setCondition(AlarmId::POST_COOL_TIMEOUT, il_.last().post_cool_timeout);
  alm_.setCondition(AlarmId::MQTT_OFFLINE, mqtt_alarm_);
  alm_.setCondition(AlarmId::WIFI_OFFLINE, wifi_alarm_);
  alm_.step(dt, sys_.state() == SysState::SERVICE);
  alm_.setCondition(AlarmId::WATCHDOG_RESET, false);  // anlık koşul: onayla kapanır
  AlarmEvent ae;
  while (alm_.popEvent(ae)) {
    const Severity sev = ae.to == AlarmState::ACTIVE_UNACK ? ae.sev : Severity::INFO;
    log(sev, EvSrc::ALARM, EvCode::ALARM_TRANSITION, (float)(uint8_t)ae.id, CmdSource::SYSTEM,
        (uint16_t)((uint8_t)ae.to | (ae.suppressed ? 0x100 : 0)));
  }
}

// ---------------- Komutlar ----------------
CmdReply ClimateCore::reply(CmdResult r, Reason rs, ValCode v) {
  CmdReply c;
  c.result = r;
  c.reason = rs;
  c.code = v;
  return c;
}

CmdReply ClimateCore::cmdLog(const CmdReply& r, CmdSource src, float val) {
  const bool ok = r.result == CmdResult::ACCEPTED || r.result == CmdResult::OVERRIDDEN;
  if (ok) last_src_ = src;
  log(ok ? Severity::INFO : Severity::WARNING, EvSrc::COMMAND, ok ? EvCode::COMMAND : EvCode::COMMAND_REJECTED, val,
      src, (uint16_t)r.result);
  return r;
}

CmdReply ClimateCore::setMode(OpMode m, CmdSource src) {
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, (float)(uint8_t)m);
  if (m == OpMode::MANUAL && src == CmdSource::MQTT && !cfg_.remote_manual_allowed)
    return cmdLog(reply(CmdResult::REJECTED_POLICY), src, (float)(uint8_t)m);
  if (m == OpMode::MANUAL && sys_.state() == SysState::FAILSAFE)
    return cmdLog(reply(CmdResult::REJECTED_STATE), src, (float)(uint8_t)m);
  const OpMode old = cfg_.operating_mode;
  if (old == OpMode::AUTO && m == OpMode::MANUAL) {
    // Bumpless: manuel talep son heat_demand ile başlar (adım 5'e yuvarlanır)
    cfg_.manual_heat_demand = clampf(std::round(heat_demand_ / 5.0f) * 5.0f, 0.0f, 100.0f);
  } else if (old == OpMode::MANUAL && m == OpMode::AUTO) {
    pid_.bumplessTo(heat_demand_, prof_.last().effective, t1f_.reading().value);
  }
  cfg_.operating_mode = m;
  manual_t_.reset();
  return cmdLog(reply(CmdResult::ACCEPTED), src, (float)(uint8_t)m);
}

CmdReply ClimateCore::setControllerEnable(bool on, CmdSource src) {
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, on);
  if (!on && !isLocal(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY), src, 0);  // D-18
  controller_enable_ = on;
  return cmdLog(reply(CmdResult::ACCEPTED), src, on);
}

CmdReply ClimateCore::setHeaterFanManual(bool on, CmdSource src) {
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, on);
  heater_fan_manual_ = on;
  const InterlockOutput& io = il_.last();
  if (!on && io.eff[HF]) {
    Reason r = (io.eff[R1] || io.eff[R2]) ? Reason::HEATER_INTERLOCK
               : io.post_cool             ? (io.boot_post_cool ? Reason::BOOT_POST_COOL : Reason::POST_COOL)
               : io.prepurge              ? Reason::PREPURGE
               : chain_request_           ? Reason::HEATER_INTERLOCK
               : saf_.last().hf_force     ? Reason::OVERTEMPERATURE
                                          : Reason::MIN_ON_TIME;
    return cmdLog(reply(CmdResult::OVERRIDDEN, r), src, 0);
  }
  return cmdLog(reply(CmdResult::ACCEPTED), src, on);
}

CmdReply ClimateCore::setVentFanManual(bool on, CmdSource src) {
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, on);
  vent_.setManual(on);
  Reason r = Reason::NONE;
  if (on) {
    if (antifreeze_) r = Reason::ANTIFREEZE_INHIBIT;
    else if (cfg_.operating_mode == OpMode::OFF) r = Reason::MODE_OFF;
    else if (chain_.heating() && cfg_.manual_vent_priority == ManualVentPriority::HEAT_WINS) r = Reason::HEATING_PRIORITY;
  } else if (saf_.last().vf_force) {
    r = Reason::OVERTEMPERATURE;
  }
  return cmdLog(reply(r == Reason::NONE ? CmdResult::ACCEPTED : CmdResult::OVERRIDDEN, r), src, on);
}

CmdReply ClimateCore::setBoost(bool on, CmdSource src) {
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, on);
  prof_.setBoost(on, cfg_);
  return cmdLog(reply(CmdResult::ACCEPTED), src, on);
}

CmdReply ClimateCore::setSchedNight(bool on, CmdSource src) {
  prof_.setSchedNight(on);  // yerel kilitte de kaydedilir, çözüme alınmaz
  return cmdLog(reply(mqttLocked(src) ? CmdResult::OVERRIDDEN : CmdResult::ACCEPTED,
                      mqttLocked(src) ? Reason::LOCAL_LOCK : Reason::NONE),
                src, on);
}

CmdReply ClimateCore::setSchedAway(bool on, CmdSource src) {
  prof_.setSchedAway(on);
  return cmdLog(reply(mqttLocked(src) ? CmdResult::OVERRIDDEN : CmdResult::ACCEPTED,
                      mqttLocked(src) ? Reason::LOCAL_LOCK : Reason::NONE),
                src, on);
}

CmdReply ClimateCore::alarmAck(CmdSource src) {
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, 0);
  alm_.ackAll();
  ++ack_count_;
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

CmdReply ClimateCore::alarmReset(CmdSource src) {
  if (!isLocal(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY), src, 0);  // MQTT: yalnız servis kanalı (F5)
  const uint8_t before = saf_.latched();
  reset_request_ = true;
  safetyStep(0);
  const uint8_t after = saf_.latched();
  // Kilidi kalkan alarmların koşulu temizlenir ve alarm sıfırlanır
  struct M { AlarmId id; uint8_t bit; };
  const M map[] = {{AlarmId::OVERTEMPERATURE, SL_OVERTEMP}, {AlarmId::HEATER_FAN_FAULT, SL_FAN_FAULT},
                   {AlarmId::HEATING_TIMEOUT, SL_HEATING_TIMEOUT}, {AlarmId::UNEXPECTED_TEMPERATURE_RISE, SL_TEMP_RISE},
                   {AlarmId::OUTPUT_FAULT, SL_OUTPUT}};
  for (const M& m : map) {
    if (!(after & m.bit)) alm_.reset(m.id);
  }
  if (before == after && (after & (uint8_t)~SL_INTERNAL)) {
    log(Severity::WARNING, EvSrc::SAFETY, EvCode::SAFETY_RESET_REFUSED, (float)after, src);
    return cmdLog(reply(CmdResult::REJECTED_STATE), src, (float)after);
  }
  return cmdLog(reply(CmdResult::ACCEPTED), src, (float)after);
}

CmdReply ClimateCore::setLocalLock(uint32_t minutes, CmdSource src) {
  if (src != CmdSource::LOCAL_WEB && src != CmdSource::LOCAL_SERVICE) return cmdLog(reply(CmdResult::REJECTED_POLICY), src, (float)minutes);
  if (minutes != 0 && (minutes < 15 || minutes > 1440)) return cmdLog(reply(CmdResult::REJECTED_INVALID), src, (float)minutes);
  local_lock_ = minutes > 0;
  local_lock_ms_ = minutes * 60000u;
  local_lock_t_.reset();
  log(Severity::INFO, EvSrc::COMMAND, EvCode::LOCAL_LOCK, (float)minutes, src);
  return cmdLog(reply(CmdResult::ACCEPTED), src, (float)minutes);
}

CmdReply ClimateCore::applyConfig(const Config& cand, CmdSource src) {
  if (!isLocal(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY), src, 0);
  const ValidationResult vr = validate(cand);
  if (!vr.ok()) {
    const CmdResult r = vr.errors[0].rule == 0 ? CmdResult::REJECTED_INVALID : CmdResult::REJECTED_RELATION;
    return cmdLog(reply(r, Reason::NONE, vr.errors[0].code), src, (float)vr.errors[0].rule);
  }
  commitConfig(cand, true);
  config_error_ = false;
  log(Severity::WARNING, EvSrc::CONFIG, EvCode::CONFIG_CHANGE, 0, src);
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

CmdReply ClimateCore::serviceEnter(CmdSource src) {
  if (src != CmdSource::LOCAL_SERVICE) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::SERVICE_ONLY), src, 0);
  if (sys_.state() != SysState::RUN || chain_.phase() != HeatPhase::IDLE || chain_request_ || il_.last().post_cool)
    return cmdLog(reply(CmdResult::REJECTED_STATE), src, 0);
  if (!sys_.serviceEnter()) return cmdLog(reply(CmdResult::REJECTED_STATE), src, 0);
  log(Severity::WARNING, EvSrc::SERVICE, EvCode::SERVICE_ENTER, 0, src);
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

CmdReply ClimateCore::serviceExit(CmdSource src) {
  if (sys_.state() != SysState::SERVICE) return cmdLog(reply(CmdResult::REJECTED_STATE), src, 0);
  sys_.serviceExit();
  for (uint8_t k = 0; k < OUT_COUNT; ++k) svc_test_[k] = false;
  log(Severity::INFO, EvSrc::SERVICE, EvCode::SERVICE_EXIT, 0, src);
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

CmdReply ClimateCore::serviceTest(uint8_t out, bool on, CmdSource src) {
  if (src != CmdSource::LOCAL_SERVICE) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::SERVICE_ONLY), src, (float)out);
  if (sys_.state() != SysState::SERVICE || out >= OUT_COUNT)
    return cmdLog(reply(CmdResult::REJECTED_STATE, Reason::SERVICE_ONLY), src, (float)out);
  if (on && (out == R1 || out == R2) && svc_test_[out == R1 ? R2 : R1])
    return cmdLog(reply(CmdResult::REJECTED_STATE), src, (float)out);  // tek seferde tek rezistans
  svc_test_[out] = on;
  svc_test_t_[out].reset();
  sys_.serviceActivity();
  log(Severity::WARNING, EvSrc::SERVICE, EvCode::SERVICE_TEST, (float)out, src, on ? 1 : 0);
  if (on && (out == R1 || out == R2) && saf_.last().lockout)
    return cmdLog(reply(CmdResult::OVERRIDDEN, saf_.last().lockout_reason), src, (float)out);
  return cmdLog(reply(CmdResult::ACCEPTED), src, (float)out);
}

CmdReply ClimateCore::otaBegin(CmdSource src) {
  if (!isLocal(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY), src, 0);
  const RoleReading& t1 = t1f_.reading();
  if (isValid(t1.value) && t1.value < cfg_.frost_guard_temperature + 2.0f)
    return cmdLog(reply(CmdResult::REJECTED_STATE, Reason::ANTIFREEZE_INHIBIT), src, t1.value);  // donma riski
  if (!sys_.otaBegin()) return cmdLog(reply(CmdResult::REJECTED_STATE), src, 0);
  log(Severity::WARNING, EvSrc::SYSTEM, EvCode::OTA_PREP, 0, src);
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

CmdReply ClimateCore::recoveryAck(CmdSource src) {
  if (!isLocal(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY), src, 0);
  if (!sys_.recoveryAck()) return cmdLog(reply(CmdResult::REJECTED_STATE), src, 0);
  selftest_t_.reset();
  alm_.setCondition(AlarmId::RESTART_STORM, false);
  alm_.reset(AlarmId::RESTART_STORM);
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

CmdReply ClimateCore::command(const char* id, const char* payload, CmdSource src) {
  if (!id) return reply(CmdResult::REJECTED_INVALID, Reason::NONE, ValCode::UNKNOWN_FIELD);
  bool b = false;
  auto onoff = [&](CmdReply (ClimateCore::*fn)(bool, CmdSource)) -> CmdReply {
    if (!parseOnOff(payload, b)) return cmdLog(reply(CmdResult::REJECTED_INVALID, Reason::NONE, ValCode::INVALID_FORMAT), src, 0);
    return (this->*fn)(b, src);
  };
  if (!std::strcmp(id, "operating_mode")) {
    const FieldInfo* f = findField("operating_mode");
    for (uint8_t i = 0; payload && i < f->enumCount; ++i)
      if (!std::strcmp(payload, f->enumNames[i])) return setMode((OpMode)i, src);
    return cmdLog(reply(CmdResult::REJECTED_INVALID, Reason::NONE, ValCode::INVALID_FORMAT), src, 0);
  }
  if (!std::strcmp(id, "controller_enable")) return onoff(&ClimateCore::setControllerEnable);
  if (!std::strcmp(id, "heater_fan_manual")) return onoff(&ClimateCore::setHeaterFanManual);
  if (!std::strcmp(id, "ventilation_fan_manual")) return onoff(&ClimateCore::setVentFanManual);
  if (!std::strcmp(id, "boost")) return onoff(&ClimateCore::setBoost);
  if (!std::strcmp(id, "sched_night")) return onoff(&ClimateCore::setSchedNight);
  if (!std::strcmp(id, "sched_away")) return onoff(&ClimateCore::setSchedAway);
  if (!std::strcmp(id, "alarm_ack")) {
    if (!payload || std::strcmp(payload, "PRESS") != 0) return reply(CmdResult::REJECTED_INVALID);  // yok sayılır
    return alarmAck(src);
  }
  // Operasyonel sayısal / seçim değerleri ve konfigürasyon alanları
  if (mqttLocked(src)) return cmdLog(reply(CmdResult::REJECTED_POLICY, Reason::LOCAL_LOCK), src, 0);
  if (!std::strcmp(id, "manual_heat_demand") && src == CmdSource::MQTT && !cfg_.remote_manual_allowed)
    return cmdLog(reply(CmdResult::REJECTED_POLICY), src, 0);
  Config cand;
  const SetResult sr = setField(cfg_, id, payload, src, cand);
  if (sr.result != CmdResult::ACCEPTED) return cmdLog(reply(sr.result, Reason::NONE, sr.code), src, (float)sr.rule);
  const FieldInfo* f = findField(id);
  const bool oper = f && (f->flags & CF_OPER);
  commitConfig(cand, true);
  if (!oper) log((f && (f->flags & CF_SC)) ? Severity::WARNING : Severity::INFO, EvSrc::CONFIG, EvCode::CONFIG_CHANGE, 0, src);
  return cmdLog(reply(CmdResult::ACCEPTED), src, 0);
}

// ---------------- Snapshot ----------------
void ClimateCore::updateSnapshot() {
  CoreSnapshot& s = snap_;
  const RoleReading& t1 = t1f_.reading();
  const RoleReading& rh = rhf_.reading();
  const RoleReading& t2 = t2f_.reading();
  const InterlockOutput& io = il_.last();
  const ProfileOutput& po = prof_.last();
  const PidOutput& pd = pid_.last();
  const PowerOutput& pw = pm_.last();
  const VentOutput& vo = vent_.last();
  const SafetyOutput& so = saf_.last();
  s.seq = ++seq_;
  s.uptime_s = up_ms_ / 1000u;
  s.temperature = t1.value;
  s.humidity = rh.value;
  s.t2 = t2.value;
  s.temperature_quality = t1.quality;
  s.humidity_quality = rh.quality;
  s.t2_quality = t2.quality;
  s.sensor_ok = t1.quality == Quality::GOOD && rh.quality == Quality::GOOD;
  s.sensor_age_s = t1.age_ms / 1000u;
  s.temperature_setpoint = cfg_.temperature_setpoint;
  s.setpoint_effective = po.effective;
  s.setpoint_source = po.source;
  s.profile = cfg_.profile;
  s.profile_active = po.active;
  s.sched_night = prof_.schedNight();
  s.sched_away = prof_.schedAway();
  s.boost = prof_.boost();
  s.boost_remaining_min = po.boost_remaining_min;
  s.operating_mode = cfg_.operating_mode;
  s.controller_enable = controller_enable_;
  s.heating_phase = chain_.phase();
  s.heating_reason = heating_reason_;
  s.failsafe_reason = so.reason;
  s.pid_output = pd.output;
  s.pid_error = pd.error;
  s.pid_p = pd.p;
  s.pid_i = pd.i;
  s.pid_d = pd.d;
  s.pid_saturation = pd.sat;
  s.anti_windup_active = pd.anti_windup;
  s.pid_tracking = pd.tracking;
  s.heat_demand = heat_demand_;
  s.manual_heat_demand = cfg_.manual_heat_demand;
  s.r1_duty = pw.duty[0];
  s.r2_duty = pw.duty[1];
  s.power_stage = pw.stage;
  s.temperature_rate = hpm_.last().temperature_rate;
  for (uint8_t k = 0; k < OUT_COUNT; ++k) s.active[k] = guard_.applied()[k];

  // Kullanıcıya dönük reason: istek (switch) ile etkin durum karşılaştırması
  const bool svc = sys_.state() == SysState::SERVICE;
  for (uint8_t k = R1; k <= R2; ++k) {
    Reason r = io.reason[k];
    if (r == Reason::NONE && svc && svc_test_[k] && io.eff[k]) r = Reason::SERVICE_TEST;
    if (r == Reason::NONE && !controller_enable_) r = Reason::CONTROLLER_DISABLED;
    s.reason[k] = r;
  }
  s.reason[HF] = (io.eff[HF] != heater_fan_manual_) ? io.reason[HF] : Reason::NONE;
  {
    const bool man = vent_.manual();
    Reason r = Reason::NONE;
    if (io.eff[VF] != man) {
      if (io.reason[VF] != Reason::NONE) r = io.reason[VF];
      else if (io.eff[VF]) r = Reason::AUTO_DEMAND;
      else if (vo.inhibit != Reason::NONE) r = vo.inhibit;
      else if (!controller_enable_) r = Reason::CONTROLLER_DISABLED;
    }
    s.reason[VF] = r;
  }
  s.heating_active = chain_.heating();
  s.ventilation_active = io.eff[VF];
  s.heater_fan_manual = heater_fan_manual_;
  s.ventilation_fan_manual = vent_.manual();
  s.ventilation_state = Ventilation::reportState(vo, io.eff[VF], io.reason[VF]);
  s.controller_state = deriveControllerState(sys_.state(), chain_.phase(), s.ventilation_state, cfg_.operating_mode);
  s.overtemperature = so.overtemp_condition || (so.latched & SL_OVERTEMP);
  const AlarmSummary as = alm_.summary();
  s.alarm = as.alarm;
  s.alarm_state = as.highest;
  s.active_alarm_count = as.active_count;
  s.unacked_alarm_count = as.unacked_count;
  s.local_lock = local_lock_;
  s.last_command_source = last_src_;
  s.ack_count = ack_count_;
  s.sys_state = sys_.state();
  s.heater_arm = arm_;
  s.ventilation_start_effective = vo.start_effective;
  s.post_cool_remaining_s = io.post_cool_remaining_ms / 1000u;
}

}  // namespace cc
