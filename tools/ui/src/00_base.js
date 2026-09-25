/* Kulübe İklim Kontrolörü — gömülü web HMI (app.js).
   Kaynak: tools/ui/src/*.js → assemble.py birleştirir. Inline handler yok; kullanıcı verisi textContent ile. */
'use strict';
(function () {
const POLL_MS = 1000, STALE_MS = 4000, CONFIRM_MS = 5000;

// ---------------------------------------------------------------- ikonlar
const ICONS = {
  plus: 'M12 5v14M5 12h14', x: 'M6 6l12 12M18 6L6 18', save: 'M5 3h11l3 3v15H5zM8 3v6h8V3M8 21v-7h8v7',
  undo: 'M9 14L4 9l5-5M4 9h10a6 6 0 0 1 0 12h-3', trash: 'M4 7h16M10 11v6M14 11v6M6 7l1 14h10l1-14M9 7V4h6v3',
  pencil: 'M4 20h4L19 9l-4-4L4 16zM13.5 6.5l4 4', pause: 'M8 5v14M16 5v14', play: 'M7 4l13 8-13 8z', check: 'M5 12.5l4.5 4.5L19 7',
  unlock: 'M5 11h14v10H5zM8 11V7a4 4 0 0 1 7.5-2', key: 'M14.5 9.5a4 4 0 1 0-3 3.9L13 15h2v2h2v2h3v-3l-5.4-5.4',
  refresh: 'M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5', logout: 'M10 5H5v14h5M15 8l4 4-4 4M19 12H9',
  wifi: 'M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M9 16a4.5 4.5 0 0 1 6 0M12 19.5h.01',
  wifioff: 'M3 3l18 18M5.5 12.5a9.5 9.5 0 0 1 5-2.4M9 16a4.5 4.5 0 0 1 6 0M12 19.5h.01M2.5 9a14 14 0 0 1 7-3.6M14 5.3a14 14 0 0 1 7.5 3.7',
  reboot: 'M12 3v6M6.3 6.3a8 8 0 1 0 11.4 0', timer: 'M12 8v5l3 2M9 2h6M12 21a8 8 0 1 0 0-16 8 8 0 0 0 0 16z',
  factory: 'M3 21V10l6 4V10l6 4V4h6v17zM7 17h2M12 17h2M17 17h2', power: 'M12 3v9M6.35 5.65a8 8 0 1 0 11.3 0',
  warn: 'M12 3.5L2.5 20h19zM12 10v4.5M12 17.5v.01',
  info: 'M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18zM12 11v5M12 7.5v.01',
  theme: 'M20 14.5A8.5 8.5 0 1 1 9.5 4a7 7 0 0 0 10.5 10.5z',
  gauge: 'M4 18a8 8 0 1 1 16 0M12 18l4-6M8 18h8',
  thermo: 'M10 13.5V5a2 2 0 1 1 4 0v8.5a4 4 0 1 1-4 0zM12 10v6',
  chart: 'M4 4v16h16M7 15l4-5 3 3 5-7',
  bell: 'M6 16V11a6 6 0 1 1 12 0v5l2 2H4zM10 21h4',
  doc: 'M6 3h9l4 4v14H6zM14 3v5h5M9 13h7M9 17h7',
  gear: 'M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6zM19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z',
  user: 'M12 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM4 21a8 8 0 0 1 16 0',
  fan: 'M12 12m-2 0a2 2 0 1 0 4 0 2 2 0 1 0-4 0M12 10c0-4 1-7 4-7 2 0 3 2 1 4l-3 3M14 12c4 0 7 1 7 4 0 2-2 3-4 1l-3-3M12 14c0 4-1 7-4 7-2 0-3-2-1-4l3-3M10 12c-4 0-7-1-7-4 0-2 2-3 4-1l3 3',
  flame: 'M12 21a6 6 0 0 0 6-6c0-4-3-6-4-10-2 2-3 4-3 6-1-1-2-2-2-4-2 2-3 5-3 8a6 6 0 0 0 6 6z',
  drop: 'M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z',
  shield: 'M12 3l8 3v6c0 5-3.5 8-8 9-4.5-1-8-4-8-9V6z',
  antenna: 'M12 12v9M8.5 8.5a5 5 0 0 1 7 0M5.6 5.6a9 9 0 0 1 12.8 0M12 12h.01',
  sliders: 'M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0M14 4v4M8 10v4M16 16v4',
  sensor: 'M9 3h6v8H9zM12 11v4M8 21h8M12 15a3 3 0 1 0 0 6',
  wrench: 'M14.7 6.3a4 4 0 0 0-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 0 0 5.4-5.4l-2.5 2.5-2.4-.6-.6-2.4z',
  lock: 'M5 11h14v10H5zM8 11V7a4 4 0 0 1 8 0v4',
  calendar: 'M4 6h16v14H4zM4 10h16M8 3v4M16 3v4M8 14h2M14 14h2M8 17h2',
  download: 'M12 4v11M7 10l5 5 5-5M5 20h14',
  upload: 'M12 20V9M7 14l5-5 5 5M5 4h14',
  bulb: 'M9 18h6M10 21h4M12 3a6 6 0 0 0-3.8 10.6c.7.6 1.3 1.5 1.3 2.4h5c0-.9.6-1.8 1.3-2.4A6 6 0 0 0 12 3z'
};
const SVGNS = 'http://www.w3.org/2000/svg';
function icon(name, cls) {
  const s = document.createElementNS(SVGNS, 'svg');
  s.setAttribute('viewBox', '0 0 24 24');
  s.setAttribute('class', cls || 'ico');
  s.setAttribute('aria-hidden', 'true');
  const p = document.createElementNS(SVGNS, 'path');
  p.setAttribute('d', ICONS[name] || ICONS.info);
  s.appendChild(p);
  return s;
}
// <button data-icon="x">Metin</button> → ikon düğme (metin sr-only); data-text → ikon + metin
function iconize(root) {
  (root || document).querySelectorAll('button[data-icon]:not([data-iz])').forEach(b => {
    b.setAttribute('data-iz', '1');
    const txt = b.textContent.trim();
    if (b.hasAttribute('data-text')) {
      b.textContent = '';
      b.append(icon(b.dataset.icon), document.createTextNode(txt));
      b.classList.add('icon-text');
    } else {
      b.textContent = '';
      const sr = document.createElement('span');
      sr.className = 'sr-only';
      sr.textContent = txt;
      b.append(icon(b.dataset.icon), sr);
      b.title = txt;
      b.classList.add('icon-only');
    }
  });
}

// ---------------------------------------------------------------- DOM yardımcıları
const $ = (s, r) => (r || document).querySelector(s);
const $$ = (s, r) => Array.from((r || document).querySelectorAll(s));
function h(tag, attrs, ...kids) {
  const e = document.createElement(tag);
  if (attrs) for (const k in attrs) {
    const v = attrs[k];
    if (v === null || v === undefined || v === false) continue;
    if (k === 'class') e.className = v;
    else if (k === 'text') e.textContent = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2), v);
    else e.setAttribute(k, v === true ? '' : v);
  }
  for (const c of kids.flat()) if (c !== null && c !== undefined && c !== false) e.append(c.nodeType ? c : document.createTextNode(String(c)));
  return e;
}
function setText(el, t) { if (el && el.textContent !== t) el.textContent = t; }
function lsGet(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }
function lsSet(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* tarayıcı depolaması yok */ } }

