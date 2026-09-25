#include "cc_vent.h"

namespace cc {

static bool usable(Quality q) { return q == Quality::GOOD || q == Quality::UNCERTAIN; }

VentOutput Ventilation::step(const Config& c, const VentInput& in) {
  VentOutput o;

  // OTA: yazım sırasında proses bekler (ota_vent_state: LAST → son durum, OFF → kapalı)
  if (in.ota) {
    o = last_;
    o.ev_manual_timeout = false;
    if (c.ota_vent_state == OtaVentState::OFF) {
      o.vf_request = false;
      o.inhibit = Reason::NONE;
    }
    o.heat_cap = kNaN;
    o.heat_inhibit = false;
    last_ = o;
    return o;
  }

  // Manuel istek zaman aşımı
  if (manual_) {
    manual_t_.add(in.dt_ms);
    if (c.manual_vent_timeout_min > 0 && manual_t_.atLeast(minToMs((float)c.manual_vent_timeout_min))) {
      manual_ = false;
      o.ev_manual_timeout = true;
    }
  }

  // Setpoint ile eşik ilişkisi (§5.3)
  const float spFloor = in.sp_effective + c.vent_sp_margin;
  const float startEff = c.ventilation_start_temperature > spFloor ? c.ventilation_start_temperature : spFloor;
  const float stopEff = startEff - (c.ventilation_start_temperature - c.ventilation_stop_temperature);
  o.start_effective = startEff;
  o.stop_effective = stopEff;

  const bool autoOk = in.controller_enable && !in.service && !in.sensor_fault && in.mode != OpMode::OFF;

  // Otomatik kaynaklar (histerezisli)
  if (autoOk && usable(in.t1_q) && isValid(in.t1)) {
    if (in.t1 > startEff) temp_high_ = true;
    else if (in.t1 < stopEff) temp_high_ = false;
  } else {
    temp_high_ = false;
  }
  if (autoOk && c.humidity_vent_enabled && usable(in.rh_q) && isValid(in.rh)) {
    if (in.rh > c.humidity_high_limit) hum_high_ = true;
    else if (in.rh < c.humidity_high_limit - c.humidity_hysteresis) hum_high_ = false;
  } else {
    hum_high_ = false;
  }
  const bool sched = autoOk && (in.program_vent || (c.ventilation_periodic_min > 0 &&
                     (in.uptime_s % 3600u) < (uint32_t)c.ventilation_periodic_min * 60u));
  const bool manualReq = manual_ && !in.service;

  if (temp_high_) o.sources |= VS_TEMP_HIGH;
  if (hum_high_) o.sources |= VS_HUMIDITY_HIGH;
  if (sched) o.sources |= VS_SCHEDULED;
  if (in.overtemp) o.sources |= VS_OVERTEMP;
  if (manualReq) o.sources |= VS_MANUAL;

  // Isıtma dönemi izleme (satır 8: ısıtma → vent changeover)
  if (in.heating_active) {
    since_heat_stop_.reset();
    last_heating_ = true;
  } else {
    since_heat_stop_.add(in.dt_ms);
    if (since_heat_stop_.atLeast(sToMs((float)c.heat_vent_changeover_s))) last_heating_ = false;
  }
  const bool heatingCtx = in.heating_active || in.heat_request;
  const bool heatToVentDelay = !in.heating_active && last_heating_;

  // --- Otomatik isteklerin koordinasyonu (§5.2 satır 5–8) ---
  Reason inhibitAuto = Reason::NONE;
  bool tempAllowed = temp_high_, humAllowed = hum_high_, schedAllowed = sched;
  if (heatingCtx) {
    bool allow = false;
    switch (c.humidity_vent_while_heating) {
      case HumVentWhileHeating::INHIBIT: allow = false; break;
      case HumVentWhileHeating::ALLOW: allow = true; break;
      case HumVentWhileHeating::ALLOW_ABOVE_SP: allow = isValid(in.t1) && in.t1 >= in.sp_effective - 0.5f; break;
    }
    if (!allow) {
      if (humAllowed || schedAllowed) inhibitAuto = Reason::HEATING_PRIORITY;
      humAllowed = schedAllowed = false;
    }
  } else if (heatToVentDelay && (tempAllowed || humAllowed || schedAllowed)) {
    inhibitAuto = Reason::CHANGEOVER_DELAY;
    tempAllowed = humAllowed = schedAllowed = false;
  }
  o.auto_request = tempAllowed || humAllowed || schedAllowed;

  // --- Manuel istek (satır 4) ---
  bool manualAllowed = manualReq;
  Reason inhibitManual = Reason::NONE;
  if (manualReq) {
    if (in.mode == OpMode::OFF) {
      manualAllowed = false;
      inhibitManual = Reason::MODE_OFF;
    } else if (heatingCtx && !in.antifreeze && c.manual_vent_priority == ManualVentPriority::HEAT_WINS) {
      manualAllowed = false;
      inhibitManual = Reason::HEATING_PRIORITY;
    }
  }
  // VENT_WINS: manuel havalandırma sürerken ısıtma talebi vent_heat_cap ile sınırlanır.
  // Antifreeze (satır 3) daha önceliklidir: sınır yok, havalandırma I-5 ile kapatılır.
  if (manualAllowed && !in.antifreeze && c.manual_vent_priority == ManualVentPriority::VENT_WINS)
    o.heat_cap = c.vent_heat_cap;

  // İstek = tüm kaynakların birleşimi; koordinasyon hepsini engelliyorsa inhibit nedeni
  o.vf_request = manualReq || temp_high_ || hum_high_ || sched;
  if (o.vf_request && !(manualAllowed || o.auto_request))
    o.inhibit = inhibitManual != Reason::NONE ? inhibitManual : inhibitAuto;

  // Vent → ısıtma changeover (satır 9): otomatik havalandırma kapandıktan sonra ısıtma başlatılmaz
  const bool autoVentOn = in.vf_effective && last_.auto_request;
  if (autoVentOn) {
    since_auto_stop_.reset();
    last_auto_ = true;
  } else {
    since_auto_stop_.add(in.dt_ms);
    if (since_auto_stop_.atLeast(sToMs((float)c.vent_heat_changeover_s))) last_auto_ = false;
  }
  o.heat_inhibit = !in.antifreeze && last_auto_ && !autoVentOn;

  o.humidity_high = hum_high_ && !humAllowed && !manualAllowed;
  last_ = o;
  return o;
}

VentState Ventilation::reportState(const VentOutput& v, bool vf_eff, Reason ilReason) {
  if (vf_eff) {
    if ((v.sources & VS_OVERTEMP) || ilReason == Reason::OVERTEMPERATURE) return VentState::FORCED;
    if (v.auto_request) return VentState::AUTO;
    if (v.sources & VS_MANUAL) return VentState::MANUAL;
    return VentState::AUTO;  // min ON tutması
  }
  if (v.vf_request) return VentState::INHIBITED;
  return VentState::OFF;
}

}  // namespace cc
