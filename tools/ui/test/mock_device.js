/* Sahte cihaz: gerçek REST şekliyle bellek içi Kulübe İklim Kontrolörü + basit kulübe termal modeli.
   Yalnız önizleme/test içindir; firmware'e girmez. window.fetch'i /api/* için değiştirir.
   Davranış lib/core'un sadeleştirilmiş kopyasıdır (PI, kademe, prestart, post-cool, havalandırma, S1/S3). */
(function () {
'use strict';
window.__SCADA_PREVIEW__ = true;
const R = (a, b) => a + Math.random() * (b - a);
const cfg = {
  adN: 'Kulübe İklim 01', mdns: 'kulube-iklim', staticEnabled: false, staticIP: '192.168.1.57', gateway: '192.168.1.1', subnet: '255.255.255.0', dns1: '', dns2: '', ap_policy: 'ON_WIFI_FAIL',
  mqtt_host: '192.168.1.10', mqtt_port: 1883, mqtt_user: 'kulube', mqtt_base: 'mqttsuite/climate', slug: 'kulube_iklim_01', state_active_s: 5, state_idle_s: 30, diag_interval_s: 60,
  discovery_enabled: true, history_discovery_enabled: false, remote_config_enabled: false, pid_remote_tuning: false, remote_manual_allowed: true, service_channel_enabled: false,
  t1_driver: 'SHT4X', rh1_driver: 'SHT4X', t2_enabled: false, sensor_model: 'SHT41@0x44', t1_offset: 0, rh1_offset: 0, t2_offset: 0, sensor_interval_s: 1, sensor_filter_tau_s: 10, sensor_stuck_s: 1800,
  output_driver_r: 'SSR_ZC', tp_window_s: 20, heater_min_on_s: 1, heater_min_off_s: 1, stage2_on: 55, stage2_off: 45, stage_min_dwell_s: 120, heater_power_w_r1: 0, heater_power_w_r2: 0, lead_rotation: 'DAILY',
  fan_prestart_s: 3, post_cool_mode: 'TIME', post_cool_seconds: 60, post_cool_min_s: 20, post_cool_max_s: 600, post_cool_safe_temp: 40,
  control_interval_s: 2, min_heat_demand: 5, max_heat_demand: 100, demand_slew_pct_per_min: 10, pid_setpoint_weight: 1, onoff_hysteresis: 0.5,
  pid_mode: 'PI', pid_kp: 20, pid_ki: 1, pid_kd: 0, pid_deadband: 0.1, setpoint_ramp_c_per_min: 0.2,
  ventilation_start_temperature: 26, ventilation_stop_temperature: 24, vent_sp_margin: 2, humidity_vent_enabled: true, humidity_high_limit: 75, humidity_hysteresis: 5, humidity_low_limit: 30,
  humidity_vent_while_heating: 'INHIBIT', manual_vent_priority: 'VENT_WINS', vent_heat_cap: 0, manual_vent_timeout_min: 120, ventilation_periodic_min: 0, vent_min_on_s: 60, vent_min_off_s: 60,
  heat_vent_changeover_s: 120, vent_heat_changeover_s: 60, ota_vent_state: 'LAST', boost_minutes: 30, sched_timeout_h: 16, manual_timeout_h: 8,
  cabin_overtemp_limit: 40, overtemp_reset_hysteresis: 3, heater_outlet_limit: 80, max_continuous_heating_min: 240, sensor_stale_s: 10, unexpected_rise_c_per_10min: 1.5, max_rise_c_per_10min: 5,
  antifreeze_enabled: true, frost_guard_temperature: 4, frost_exit_hysteresis: 1, setpoint_frost: 5, service_timeout_min: 30, service_test_max_s: 120, restart_storm_limit: 5, restart_storm_window_min: 30,
  user: 'admin', guestRead: false, session_hours: 8,
  ledB: 20, cls0: '#00ff00', cls1: '#ff8000', cls2: '#ff0000', clw0: '#ff0000', clw1: '#00ff00', clw2: '#0000ff',
  clq0: '#ff0000', clq1: '#00ff00', clq2: '#000000', clm0: '#ff0000', clm1: '#00ff00', clm2: '#000000',
  clr0: '#000000', clr1: '#ff8000', clr2: '#ff0000', clf0: '#000000', clf1: '#00ffff', clf2: '#0000ff',
  temperature_setpoint: 22, setpoint_night: 18, setpoint_away: 12, setpoint_boost: 23, operating_mode: 'AUTO', profile: 'DAY', manual_heat_demand: 40
};
const NET = {try: 3, result: 'CONNECTED', fail: 'NONE', phase: 'ONLINE', hold: 0, retryAt: 0};
const flags = {ap: false, scanN: 0, speed: 10, unplug: false, overtemp: false, broker: true, control: 'apply', offline: false};
const S = {
  t: 0, T: 16.8, Tout: -2, RH: 58, Tf: 16.8, cfgRev: 44, seq: 0, boot: 37,
  sched_night: false, sched_away: false, boost: false, boostT: 0, ctrlEnable: true, hfMan: false, vfMan: false, vfManT: 0, lockMs: 0,
  I: 0, demand: 0, pidOut: 0, active: false, stage: 0, stageT: 999, lead: 0, win: [0, 10000], on: [0, 0],
  R: [false, false], HF: false, VF: false, hfOnT: 0, pc: false, pcT: 0, wasHeat: false, prevR: false,
  eff: 22, af: false, lastGood: 0, sensorQ: 'GOOD', otT: 0, latched: false, failsafe: 'NONE', service: false, svcT: 0, svcTest: [false, false, false, false],
  heatStopT: 1e9, tempHigh: false, humHigh: false, sw: [20511, 9120, 2210, 480], onMs: [0, 0, 0, 0], today: [214 * 60, 41 * 60, 255 * 60, 12 * 60],
  alarms: {}, hist: [], events: [], evSeq: 5500, trend: [], trendAcc: 0, vfReason: 'NONE', r1Reason: 'NONE', r2Reason: 'NONE', hfReason: 'NONE',
  lastCmdSrc: 'LOCAL_WEB', ackCount: 3, epoch: 1790294400 + 40 * 60
};
function ev(sev, src, msg) { S.events.push({seq: ++S.evSeq, ts: Math.floor(S.epoch + S.t / 1000), up: Math.floor(86400 + S.t / 1000), sev, src, msg}); if (S.events.length > 200) S.events.shift(); }
function alarmSet(code, sev, cond, latching, condText) {
  const a = S.alarms[code];
  if (cond) {
    if (!a || a.state === 'normal' || a.state === 'cleared_unacknowledged' && false) {
      if (!a || a.state === 'normal') { S.alarms[code] = {code, sev, state: 'active_unacknowledged', since: Math.floor(S.epoch + S.t / 1000), latched: latching, cond: condText}; ev(sev, 'ALARM', code + ' etkin'); S.hist.unshift({code, sev, since: S.alarms[code].since, text: 'etkin'}); }
    } else if (a.state === 'cleared_unacknowledged' || a.state === 'latched') { a.state = 'active_unacknowledged'; a.since = Math.floor(S.epoch + S.t / 1000); }
    if (S.alarms[code]) S.alarms[code].cond = condText;
  } else if (a) {
    a.cond = '';
    if (a.state === 'active_unacknowledged') { a.state = 'cleared_unacknowledged'; S.hist.unshift({code, sev, since: Math.floor(S.epoch + S.t / 1000), text: 'giderildi'}); }
    else if (a.state === 'active_acknowledged') a.state = a.latched ? 'latched' : 'normal';
  }
  S.hist = S.hist.slice(0, 50);
}
// ---------------------------------------------------------------- yerel programlar (lib/core/cc_schedule'ın JS eşi)
const TZ = 180;
const PG = {enabled: true, hold: null, last: {climate: -1, vent: -1}, res: null, resMin: -1, list: [
  {name: 'Sabah ısıtması', enabled: true, kind: 'WEEKLY', days: 31, date_from: '', date_to: '', start: '06:30', end: 'END_TIME', end_time: '08:30', duration: 120, action: 'SETPOINT', setpoint: 22.5, profile: 'NIGHT'},
  {name: 'Gece', enabled: true, kind: 'WEEKLY', days: 127, date_from: '', date_to: '', start: '23:00', end: 'END_TIME', end_time: '06:00', duration: 420, action: 'PROFILE', setpoint: 18, profile: 'NIGHT'},
  {name: 'Cumartesi atölye', enabled: true, kind: 'WEEKLY', days: 32, date_from: '', date_to: '', start: '14:00', end: 'DURATION', end_time: '17:00', duration: 180, action: 'SETPOINT', setpoint: 24, profile: 'NIGHT'},
  {name: 'Öğle havalandırma', enabled: true, kind: 'WEEKLY', days: 127, date_from: '', date_to: '', start: '12:00', end: 'DURATION', end_time: '12:20', duration: 20, action: 'VENTILATE', setpoint: 20, profile: 'NIGHT'},
  {name: 'Misafir', enabled: true, kind: 'ONCE', days: 0, date_from: '2026-10-03', date_to: '2026-10-03', start: '14:00', end: 'DURATION', end_time: '', duration: 4320, action: 'SETPOINT', setpoint: 23, profile: 'NIGHT'},
  {name: 'Yılbaşı tatili', enabled: true, kind: 'DATE_RANGE', days: 0, date_from: '2026-12-24', date_to: '2027-01-02', start: '00:00', end: 'ALL_DAY', end_time: '', duration: 1440, action: 'PROFILE', setpoint: 18, profile: 'AWAY'},
  {name: 'Yaz: ısıtma yok', enabled: false, kind: 'DATE_RANGE', days: 0, date_from: '2027-06-01', date_to: '2027-09-15', start: '00:00', end: 'ALL_DAY', end_time: '', duration: 1440, action: 'HEATING_OFF', setpoint: 18, profile: 'NIGHT'}
]};
const pgMin = s => { const m = /^(\d\d):(\d\d)$/.exec(s || ''); return m && +m[1] < 24 && +m[2] < 60 ? +m[1] * 60 + +m[2] : -1; };
const pgDay = s => { const m = /^(\d{4})-(\d\d)-(\d\d)$/.exec(s || ''); if (!m || +m[1] < 2000 || +m[1] > 2199) return null; const d = Date.UTC(+m[1], +m[2] - 1, +m[3]); return new Date(d).getUTCDate() === +m[3] ? d / 86400000 : null; };
const pgWd = d => ((d % 7) + 7 + 3) % 7;
const CLS = {WEEKLY: 1, DATE_RANGE: 2, ONCE: 3};
function pgLen(p) { return p.end === 'ALL_DAY' ? 1440 : p.end === 'END_TIME' ? ((pgMin(p.end_time) - pgMin(p.start) + 1440) % 1440 || 1440) : +p.duration; }
function pgOn(p, d) {
  if (p.kind === 'WEEKLY') return (p.days >> pgWd(d)) & 1;
  if (p.kind === 'DATE_RANGE') return d >= pgDay(p.date_from) && d <= pgDay(p.date_to) && (!p.days || ((p.days >> pgWd(d)) & 1));
  return d === pgDay(p.date_from);
}
function pgOcc(p, t) {   // t: yerel dakika → etkin oluşum [s,e) veya null
  const today = Math.floor(t / 1440), len = pgLen(p), back = p.kind === 'ONCE' ? 7 : 1;
  for (let d = today; d >= today - back; d--) {
    if (!pgOn(p, d)) continue;
    const s = d * 1440 + (p.end === 'ALL_DAY' ? 0 : pgMin(p.start));
    if (t >= s && t < s + len) return {s, e: s + len};
  }
  return null;
}
function pgEvalAt(t) {
  const best = {climate: null, vent: null};
  let held = false;
  PG.list.forEach((p, i) => {
    if (!p.enabled) return;
    const o = pgOcc(p, t);
    if (!o) return;
    const ch = p.action === 'VENTILATE' ? 'vent' : 'climate', b = best[ch];
    const c = {i, s: o.s, e: o.e, cls: CLS[p.kind]};
    if (ch === 'climate' && PG.hold && PG.hold.i === i && PG.hold.s === o.s) { held = true; return; }
    if (!b || c.cls > b.cls || (c.cls === b.cls && c.s > b.s)) best[ch] = c;
  });
  return {climate: best.climate ? best.climate.i : -1, until: best.climate ? best.climate.e : -1, held,
    vent: best.vent ? best.vent.i : -1, vent_until: best.vent ? best.vent.e : -1};
}
function pgEval(t, withNext) {
  const r = pgEvalAt(t);
  r.next_change = -1;
  if (withNext) {
    const cand = new Set();
    const today = Math.floor(t / 1440);
    PG.list.forEach(p => {
      if (!p.enabled) return;
      for (let d = today - 7; d <= today + 8; d++) if (pgOn(p, d)) { const s = d * 1440 + (p.end === 'ALL_DAY' ? 0 : pgMin(p.start)); cand.add(s); cand.add(s + pgLen(p)); }
    });
    const lst = [...cand].filter(x => x > t && x <= t + 8 * 1440).sort((a, b) => a - b);
    for (const x of lst) { const q = pgEvalAt(x); if (q.climate !== r.climate || q.vent !== r.vent) { r.next_change = x; break; } }
  }
  return r;
}
function pgValidate(list) {
  const E = (code, index) => ({code, index, message: code});
  if (!Array.isArray(list) || list.length > 16) return E('TOO_MANY', -1);
  for (let i = 0; i < list.length; i++) {
    const p = list[i];
    const nb = new TextEncoder().encode(String(p.name || '').trim()).length;
    if (!nb || nb > 23) return E('NAME', i);
    if (!CLS[p.kind]) return E('DAYS', i);
    if (!(p.days >= 0 && p.days <= 127) || (p.kind === 'WEEKLY' && !p.days)) return E('DAYS', i);
    if (p.kind !== 'WEEKLY') {
      const a = pgDay(p.date_from), b = p.kind === 'ONCE' ? a : pgDay(p.date_to);
      if (a === null || b === null || b < a) return E('DATE_ORDER', i);
      if (b - a > 365) return E('DATE_SPAN', i);
    }
    if (!['END_TIME', 'DURATION', 'ALL_DAY'].includes(p.end)) return E('END', i);
    if (p.end !== 'ALL_DAY' && pgMin(p.start) < 0) return E('START', i);
    if (p.end === 'END_TIME' && (pgMin(p.end_time) < 0 || p.end_time === p.start)) return E('END', i);
    if (p.end === 'DURATION' && !(p.duration >= 1 && p.duration <= (p.kind === 'ONCE' ? 10080 : 1440) && Number.isInteger(+p.duration))) return E('DURATION', i);
    if (p.action === 'SETPOINT') {
      if (!(p.setpoint >= 5 && p.setpoint <= 30 && Number.isInteger(p.setpoint * 2))) return E('SETPOINT', i);
      if (p.setpoint > cfg.cabin_overtemp_limit - 10) return E('SAFETY_MARGIN', i);
    } else if (p.action === 'PROFILE') { if (!['NIGHT', 'AWAY', 'FROST'].includes(p.profile)) return E('PROFILE', i); }
    else if (!['HEATING_OFF', 'VENTILATE'].includes(p.action)) return E('PROFILE', i);
  }
  return null;
}
const pgNow = () => Math.floor((S.epoch + S.t / 1000) / 60) + TZ;
function pgStep() {
  const t = pgNow();
  if (t === PG.resMin) return PG.res;
  PG.resMin = t;
  const r = PG.enabled ? pgEvalAt(t) : {climate: -1, vent: -1, until: -1, vent_until: -1, held: false};
  if (PG.hold && !r.held) PG.hold = null;
  ['climate', 'vent'].forEach(ch => {
    if (r[ch] !== PG.last[ch]) {
      if (PG.last[ch] >= 0 && PG.list[PG.last[ch]]) ev('INFO', 'PROGRAM', 'Program bitti: ' + PG.list[PG.last[ch]].name);
      if (r[ch] >= 0) ev('INFO', 'PROGRAM', 'Program başladı: ' + PG.list[r[ch]].name);
      PG.last[ch] = r[ch];
    }
  });
  PG.res = r;
  return r;
}
const pgIso = m => m < 0 ? '' : new Date(m * 60000).toISOString().slice(0, 16);
// ---------------------------------------------------------------- simülasyon adımı (100 ms)
function step() {
  const dt = 100;
  S.t += dt;
  const c = cfg;
  // Kulübe fiziği: rezistans ısısı fanla aktarılır; havalandırma kaybı artırır
  const p = (S.R[0] ? 50 : 0) + (S.R[1] ? 50 : 0);
  const loss = (S.T - S.Tout) * (1 + (S.VF ? 0.6 : 0));
  S.T += dt / 1000 * (0.32 * p * (S.HF ? 1 : 0.6) - loss) / 1500;
  S.RH += dt / 1000 * ((S.VF ? -0.02 : 0.004) + (p > 0 ? -0.003 : 0.001));
  S.RH = Math.max(35, Math.min(92, S.RH));
  // Sensör
  let reading = S.T + R(-0.02, 0.02) + (flags.overtemp ? 25 : 0);
  if (!flags.unplug) { S.Tf += (reading - S.Tf) * 0.1 / (10 + 0.1) * 10; S.lastGood = S.t; }
  const age = (S.t - S.lastGood) / 1000;
  S.sensorQ = flags.unplug ? (age > c.sensor_stale_s ? 'STALE' : 'UNCERTAIN') : 'GOOD';
  if (flags.unplug && age > 3) S.sensorQ = 'BAD';
  const sensorOk = S.sensorQ === 'GOOD' || S.sensorQ === 'UNCERTAIN';
  // Safety S1 / S3
  if (sensorOk && S.Tf >= c.cabin_overtemp_limit) { S.otT += dt; if (S.otT >= 10000 && !S.latched) { S.latched = true; ev('CRITICAL', 'SAFETY', 'Aşırı sıcaklık kilidi (T1 ' + S.Tf.toFixed(1) + ' °C)'); } } else S.otT = 0;
  const sensorFault = S.sensorQ === 'BAD' || S.sensorQ === 'STALE';
  const prevFs = S.failsafe;
  S.failsafe = S.latched ? 'OVERTEMP' : (sensorFault ? 'SENSOR_FAULT' : 'NONE');
  if (S.failsafe !== prevFs) ev(S.failsafe === 'NONE' ? 'INFO' : 'CRITICAL', 'STATE', S.failsafe === 'NONE' ? 'Normal çalışmaya dönüldü' : 'Güvenli durum: ' + S.failsafe);
  const lock = S.failsafe !== 'NONE';
  // Profil
  if (S.boost) { S.boostT += dt; if (S.boostT >= c.boost_minutes * 60000) { S.boost = false; ev('INFO', 'CONTROLLER', 'Boost süresi doldu'); } }
  const pr = pgStep(), pp = !S.boost && c.profile === 'DAY' && pr.climate >= 0 ? PG.list[pr.climate] : null;
  let prof = S.boost ? 'BOOST' : (c.profile !== 'DAY' ? c.profile : pp ? (pp.action === 'PROFILE' ? pp.profile : 'PROGRAM') : (S.sched_away && !S.lockMs ? 'AWAY' : (S.sched_night && !S.lockMs ? 'NIGHT' : 'DAY')));
  const spMap = {DAY: c.temperature_setpoint, NIGHT: c.setpoint_night, AWAY: c.setpoint_away, FROST: c.setpoint_frost, BOOST: c.setpoint_boost,
    PROGRAM: pp && pp.action === 'SETPOINT' ? pp.setpoint : c.temperature_setpoint};
  let target = spMap[prof], src = c.operating_mode === 'MANUAL' ? 'MANUAL' : pp ? 'PROGRAM' : prof;
  S.heatSuspend = !!(pp && pp.action === 'HEATING_OFF');
  S.progVent = pr.vent >= 0;
  if (c.antifreeze_enabled && S.ctrlEnable && !S.service && S.sensorQ === 'GOOD') {
    if (!S.af && S.Tf < c.frost_guard_temperature) { S.af = true; ev('WARNING', 'CONTROLLER', 'Donma koruması devrede'); }
    else if (S.af && S.Tf >= c.setpoint_frost + c.frost_exit_hysteresis) S.af = false;
  } else S.af = false;
  if (S.af && (c.operating_mode !== 'AUTO' || c.setpoint_frost >= target)) { target = c.setpoint_frost; src = 'ANTIFREEZE'; }
  if (target <= S.eff || prof === 'BOOST' || c.setpoint_ramp_c_per_min <= 0) S.eff = target;
  else { S.eff = Math.min(target, Math.max(S.eff + c.setpoint_ramp_c_per_min * dt / 60000, Math.min(S.Tf, target))); }
  S.src = src; S.prof = prof;
  if (S.lockMs > 0) S.lockMs = Math.max(0, S.lockMs - dt);
  // Kontrol (2 s)
  if (S.t % (c.control_interval_s * 1000) === 0) {
    const base = !lock && S.ctrlEnable && !S.service && sensorOk;
    const mode = c.operating_mode;
    let srcD = 'NONE';
    if (base) srcD = mode === 'AUTO' ? (S.heatSuspend && !S.af ? 'NONE' : 'PID') : mode === 'MANUAL' ? 'MANUAL' : (S.af ? 'PID' : 'NONE');
    const e = S.eff - S.Tf, ts = c.control_interval_s / 60;
    const P = c.pid_kp * e;
    if (srcD === 'PID') {
      const u = P + S.I;
      if (!((u >= c.max_heat_demand && e > 0) || (u <= 0 && e < 0)) && Math.abs(e) >= c.pid_deadband) S.I += c.pid_ki * e * ts;
      S.I = Math.max(-100, Math.min(100, S.I));
      S.pidOut = Math.max(0, Math.min(c.max_heat_demand, P + S.I));
    } else if (srcD === 'MANUAL') { S.pidOut = c.manual_heat_demand; S.I = c.manual_heat_demand - P; } else { S.pidOut = 0; if (lock) S.I = 0; }
    S.P = P;
    let tgt = srcD === 'NONE' ? 0 : Math.min(S.pidOut, c.max_heat_demand);
    if (S.vfMan && c.manual_vent_priority === 'VENT_WINS' && !S.af && srcD !== 'NONE') tgt = Math.min(tgt, c.vent_heat_cap);
    if (S.active) S.active = tgt >= c.min_heat_demand; else S.active = tgt >= c.min_heat_demand + 2;
    if (!S.active) tgt = 0;
    S.demand = tgt <= S.demand ? tgt : Math.min(tgt, S.demand + c.demand_slew_pct_per_min * ts);
    S.hreason = S.demand > 0 ? (src === 'ANTIFREEZE' ? 'ANTIFREEZE' : srcD === 'MANUAL' ? 'MANUAL' : prof === 'BOOST' ? 'BOOST' : 'PID') : 'NONE';
  }
  // PowerManager
  const d = S.demand;
  S.stageT += dt;
  const prevStage = S.stage;
  if (d <= 0) S.stage = 0;
  else if (S.stage === 0) S.stage = 1;
  else if (S.stage === 1 && d >= c.stage2_on && S.stageT >= c.stage_min_dwell_s * 1000) S.stage = 2;
  else if (S.stage === 2 && d <= c.stage2_off && S.stageT >= c.stage_min_dwell_s * 1000) S.stage = 1;
  if (S.stage !== prevStage) { S.stageT = 0; if (S.stage === 2) ev('INFO', 'CONTROLLER', 'Kademe 2 (talep ' + d.toFixed(0) + ' %) → R2'); if (prevStage === 2) ev('INFO', 'CONTROLLER', 'Kademe 1'); }
  const duty = [0, 0];
  if (S.stage === 1) duty[S.lead] = Math.min(100, 2 * d);
  if (S.stage === 2) { duty[S.lead] = 100; duty[1 - S.lead] = Math.max(0, 2 * (d - 50)); }
  S.duty = duty;
  const W = c.tp_window_s * 1000, req = [false, false];
  for (let i = 0; i < 2; i++) {
    S.win[i] = (S.win[i] + dt) % W;
    let on = duty[i] / 100 * W;
    if (on < c.heater_min_on_s * 1000) on = 0;
    if (W - on < c.heater_min_off_s * 1000 && on > 0) on = W;
    req[i] = S.win[i] < on;
  }
  // Interlock
  const chain = S.service ? (S.svcTest[0] || S.svcTest[1]) : d > 0 && !lock;
  const rq = S.service ? [S.svcTest[0], S.svcTest[1]] : req;
  const fanReady = S.HF && S.hfOnT >= c.fan_prestart_s * 1000;
  const nR = [false, false], rr = ['NONE', 'NONE'];
  for (let i = 0; i < 2; i++) {
    const want = rq[i] && chain;
    if (lock) { nR[i] = false; if (rq[i]) rr[i] = S.latched ? 'OVERTEMPERATURE_LOCKOUT' : 'SENSOR_FAULT'; }
    else if (want && !S.R[i]) { if (!fanReady) rr[i] = 'FAN_PRESTART'; else nR[i] = true; }
    else nR[i] = want;
  }
  const anyR = nR[0] || nR[1];
  if (anyR || chain) { S.pc = false; S.wasHeat = true; }
  else if (S.wasHeat) { S.wasHeat = false; S.pc = true; S.pcT = 0; ev('INFO', 'STATE', 'Soğutma (post-cool) başladı, ' + c.post_cool_seconds + ' s'); }
  if (S.pc) { S.pcT += dt; if (S.pcT >= c.post_cool_seconds * 1000) { S.pc = false; ev('INFO', 'OUTPUT', 'Isıtıcı fanı soğutması bitti'); } }
  const hfReq = S.service ? S.svcTest[2] : S.hfMan;
  const nHF = anyR || chain || S.pc || hfReq;
  const prepurge = chain && !anyR && !fanReady;
  S.hfReason = nHF !== hfReq ? (anyR ? 'HEATER_INTERLOCK' : prepurge ? 'PREPURGE' : chain ? 'HEATER_INTERLOCK' : 'POST_COOL') : 'NONE';
  // Havalandırma
  if (S.vfMan) { S.vfManT += dt; if (c.manual_vent_timeout_min && S.vfManT >= c.manual_vent_timeout_min * 60000) { S.vfMan = false; ev('INFO', 'CONTROLLER', 'Manuel havalandırma süresi doldu'); } }
  const startEff = Math.max(c.ventilation_start_temperature, S.eff + c.vent_sp_margin), stopEff = startEff - (c.ventilation_start_temperature - c.ventilation_stop_temperature);
  S.startEff = startEff;
  const autoOk = S.ctrlEnable && !S.service && !sensorFault && c.operating_mode !== 'OFF';
  if (autoOk) { if (S.Tf > startEff) S.tempHigh = true; else if (S.Tf < stopEff) S.tempHigh = false; } else S.tempHigh = false;
  if (autoOk && c.humidity_vent_enabled) { if (S.RH > c.humidity_high_limit) S.humHigh = true; else if (S.RH < c.humidity_high_limit - c.humidity_hysteresis) S.humHigh = false; } else S.humHigh = false;
  const heating = chain || anyR;
  if (heating) S.heatStopT = 0; else S.heatStopT += dt;
  const humAllowed = S.humHigh && (!heating || c.humidity_vent_while_heating === 'ALLOW') && S.heatStopT >= c.heat_vent_changeover_s * 1000;
  const tempAllowed = S.tempHigh && S.heatStopT >= c.heat_vent_changeover_s * 1000;
  const progAllowed = S.progVent && autoOk && S.heatStopT >= c.heat_vent_changeover_s * 1000;
  const vfReq = S.vfMan || S.tempHigh || S.humHigh || (S.progVent && autoOk);
  let nVF = (S.vfMan && c.operating_mode !== 'OFF') || tempAllowed || humAllowed || progAllowed;
  S.vfReason = 'NONE';
  const otForce = S.latched && S.Tf >= c.cabin_overtemp_limit - c.overtemp_reset_hysteresis;
  if (otForce) nVF = true;
  else if (S.af && vfReq) { nVF = false; S.vfReason = 'ANTIFREEZE_INHIBIT'; }
  if (S.service) nVF = S.svcTest[3];
  if (nVF !== S.vfMan) S.vfReason = otForce ? 'OVERTEMPERATURE' : nVF ? 'AUTO_DEMAND' : (S.vfReason !== 'NONE' ? S.vfReason : (c.operating_mode === 'OFF' ? 'MODE_OFF' : heating ? 'HEATING_PRIORITY' : 'CHANGEOVER_DELAY'));
  S.vsrc = [S.tempHigh && 'TEMP_HIGH', S.humHigh && 'HUMIDITY_HIGH', S.progVent && 'SCHEDULED', S.vfMan && 'MANUAL', otForce && 'OVERTEMP'].filter(Boolean);
  // Uygula + sayaçlar
  const next = [nR[0], nR[1], nHF, nVF];
  const cur = [S.R[0], S.R[1], S.HF, S.VF];
  for (let k = 0; k < 4; k++) {
    if (next[k] && !cur[k]) { S.sw[k]++; if (k >= 2) ev('INFO', 'OUTPUT', ['R1', 'R2', 'Isıtıcı fanı', 'Havalandırma'][k] + ' ON'); }
    if (!next[k] && cur[k] && k >= 2) ev('INFO', 'OUTPUT', ['R1', 'R2', 'Isıtıcı fanı', 'Havalandırma'][k] + ' OFF');
    if (next[k]) S.today[k] += dt / 1000;
  }
  if (anyR && !S.prevR) ev('INFO', 'STATE', 'Isıtma başladı (T1 ' + S.Tf.toFixed(1) + ', SP ' + S.eff.toFixed(1) + ')');
  S.prevR = anyR;
  S.hfOnT = nHF ? (S.HF ? S.hfOnT + dt : dt) : 0;
  S.R = nR; S.HF = nHF; S.VF = nVF;
  S.r1Reason = rr[0]; S.r2Reason = rr[1];
  S.phase = lock && (S.pc || anyR) ? 'LOCKOUT' : anyR ? 'ACTIVE' : prepurge ? 'PREPURGE' : chain ? 'ACTIVE' : S.pc ? 'POST_COOL' : 'IDLE';
  const vs = nVF ? (otForce ? 'FORCED' : S.vfMan ? 'MANUAL' : 'AUTO') : (vfReq ? 'INHIBITED' : 'OFF');
  S.vstate = vs;
  S.cstate = S.service ? 'SERVICE' : lock ? 'FAILSAFE' : (S.phase === 'ACTIVE' || S.phase === 'PREPURGE') ? 'HEATING' : S.phase === 'POST_COOL' ? 'POST_COOL' : (nVF ? 'VENTILATING' : (c.operating_mode === 'MANUAL' ? 'MANUAL' : c.operating_mode === 'OFF' ? 'OFF' : 'IDLE'));
  if (S.service) { S.svcT += dt; S.svcTest.forEach((v, i) => { if (v && S.svcT > c.service_test_max_s * 1000) S.svcTest[i] = false; }); }
  // Alarmlar (250 ms)
  if (S.t % 250 === 0) {
    alarmSet('OVERTEMPERATURE', 'CRITICAL', S.otT > 0 || otForce, true, S.otT > 0 ? 'T1 ' + S.Tf.toFixed(1) + ' °C ≥ ' + c.cabin_overtemp_limit.toFixed(1) + ' °C' : '');
    alarmSet('SENSOR_FAULT', 'CRITICAL', sensorFault, false, sensorFault ? 'T1 ' + S.sensorQ : '');
    alarmSet('MQTT_OFFLINE', 'WARNING', !flags.broker, false, '');
    alarmSet('HUMIDITY_HIGH', 'INFO', S.humHigh && !nVF, false, 'RH ' + S.RH.toFixed(0) + ' %');
    alarmSet('FROST_RISK_NO_SENSOR', 'CRITICAL', sensorFault && S.Tf < c.frost_guard_temperature + 2, false, '');
  }
  // Trend (5 s)
  S.trendAcc += dt;
  if (S.trendAcc >= 5000) {
    S.trendAcc = 0;
    S.trend.push([S.epoch + S.t / 1000, sensorOk ? S.Tf : null, S.eff, S.RH, d, (S.R[0] ? 1 : 0) | (S.R[1] ? 2 : 0) | (S.HF ? 4 : 0) | (S.VF ? 8 : 0)]);
    if (S.trend.length > 17280) S.trend.shift();
  }
}
// Geçmiş: 3 saatlik başlangıç (yerel 03:40 → 06:40: Gece programı biter, 06:30 Sabah ısıtması başlar)
(function preroll() {
  for (let i = 0; i < 3 * 36000; i++) step();
})();
let acc = 0, lastWall = performance.now();
setInterval(() => {
  const now = performance.now();
  acc += (now - lastWall) * flags.speed;
  lastWall = now;
  let n = 0;
  while (acc >= 100 && n < 20000) { step(); acc -= 100; n++; }
}, 100);

// ---------------------------------------------------------------- API
const onoff = b => b ? 'ON' : 'OFF';
function data() {
  const c = cfg;
  S.seq++;
  const q = S.sensorQ, ok = q === 'GOOD' || q === 'UNCERTAIN';
  const al = Object.values(S.alarms).filter(a => a.state !== 'normal');
  const rank = {INFO: 1, WARNING: 2, CRITICAL: 3};
  const hi = al.reduce((m, a) => rank[a.sev] > rank[m] ? a.sev : m, 'NORMAL');
  return {
    v: 1, seq: S.seq, ts: Math.floor(S.epoch + S.t / 1000), uptime: Math.floor(86400 + S.t / 1000),
    ap_mode: flags.ap, ap_name: 'SCADA_AP_3C71BF4A', ap_ip: '192.168.4.1', wifi_ssid: flags.ssid !== undefined ? flags.ssid : 'Kulube-Ag', net_note: '',
    net_phase: flags.ap && NET.phase === 'ONLINE' && !NET.hold ? 'AP_ONLY' : NET.phase, net_setup: !flags.ap ? 'NONE' : NET.hold > Date.now() ? 'HANDOVER' : (flags.ssid ? 'RECOVERY' : 'FIRST'),
    net_try: NET.try, net_result: NET.result, net_fail: NET.fail, net_fail_code: NET.fail === 'AUTH' ? 15 : 0,
    net_retry_s: flags.ap && flags.ssid && NET.phase === 'WAITING' ? Math.max(0, Math.round((NET.retryAt - Date.now()) / 1000)) : 0,
    ap_close_s: NET.hold > Date.now() ? Math.ceil((NET.hold - Date.now()) / 1000) : 0, ap_clients: flags.ap ? 1 : 0,
    sta_ip: NET.phase === 'ONLINE' ? '192.168.1.57' : '', static_ip: cfg.staticEnabled,
    device_name: c.adN, ip: NET.phase === 'ONLINE' ? '192.168.1.57' : '192.168.4.1', mdns: c.mdns, client_ip: '192.168.1.20', fw_version: '1.0.0', fw_build: 'r12',
    wifi_ok: NET.phase === 'ONLINE', wifi_rssi: NET.phase === 'ONLINE' ? -61 : null, mqtt_status: flags.broker ? 'CONNECTED' : 'BACKOFF', time_valid: 'ON', password_set: true, ota_password_set: !!flags.otaPw, ota_ready: NET.phase === 'ONLINE',
    temperature: ok ? +S.Tf.toFixed(1) : null, humidity: +S.RH.toFixed(1), temperature_quality: q, humidity_quality: 'GOOD', t2: null, t2_quality: 'DISABLED',
    sensor_ok: onoff(q === 'GOOD'), sensor_age_s: Math.round((S.t - S.lastGood) / 1000),
    temperature_setpoint: c.temperature_setpoint, setpoint_effective: +S.eff.toFixed(2), setpoint_source: S.src,
    setpoint_night: c.setpoint_night, setpoint_away: c.setpoint_away, setpoint_frost: c.setpoint_frost, setpoint_boost: c.setpoint_boost, boost_minutes: c.boost_minutes,
    frost_guard_temperature: c.frost_guard_temperature,
    programs_enabled: onoff(PG.enabled), program_active: PG.res && PG.res.climate >= 0 ? PG.list[PG.res.climate].name : '—',
    program_until: PG.res && PG.res.climate >= 0 ? pgIso(PG.res.until) : '', program_held: onoff(PG.res && PG.res.held),
    profile: c.profile, profile_active: S.prof, sched_night: onoff(S.sched_night), sched_away: onoff(S.sched_away), boost: onoff(S.boost),
    boost_remaining_min: S.boost ? Math.ceil((c.boost_minutes * 60000 - S.boostT) / 60000) : 0,
    operating_mode: c.operating_mode, controller_enable: onoff(S.ctrlEnable), controller_state: S.cstate, heating_phase: S.phase,
    ventilation_state: S.vstate, heating_reason: S.hreason || 'NONE', failsafe_reason: S.failsafe,
    pid_output: +S.pidOut.toFixed(1), heat_demand: +S.demand.toFixed(1), manual_heat_demand: c.manual_heat_demand,
    pid_error: ok ? +(S.eff - S.Tf).toFixed(2) : null, pid_p: +(S.P || 0).toFixed(1), pid_i: +S.I.toFixed(1), pid_d: 0,
    pid_saturation: S.pidOut >= c.max_heat_demand && S.demand > 0 ? 'HIGH' : 'NONE', anti_windup_active: onoff(S.pidOut >= c.max_heat_demand), pid_tracking: onoff(c.operating_mode === 'MANUAL' || S.failsafe !== 'NONE'),
    r1_duty: S.duty ? +S.duty[0].toFixed(1) : 0, r2_duty: S.duty ? +S.duty[1].toFixed(1) : 0, power_stage: S.stage, stage2_on: c.stage2_on, stage2_off: c.stage2_off,
    temperature_rate: (() => { const n = S.trend.length; if (n < 121) return null; const a = S.trend[n - 121][1], b = S.trend[n - 1][1]; return a === null || b === null ? null : +((b - a) * 6).toFixed(1); })(),
    r1_active: onoff(S.R[0]), r2_active: onoff(S.R[1]), heater_fan_active: onoff(S.HF), ventilation_fan_active: onoff(S.VF),
    heating_active: onoff(S.phase === 'ACTIVE' || S.phase === 'PREPURGE'), ventilation_active: onoff(S.VF),
    heater_fan_manual: onoff(S.hfMan), ventilation_fan_manual: onoff(S.vfMan),
    r1_reason: S.r1Reason, r2_reason: S.r2Reason, heater_fan_reason: S.hfReason, ventilation_fan_reason: S.vfReason,
    post_cool_remaining_s: S.pc ? Math.ceil((c.post_cool_seconds * 1000 - S.pcT) / 1000) : 0,
    r1_minutes_today: S.today[0] / 60, r2_minutes_today: S.today[1] / 60, heater_fan_minutes_today: S.today[2] / 60, ventilation_fan_minutes_today: S.today[3] / 60,
    r1_switch_count: S.sw[0], r2_switch_count: S.sw[1], heater_fan_switch_count: S.sw[2], ventilation_fan_switch_count: S.sw[3],
    svc_test_r1: onoff(S.svcTest[0]), svc_test_r2: onoff(S.svcTest[1]), svc_test_heater_fan: onoff(S.svcTest[2]), svc_test_ventilation_fan: onoff(S.svcTest[3]),
    service_remaining_s: S.service ? Math.max(0, cfg.service_timeout_min * 60 - S.svcT / 1000) : 0,
    vent_sources: S.vsrc, ventilation_start_effective: S.startEff, ventilation_start_temperature: c.ventilation_start_temperature, ventilation_stop_temperature: c.ventilation_stop_temperature,
    humidity_high_limit: c.humidity_high_limit, humidity_hysteresis: c.humidity_hysteresis, humidity_vent_while_heating: c.humidity_vent_while_heating, manual_vent_priority: c.manual_vent_priority,
    overtemperature: onoff(S.latched || S.otT > 0), alarm: onoff(al.length > 0), alarm_state: hi,
    active_alarm_count: al.filter(a => a.state !== 'cleared_unacknowledged').length, unacked_alarm_count: al.filter(a => a.state.indexOf('unack') >= 0).length,
    local_lock: onoff(S.lockMs > 0), local_lock_remaining_s: Math.round(S.lockMs / 1000), last_command_source: S.lastCmdSrc, ack_count: S.ackCount,
    free_heap: 182340 - (S.seq % 7) * 64, min_heap: 151220, control_loop_max_ms: 11, reset_reason: 'POWER_ON', boot_count: S.boot, fault_boot_count: 1,
    led_ok: true, led_states: [hi === 'CRITICAL' || S.failsafe !== 'NONE' ? 2 : hi === 'WARNING' ? 1 : 0, NET.phase === 'ONLINE' ? 1 : flags.ap ? 2 : 0,
      flags.broker ? 1 : 0, NET.phase === 'ONLINE' ? 1 : 0, (S.R[0] ? 1 : 0) + (S.R[1] ? 1 : 0), S.VF ? 2 : S.HF ? 1 : 0],
    mqtt_reconnects: 2, sensor_error_rate_10m: flags.unplug ? 100 : 0, sensor_model: cfg.sensor_model, config_rev: S.cfgRev
  };
}
const OPS = {OFF: 1, AUTO: 1, MANUAL: 1, VENT_ONLY: 1};
function cmd(id, value) {
  const c = cfg;
  const num = Number(value);
  const bool = value === 'ON';
  const lim = {temperature_setpoint: [5, 30, 0.5], manual_heat_demand: [0, 100, 5], setpoint_night: [5, 30, 0.5], setpoint_away: [5, 25, 0.5], setpoint_frost: [4, 12, 0.5], setpoint_boost: [10, 30, 0.5]};
  if (lim[id]) {
    const [a, b, st] = lim[id];
    if (!(num >= a && num <= b) || Math.abs(num / st - Math.round(num / st)) > 1e-6) return {result: 'REJECTED_INVALID', reason: 'NONE'};
    if (id === 'setpoint_frost' && num <= c.frost_guard_temperature) return {result: 'REJECTED_RELATION', reason: 'NONE'};
    c[id] = num;
    if (id === 'temperature_setpoint' || id.indexOf('setpoint_') === 0) ev('INFO', 'COMMAND', id + ' → ' + num.toFixed(1) + ' (yerel web)');
    return {result: 'ACCEPTED', reason: 'NONE'};
  }
  switch (id) {
    case 'operating_mode':
      if (!OPS[value]) return {result: 'REJECTED_INVALID'};
      if (value === 'MANUAL' && S.failsafe !== 'NONE') return {result: 'REJECTED_STATE', reason: S.failsafe === 'OVERTEMP' ? 'OVERTEMPERATURE_LOCKOUT' : 'SENSOR_FAULT'};
      if (c.operating_mode === 'AUTO' && value === 'MANUAL') c.manual_heat_demand = Math.round(S.demand / 5) * 5;
      c.operating_mode = value; ev('INFO', 'COMMAND', 'Mod → ' + value + ' (yerel web)'); return {result: 'ACCEPTED'};
    case 'profile': if (!['DAY', 'NIGHT', 'AWAY', 'FROST'].includes(value)) return {result: 'REJECTED_INVALID'}; c.profile = value; ev('INFO', 'COMMAND', 'Profil → ' + value); return {result: 'ACCEPTED'};
    case 'boost': S.boost = bool; S.boostT = 0; ev('INFO', 'COMMAND', 'Boost ' + value); return {result: 'ACCEPTED'};
    case 'controller_enable': S.ctrlEnable = bool; ev('WARNING', 'COMMAND', 'Kontrolör ' + (bool ? 'etkin' : 'KAPALI') + ' (yerel web)'); return {result: 'ACCEPTED'};
    case 'heater_fan_manual': S.hfMan = bool; return !bool && S.HF && S.hfReason !== 'NONE' || !bool && (S.R[0] || S.R[1] || S.pc) ? {result: 'OVERRIDDEN', reason: S.R[0] || S.R[1] ? 'HEATER_INTERLOCK' : 'POST_COOL'} : {result: 'ACCEPTED'};
    case 'ventilation_fan_manual': S.vfMan = bool; S.vfManT = 0; return bool && S.af ? {result: 'OVERRIDDEN', reason: 'ANTIFREEZE_INHIBIT'} : {result: 'ACCEPTED'};
    case 'programs_enabled': PG.enabled = bool; PG.resMin = -1; ev('INFO', 'COMMAND', 'Yerel programlar ' + (bool ? 'etkin' : 'kapalı') + ' (yerel web)'); return {result: 'ACCEPTED'};
    case 'program_hold': {
      const r = pgEvalAt(pgNow());
      if (!PG.enabled || r.climate < 0) return {result: 'REJECTED_STATE', reason: 'NONE'};
      PG.hold = {i: r.climate, s: pgOcc(PG.list[r.climate], pgNow()).s}; PG.resMin = -1;
      ev('INFO', 'PROGRAM', 'Program atlandı: ' + PG.list[r.climate].name + ' (yerel web)'); return {result: 'ACCEPTED'};
    }
    case 'local_lock_min': S.lockMs = num * 60000; ev('INFO', 'COMMAND', num ? 'Yerel kilit ' + num + ' dk' : 'Yerel kilit kaldırıldı'); return {result: 'ACCEPTED'};
    default: return {result: 'REJECTED_INVALID', reason: 'NONE'};
  }
}
function trend(win) {
  const tEnd = S.epoch + S.t / 1000, res = win > 3600 ? 60 : 5;
  const pts = S.trend.filter(p => p[0] >= tEnd - win);
  const out = {res, clock: true, t: [], T: [], SP: [], RH: [], D: [], B: [], boot_note: 'cihaz 24 sa önce başladı'};
  const stepN = res / 5;
  for (let i = 0; i < pts.length; i += stepN) {
    const p = pts[i];
    out.t.push(p[0]); out.T.push(p[1] === null ? null : +p[1].toFixed(2)); out.SP.push(+p[2].toFixed(2)); out.RH.push(+p[3].toFixed(1)); out.D.push(+p[4].toFixed(1));
    let b = 0; for (let j = i; j < Math.min(pts.length, i + stepN); j++) b |= pts[j][5];
    out.B.push(b);
  }
  return out;
}
const SECRET_KEYS = ['mqtt_password', 'otaPw', 'pass'];
function validate(c) {
  const e = (field, message) => ({field, message});
  if (!(c.ventilation_stop_temperature <= c.ventilation_start_temperature - 1)) return e('ventilation_stop_temperature', 'Durma sıcaklığı başlamanın en az 1 °C altında olmalı (V1).');
  if (!(c.stage2_off <= c.stage2_on - 5)) return e('stage2_off', 'Kademe 2 kapanışı açılışın en az 5 % altında olmalı (V7).');
  if (c.output_driver_r === 'RELAY' && (c.tp_window_s < 300 || c.heater_min_on_s < 60)) return e('tp_window_s', 'Röle profilinde pencere ≥ 300 s ve asgari açık ≥ 60 s olmalı (V8).');
  if (!(c.heater_min_on_s + c.heater_min_off_s <= c.tp_window_s)) return e('tp_window_s', 'Asgari açık + kapalı süre pencereyi aşamaz (V9).');
  if (c.post_cool_mode !== 'TIME' && !c.t2_enabled) return e('post_cool_mode', 'Bu yöntem T2 sensörü gerektirir (V11).');
  if (!(c.cabin_overtemp_limit >= Math.max(c.setpoint_boost, c.temperature_setpoint) + 10)) return e('cabin_overtemp_limit', 'Aşırı sıcaklık limiti en yüksek hedefin en az 10 °C üstünde olmalı (V5).');
  if (!(c.cabin_overtemp_limit >= c.ventilation_start_temperature + 5)) return e('cabin_overtemp_limit', 'Aşırı sıcaklık limiti havalandırma başlamasının en az 5 °C üstünde olmalı (V6).');
  if (!(c.frost_guard_temperature < c.setpoint_frost)) return e('frost_guard_temperature', 'Devreye girme sıcaklığı donma hedefinin altında olmalı (V4).');
  if (c.pid_mode === 'PI' && !(c.pid_ki > 0)) return e('pid_ki', 'PI modunda Ki > 0 olmalı (V12).');
  if (!(c.pid_ki <= c.pid_kp)) return e('pid_ki', 'Ki, Kp’den büyük olamaz (Ti ≥ 1 dk, V13).');
  if (!(c.ledB >= 0 && c.ledB <= 100)) return e('ledB', 'LED parlaklığı %0–100 olmalı.');
  const bad = Object.keys(c).find(k => /^cl[swqmrf][0-2]$/.test(k) && !/^#[0-9a-f]{6}$/i.test(c[k]));
  if (bad) return e(bad, 'LED rengi #rrggbb biçiminde olmalı.');
  return null;
}
// Bağlantı denemesi benzetimi: 4 s sonra başarı (AP'deyse 120 s devir) veya hata sınıfı
function netAttempt(fail) {
  NET.try++; NET.result = 'TRYING'; NET.fail = 'NONE'; NET.phase = 'CONNECTING';
  const wasAp = flags.ap;
  setTimeout(() => {
    if (fail) { NET.result = 'FAILED'; NET.fail = fail; NET.phase = 'WAITING'; NET.retryAt = Date.now() + 300000; flags.ap = true; return; }
    NET.result = 'CONNECTED'; NET.phase = 'ONLINE';
    if (wasAp) { NET.hold = Date.now() + 120000; setTimeout(() => { if (NET.hold && NET.hold <= Date.now() + 500) { NET.hold = 0; flags.ap = false; } }, 120000); }
    else flags.ap = false;
    ev('INFO', 'NET', 'Wi-Fi bağlandı (' + flags.ssid + ')');
  }, 4000);
}
const json = (o, status) => new Response(JSON.stringify(o), {status: status || 200, headers: {'Content-Type': 'application/json'}});
const realFetch = window.fetch.bind(window);
window.fetch = async function (url, opt) {
  const u = new URL(url, location.href);
  if (!u.pathname.startsWith('/api/') && u.pathname !== '/scan') return realFetch(url, opt);
  await new Promise(r => setTimeout(r, R(20, 70)));
  if (flags.offline) return json({message: 'Cihaza ulaşılamıyor'}, 503);
  const body = opt && opt.body ? JSON.parse(opt.body) : null;
  const p = u.pathname;
  if (p === '/api/data') return json(data());
  if (p === '/api/cmd') {
    if (flags.control === 'reject') return json({result: 'REJECTED_BUSY', message: 'Komut kuyruğu dolu'}, 503);
    const r = cmd(body.id, body.value);
    S.lastCmdSrc = 'LOCAL_WEB';
    r.id = body.id; r.value = body.value; r.seq = S.seq + 1; r.reason = r.reason || 'NONE';
    if (flags.control === 'delay') await new Promise(res => setTimeout(res, 2500));
    return r.result === 'ACCEPTED' || r.result === 'OVERRIDDEN' ? json(r) : json(Object.assign({message: r.result}, r), 409);
  }
  if (p === '/api/programs' && !body) {
    const t = pgNow(), r = PG.enabled ? pgEval(t, true) : {climate: -1, vent: -1, until: -1, vent_until: -1, held: false, next_change: -1};
    return json({enabled: PG.enabled, time_valid: true, now: t, tz_offset_min: TZ, active: {climate: r.climate, vent: r.vent, until: r.until, vent_until: r.vent_until, held: r.held},
      next_change: r.next_change, list: PG.list});
  }
  if (p === '/api/programs') {
    const err = pgValidate(body.list);
    if (err) return json(Object.assign(err, {message: 'Program listesi reddedildi: ' + err.code}), 400);
    PG.list = body.list.map(x => Object.assign({}, x, {name: String(x.name).trim()}));
    PG.hold = null; PG.resMin = -1; PG.last = {climate: -1, vent: -1};
    S.cfgRev++;
    ev('WARNING', 'CONFIG', 'Program listesi değişti (' + PG.list.length + ' program)');
    return json({message: 'Kaydedildi'});
  }
  if (p === '/api/trend') return json(trend(Number(u.searchParams.get('win')) || 900));
  if (p === '/scan') {
    if (flags.scanN++ < 2) return json({pending: true, networks: []});
    flags.scanN = 0;
    return json({pending: false, networks: [
      {ssid: 'Kulube-Ag', rssi: -58, secure: true, channel: 6}, {ssid: 'Kulube-Ag', rssi: -71, secure: true, channel: 11},
      {ssid: 'Misafir', rssi: -69, secure: false, channel: 1}, {ssid: 'TurkTelekom_ZX91', rssi: -80, secure: true, channel: 9},
      {ssid: 'Atolye 5G', rssi: -62, secure: true, channel: 36}]});
  }
  if (p === '/api/settings' && body && 'ssid' in body && Object.keys(body).every(k => k === 'ssid' || k === 'pass')) {
    if (!body.ssid || body.ssid.length > 32) return json({message: 'SSID 1–32 karakter olmalı', field: 'ssid'}, 400);
    if (body.pass && (body.pass.length < 8 || body.pass.length > 64)) return json({message: 'Wi-Fi parolası 8–64 karakter veya boş (açık ağ) olmalı', field: 'pass'}, 400);
    const base = NET.try;
    flags.ssid = body.ssid; flags.passSet = !!body.pass;
    ev('WARNING', 'NET', 'Wi-Fi kimliği değişti: ' + body.ssid);
    netAttempt(body.ssid === 'TurkTelekom_ZX91' ? 'NOT_FOUND' : body.pass === 'yanlisparola' ? 'AUTH' : null);
    return json({message: 'Ayarlar kaydedildi. Cihaz “' + body.ssid + '” ağına bağlanmayı deneyecek.', reconnect: true, net_try_base: base});
  }
  if (p === '/api/net/retry') { if (!flags.ssid) return json({message: 'Kayıtlı Wi-Fi ağı yok'}, 409); const base = NET.try; netAttempt(NET.fail !== 'NONE' ? NET.fail : null); return json({message: 'Kayıtlı ağ yeniden deneniyor.', net_try_base: base}); }
  if (p === '/api/net/finish') { if (!(NET.hold > Date.now())) return json({message: 'Kurulum ağı devir durumunda değil'}, 409); NET.hold = 0; setTimeout(() => { flags.ap = false; }, 300); return json({message: 'Kurulum ağı kapatılıyor.'}); }
  if (p === '/api/reset-wifi') { NET.phase = 'AP_ONLY'; NET.result = 'NONE'; NET.try++; flags.ap = true; flags.ssid = ''; ev('WARNING', 'NET', 'Wi-Fi kimliği silindi; kurulum AP’si açıldı'); return json({message: 'Wi-Fi silindi; kurulum AP’si açıldı (SCADA_AP_3C71BF4A)'}); }
  if (p === '/api/settings' && !body) {
    const out = Object.assign({}, cfg, {otaPasswordSet: !!flags.otaPw, mqPwSet: true, servicePinSet: true, ssid: flags.ap ? (flags.ssid || '') : (flags.ssid || 'Kulube-Ag'), passSet: flags.passSet !== false});
    return json(out);
  }
  if (p === '/api/settings') {
    const cand = Object.assign({}, cfg);
    Object.keys(body).forEach(k => { if (!SECRET_KEYS.includes(k)) cand[k] = body[k]; });
    const err = validate(cand);
    if (err) return json(err, 400);
    Object.assign(cfg, cand);
    S.cfgRev++;
    ev('WARNING', 'CONFIG', 'Konfigürasyon değişti (rev ' + S.cfgRev + ')');
    return json({message: 'Kaydedildi (rev ' + S.cfgRev + ')'});
  }
  if (p === '/api/alarms') {
    const active = Object.values(S.alarms).filter(a => a.state !== 'normal').map(a => Object.assign({}, a, {latched: a.latched && a.state === 'latched' || a.latched && S.latched && a.code === 'OVERTEMPERATURE'}));
    return json({active, history: S.hist.slice(0, 30)});
  }
  if (p === '/api/alarms/ack') {
    Object.values(S.alarms).forEach(a => {
      if (body.code !== 'ALL' && a.code !== body.code) return;
      if (a.state === 'active_unacknowledged') a.state = 'active_acknowledged';
      else if (a.state === 'cleared_unacknowledged') a.state = a.latched && S.latched ? 'latched' : 'normal';
    });
    S.ackCount++;
    return json({message: 'Onaylandı'});
  }
  if (p === '/api/alarms/reset') {
    if (S.latched && S.Tf >= cfg.cabin_overtemp_limit - cfg.overtemp_reset_hysteresis) return json({message: 'Koşul sürüyor: T1 ' + S.Tf.toFixed(1) + ' °C ≥ ' + (cfg.cabin_overtemp_limit - cfg.overtemp_reset_hysteresis).toFixed(1) + ' °C'}, 409);
    S.latched = false;
    Object.values(S.alarms).forEach(a => { if (a.state === 'latched') a.state = 'normal'; });
    ev('WARNING', 'SAFETY', 'Güvenlik kilidi sıfırlandı (yerel web)');
    return json({message: 'Kilit sıfırlandı'});
  }
  if (p === '/api/events') return json({events: S.events, overwritten: 0});
  if (p === '/api/session') return json({user: 'admin', role: 'admin', expires: '8 sa'});
  if (p === '/api/login') return body.password ? json({message: 'Giriş yapıldı'}) : json({message: 'Kullanıcı adı veya parola hatalı'}, 401);
  if (p === '/api/logout') return json({message: 'Çıkış yapıldı'});
  if (p === '/api/password') return json({message: 'Parola kaydedildi; bütün oturumlar kapatıldı'});
  if (p === '/api/service/pin') return json({message: 'Kaydedildi'});
  if (p === '/api/service/enter') {
    if (!body.pin) return json({message: 'Servis PIN’i gerekli'}, 403);
    if (S.demand > 0 || S.pc || S.R[0] || S.R[1]) return json({message: 'Isıtma durup soğutma bitmeden servis moduna girilemez'}, 409);
    S.service = true; S.svcT = 0; ev('WARNING', 'SERVICE', 'Servis modu etkin'); return json({message: 'Servis modu etkin'});
  }
  if (p === '/api/service/exit') { S.service = false; S.svcTest = [false, false, false, false]; ev('INFO', 'SERVICE', 'Servis modundan çıkıldı'); return json({message: 'Çıkıldı'}); }
  if (p === '/api/service/test') {
    if (!S.service) return json({message: 'Yalnız servis modunda'}, 409);
    if (body.on && body.out < 2 && S.svcTest[1 - body.out]) return json({message: 'Tek seferde tek rezistans test edilebilir'}, 409);
    S.svcTest[body.out] = body.on; S.svcT = 0; ev('WARNING', 'SERVICE', 'Çıkış testi ' + ['R1', 'R2', 'HF', 'VF'][body.out] + (body.on ? ' ON' : ' OFF'));
    return json({message: 'Tamam'});
  }
  if (p === '/api/service/reset-counters') { ev('WARNING', 'SERVICE', 'Sayaçlar sıfırlandı: ' + body.out); return json({message: 'Sayaçlar sıfırlandı'}); }
  if (p === '/api/ota/password') {
    if (body.password && (body.password.length < 8 || body.password.length > 64)) return json({message: 'OTA parolası 8–64 karakter olmalı.', field: 'otaPw'}, 400);
    flags.otaPw = !!body.password;
    ev('WARNING', 'CONFIG', body.password ? 'OTA parolası değişti' : 'OTA parolası kaldırıldı (parolasız OTA)');
    return json({message: body.password ? 'OTA parolası kaydedildi; yüklemede --auth gerekir.' : 'OTA parolası kaldırıldı; OTA parolasız açık.', otaPasswordSet: !!body.password});
  }
  if (p === '/api/ota/begin') return json({message: 'Önizleme: firmware yazılmaz'}, 409);
  if (['/api/reboot', '/api/factory-reset'].includes(p)) return json({message: 'Önizleme: cihaz işlemi yapılmadı'});
  return json({message: 'Bilinmeyen uç: ' + p}, 404);
};

// ---------------------------------------------------------------- önizleme paneli (cihaz arayüzünün parçası değildir)
function panel() {
  const st = document.createElement('style');
  st.textContent = '.mock{position:fixed;left:16px;bottom:calc(16px + env(safe-area-inset-bottom,0px));z-index:60;max-width:min(330px,calc(100vw - 32px));background:var(--bg-raised);border:1px dashed var(--amber);border-radius:6px;padding:6px 10px;font:400 11px var(--font-mono);color:var(--text)}' +
    '.mock summary{cursor:pointer;color:var(--amber)}.mock .g{display:grid;gap:6px;margin-top:6px}.mock label{display:flex;gap:6px;align-items:center}.mock select{min-height:28px}';
  document.head.append(st);
  const d = document.createElement('details');
  d.className = 'mock';
  d.innerHTML = '<summary>Önizleme · sahte cihaz</summary><div class="g">' +
    '<label>Hız <select id="mk-speed"><option value="1">×1 gerçek</option><option value="10" selected>×10</option><option value="60">×60</option></select></label>' +
    '<label><input type="checkbox" id="mk-unplug"> Sensör kablosunu çek</label>' +
    '<label><input type="checkbox" id="mk-ot"> Aşırı sıcaklık (+25 °C okuma)</label>' +
    '<label><input type="checkbox" id="mk-broker" checked> MQTT broker erişilebilir</label>' +
    '<label>Komut <select id="mk-ctl"><option value="apply">uygula</option><option value="delay">gecikmeli</option><option value="reject">reddet (503)</option></select></label>' +
    '<label><input type="checkbox" id="mk-off"> Cihaz çevrimdışı (bayat veri)</label>' +
    '<label><input type="checkbox" id="mk-ap"> AP kurulum modu</label>' +
    '<label>Dış sıcaklık <select id="mk-out"><option value="-10">−10 °C</option><option value="-2" selected>−2 °C</option><option value="10">10 °C</option><option value="30">30 °C</option></select></label>' +
    '<span>Başlangıçta 3 sa geçmiş simüle edildi. Firmware işlemleri yapılmaz.</span></div>';
  document.body.append(d);
  const g = id => document.getElementById(id);
  g('mk-speed').onchange = e => { flags.speed = Number(e.target.value); };
  g('mk-unplug').onchange = e => { flags.unplug = e.target.checked; };
  g('mk-ot').onchange = e => { flags.overtemp = e.target.checked; };
  g('mk-broker').onchange = e => { flags.broker = e.target.checked; };
  g('mk-ctl').onchange = e => { flags.control = e.target.value; };
  g('mk-off').onchange = e => { flags.offline = e.target.checked; };
  g('mk-ap').onchange = e => { flags.ap = e.target.checked; if (flags.ap) { flags.ssid = ''; NET.phase = 'AP_ONLY'; NET.result = 'NONE'; } else { flags.ssid = 'Kulube-Ag'; NET.phase = 'ONLINE'; NET.result = 'CONNECTED'; NET.hold = 0; } };
  setInterval(() => { if (g('mk-ap').checked !== flags.ap) g('mk-ap').checked = flags.ap; }, 1000);
  g('mk-out').onchange = e => { S.Tout = Number(e.target.value); };
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', panel); else panel();
})();