// ---------------------------------------------------------------- biçim
const fmt = {
  t: v => (v === null || v === undefined || Number.isNaN(v)) ? '—' : Number(v).toFixed(1),
  pct: v => (v === null || v === undefined) ? '—' : Number(v).toFixed(0) + ' %',
  dur(s) {
    if (s === null || s === undefined) return '—';
    s = Math.max(0, Math.round(s));
    const d = Math.floor(s / 86400), hh = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
    if (d) return d + ' g ' + hh + ' sa';
    if (hh) return hh + ' sa ' + m + ' dk';
    if (m) return m + ' dk ' + (s % 60) + ' s';
    return s + ' s';
  },
  hm(min) { min = Math.round(min || 0); return Math.floor(min / 60) + ':' + String(min % 60).padStart(2, '0'); },
  kb: b => (b / 1024).toFixed(0) + ' KB',
  int: v => Number(v || 0).toLocaleString('tr-TR'),
  clock(ts) { const d = new Date(ts * 1000); return d.toLocaleTimeString('tr-TR', {hour12: false}); },
  age(ms) { const s = Math.round(ms / 1000); return s <= 1 ? 'şimdi' : s + ' sn önce'; }
};

// ---------------------------------------------------------------- sözlükler
const MODE_TR = {OFF: 'KAPALI', AUTO: 'OTO', MANUAL: 'MANUEL', VENT_ONLY: 'HAVALANDIRMA'};
const STATE_TR = {BOOT: 'AÇILIYOR', SELF_TEST: 'ÖZ TEST', RECOVERY: 'KURTARMA', OTA: 'GÜNCELLEME', FAILSAFE: 'GÜVENLİ DURUM',
  SERVICE: 'SERVİS', HEATING: 'ISITIYOR', POST_COOL: 'SOĞUTMA', VENTILATING: 'HAVALANDIRIYOR', MANUAL: 'MANUEL', OFF: 'KAPALI', IDLE: 'BEKLEMEDE'};
const STATE_ICON = {HEATING: 'flame', POST_COOL: 'fan', VENTILATING: 'fan', FAILSAFE: 'warn', OTA: 'upload', SERVICE: 'wrench', OFF: 'power'};
const STATE_SEV = {FAILSAFE: 'crit', RECOVERY: 'crit', SERVICE: 'warn', OTA: 'warn', HEATING: 'ok', POST_COOL: 'ok', VENTILATING: 'ok'};
const VSTATE_TR = {OFF: 'KAPALI', AUTO: 'OTOMATİK', MANUAL: 'MANUEL', FORCED: 'ZORUNLU (güvenlik)', INHIBITED: 'ENGELLİ'};
const PROFILE_TR = {DAY: 'GÜNDÜZ', NIGHT: 'GECE', AWAY: 'UZAKTA', FROST: 'DONMA', BOOST: 'BOOST', ANTIFREEZE: 'DONMA KORUMASI', MANUAL: 'MANUEL', PROGRAM: 'PROGRAM'};
const HREASON_TR = {PID: 'PID', MANUAL: 'Manuel talep', ANTIFREEZE: 'Donma koruması', BOOST: 'Boost', SERVICE_TEST: 'Servis testi', NONE: '—'};
const FAILSAFE_TR = {SENSOR_FAULT: 'Sensör arızası', OVERTEMP: 'Aşırı sıcaklık', HEATING_TIMEOUT: 'Isıtma süre aşımı', CONFIG_ERROR: 'Konfigürasyon hatası',
  INTERNAL_FAULT: 'İç hata', HEATER_FAN_FAULT: 'Isıtıcı fanı arızası', OUTPUT_FAULT: 'Çıkış arızası', TEMP_RISE: 'Beklenmeyen sıcaklık artışı', NONE: '—'};
const QUALITY_TR = {GOOD: 'İYİ', UNCERTAIN: 'ŞÜPHELİ', STALE: 'BAYAT', BAD: 'HATALI', MISSING: 'YOK', DISABLED: 'DONANIMDA YOK'};
const REASON_TR = {NONE: '—', HEATER_INTERLOCK: 'Isıtıcı interlock’u', POST_COOL: 'Soğutma (post-cool)', PREPURGE: 'Ön havalandırma',
  FAN_PRESTART: 'Fan ön çalışması', BOOT_POST_COOL: 'Açılış soğutması', SAFETY_LOCKOUT: 'Güvenlik kilidi', OVERTEMPERATURE: 'Aşırı sıcaklık',
  OVERTEMPERATURE_LOCKOUT: 'Aşırı sıcaklık kilidi', SENSOR_FAULT: 'Sensör arızası', ANTIFREEZE_INHIBIT: 'Donma koruması engeli',
  HEATING_PRIORITY: 'Isıtma önceliği', VENT_PRIORITY: 'Havalandırma önceliği', MIN_ON_TIME: 'Asgari açık süre', MIN_OFF_TIME: 'Asgari kapalı süre',
  CHANGEOVER_DELAY: 'Geçiş gecikmesi', LOCAL_LOCK: 'Yerel kilit', SERVICE_ONLY: 'Yalnız servis modunda', SERVICE_TEST: 'Servis testi',
  OTA: 'Güncelleme', CONTROLLER_DISABLED: 'Kontrolör kapalı', AUTO_DEMAND: 'Otomatik istek', MODE_OFF: 'Mod KAPALI'};
const REASON_SEV = {SAFETY_LOCKOUT: 'crit', OVERTEMPERATURE: 'crit', OVERTEMPERATURE_LOCKOUT: 'crit', SENSOR_FAULT: 'crit',
  HEATER_INTERLOCK: 'warn', ANTIFREEZE_INHIBIT: 'warn', OTA: 'warn', CONTROLLER_DISABLED: 'warn', MODE_OFF: 'warn', LOCAL_LOCK: 'warn',
  HEATING_PRIORITY: 'warn', VENT_PRIORITY: 'warn'};
const RESULT_TR = {ACCEPTED: 'Uygulandı', OVERRIDDEN: 'İstek kaydedildi', REJECTED_INVALID: 'Geçersiz değer', REJECTED_RELATION: 'Ayar ilişkisi bozuluyor',
  REJECTED_POLICY: 'Yetki/politika nedeniyle reddedildi', REJECTED_BUSY: 'Cihaz meşgul', REJECTED_STATE: 'Mevcut durumda yapılamaz'};
const OUT = [
  {k: 'r1', no: '01', name: 'R1 rezistans', short: 'R1', ico: 'flame', req: null},
  {k: 'r2', no: '02', name: 'R2 rezistans', short: 'R2', ico: 'flame', req: null},
  {k: 'heater_fan', no: '03', name: 'Isıtıcı fanı', short: 'Isıt. fanı', ico: 'fan', req: 'heater_fan_manual'},
  {k: 'ventilation_fan', no: '04', name: 'Havalandırma', short: 'Havalan.', ico: 'fan', req: 'ventilation_fan_manual'}
];
const ALARM_TR = {SENSOR_FAULT: 'Sensör arızası', SENSOR_STALE: 'Sensör verisi bayat', OVERTEMPERATURE: 'Aşırı sıcaklık', HEATER_FAN_FAULT: 'Isıtıcı fanı arızası',
  HEATING_TIMEOUT: 'Isıtma süre aşımı', HEATING_PERFORMANCE_LOW: 'Isıtma performansı düşük', UNEXPECTED_TEMPERATURE_RISE: 'Beklenmeyen sıcaklık artışı',
  OUTPUT_FAULT: 'Çıkış arızası', MQTT_OFFLINE: 'MQTT bağlantısı yok', WIFI_OFFLINE: 'Wi-Fi bağlantısı yok', CONFIGURATION_ERROR: 'Konfigürasyon hatası',
  WATCHDOG_RESET: 'Watchdog yeniden başlatması', RESTART_STORM: 'Aşırı yeniden başlatma', INTERNAL_FAULT: 'İç yazılım hatası',
  FROST_RISK_NO_SENSOR: 'Donma riski, sensör yok', HUMIDITY_HIGH: 'Nem yüksek', HUMIDITY_LOW: 'Nem düşük', TIME_INVALID: 'Saat geçersiz',
  RELAY_LIFE_WARNING: 'Röle ömrü uyarısı', POST_COOL_TIMEOUT: 'Soğutma süre aşımı'};
const ASTATE_TR = {active_unacknowledged: 'Aktif · Onaysız', active_acknowledged: 'Aktif · Onaylı', cleared_unacknowledged: 'Giderildi · Onaysız', latched: 'Giderildi · Sıfırlama gerekli'};
const SEV_CLASS = {CRITICAL: 'critical', WARNING: 'warn', INFO: 'info'};
const SEV_TR = {CRITICAL: 'KRİTİK', WARNING: 'UYARI', INFO: 'BİLGİ', NORMAL: 'NORMAL'};
