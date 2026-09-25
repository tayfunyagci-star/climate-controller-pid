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
  upload: 'M12 20V9M7 14l5-5 5 5M5 4h14'
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

// ---------------------------------------------------------------- API
async function api(path, body, method) {
  const opt = {cache: 'no-store', credentials: 'same-origin', headers: {}};
  if (body !== undefined) {
    opt.method = method || 'POST';
    opt.headers['Content-Type'] = 'application/json';
    opt.headers['X-SCADA'] = '1';
    opt.body = JSON.stringify(body);
  }
  const r = await fetch(path, opt);
  let j = null;
  try { j = await r.json(); } catch (e) { j = null; }
  if (!r.ok) {
    const err = new Error((j && j.message) || ('HTTP ' + r.status));
    err.status = r.status;
    err.body = j;
    throw err;
  }
  return j;
}

// ---------------------------------------------------------------- bildirim + diyalog
function toast(msg, isErr) {
  const box = $('#toasts');
  const t = h('div', {class: 'toast' + (isErr ? ' err' : ''), role: isErr ? 'alert' : 'status'}, icon(isErr ? 'warn' : 'check'), h('span', {text: msg}));
  const close = h('button', {type: 'button', 'data-icon': 'x', onclick: () => t.remove()}, 'Kapat');
  t.append(close);
  iconize(t);
  box.append(t);
  while (box.children.length > 3) box.firstChild.remove();
  if (!isErr) setTimeout(() => t.remove(), 5000);
}
let dlgResolve = null, dlgTrigger = null;
function confirmDlg(title, body, okText, danger) {
  const d = $('#dlg');
  setText($('#dlg-title'), title);
  const b = $('#dlg-body');
  b.textContent = '';
  if (typeof body === 'string') b.append(h('p', {text: body}));
  else b.append(body);
  const ok = $('#dlg-ok');
  ok.textContent = okText || 'Onayla';
  ok.className = danger ? 'danger' : 'primary';
  dlgTrigger = document.activeElement;
  return new Promise(res => {
    dlgResolve = res;
    if (d.showModal) d.showModal(); else d.setAttribute('open', '');
  });
}
function closeDlg(v) {
  const d = $('#dlg');
  if (d.open) d.close();
  if (dlgResolve) { dlgResolve(v); dlgResolve = null; }
  if (dlgTrigger && dlgTrigger.focus) dlgTrigger.focus();
}

// ---------------------------------------------------------------- canlı veri
let D = null;            // son /api/data
let lastOk = 0;          // son başarılı veri zamanı
let seenBuild = null;
const pageUpdaters = {}; // bölüm → güncelleme fonksiyonu
function stale() { return !D || Date.now() - lastOk > STALE_MS; }

async function poll() {
  try {
    const d = await api('/api/data');
    if (seenBuild && d.fw_build && d.fw_build !== seenBuild) toast('Yeni firmware çalışıyor: ' + d.fw_build);
    seenBuild = d.fw_build || seenBuild;
    D = d;
    lastOk = Date.now();
  } catch (e) { /* bayatlık aşağıda görünür */ }
  renderAll();
}
function renderAll() {
  renderShell();
  const r = currentRoute;
  if (pageUpdaters[r]) pageUpdaters[r](D, stale());
  pending.forEach(p => p.check());
  wifiTick();
}

// ---------------------------------------------------------------- komut akışı
// hazır → gönderiliyor → onay bekleniyor → onaylandı | reddedildi | zaman aşımı
const pending = new Set();
function fieldMatches(d, id, value) {
  if (!d || !(id in d)) return true;  // durum alanı olmayan komut (ör. alarm_ack): kabul yeterli
  const cur = d[id];
  if (typeof cur === 'number') return Math.abs(cur - Number(value)) < 1e-6;
  return String(cur) === String(value);
}
async function sendCmd(id, value, ui) {
  // ui: {btns:[], msg: el, label}
  if (ui && ui.busy) return;  // ikinci tıklama engellenir
  if (stale()) { showMsg(ui, 'critical', 'Veri bayat: komut gönderilmedi.'); return; }
  const btns = (ui && ui.btns) || [];
  const setBusy = on => {
    if (ui) ui.busy = on;
    btns.forEach(b => { b.setAttribute('aria-busy', on ? 'true' : 'false'); b.setAttribute('aria-disabled', on ? 'true' : 'false'); });
  };
  setBusy(true);
  showMsg(ui, 'info', 'Gönderiliyor…');
  let res;
  try {
    res = await api('/api/cmd', {id, value: String(value)});
  } catch (e) {
    setBusy(false);
    const b = e.body || {};
    showMsg(ui, 'critical', (RESULT_TR[b.result] || e.message) + (b.reason && b.reason !== 'NONE' ? ' · ' + (REASON_TR[b.reason] || b.reason) : '') + '.');
    return;
  }
  if (!res || (res.result !== 'ACCEPTED' && res.result !== 'OVERRIDDEN')) {
    setBusy(false);
    showMsg(ui, 'critical', (RESULT_TR[res && res.result] || 'Reddedildi') + '.');
    return;
  }
  showMsg(ui, 'info', 'Onay bekleniyor…');
  const t0 = Date.now();
  const p = {
    check() {
      if (D && D.seq >= (res.seq || 0) && fieldMatches(D, id, value)) {
        pending.delete(p);
        setBusy(false);
        if (res.result === 'OVERRIDDEN')
          showMsg(ui, 'warn', 'İstek kaydedildi; etkin durum farklı: ' + (REASON_TR[res.reason] || res.reason) + '.');
        else showMsg(ui, null, '');
        if (ui && ui.onDone) ui.onDone();
      } else if (Date.now() - t0 > CONFIRM_MS) {
        pending.delete(p);
        setBusy(false);
        showMsg(ui, 'critical', 'Zaman aşımı: cihaz durumu komutu doğrulamadı.');
      }
    }
  };
  pending.add(p);
}
function showMsg(ui, sev, text) {
  if (!ui || !ui.msg) return;
  const m = ui.msg;
  m.textContent = '';
  m.className = 'cmd-msg';
  if (!text) return;
  const r = h('div', {class: 'notice ' + (sev || 'info'), role: sev === 'critical' ? 'alert' : 'status'}, icon(sev === 'info' ? 'timer' : 'warn'), h('span', {text}));
  m.append(r);
}

// ---------------------------------------------------------------- yönlendirme
const ROUTES = {'/': 'overview', '/control': 'control', '/programs': 'programs', '/trends': 'trends', '/outputs': 'outputs', '/alarms': 'alarms',
  '/events': 'events', '/settings': 'settings', '/login': 'login'};
const useHash = !!window.__SCADA_PREVIEW__;
let currentRoute = 'overview';
const built = {};
function routeFromLocation() {
  const hsh = location.hash.replace('#', '');
  if (useHash && hsh && $('#' + hsh) && $('#' + hsh).tagName === 'SECTION') return hsh;
  return ROUTES[location.pathname] || 'overview';
}
function go(route, push) {
  currentRoute = route;
  $$('main > section').forEach(s => { s.hidden = s.id !== route; });
  $$('nav.menu a').forEach(a => {
    if (a.dataset.route === route) a.setAttribute('aria-current', 'page');
    else a.removeAttribute('aria-current');
  });
  if (!built[route] && builders[route]) { builders[route]($('#' + route)); built[route] = true; iconize($('#' + route)); }
  document.title = (D && D.device_name ? D.device_name : 'Kulübe İklim') + ' · ' + $('#' + route).dataset.title;
  if (push) {
    const path = Object.keys(ROUTES).find(k => ROUTES[k] === route) || '/';
    if (useHash) history.replaceState(null, '', '#' + route);
    else history.pushState(null, '', path);
  }
  renderShell();
  if (pageUpdaters[route]) pageUpdaters[route](D, stale());
  if (onEnter[route]) onEnter[route]();
}
const builders = {};
const onEnter = {};

// ---------------------------------------------------------------- ortak başlık / durum çubuğu
function sectionHead(title, extra) {
  const sb = h('div', {class: 'statusbar', 'aria-label': 'Bağlantı durumu'},
    h('span', {class: 'pill live', 'data-pill': 'live'}, h('i'), h('span', {text: 'Bağlanıyor'})),
    h('span', {class: 'pill', 'data-pill': 'wifi'}, h('i'), h('span', {text: 'Wi-Fi · —'})),
    h('span', {class: 'pill', 'data-pill': 'mqtt'}, h('i'), h('span', {text: 'MQTT · —'})),
    h('span', {class: 'pill', 'data-pill': 'sensor'}, h('i'), h('span', {text: 'Sensör · —'})),
    h('span', {class: 'pill', 'data-pill': 'time'}, h('i'), h('span', {text: 'Saat · —'})));
  return h('div', {class: 'section-head'}, h('h2', {text: title}), extra || null, sb);
}
function setPill(el, cls, text) {
  el.classList.remove('ok', 'bad', 'warn');
  if (cls) el.classList.add(cls);
  setText(el.lastChild, text);
}
function renderShell() {
  const st = stale();
  $$('[data-pill=live]').forEach(p => {
    p.classList.toggle('stale', st);
    setText(p.lastChild, !D ? 'Bağlanıyor' : (st ? 'Bayat · ' + fmt.age(Date.now() - lastOk) : 'Canlı · ' + fmt.age(Date.now() - lastOk)));
  });
  if (!D) return;
  const q = D.temperature_quality;
  $$('[data-pill=wifi]').forEach(p => setPill(p, D.wifi_ok ? 'ok' : 'bad', 'Wi-Fi · ' + (D.wifi_ok ? 'Hazır' : 'Yok')));
  $$('[data-pill=mqtt]').forEach(p => setPill(p, D.mqtt_status === 'CONNECTED' ? 'ok' : (D.mqtt_status === 'DISABLED' ? null : 'bad'),
    'MQTT · ' + ({CONNECTED: 'Hazır', DISABLED: 'Kapalı', CONNECTING: 'Bağlanıyor', BACKOFF: 'Yok', AUTH_FAIL: 'Kimlik hatası'}[D.mqtt_status] || '—')));
  $$('[data-pill=sensor]').forEach(p => setPill(p, q === 'GOOD' ? 'ok' : (q === 'UNCERTAIN' ? 'warn' : 'bad'), 'Sensör · ' + (QUALITY_TR[q] || '—')));
  $$('[data-pill=time]').forEach(p => setPill(p, D.time_valid === 'ON' ? 'ok' : 'warn', 'Saat · ' + (D.time_valid === 'ON' ? 'Eşitli' : 'Bekleniyor')));
  // kimlik
  setText($('#dev-name'), D.device_name || 'Kulübe İklim');
  setText($('#id-ip'), D.ip || '—');
  setText($('#id-mdns'), (D.mdns || '—') + '.local');
  setText($('#id-client'), D.client_ip || '—');
  setText($('#id-fw'), D.fw_version || '—');
  setText($('#id-rev'), D.fw_build || '—');
  setText($('#foot-fw'), 'FW ' + (D.fw_version || '—') + ' · ' + (D.fw_build || '—'));
  setText($('#foot-time'), D.ts ? fmt.clock(D.ts) : 'Saat bekleniyor');
  // sistem durumu
  setText($('#diag-sum'), 'çalışma ' + fmt.dur(D.uptime) + ' · boş RAM ' + fmt.kb(D.free_heap || 0));
  renderDiag();
  renderGlobalNotices(st);
}
function renderDiag() {
  const m = $('#diag-metrics');
  if (!$('#diag').open) return;
  const rows = [
    ['Çalışma süresi', fmt.dur(D.uptime)], ['Boş / en düşük RAM', fmt.kb(D.free_heap || 0) + ' / ' + fmt.kb(D.min_heap || 0)],
    ['En uzun kontrol döngüsü', (D.control_loop_max_ms ?? '—') + ' ms'], ['Reset nedeni', D.reset_reason || '—'],
    ['Boot / hatalı boot', (D.boot_count ?? '—') + ' / ' + (D.fault_boot_count ?? '—')], ['Wi-Fi RSSI', (D.wifi_rssi ?? '—') + ' dBm'],
    ['MQTT yeniden bağlanma', D.mqtt_reconnects ?? '—'], ['Sensör hata oranı (10 dk)', (D.sensor_error_rate_10m ?? 0).toFixed(1) + ' %'],
    ['Sensör', D.sensor_model || '—'], ['Konfigürasyon rev.', D.config_rev ?? '—'], ['Durum sırası', D.seq ?? '—'], ['Kaynak', D.last_command_source || '—']
  ];
  if (m.children.length !== rows.length) {
    m.textContent = '';
    rows.forEach(r => m.append(h('div', null, h('dt', {text: r[0]}), h('dd', {text: String(r[1])}))));
  } else rows.forEach((r, i) => setText(m.children[i].lastChild, String(r[1])));
}
function renderGlobalNotices(st) {
  const box = $('#global-notices');
  const list = [];
  // Ağ değişikliği sırasında beklenen kopma: alarm yağmuru yerine tek sakin not (yönergeler Wi-Fi penceresinde)
  if (st && netTransitionActive()) list.push(['warn', 'Ağ değişikliği sürüyor: bu adresle bağlantı kesildi (son veri ' + (D ? fmt.age(Date.now() - lastOk) : '—') + ' önce). Kumandalar devre dışı; kontrol cihazda çalışmaya devam eder.']);
  else if (st) list.push(['critical', 'Veri bayat: son geçerli veri ' + (D ? fmt.age(Date.now() - lastOk) : '—') + '. Kumandalar devre dışı.']);
  if (D) {
    if (D.controller_state === 'FAILSAFE') list.push(['critical', 'GÜVENLİ DURUM · ' + (FAILSAFE_TR[D.failsafe_reason] || D.failsafe_reason) + '. Rezistanslar kapalı.']);
    if (D.controller_enable === 'OFF') list.push(['critical', 'Kontrolör kapalı: donma koruması dahil otomatik kontrol devre dışı.']);
    if (D.controller_state === 'SERVICE') list.push(['warn', 'SERVİS MODU etkin · kalan ' + fmt.dur(D.service_remaining_s) + '. Donma koruması devre dışı.']);
    if (D.local_lock === 'ON') list.push(['warn', 'Yerel kilit etkin: MQTT operasyonel komutları reddediliyor.']);
    if (D.ap_mode && D.net_setup !== 'HANDOVER' && !(currentRoute === 'overview' && !setupCollapsed))
      list.push(['warn', (D.wifi_ssid ? 'Cihaz kayıtlı Wi-Fi ağına bağlanamadı; ' : 'Wi-Fi kurulumu tamamlanmadı; ') + 'kurulum ağı “' + (D.ap_name || 'SCADA_AP') + '” açık (' + (D.ap_ip || '192.168.4.1') + '). Ağ seçimi: Genel Bakış.']);
    if (D.net_note) list.push(['warn', D.net_note]);
    if (D.password_set === false) list.push(['warn', 'Web parolası tanımlı değil. Ayarlar › Erişim bölümünden parola belirleyin.']);
  }
  const key = JSON.stringify(list);
  if (box.dataset.k === key) return;
  box.dataset.k = key;
  box.textContent = '';
  list.forEach(([sev, t]) => box.append(h('div', {class: 'notice ' + sev, role: sev === 'critical' ? 'alert' : 'status'}, icon('warn'), h('span', {text: t}))));
}
function reasonEl(code, remaining) {
  if (!code || code === 'NONE') return h('span', {class: 'dim', text: '—'});
  const sev = REASON_SEV[code] || 'info';
  const txt = (REASON_TR[code] || code) + (remaining ? ' · ' + remaining + ' s' : '');
  return h('span', {class: 'reason ' + sev, title: code}, icon(sev === 'info' ? 'timer' : 'warn'), h('span', {text: txt}));
}
function onoff(v) { return v === 'ON' || v === true; }

// ================================================================= KURULUM + Wi-Fi KURTARMA AKIŞI
// scada-wifi-onboarding tasarımı (docs/NETWORK.md §3). Temel kurallar:
//   - Kaydedildi ≠ bağlandı: başarı yalnız cihazın güncel verisi (net_try > taban, net_result=CONNECTED, sta_ip) ile gösterilir.
//   - Yanıt alınamadı ≠ işlem yapılmadı: istek otomatik tekrarlanmaz; sonuç net_try/wifi_ssid ile doğrulanır.
//   - Beklenen kopmada genel "veri bayat" alarmı yerine sakin durum notu; yönergeler sayfada kalır.
//   - Kopma nedeni cihazdan gelen sınıftır (net_fail) ve olasılık diliyle sunulur; süreler cihaz sayaçlarından.
// API: GET /scan · POST /api/settings {ssid, pass} → {net_try_base, reconnect} · POST /api/net/retry · POST /api/net/finish
//      /api/data: ap_mode, ap_name, ap_ip, mdns, wifi_ssid, wifi_ok, wifi_rssi, sta_ip, static_ip, net_phase, net_setup,
//                 net_try, net_result, net_fail, net_fail_code, net_retry_s, ap_close_s, time_valid
const enc = new TextEncoder();
const bytes = s => enc.encode(s).length;
const FAIL_TR = {
  NOT_FOUND: 'Cihaz bu ağı göremedi. Ağ 5 GHz olabilir, kapsama dışında olabilir ya da adı farklı yazılmış olabilir.',
  AUTH: 'Ağ bağlantıyı doğrulamadı. En olası neden yanlış parola; zayıf sinyal de aynı sonucu verebilir.',
  ASSOC: 'Erişim noktası bağlantıyı kabul etmedi (ör. istemci sınırı, MAC filtresi veya uyumsuz güvenlik ayarı).',
  NO_IP: 'Kablosuz bağlantı kuruldu ancak IP adresi alınamadı. Modemde DHCP’nin açık olduğunu kontrol edin.',
  SIGNAL_LOST: 'Erişim noktasının sinyali kayboldu. Cihaza daha yakın bir erişim noktası deneyin.',
  NONE: 'Cihaz bir hata nedeni bildirmedi.'
};
function failText(d) {
  if (!d) return FAIL_TR.NONE;
  let t = FAIL_TR[d.net_fail] || ('Cihaz bağlantıyı tamamlayamadı (kod ' + (d.net_fail_code || '—') + ').');
  if (d.net_fail === 'NO_IP' && d.static_ip) t += ' Sabit IP ayarları bu ağa uymuyor olabilir.';
  return t;
}
function rssiText(r) {
  if (r === null || r === undefined) return '—';
  return (r >= -55 ? 'Çok iyi' : r >= -67 ? 'İyi' : r >= -75 ? 'Orta' : 'Zayıf') + ' (' + r + ' dBm)';
}
const devUrl = d => 'http://' + ((d && d.mdns) || 'kulube-iklim') + '.local';
const apName = d => (d && d.ap_name) || 'SCADA_AP';
const apUrl = d => 'http://' + ((d && d.ap_ip) || '192.168.4.1');
// Sayfa kurulum ağı üzerinden mi açıldı (AP istemcisi 192.168.4.x)?
const onApLink = () => location.hostname === '192.168.4.1' || !!(D && D.ap_mode && !D.wifi_ok) || /^192\.168\.4\./.test((D && D.client_ip) || '');

// ---------------------------------------------------------------- geçiş durumu (NT)
// ctx: 'ap' (kurulum ağından) | 'sta' (normal ağdan) | 'reset' (Wi-Fi silindi)
// stage: sending → saved → (trying) → connected | failed | uncertain | finished
let NT = null;
function netTransitionActive() { return !!NT && !['failed', 'finished-sta'].includes(NT.stage) && Date.now() - NT.t0 < 15 * 60000; }
function ntFresh() { return !!D && !stale() && lastOk >= NT.sentAt; }

// ---------------------------------------------------------------- küçük bileşenler
function addrBox(label, url, id) {
  const val = h('input', {type: 'text', readonly: '', value: url, class: 'addr-val mono', id, 'aria-label': label});
  const copy = h('button', {type: 'button', 'data-icon': 'doc', 'data-text': ''}, 'Kopyala');
  const note = h('span', {class: 'field-hint', role: 'status'});
  copy.addEventListener('click', async () => {
    let ok = false;
    try { if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(val.value); ok = true; } } catch (e) { ok = false; }
    if (!ok) { val.focus(); val.select(); try { ok = document.execCommand('copy'); } catch (e) { ok = false; } }
    setText(note, ok ? 'Kopyalandı.' : 'Adres seçildi; uzun basıp kopyalayın.');
  });
  return h('div', {class: 'addr'}, h('label', {for: id, text: label}), h('div', {class: 'addr-row'}, val, copy), note);
}
function stageList() {
  const mk = (k, t) => h('li', {'data-k': k, class: 'pending'}, icon('timer'), h('span', {text: t}));
  return h('ol', {class: 'stages', 'aria-label': 'Bağlantı aşamaları'}, mk('saved', 'Ayarlar kaydedildi'), mk('try', 'Ağa bağlanılıyor'), mk('ok', 'Bağlantı cihazdan doğrulandı'));
}
function setStage(list, k, st, text) {
  const li = list.querySelector('[data-k=' + k + ']');
  if (!li) return;
  if (text) setText(li.lastChild, text);
  if (li.className === st) return;
  li.className = st;
  li.firstChild.replaceWith(icon(st === 'done' ? 'check' : st === 'fail' ? 'warn' : 'timer'));
  li.setAttribute('aria-current', st === 'active' ? 'step' : 'false');
}
function helpUnreachable() {
  const d = D;
  return h('details', {class: 'help'}, h('summary', {text: 'Cihaza ulaşamıyorum'}),
    h('ol', {class: 'steps'},
      h('li', {text: 'Telefonunuzun veya bilgisayarınızın seçtiğiniz Wi-Fi ağına bağlı olduğunu kontrol edin.'}),
      h('li', {text: 'Cihaz adresini yeniden açın: ' + devUrl(d) + ' — açılmazsa modem/router arayüzündeki bağlı cihazlar listesinden “' + ((d && d.mdns) || 'kulube-iklim') + '” adlı cihazın IP adresini bulun.'}),
      h('li', {text: 'Kurulum ağı “' + apName(d) + '” yeniden görünüyorsa cihaz ağa bağlanamamıştır: ona bağlanın (parola cihaz etiketinde) ve ' + apUrl(d) + ' adresini açın.'}),
      h('li', {text: 'Kurulum ağında bilgileri kontrol edip yeniden kaydedin.'})),
    h('p', {class: 'field-hint', text: '.local adları bazı Android ve Windows cihazlarda çözülmez; bu durumda IP adresini kullanın. Yeni adreste oturum yeniden istenebilir.'}),
    h('p', {class: 'field-hint', text: 'Hiçbiri olmazsa: cihazdaki BOOT düğmesini 10 saniye basılı tutun. Yalnız Wi-Fi bilgileri silinir ve kurulum ağı açılır; diğer ayarlar korunur.'}));
}

// ---------------------------------------------------------------- sihirbaz diyaloğu
let W = null;            // diyalog öğeleri
let wifiScanning = false, netCfg = null;
let sel = null;          // {ssid, secure, manual}

function wifiDialog() {
  if (W) return W;
  const stepper = h('ol', {class: 'stepper', 'aria-label': 'Kurulum adımları'},
    h('li', {'data-s': 'scan', text: 'Ağ seç'}), h('li', {'data-s': 'creds', text: 'Bilgiler'}), h('li', {'data-s': 'progress', text: 'Bağlantı'}));
  // --- 1. ağ seçimi
  const status = h('p', {id: 'wifi-status', role: 'status', class: 'scan-status'});
  const list = h('div', {id: 'networks', class: 'wifi-list', role: 'list', 'aria-label': 'Bulunan 2.4 GHz ağlar'});
  const rescan = h('button', {type: 'button', id: 'wifi-rescan', 'data-icon': 'refresh', 'data-text': ''}, 'Yeniden tara');
  const hiddenBtn = h('button', {type: 'button', id: 'wifi-hidden', 'aria-expanded': 'false', 'aria-controls': 'wifi-manual'}, 'Ağım görünmüyor');
  const mSsid = h('input', {type: 'text', id: 'wifi-m-ssid', autocomplete: 'off', autocapitalize: 'off', spellcheck: 'false', 'aria-describedby': 'wifi-m-err'});
  const mSec = h('input', {type: 'checkbox', id: 'wifi-m-sec', checked: ''});
  const mErr = h('p', {id: 'wifi-m-err', class: 'field-err', role: 'alert'});
  const mGo = h('button', {type: 'button', class: 'primary'}, 'Devam');
  const manual = h('div', {id: 'wifi-manual', class: 'manual', hidden: ''},
    h('ul', {class: 'steps'},
      h('li', {text: 'Modemin 2.4 GHz yayınının açık olduğundan emin olun; 5 GHz ağlar listede görünmez.'}),
      h('li', {text: 'Cihaza yaklaşın veya erişim noktasını yaklaştırıp yeniden tarayın.'}),
      h('li', {text: 'Gizli bir ağ kullanıyorsanız adını aşağıya büyük/küçük harfe dikkat ederek yazın.'})),
    h('div', {class: 'field'}, h('label', {for: 'wifi-m-ssid', text: 'Wi-Fi ağ adı (SSID)'}), mSsid, mErr),
    h('div', {class: 'field toggle'}, mSec, h('label', {for: 'wifi-m-sec', text: 'Bu ağ parola istiyor'})),
    h('div', {class: 'btn-row'}, mGo));
  const vScan = h('div', {'data-v': 'scan'},
    h('div', {class: 'notice info'}, icon('info'), h('span', {text: 'Yalnızca 2.4 GHz ağlar desteklenir.'})),
    status, list, h('div', {class: 'btn-row'}, rescan, hiddenBtn), manual);
  // --- 2. bilgiler
  const selName = h('p', {class: 'sel-ssid', id: 'wifi-sel'});
  const other = h('button', {type: 'button', class: 'link-btn'}, 'Başka ağ seç');
  const pw = h('input', {type: 'password', id: 'wifi-password', autocomplete: 'new-password', autocapitalize: 'off', spellcheck: 'false', 'aria-describedby': 'wifi-pw-err wifi-pw-hint'});
  const eye = h('button', {type: 'button', class: 'eye', 'aria-pressed': 'false', 'aria-controls': 'wifi-password'}, 'Göster');
  const pwErr = h('p', {id: 'wifi-pw-err', class: 'field-err'});
  const pwHint = h('p', {id: 'wifi-pw-hint', class: 'field-hint', text: '8–63 karakter. Büyük/küçük harf ayrımı vardır.'});
  const pwBox = h('div', {class: 'field'}, h('label', {for: 'wifi-password', text: 'Wi-Fi parolası'}), h('div', {class: 'pw-row'}, pw, eye), pwErr, pwHint);
  const openNote = h('div', {class: 'notice info', hidden: ''}, icon('info'), h('span', {text: 'Bu ağ parola istemiyor.'}));
  const staticNote = h('div', {class: 'notice info', hidden: ''}, icon('info'), h('span'));
  const plan = h('div', {class: 'plan', 'aria-label': 'Kaydettiğinizde'});
  const back = h('button', {type: 'button'}, 'Geri');
  const save = h('button', {type: 'submit', class: 'primary', 'data-icon': 'save', 'data-text': ''}, 'Kaydet ve bağlan');
  const saveErr = h('div', {class: 'cmd-msg'});
  const vCreds = h('form', {'data-v': 'creds', novalidate: ''},
    h('div', {class: 'sel-head'}, h('span', {class: 'lbl', text: 'Seçilen ağ'}), selName, other),
    pwBox, openNote, staticNote, plan, saveErr, h('div', {class: 'dlg-acts'}, back, save));
  // --- 3. bağlantı
  const stages = stageList();
  const pBody = h('div', {class: 'p-body', role: 'status', 'aria-live': 'polite'});
  const pActs = h('div', {class: 'dlg-acts'});
  const vProg = h('div', {'data-v': 'progress'}, stages, pBody, pActs);

  const close = h('button', {type: 'button', 'data-icon': 'x'}, 'Kapat');
  const dlg = h('dialog', {id: 'wifi-dialog', class: 'wide setup', 'aria-labelledby': 'wifi-dlg-t'},
    h('div', {class: 'dlg-head'}, h('h2', {id: 'wifi-dlg-t', text: 'Wi-Fi ağına bağlan'}), close), stepper, vScan, vCreds, vProg);
  document.body.append(dlg);
  iconize(dlg);
  let trigger = null;
  close.addEventListener('click', () => dlg.close());
  dlg.addEventListener('close', () => { pw.value = ''; pw.type = 'password'; eye.setAttribute('aria-pressed', 'false'); setText(eye, 'Göster'); if (trigger && trigger.focus) trigger.focus(); });
  rescan.addEventListener('click', scanWifi);
  hiddenBtn.addEventListener('click', () => {
    const open = manual.hidden;
    manual.hidden = !open;
    hiddenBtn.setAttribute('aria-expanded', open ? 'true' : 'false');
    if (open) mSsid.focus();
  });
  mGo.addEventListener('click', () => {
    const v = mSsid.value;
    const n = bytes(v);
    if (!n || n > 32) { setText(mErr, n ? 'Ağ adı en çok 32 bayt olabilir (Türkçe karakterler 2 bayt sayılır).' : 'Ağ adını yazın.'); mSsid.setAttribute('aria-invalid', 'true'); mSsid.focus(); return; }
    setText(mErr, ''); mSsid.removeAttribute('aria-invalid');
    choose({ssid: v, secure: mSec.checked, manual: true});
  });
  other.addEventListener('click', () => { show('scan'); if (!list.children.length) scanWifi(); });
  back.addEventListener('click', () => show('scan'));
  eye.addEventListener('click', () => {
    const on = pw.type === 'password';
    pw.type = on ? 'text' : 'password';
    eye.setAttribute('aria-pressed', on ? 'true' : 'false');
    setText(eye, on ? 'Gizle' : 'Göster');
    pw.focus();
  });
  pw.addEventListener('input', () => { setText(pwErr, ''); pw.removeAttribute('aria-invalid'); });
  vCreds.addEventListener('submit', e => { e.preventDefault(); submitWifi(); });

  function show(v) {
    [vScan, vCreds, vProg].forEach(x => { x.hidden = x.dataset.v !== v; });
    const order = ['scan', 'creds', 'progress'];
    stepper.querySelectorAll('li').forEach(li => {
      const i = order.indexOf(li.dataset.s), c = order.indexOf(v);
      li.className = i < c ? 'done' : i === c ? 'current' : '';
      if (i === c) li.setAttribute('aria-current', 'step'); else li.removeAttribute('aria-current');
    });
    close.hidden = false;
  }
  W = {dlg, show, list, status, rescan, manual, hiddenBtn, selName, pw, pwErr, pwBox, openNote, staticNote, plan, save, saveErr, stages, pBody, pActs,
    setTrigger: t => { trigger = t; }};
  return W;
}

function openWifiDialog(ev, opts) {
  const w = wifiDialog();
  w.setTrigger(document.activeElement);
  if (!w.dlg.open) { if (w.dlg.showModal) w.dlg.showModal(); else w.dlg.setAttribute('open', ''); }
  loadNetCfg();
  if (opts && opts.retry) { startRetry(); return; }
  if (NT && netTransitionActive()) { w.show('progress'); renderProgress(); return; }
  w.show('scan');
  scanWifi();
}
async function loadNetCfg() {
  try { netCfg = await api('/api/settings'); } catch (e) { /* sabit IP bilgisi yoksa not gösterilmez */ }
}

async function scanWifi() {
  if (wifiScanning) return;
  const w = wifiDialog();
  wifiScanning = true;
  w.rescan.disabled = true;
  w.list.textContent = '';
  w.list.setAttribute('aria-busy', 'true');
  w.status.className = 'scan-status busy';
  setText(w.status, 'Ağlar aranıyor…');
  try {
    let d;
    for (let i = 0; i < 20; i++) {
      d = await api('/scan');
      if (!d.pending) break;
      await new Promise(r => setTimeout(r, 700));
    }
    if (d.pending) throw Object.assign(new Error('Tarama zaman aşımına uğradı. Yeniden tarayın.'), {kind: 'timeout'});
    const uniq = new Map();
    for (const n of d.networks || []) {
      if (!n.ssid || (n.channel && n.channel > 14)) continue;
      const key = n.ssid + ':' + !!n.secure;
      if (!uniq.has(key) || n.rssi > uniq.get(key).rssi) uniq.set(key, n);
    }
    const rows = [...uniq.values()].sort((a, b) => b.rssi - a.rssi);
    rows.forEach(n => {
      const bars = n.rssi >= -55 ? 4 : n.rssi >= -67 ? 3 : n.rssi >= -75 ? 2 : 1;
      const sig = svgEl('svg', {viewBox: '0 0 20 14', class: 'sig', 'aria-hidden': 'true'});
      for (let k = 0; k < 4; k++) sig.append(svgEl('rect', {x: String(k * 5), y: String(10 - k * 3), width: '3.5', height: String(4 + k * 3), class: k < bars ? 'on' : 'off'}));
      const isSel = sel && sel.ssid === n.ssid && sel.secure === !!n.secure;
      const b = h('button', {type: 'button', class: 'wifi-row', 'aria-pressed': isSel ? 'true' : 'false',
        'aria-label': n.ssid + ', sinyal ' + ['zayıf', 'orta', 'iyi', 'çok iyi'][bars - 1] + ', ' + (n.secure ? 'parolalı' : 'açık ağ')},
        sig, h('span', {class: 'ssid', text: n.ssid}),
        h('span', {class: 'meta'}, h('span', {class: 'sig-t', text: ['Zayıf', 'Orta', 'İyi', 'Çok iyi'][bars - 1]}), h('span', {class: 'num dim', text: n.rssi + ' dBm'})),
        n.secure ? h('span', {class: 'sec'}, icon('lock'), h('span', {text: 'Parolalı'})) : h('span', {class: 'sec dim', text: 'Açık'}),
        icon('check', 'ico sel-mark'));
      b.addEventListener('click', () => choose({ssid: n.ssid, secure: !!n.secure, manual: false}));
      w.list.append(h('div', {role: 'listitem'}, b));
    });
    setText(w.status, rows.length ? rows.length + ' ağ bulundu. Bağlanacağınız ağı seçin.' : '2.4 GHz ağ bulunamadı. Cihaza yaklaşıp yeniden tarayın veya “Ağım görünmüyor”a dokunun.');
    w.status.className = 'scan-status' + (rows.length ? '' : ' empty');
  } catch (e) {
    w.status.className = 'scan-status err';
    if (e.status === 401 || e.status === 403) setText(w.status, 'Ağ taramak için oturum gerekiyor. Oturum sayfasından giriş yapıp yeniden deneyin.');
    else if (e.kind === 'timeout') setText(w.status, e.message);
    else if (!e.status) setText(w.status, 'Cihazla iletişim kesildi. Kurulum ağına bağlı olduğunuzu kontrol edip yeniden tarayın.');
    else setText(w.status, 'Tarama başarısız: ' + e.message);
  } finally { wifiScanning = false; w.rescan.disabled = false; w.list.setAttribute('aria-busy', 'false'); }
}

function choose(n) {
  const w = wifiDialog();
  sel = n;
  w.list.querySelectorAll('.wifi-row').forEach(b => b.setAttribute('aria-pressed', 'false'));
  setText(w.selName, n.ssid);
  w.pw.value = '';
  setText(w.pwErr, '');
  w.pwBox.hidden = !n.secure;
  w.openNote.hidden = n.secure;
  setText(w.saveErr, '');
  const st = netCfg && netCfg.staticEnabled;
  w.staticNote.hidden = !st;
  if (st) setText(w.staticNote.lastChild, 'Mevcut sabit IP ayarları (' + netCfg.staticIP + ') bu ağda da kullanılacak. Bu ağa uymazsa cihaz 20 sn sonra DHCP ile adres almayı dener. Ayrıntılar: Ayarlar › Ağ.');
  renderPlan();
  w.show('creds');
  if (n.secure) w.pw.focus(); else w.save.focus();
}

// Kaydetmeden önce: ne olacak, hangi adrese gidilecek (bağlantı kesilse de okunabilir kalır)
function renderPlan() {
  const w = wifiDialog(), d = D, ap = onApLink();
  const s = sel ? sel.ssid : '';
  w.plan.textContent = '';
  const items = ap ? [
    'Cihaz yeniden başlamadan “' + s + '” ağına bağlanmayı dener (genellikle 20 saniyeden kısa; sabit IP varsa 40 saniyeye kadar).',
    'Bu sayfa sonucu cihazdan okuyup gösterir. Bağlantı kurulursa kurulum ağı en çok 2 dakika daha açık kalır.',
    'Cihaz yeni ağın kanalına geçerken telefonunuz kurulum ağından kısa süre kopabilir; bu beklenen bir durumdur.',
    'Isıtma kontrolü ve güvenlik işlevleri bu sırada çalışmaya devam eder.'
  ] : [
    'Cihaz yeniden başlamadan “' + s + '” ağına geçer; bu sayfayla bağlantınız kesilecek.',
    'Bağlanırsa: “' + s + '” ağına bağlı bir telefon veya bilgisayardan aşağıdaki cihaz adresini açın.',
    'Bağlanamazsa: yaklaşık 20–40 saniye sonra cihaz “' + apName(d) + '” kurulum ağını açar; ona bağlanıp kurulum adresinden bilgileri düzeltin.',
    'Isıtma kontrolü ve güvenlik işlevleri bu sırada çalışmaya devam eder.'
  ];
  w.plan.append(h('h3', {text: 'Kaydettiğinizde'}), h('ul', {class: 'steps'}, items.map(t => h('li', {text: t}))),
    h('dl', {class: 'kv'}, h('dt', {text: 'Cihaz adresi'}), h('dd', {text: devUrl(d)}),
      h('dt', {text: 'Kurulum ağı'}), h('dd', {text: apName(d)}), h('dt', {text: 'Kurulum adresi'}), h('dd', {text: apUrl(d)})),
    h('p', {class: 'field-hint', text: 'Bu adresleri not alın; bağlantı kesildiğinde de bu pencerede kalır.'}));
}

function validateCreds() {
  const w = wifiDialog();
  if (!sel) return false;
  const n = bytes(sel.ssid);
  if (!n || n > 32) { setText(w.saveErr, 'Ağ adı 1–32 bayt olmalı.'); return false; }
  if (!sel.secure) return true;
  const p = w.pw.value, b = bytes(p);
  let err = '';
  if (!p.length) err = 'Parolayı yazın.';
  else if (b < 8) err = 'Parola en az 8 karakter olmalı.';
  else if (b === 64 && !/^[0-9a-fA-F]{64}$/.test(p)) err = '64 karakterlik anahtar yalnız 0-9 ve a-f içerebilir. Parola en çok 63 bayt olabilir.';
  else if (b > 64) err = 'Parola en çok 63 bayt olabilir (Türkçe karakterler 2 bayt sayılır).';
  if (err) { setText(w.pwErr, err); w.pw.setAttribute('aria-invalid', 'true'); w.pw.focus(); return false; }
  return true;
}

async function submitWifi() {
  const w = wifiDialog();
  if (NT && NT.stage === 'sending') return;
  setText(w.saveErr, '');
  if (!validateCreds()) return;
  const pass = sel.secure ? w.pw.value : '';
  if (sel.secure && pass !== pass.trim() && !w.save.dataset.ack) {
    w.save.dataset.ack = '1';
    showMsg({msg: w.saveErr}, 'warn', 'Parolanın başında veya sonunda boşluk var. Bilerek eklediyseniz yeniden “Kaydet ve bağlan”a basın.');
    return;
  }
  delete w.save.dataset.ack;
  w.save.setAttribute('aria-busy', 'true');
  w.save.disabled = true;
  NT = {ctx: onApLink() ? 'ap' : 'sta', ssid: sel.ssid, secure: sel.secure, base: D ? D.net_try : null, t0: Date.now(), sentAt: Date.now(), stage: 'sending', resp: null};
  let r = null;
  try {
    r = await api('/api/settings', {ssid: sel.ssid, pass});
    w.pw.value = '';
    NT.base = r.net_try_base;
    NT.stage = 'saved';
    NT.sentAt = Date.now();
    NT.resp = r;
    if (r.reconnect === false) {           // aynı bilgiler: kullanıcı yeniden deneme istiyor
      try { const q = await api('/api/net/retry', {}); NT.base = q.net_try_base; } catch (e) { NT.stage = 'uncertain'; }
    }
  } catch (e) {
    w.save.removeAttribute('aria-busy');
    w.save.disabled = false;
    if (e.status) {
      NT = null;
      if (e.status === 400 && e.body && e.body.field === 'pass') { setText(w.pwErr, e.message); w.pw.setAttribute('aria-invalid', 'true'); w.pw.focus(); }
      else if (e.status === 401 || e.status === 403) showMsg({msg: w.saveErr}, 'critical', 'Bu işlem için oturum gerekiyor. Oturum açıp yeniden deneyin.');
      else if (e.status === 507) showMsg({msg: w.saveErr}, 'critical', 'Ayarlar kalıcı olarak kaydedilemedi; önceki ağ ayarları korundu. Yeniden deneyin, sorun sürerse cihazı servis için bildirin.');
      else if (e.status === 409) showMsg({msg: w.saveErr}, 'warn', 'Cihaz meşgul: ' + e.message);
      else showMsg({msg: w.saveErr}, 'critical', e.message);
      return;
    }
    NT.stage = 'uncertain';                 // yanıt yok: kaydedilmiş olabilir; otomatik tekrar yok
  }
  w.save.removeAttribute('aria-busy');
  w.save.disabled = false;
  w.show('progress');
  renderProgress();
}

async function startRetry() {
  const w = wifiDialog();
  NT = {ctx: onApLink() ? 'ap' : 'sta', ssid: (D && D.wifi_ssid) || '', secure: true, retry: true, base: D ? D.net_try : null, t0: Date.now(), sentAt: Date.now(), stage: 'sending'};
  w.show('progress');
  renderProgress();
  try {
    const q = await api('/api/net/retry', {});
    NT.base = q.net_try_base;
    NT.stage = 'saved';
    NT.sentAt = Date.now();
  } catch (e) {
    if (e.status) { NT.stage = 'failed'; NT.err = e.message; } else NT.stage = 'uncertain';
  }
  renderProgress();
}

async function finishSetup(btn) {
  btn.setAttribute('aria-busy', 'true');
  btn.disabled = true;
  NT = NT || {ctx: 'ap', ssid: (D && D.wifi_ssid) || '', t0: Date.now(), sentAt: Date.now(), base: D ? D.net_try - 1 : 0};
  NT.addr = {ip: D && D.sta_ip, mdns: devUrl(D), ssid: (D && D.wifi_ssid) || NT.ssid};
  try { await api('/api/net/finish', {}); } catch (e) { /* yanıt yoksa da AP kapanmış olabilir: yönergeler aynı */ }
  NT.stage = 'finished';
  NT.t0 = Date.now();
  const w = wifiDialog();
  if (!w.dlg.open) openWifiDialog(null);
  w.show('progress');
  renderProgress();
}

// ---------------------------------------------------------------- ilerleme görünümü (her poll'da)
function progressState() {
  if (!NT) return 'idle';
  if (['sending', 'finished', 'finished-sta'].includes(NT.stage) || (NT.stage === 'failed' && NT.err)) return NT.stage;
  const d = D, fresh = ntFresh();
  if (fresh && NT.base !== null && d.net_try > NT.base) {
    if (NT.stage === 'uncertain' && d.wifi_ssid === NT.ssid) NT.stage = 'saved';   // kayıt cihaz verisiyle doğrulandı
    if (d.net_result === 'CONNECTED' && d.sta_ip) return NT.stage = 'connected';
    if (d.net_result === 'FAILED') return NT.stage = 'failed';
    return 'trying';
  }
  if (NT.stage === 'connected' && fresh) return 'connected';
  if (NT.stage === 'uncertain') return 'uncertain';
  if (!fresh && Date.now() - NT.sentAt > 2500) return NT.stage === 'connected' ? 'lost-after' : 'lost';
  return 'waiting';
}
function renderProgress() {
  const w = W;
  if (!w || !NT) return;
  const d = D, st = progressState(), s = NT.ssid;
  const body = w.pBody, acts = w.pActs;
  const closeS = d && d.ap_mode && d.ap_close_s > 0 ? d.ap_close_s : 0;
  const key = st + '|' + (d ? [!!closeS, d.sta_ip, d.time_valid, d.net_fail, d.mqtt_status, d.wifi_ssid].join(',') : '') + '|' + NT.stage + '|' + (Date.now() - NT.sentAt > 70000);
  const cd = $('#p-close');
  if (cd) setText(cd, String(closeS));
  if (body.dataset.k === key) return;
  body.dataset.k = key;
  body.textContent = '';
  acts.textContent = '';
  const S = w.stages;
  const saved = ['saved', 'connected', 'failed'].includes(NT.stage) || st === 'trying' || st === 'finished';
  setStage(S, 'saved', saved ? 'done' : (st === 'sending' ? 'active' : 'pending'), NT.retry ? 'Yeniden deneme isteği alındı' : (NT.stage === 'uncertain' ? 'Kayıt doğrulanamadı' : 'Ayarlar kaydedildi'));
  const conn = st === 'connected' || st === 'finished' || st === 'lost-after';
  setStage(S, 'try', conn ? 'done' : st === 'failed' ? 'fail' : (st === 'trying' || st === 'waiting' || st === 'lost') && saved ? 'active' : 'pending',
    '“' + s + '” ağına ' + (conn ? 'bağlandı' : st === 'failed' ? 'bağlanılamadı' : 'bağlanılıyor'));
  setStage(S, 'ok', st === 'connected' || st === 'finished' || st === 'lost-after' ? 'done' : 'pending');
  const para = t => h('p', {text: t});
  const note = (sev, t) => h('div', {class: 'notice ' + sev, role: sev === 'critical' ? 'alert' : 'status'}, icon(sev === 'info' ? 'timer' : sev === 'ok' ? 'check' : 'warn'), h('span', {text: t}));
  const btn = (t, cls, fn, ico) => { const b = h('button', {type: 'button', class: cls || '', 'data-icon': ico || null, 'data-text': ico ? '' : null}, t); b.addEventListener('click', () => fn(b)); acts.append(b); return b; };
  const closeBtn = t => btn(t || 'Kapat', '', () => w.dlg.close());
  switch (st) {
    case 'sending':
      body.append(h('p', {class: 'busy', text: NT.retry ? 'Yeniden deneme isteniyor…' : 'Ayarlar kaydediliyor…'}));
      break;
    case 'waiting':
    case 'trying':
      body.append(h('p', {class: 'busy', text: st === 'trying' ? 'Cihaz “' + s + '” ağına bağlanmayı deniyor. Bu genellikle 20 saniyeden kısa sürer.' : 'Cihaz bağlantı denemesini başlatıyor…'}));
      if (d && d.net_phase === 'CONNECTING' && d.static_ip) body.append(para('Sabit IP başarısız olursa cihaz DHCP ile bir kez daha dener.'));
      if (Date.now() - NT.sentAt > 70000) body.append(note('warn', 'Sonuç beklenenden uzun sürdü. Sayfa cihazdan veri almaya devam ediyor.'));
      if (NT.ctx !== 'ap') body.append(planAddresses());
      closeBtn('Arka planda sürsün');
      break;
    case 'uncertain':
      body.append(note('warn', 'İşlem sonucu doğrulanamadı: ayarlar kaydedilmiş ve cihaz yeni ağa geçiyor olabilir. Aynı isteği yeniden göndermeyin; önce cihaza erişimi kontrol edin.'),
        planAddresses(), helpUnreachable());
      closeBtn();
      break;
    case 'lost':
      if (NT.ctx === 'ap') body.append(note('warn', 'Kurulum ağıyla bağlantı kesildi. Cihaz yeni ağın kanalına geçerken bu olabilir. Telefonunuz kurulum ağına kendiliğinden dönerse sonuç burada görünür.'),
        para('Kurulum ağı listeden kaybolduysa cihaz büyük olasılıkla bağlanmış ve kurulum ağını kapatmıştır: telefonunuzu “' + s + '” ağına bağlayıp cihaz adresini açın.'));
      else body.append(note('info', 'Cihaz ağ değiştiriyor; bu adresle bağlantı kesildi. Bu beklenen bir durumdur, kontrol çalışmaya devam eder.'));
      body.append(planAddresses(), helpUnreachable());
      closeBtn();
      break;
    case 'connected': {
      const ip = d.sta_ip;
      body.append(h('h3', {class: 'ok-title', text: 'Cihaz Wi-Fi ağına bağlandı.'}),
        h('dl', {class: 'kv'}, h('dt', {text: 'Ağ'}), h('dd', {text: d.wifi_ssid || s}), h('dt', {text: 'Sinyal'}), h('dd', {text: rssiText(d.wifi_rssi)}),
          h('dt', {text: 'Saat eşitlemesi'}), h('dd', {text: d.time_valid === 'ON' ? 'Eşitli' : 'Bekleniyor (internet gerektirir)'}),
          h('dt', {text: 'MQTT'}), h('dd', {text: d.mqtt_status === 'CONNECTED' ? 'Bağlı' : d.mqtt_status === 'DISABLED' ? 'Yapılandırılmadı' : 'Bağlı değil'})),
        addrBox('Cihaz adresi (IP)', 'http://' + ip, 'addr-ip'), addrBox('Cihaz adı (mDNS)', devUrl(d), 'addr-mdns'));
      if (closeS) {
        body.append(h('p', null, 'Telefonunuzu “' + (d.wifi_ssid || s) + '” ağına bağlayın, ardından yukarıdaki adreslerden birini açın. Kurulum ağı ',
          h('b', {id: 'p-close', class: 'num', text: String(closeS)}), ' sn sonra kapanacak.'));
        btn('Kurulumu bitir', 'primary', finishSetup, 'check');
        closeBtn('Kontrol paneline dön');
      } else {
        body.append(para(NT.ctx === 'ap' ? 'Kurulum ağı kapandı. Telefonunuzu “' + (d.wifi_ssid || s) + '” ağına bağlayıp cihaz adresini açın.' : 'Cihaz bu adresten erişilebilir durumda.'));
        closeBtn('Kontrol paneline dön');
      }
      body.append(h('p', {class: 'field-hint', text: '.local adı bazı Android ve Windows cihazlarda çözülmez; IP adresini kullanın. Web yönetim parolası belirlemeniz önerilir (Ayarlar › Erişim).'}));
      break;
    }
    case 'lost-after':
    case 'finished': {
      const a = NT.addr || {ip: d && d.sta_ip, mdns: devUrl(d), ssid: s};
      NT.addr = a;
      body.append(h('h3', {class: 'ok-title', text: 'Kurulum tamamlandı.'}),
        para(st === 'lost-after' ? 'Cihaz “' + (a.ssid || s) + '” ağına bağlandı; kurulum ağı kapandığı için bu sayfanın bağlantısı kesildi.' : 'Cihaz “' + (a.ssid || s) + '” ağına bağlı; kurulum ağı kapatılıyor. Bu sayfanın bağlantısı birazdan kesilecek.'),
        h('ol', {class: 'steps'}, h('li', {text: 'Telefonunuzu “' + (a.ssid || s) + '” ağına bağlayın.'}), h('li', {text: 'Aşağıdaki adreslerden birini açın.'})));
      if (a.ip) body.append(addrBox('Cihaz adresi (IP)', 'http://' + a.ip, 'addr-ip2'));
      body.append(addrBox('Cihaz adı (mDNS)', a.mdns, 'addr-mdns2'), helpUnreachable());
      if (a.ip) acts.append(h('a', {class: 'btn primary', href: 'http://' + a.ip + '/', rel: 'noopener', text: 'Cihaz panelini aç'}));
      closeBtn();
      break;
    }
    case 'failed':
      if (NT.err) { body.append(note('critical', NT.err)); closeBtn(); break; }
      body.append(note('critical', 'Cihaz “' + s + '” ağına bağlanamadı.'), para(failText(d)),
        para(d && d.ap_mode ? 'Kurulum ağı açık kaldı. Kaydedilen bilgiler cihazda duruyor ve 5 dakikada bir yeniden denenir; bilgileri düzeltip yeniden kaydedebilirsiniz.' : 'Kaydedilen bilgiler cihazda duruyor.'));
      if (NT.secure && !NT.retry) btn('Parolayı yeniden gir', 'primary', () => { if (sel && sel.ssid === s) choose(sel); else wifiDialog().show('scan'); }, 'key');
      btn('Başka ağ seç', NT.secure && !NT.retry ? '' : 'primary', () => { wifiDialog().show('scan'); scanWifi(); });
      btn('Şimdi yeniden dene', '', () => startRetry(), 'refresh');
      break;
  }
  iconize(body);
  iconize(acts);
}
function planAddresses() {
  const d = D, s = NT ? NT.ssid : '';
  return h('dl', {class: 'kv'}, h('dt', {text: 'Bağlanırsa'}), h('dd', {text: devUrl(d) + ' (“' + s + '” ağından)'}),
    h('dt', {text: 'Bağlanamazsa'}), h('dd', {text: apName(d) + ' → ' + apUrl(d)}));
}
// renderAll → her poll
function wifiTick() {
  if (NT && NT.stage === 'finished' && Date.now() - NT.t0 > 600000) NT.stage = 'finished-sta';
  if (W && W.dlg.open && !W.pBody.closest('[hidden]')) renderProgress();
  if (NT && NT.ctx !== 'reset' && !(W && W.dlg.open)) progressState();
}

// ---------------------------------------------------------------- Genel Bakış: kurulum / kurtarma kartı
let setupCollapsed = false;
function apSetupPanel() {
  const P = {};
  P.badge = h('span', {class: 'badge', text: 'Kurulum ağına bağlı'});
  P.title = h('h3', {id: 'ap-h'});
  P.why = h('p');
  P.reason = h('p', {class: 'reason-t'});
  P.retry = h('p', {class: 'field-hint num'});
  P.primary = h('button', {type: 'button', class: 'primary', 'data-icon': 'wifi', 'data-text': ''}, 'Wi-Fi ağı seç');
  P.primary.addEventListener('click', e => openWifiDialog(e));
  P.retryBtn = h('button', {type: 'button', 'data-icon': 'refresh', 'data-text': ''}, 'Kayıtlı ağı şimdi dene');
  P.retryBtn.addEventListener('click', e => openWifiDialog(e, {retry: true}));
  P.finish = h('button', {type: 'button', class: 'primary', 'data-icon': 'check', 'data-text': ''}, 'Kurulumu bitir');
  P.finish.addEventListener('click', () => finishSetup(P.finish));
  P.addr = h('div');
  P.collapse = h('button', {type: 'button', class: 'link-btn'}, 'Cihaz panelini aç');
  P.collapse.addEventListener('click', () => { setupCollapsed = true; updateApPanel(D); });
  P.expand = h('button', {type: 'button', class: 'link-btn'}, 'Kuruluma dön');
  P.expand.addEventListener('click', () => { setupCollapsed = false; updateApPanel(D); P.title.focus(); });
  P.body = h('div', {class: 'setup-body'},
    h('div', {class: 'setup-top'}, h('span', {class: 'eyebrow', id: 'ap-eyebrow', text: 'KURULUM'}), P.badge),
    P.title, P.why, P.reason, P.retry, P.addr,
    h('dl', {class: 'kv'}, h('dt', {text: 'Kurulum ağı'}), h('dd', {id: 'ap-name', class: 'mono'}, 'SCADA_AP'),
      h('dt', {text: 'Kurulum adresi'}), h('dd', {id: 'ap-ip', class: 'mono'}, 'http://192.168.4.1')),
    h('div', {class: 'btn-row'}, P.primary, P.finish, P.retryBtn, P.collapse),
    h('p', {class: 'field-hint', id: 'ap-hint', text: 'Bu ağda internet bağlantısı olmaması normaldir. Kurulum bitene kadar bu ağa bağlı kalın. Isıtma kontrolü ve güvenlik işlevleri kurulum sırasında çalışmaya devam eder.'}));
  P.bar = h('div', {class: 'setup-bar', hidden: ''}, icon('wifi'), h('span', {id: 'ap-bar-t', text: 'Wi-Fi kurulumu tamamlanmadı.'}), P.expand);
  P.title.setAttribute('tabindex', '-1');
  const sec = h('section', {class: 'panel ap-setup', id: 'ap-setup', hidden: '', 'aria-labelledby': 'ap-h'}, P.body, P.bar);
  sec._p = P;
  return sec;
}
function updateApPanel(d) {
  const sec = $('#ap-setup');
  if (!sec || !d) return;
  const P = sec._p;
  sec.hidden = !d.ap_mode;
  if (!d.ap_mode) { setupCollapsed = false; return; }
  const mode = d.net_setup || (d.wifi_ssid ? 'RECOVERY' : 'FIRST');
  P.body.hidden = setupCollapsed;
  P.bar.hidden = !setupCollapsed;
  sec.classList.toggle('ok', mode === 'HANDOVER');
  P.badge.hidden = !onApLink();
  setText($('#ap-name'), apName(d));
  setText($('#ap-ip'), apUrl(d));
  P.addr.textContent = '';
  P.finish.hidden = mode !== 'HANDOVER';
  P.primary.hidden = mode === 'HANDOVER';
  P.retryBtn.hidden = mode !== 'RECOVERY' || d.net_phase === 'CONNECTING';
  P.reason.hidden = P.retry.hidden = true;
  if (mode === 'FIRST') {
    setText($('#ap-eyebrow'), 'KURULUM');
    setText(P.title, 'Cihazınızı Wi-Fi ağına bağlayın');
    setText(P.why, 'Cihaz henüz bir Wi-Fi ağına kayıtlı değil. Kendi kurulum ağını yayınlıyor.');
    setText($('#ap-bar-t'), 'Wi-Fi kurulumu tamamlanmadı.');
  } else if (mode === 'RECOVERY') {
    setText($('#ap-eyebrow'), 'BAĞLANTI KURTARMA');
    setText(P.title, 'Cihaz kayıtlı ağa bağlanamadı');
    setText(P.why, '“' + d.wifi_ssid + '” ağına bağlanılamadığı için kurulum ağı açıldı.');
    if (d.net_result === 'FAILED') { P.reason.hidden = false; setText(P.reason, failText(d)); }
    P.retry.hidden = false;
    setText(P.retry, d.net_phase === 'CONNECTING' ? 'Kayıtlı ağ şu anda deneniyor…' :
      'Cihaz kayıtlı ağı 5 dakikada bir yeniden dener' + (d.net_retry_s ? ' · sonraki deneme yaklaşık ' + fmt.dur(d.net_retry_s) + ' sonra.' : '.') + ' Bağlanırsa kurulum ağı kapanabilir.');
    setText($('#ap-bar-t'), 'Cihaz kayıtlı ağa bağlanamadı.');
  } else {
    setText($('#ap-eyebrow'), 'BAĞLANDI · DEVİR');
    setText(P.title, 'Cihaz Wi-Fi ağına bağlandı');
    setText(P.why, '“' + d.wifi_ssid + '” ağına bağlandı. Kurulum ağı ' + (d.ap_close_s || 0) + ' sn sonra kapanacak.');
    if (d.sta_ip) P.addr.append(addrBox('Cihaz adresi (IP)', 'http://' + d.sta_ip, 'ap-addr-ip'));
    P.addr.append(addrBox('Cihaz adı (mDNS)', devUrl(d), 'ap-addr-mdns'));
    iconize(P.addr);
    setText($('#ap-bar-t'), 'Cihaz bağlandı; kurulum ağı kapanacak.');
  }
}

// ---------------------------------------------------------------- Ayarlar › Bakım: Wi-Fi sil (ayrı kapsam)
async function resetWifiFlow(btn, out) {
  const d = D;
  const body = h('div', null,
    h('p', {text: 'Wi-Fi adı ve parolası silinecek, sabit IP kullanımı kapatılacak. Cihaz yeniden başlamadan kurulum ağını açacak; bu sayfayla bağlantınız kesilecek.'}),
    h('p', {text: 'Diğer cihaz ayarları korunacak. Isıtma kontrolü ve güvenlik işlevleri çalışmaya devam eder.'}),
    h('dl', {class: 'kv'}, h('dt', {text: 'Kurulum ağı'}), h('dd', {text: apName(d)}), h('dt', {text: 'Parola'}), h('dd', {text: 'Cihaz etiketinde'}),
      h('dt', {text: 'Kurulum adresi'}), h('dd', {text: apUrl(d)})));
  if (!(await confirmDlg('Wi-Fi bilgilerini sil', body, 'Wi-Fi bilgilerini sil', true))) return;
  btn.setAttribute('aria-busy', 'true');
  NT = {ctx: 'reset', ssid: '', t0: Date.now(), sentAt: Date.now(), base: null, stage: 'saved'};
  let msg;
  try { await api('/api/reset-wifi', {}); msg = 'Wi-Fi bilgileri silindi. Kurulum ağı açılıyor.'; }
  catch (e) {
    if (e.status) { NT = null; btn.removeAttribute('aria-busy'); toast(e.message, true); return; }
    msg = 'İşlem sonucu doğrulanamadı; Wi-Fi bilgileri silinmiş ve kurulum ağı açılıyor olabilir. İsteği yeniden göndermeyin.';
  }
  btn.removeAttribute('aria-busy');
  out.textContent = '';
  out.append(h('div', {class: 'notice warn', role: 'status'}, icon('wifioff'), h('span', {text: msg})),
    h('ol', {class: 'steps'}, h('li', {text: 'Telefonunuzu “' + apName(d) + '” ağına bağlayın (parola cihaz etiketinde).'}),
      h('li', {text: apUrl(d) + ' adresini açın ve yeni Wi-Fi ağını seçin.'})));
}

// ================================================================= GENEL BAKIŞ
// Dinamik genişlik CSS ile değil SVG özniteliğiyle (CSP: inline stil yok)
function svgEl(tag, attrs) {
  const e = document.createElementNS(SVGNS, tag);
  for (const k in attrs || {}) e.setAttribute(k, attrs[k]);
  return e;
}
function meterSvg(id, label, marks) {
  const s = svgEl('svg', {viewBox: '0 0 100 10', preserveAspectRatio: 'none', class: 'bar', id, role: 'meter', 'aria-label': label,
    'aria-valuemin': '0', 'aria-valuemax': '100'});
  s.append(svgEl('rect', {class: 'bar-fill', x: '0', y: '0', width: '0', height: '10'}));
  (marks || []).forEach(() => s.append(svgEl('line', {class: 'bar-mark', x1: '0', x2: '0', y1: '0', y2: '10'})));
  return s;
}
function setMeter(s, v, marks) {
  v = Math.max(0, Math.min(100, Number(v) || 0));
  s.querySelector('.bar-fill').setAttribute('width', v.toFixed(1));
  s.setAttribute('aria-valuenow', v.toFixed(0));
  $$('.bar-mark', s).forEach((l, i) => { l.setAttribute('x1', marks[i]); l.setAttribute('x2', marks[i]); });
}
function radioGroup(name, options, labelText) {
  const g = h('div', {class: 'radio-group', role: 'radiogroup', 'aria-label': labelText});
  options.forEach(([v, t]) => {
    g.append(h('label', {class: 'radio-choice'}, h('input', {type: 'radio', name, value: v, id: name + '-' + v}), h('span', {text: t})));
  });
  return g;
}
function setRadio(g, v, disabled) {
  $$('input', g).forEach(i => {
    if (!g.dataset.busy) i.checked = i.value === v;
    i.disabled = !!disabled;
  });
}
function stateBadge(code) {
  const sev = STATE_SEV[code];
  return h('span', {class: 'state-badge ' + (sev === 'crit' ? 'reason crit' : sev === 'warn' ? 'reason warn' : '')},
    icon(STATE_ICON[code] || 'check'), h('span', {text: STATE_TR[code] || code || '—'}));
}
function setpointForm(prefix) {
  const inp = h('input', {type: 'number', id: prefix + '-sp', min: '5', max: '30', step: '0.5', inputmode: 'decimal', 'aria-describedby': prefix + '-sp-hint'});
  const btn = h('button', {type: 'submit', class: 'primary', 'data-icon': 'check', 'data-text': ''}, 'Uygula');
  const msg = h('div', {class: 'cmd-msg'});
  const ui = {btns: [btn], msg, onDone: () => { delete inp.dataset.dirty; }};
  inp.addEventListener('input', () => { inp.dataset.dirty = '1'; });
  const f = h('form', {class: 'inline-form', novalidate: ''},
    h('label', {for: inp.id, text: 'Hedef'}), inp, h('span', {class: 'dim', text: '°C'}), btn,
    h('small', {class: 'field-hint', id: prefix + '-sp-hint', text: '5–30 °C, 0.5 adım'}));
  f.addEventListener('submit', e => {
    e.preventDefault();
    const v = Number(inp.value);
    if (!inp.value || v < 5 || v > 30 || Math.abs(v * 2 - Math.round(v * 2)) > 1e-6) {
      showMsg(ui, 'critical', 'Hedef 5–30 °C aralığında ve 0.5 adımında olmalı; değer değiştirilmedi.');
      return;
    }
    sendCmd('temperature_setpoint', v.toFixed(1), ui);
  });
  return {el: h('div', null, f, msg), inp, btn, ui};
}

builders.overview = sec => {
  sec.append(sectionHead('Genel Bakış'), apSetupPanel());
  // PROSES
  const t1 = h('span', {class: 'gauge-value num', id: 'ov-t1'}, '—');
  const rh = h('span', {class: 'gauge-value sm num', id: 'ov-rh'}, '—');
  const sp = setpointForm('ov');
  const proc = h('section', {class: 'panel', 'aria-labelledby': 'ov-proc-h'},
    h('h3', {id: 'ov-proc-h', text: 'Proses'}),
    h('div', {class: 'pv'},
      h('div', null, h('div', {class: 'lbl', text: 'Kulübe sıcaklığı'}), t1),
      h('div', null, h('div', {class: 'lbl', text: 'Nem'}), rh)),
    sp.el,
    h('dl', {class: 'kv'},
      h('dt', {text: 'Etkin hedef'}), h('dd', {id: 'ov-spe'}, '—'),
      h('dt', {text: 'Değişim'}), h('dd', {id: 'ov-rate'}, '—'),
      h('dt', {text: 'Sensör kalitesi'}), h('dd', {id: 'ov-q'}, '—')));
  // KONTROLÖR
  const modes = radioGroup('ov-mode', [['OFF', 'KAPALI'], ['AUTO', 'OTO'], ['MANUAL', 'MANUEL'], ['VENT_ONLY', 'HAVALANDIRMA']], 'Çalışma modu');
  const modeMsg = h('div', {class: 'cmd-msg'});
  const modeUi = {btns: $$('input', modes), msg: modeMsg, onDone: () => { delete modes.dataset.busy; }};
  modes.addEventListener('change', e => {
    const v = e.target.value;
    modes.dataset.busy = '1';
    if (v === 'OFF') {
      confirmDlg('Mod: KAPALI', 'Isıtma durdurulacak. Donma koruması (T1 < 4 °C) etkin kalır. Devam edilsin mi?', 'Kapat').then(ok => {
        if (ok) sendCmd('operating_mode', v, modeUi);
        else { delete modes.dataset.busy; renderAll(); }
      });
    } else sendCmd('operating_mode', v, modeUi);
  });
  const ctrl = h('section', {class: 'panel', 'aria-labelledby': 'ov-ctrl-h'},
    h('h3', {id: 'ov-ctrl-h', text: 'Kontrolör'}),
    h('div', {class: 'field'}, h('span', {class: 'lbl', text: 'Mod'}), modes), modeMsg,
    h('dl', {class: 'kv'},
      h('dt', {text: 'Durum'}), h('dd', {id: 'ov-state'}, '—'),
      h('dt', {text: 'Profil'}), h('dd', {id: 'ov-prof'}, '—'),
      h('dt', {text: 'Talep'}), h('dd', {id: 'ov-dem'}, '—'),
      h('dt', {text: 'Kademe'}), h('dd', {id: 'ov-stage'}, '—')));
  // TALEP
  const bar = meterSvg('ov-bar', 'Isı talebi', [55, 45]);
  const dem = h('section', {class: 'panel', 'aria-labelledby': 'ov-dem-h'},
    h('h3', {id: 'ov-dem-h', text: 'Isı talebi'}), bar,
    h('div', {class: 'bar-legend'}, h('span', {id: 'ov-dem-l'}, '—'), h('span', {id: 'ov-dem-r', text: 'kademe 2 eşiği 55 % · dönüş 45 %'})));
  // ÇIKIŞLAR
  const cards = h('div', {class: 'cards', id: 'ov-cards'});
  OUT.forEach(o => cards.append(h('article', {class: 'card', id: 'ov-c-' + o.k, 'aria-label': o.name},
    h('div', {class: 'card-head'}, h('span', {class: 'no', text: o.no}), icon(o.ico), h('span', {text: o.name})),
    h('div', {class: 'state'}, h('span', {class: 'dot'}), h('span', {class: 'st', text: '—'})),
    h('div', {class: 'meta m1'}, '—'), h('div', {class: 'meta m2'}, ''), h('div', {class: 'why'}))));
  // SAĞLIK
  const health = h('section', {class: 'panel', 'aria-labelledby': 'ov-h-h'}, h('h3', {id: 'ov-h-h', text: 'Sağlık'}),
    h('div', {class: 'statusbar', id: 'ov-health'}));
  sec.append(h('div', {class: 'stack'}, h('div', {class: 'grid2'}, proc, ctrl), dem, cards, health,
    h('p', {class: 'field-hint', text: 'Gösterilen çıkış durumu komutlanan çıkıştır; fiziksel geri bildirim (akım/RPM) yoktur.'})));
  pageUpdaters.overview = (d, st) => {
    if (!d) return;
    updateApPanel(d);
    setText($('#ov-t1'), fmt.t(d.temperature));
    if ($('#ov-t1').childNodes.length === 1) $('#ov-t1').append(h('small', {text: '°C'}));
    setText($('#ov-rh'), fmt.t(d.humidity));
    if ($('#ov-rh').childNodes.length === 1) $('#ov-rh').append(h('small', {text: '%'}));
    if (!sp.inp.dataset.dirty && document.activeElement !== sp.inp) sp.inp.value = d.temperature_setpoint.toFixed(1);
    sp.btn.disabled = st;
    setText($('#ov-spe'), fmt.t(d.setpoint_effective) + ' °C · ' + (PROFILE_TR[d.setpoint_source] || d.setpoint_source) +
      (d.setpoint_effective < d.temperature_setpoint && d.profile_active === 'DAY' ? ' · rampa' : ''));
    setText($('#ov-rate'), d.temperature_rate === null ? '—' : (d.temperature_rate >= 0 ? '+' : '') + d.temperature_rate.toFixed(1) + ' °C/sa');
    setText($('#ov-q'), (QUALITY_TR[d.temperature_quality] || '—') + ' · yaş ' + d.sensor_age_s + ' s');
    setRadio(modes, d.operating_mode, st);
    const s = $('#ov-state'); s.textContent = ''; s.append(stateBadge(d.controller_state),
      d.heating_reason && d.heating_reason !== 'NONE' ? h('span', {class: 'dim', text: ' · ' + HREASON_TR[d.heating_reason]}) : '');
    setText($('#ov-prof'), (PROFILE_TR[d.profile_active] || d.profile_active) + (d.boost === 'ON' ? ' · kalan ' + d.boost_remaining_min + ' dk' : '') +
      (d.program_active && d.program_active !== '—' ? ' · ' + d.program_active + (d.program_until ? ' → ' + String(d.program_until).slice(11, 16) : '') : ''));
    setText($('#ov-dem'), d.heat_demand.toFixed(1) + ' %  (PID ' + d.pid_output.toFixed(1) + ' %)');
    setText($('#ov-stage'), d.power_stage + ' · R1 ' + d.r1_duty.toFixed(0) + ' % · R2 ' + d.r2_duty.toFixed(0) + ' %');
    setMeter($('#ov-bar'), d.heat_demand, [d.stage2_on || 55, d.stage2_off || 45]);
    setText($('#ov-dem-l'), d.heat_demand.toFixed(0) + ' %' + (Math.abs(d.pid_output - d.heat_demand) > 0.5 ? ' · talep sınırlandı' : ''));
    OUT.forEach(o => {
      const c = $('#ov-c-' + o.k);
      const on = onoff(d[o.k + '_active']);
      const unknown = st;
      c.classList.toggle('on', on && !unknown);
      c.classList.toggle('off', !on && !unknown);
      setText($('.st', c), unknown ? 'BİLİNMİYOR' : (on ? 'ÇALIŞIYOR' : 'KAPALI'));
      if (o.k === 'r1' || o.k === 'r2') {
        setText($('.m1', c), 'Oran ' + d[o.k + '_duty'].toFixed(0) + ' %');
        setText($('.m2', c), 'Bugün ' + fmt.hm(d[o.k + '_minutes_today']));
      } else {
        setText($('.m1', c), 'İstek ' + (onoff(d[o.req]) ? 'AÇIK' : 'KAPALI'));
        setText($('.m2', c), o.k === 'ventilation_fan' ? ('Durum ' + (VSTATE_TR[d.ventilation_state] || '—')) : '');
      }
      const why = $('.why', c);
      const r = d[o.k + '_reason'];
      if (why.dataset.r !== r) {
        why.dataset.r = r;
        why.textContent = '';
        if (r && r !== 'NONE') why.append(reasonEl(r));
      }
    });
    const hb = $('#ov-health');
    const items = [
      [d.sensor_ok === 'ON' ? 'ok' : 'bad', 'Sensör ' + (QUALITY_TR[d.temperature_quality] || '—') + ' (' + d.sensor_age_s + ' s)'],
      [d.wifi_ok ? 'ok' : 'bad', 'Wi-Fi ' + (d.wifi_rssi ?? '—') + ' dBm'],
      [d.mqtt_status === 'CONNECTED' ? 'ok' : 'bad', 'MQTT ' + (d.mqtt_status === 'CONNECTED' ? 'bağlı' : 'bağlı değil')],
      [d.controller_state === 'FAILSAFE' ? 'bad' : 'ok', d.controller_state === 'FAILSAFE' ? 'Cihaz güvenli durumda' : 'Cihaz normal'],
      [d.alarm === 'ON' ? (d.alarm_state === 'CRITICAL' ? 'bad' : 'warn') : 'ok',
        d.alarm === 'ON' ? d.active_alarm_count + ' alarm · ' + (SEV_TR[d.alarm_state] || '') : 'Alarm yok']];
    if (hb.children.length !== items.length) { hb.textContent = ''; items.forEach(() => hb.append(h('span', {class: 'pill'}, h('i'), h('span')))); }
    items.forEach((it, i) => setPill(hb.children[i], it[0], it[1]));
  };
};

// ================================================================= KONTROL
function hTabs(id, tabs, onSel) {
  const list = h('div', {class: 'tabs-h', role: 'tablist', 'aria-label': 'Kontrol bölümleri'});
  const panels = [];
  tabs.forEach(([k, label], i) => {
    const b = h('button', {type: 'button', role: 'tab', id: id + '-tab-' + k, 'aria-controls': id + '-p-' + k, 'aria-selected': i === 0 ? 'true' : 'false', tabindex: i === 0 ? '0' : '-1'}, label);
    list.append(b);
    panels.push(h('div', {role: 'tabpanel', id: id + '-p-' + k, 'aria-labelledby': b.id, hidden: i !== 0}));
  });
  const select = k => {
    $$('[role=tab]', list).forEach(b => {
      const on = b.id === id + '-tab-' + k;
      b.setAttribute('aria-selected', on ? 'true' : 'false');
      b.tabIndex = on ? 0 : -1;
    });
    panels.forEach(p => { p.hidden = p.id !== id + '-p-' + k; });
    lsSet('scada-' + id + '-tab', k);
    if (onSel) onSel(k);
  };
  list.addEventListener('click', e => { const b = e.target.closest('[role=tab]'); if (b) select(b.id.split('-tab-')[1]); });
  list.addEventListener('keydown', e => {
    const bs = $$('[role=tab]', list), i = bs.indexOf(document.activeElement);
    let j = -1;
    if (e.key === 'ArrowRight' || e.key === 'ArrowDown') j = (i + 1) % bs.length;
    else if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') j = (i - 1 + bs.length) % bs.length;
    else if (e.key === 'Home') j = 0;
    else if (e.key === 'End') j = bs.length - 1;
    if (j >= 0) { e.preventDefault(); bs[j].focus(); bs[j].click(); }
  });
  const saved = lsGet('scada-' + id + '-tab');
  if (saved && tabs.some(t => t[0] === saved)) setTimeout(() => select(saved), 0);
  return {list, panels};
}
function cmdButton(label, iconName, onClick, cls) {
  const b = h('button', {type: 'button', class: cls || '', 'data-icon': iconName, 'data-text': ''}, label);
  b.addEventListener('click', onClick);
  return b;
}
function switchRow(id, label, hint, confirmOff) {
  // Anahtar (switch) isteği: istek / etkin / neden üçlüsü
  const cb = h('input', {type: 'checkbox', id: 'sw-' + id});
  const msg = h('div', {class: 'cmd-msg'});
  const ui = {btns: [cb], msg};
  cb.addEventListener('change', () => {
    const v = cb.checked ? 'ON' : 'OFF';
    if (!cb.checked && confirmOff) {
      confirmDlg(confirmOff[0], confirmOff[1], confirmOff[2], true).then(ok => { if (ok) sendCmd(id, v, ui); else renderAll(); });
    } else sendCmd(id, v, ui);
  });
  return {el: h('div', {class: 'field full'}, h('div', {class: 'field toggle'}, cb, h('label', {for: cb.id, text: label})),
    hint ? h('small', {class: 'field-hint', text: hint}) : null, msg), cb, ui};
}
function numberCmd(id, label, unit, min, max, step, hint) {
  const inp = h('input', {type: 'number', id: 'n-' + id, min, max, step, inputmode: 'decimal'});
  const btn = h('button', {type: 'button', 'data-icon': 'check'}, label + ' uygula');
  const msg = h('div', {class: 'cmd-msg'});
  const ui = {btns: [btn], msg, onDone: () => delete inp.dataset.dirty};
  inp.addEventListener('input', () => { inp.dataset.dirty = '1'; });
  btn.addEventListener('click', () => {
    const v = Number(inp.value);
    const ok = inp.value !== '' && v >= Number(min) && v <= Number(max) && Math.abs(v / Number(step) - Math.round(v / Number(step))) < 1e-6;
    if (!ok) { showMsg(ui, 'critical', label + ': ' + min + '–' + max + ' ' + unit + ', adım ' + step + '. Değer değiştirilmedi.'); return; }
    sendCmd(id, String(v), ui);
  });
  const el = h('div', {class: 'field'}, h('label', {for: inp.id, text: label + ' (' + unit + ')'}),
    h('div', {class: 'row'}, inp, btn), hint ? h('small', {class: 'field-hint', text: hint}) : null, msg);
  return {el, inp, btn, ui};
}
function syncNum(n, v, st, dis) {
  if (!n.inp.dataset.dirty && document.activeElement !== n.inp) n.inp.value = v;
  n.btn.disabled = st || !!dis;
  n.inp.disabled = !!dis;
}

builders.control = sec => {
  sec.append(sectionHead('Kontrol'));
  const t = hTabs('ctl', [['climate', 'İklim'], ['profiles', 'Profiller'], ['vent', 'Havalandırma'], ['pid', 'PID']]);
  sec.append(t.list, ...t.panels);
  const [pClimate, pProf, pVent, pPid] = t.panels;

  // ---- İklim
  const modes = radioGroup('ctl-mode', [['OFF', 'KAPALI'], ['AUTO', 'OTO'], ['MANUAL', 'MANUEL'], ['VENT_ONLY', 'HAVALANDIRMA']], 'Çalışma modu');
  const modeMsg = h('div', {class: 'cmd-msg'});
  const modeUi = {btns: $$('input', modes), msg: modeMsg, onDone: () => delete modes.dataset.busy};
  modes.addEventListener('change', e => { modes.dataset.busy = '1'; sendCmd('operating_mode', e.target.value, modeUi); });
  const sp = setpointForm('ctl');
  const man = numberCmd('manual_heat_demand', 'Manuel talep', '%', '0', '100', '5', 'Yalnız MANUEL modda etkin; 8 sa sonra OTO’ya döner.');
  const en = switchRow('controller_enable', 'Kontrolör etkin', 'Kapatma yalnız bu yerel arayüzden yapılır; donma koruması dahil otomatik kontrol durur.',
    ['Kontrolörü kapat', 'Donma koruması dahil bütün otomatik kontrol duracak. Kulübe donabilir. Devam edilsin mi?', 'Kontrolörü kapat']);
  const lockSel = h('select', {id: 'ctl-lock'}, ...[[0, 'Kilit yok'], [15, '15 dk'], [60, '1 sa'], [240, '4 sa'], [1440, '24 sa']].map(([v, tx]) => h('option', {value: String(v), text: tx})));
  const lockMsg = h('div', {class: 'cmd-msg'});
  const lockBtn = cmdButton('Kilidi uygula', 'lock', () => sendCmd('local_lock_min', lockSel.value, {btns: [lockBtn], msg: lockMsg}));
  pClimate.append(h('div', {class: 'grid2'},
    h('section', {class: 'panel'}, h('h3', {text: 'Mod ve hedef'}),
      h('div', {class: 'field'}, h('span', {class: 'lbl', text: 'Çalışma modu'}), modes), modeMsg, sp.el,
      h('dl', {class: 'kv'}, h('dt', {text: 'Etkin hedef'}), h('dd', {id: 'ctl-spe'}, '—'), h('dt', {text: 'Kaynak'}), h('dd', {id: 'ctl-src'}, '—'))),
    h('section', {class: 'panel'}, h('h3', {text: 'Manuel ve yetki'}),
      h('div', {class: 'form-grid'}, man.el, en.el,
        h('div', {class: 'field full'}, h('label', {for: 'ctl-lock', text: 'Yerel kilit (MQTT komutlarını durdurur)'}),
          h('div', {class: 'row'}, lockSel, lockBtn), h('small', {class: 'field-hint', id: 'ctl-lock-st'}, '—'), lockMsg)))));

  // ---- Profiller
  const prof = radioGroup('ctl-prof', [['DAY', 'GÜNDÜZ'], ['NIGHT', 'GECE'], ['AWAY', 'UZAKTA'], ['FROST', 'DONMA']], 'Seçili profil');
  const profMsg = h('div', {class: 'cmd-msg'});
  const profUi = {btns: $$('input', prof), msg: profMsg, onDone: () => delete prof.dataset.busy};
  prof.addEventListener('change', e => { prof.dataset.busy = '1'; sendCmd('profile', e.target.value, profUi); });
  const boostMsg = h('div', {class: 'cmd-msg'});
  const boostBtn = cmdButton('Boost başlat', 'flame', () => sendCmd('boost', D && D.boost === 'ON' ? 'OFF' : 'ON', {btns: [boostBtn], msg: boostMsg}), 'primary');
  const psp = [['setpoint_night', 'Gece'], ['setpoint_away', 'Uzakta'], ['setpoint_frost', 'Donma'], ['setpoint_boost', 'Boost']]
    .map(([k, l]) => numberCmd(k, l, '°C', k === 'setpoint_frost' ? '4' : '5', k === 'setpoint_away' ? '25' : (k === 'setpoint_frost' ? '12' : '30'), '0.5'));
  const profTable = h('table', {class: 'out'}, h('thead', null, h('tr', null, ...['Profil', 'Hedef', 'Kaynak', 'Durum'].map(x => h('th', {text: x})))),
    h('tbody', {id: 'ctl-prof-rows'}));
  pProf.append(h('div', {class: 'grid2'},
    h('section', {class: 'panel'}, h('h3', {text: 'Profil seçimi'}),
      h('div', {class: 'field'}, h('span', {class: 'lbl', text: 'Açık seçim (zamanlamadan önceliklidir)'}), prof), profMsg,
      h('dl', {class: 'kv'}, h('dt', {text: 'Etkin profil'}), h('dd', {id: 'ctl-pa'}, '—'),
        h('dt', {text: 'Yerel program'}), h('dd', {id: 'ctl-pg'}, '—'),
        h('dt', {text: 'Suite gece'}), h('dd', {id: 'ctl-sn'}, '—'), h('dt', {text: 'Suite uzakta'}), h('dd', {id: 'ctl-sa'}, '—'),
        h('dt', {text: 'Donma koruması'}), h('dd', {id: 'ctl-af'}, '—')),
      h('p', {class: 'field-hint', text: 'Gece/Uzakta istekleri MQTT Suite Programs tarafından açılıp kapatılır; 16 sa sonra kendiliğinden düşer. Öncelik: BOOST › açık seçim › Uzakta › Gece › Gündüz.'}),
      h('div', {class: 'btn-row'}, boostBtn, h('span', {class: 'dim', id: 'ctl-boost-st'})), boostMsg),
    h('section', {class: 'panel'}, h('h3', {text: 'Profil hedefleri'}), h('div', {class: 'table-wrap'}, profTable),
      h('div', {class: 'form-grid'}, ...psp.map(n => n.el)))));

  // ---- Havalandırma
  const vsw = switchRow('ventilation_fan_manual', 'Manuel havalandırma isteği', '120 dk sonra kendiliğinden kapanır. Donma koruması ve ısıtma önceliği kuralları geçerlidir.');
  pVent.append(h('div', {class: 'grid2'},
    h('section', {class: 'panel'}, h('h3', {text: 'Havalandırma fanı'}), vsw.el,
      h('dl', {class: 'kv'}, h('dt', {text: 'İstek'}), h('dd', {id: 'ctl-v-req'}, '—'), h('dt', {text: 'Etkin'}), h('dd', {id: 'ctl-v-eff'}, '—'),
        h('dt', {text: 'Neden'}), h('dd', {id: 'ctl-v-why'}, '—'), h('dt', {text: 'Durum'}), h('dd', {id: 'ctl-v-st'}, '—'))),
    h('section', {class: 'panel'}, h('h3', {text: 'Otomatik istek kaynakları'}),
      h('div', {class: 'alarm-list', id: 'ctl-v-src'}),
      h('dl', {class: 'kv'}, h('dt', {text: 'Başlama / durma'}), h('dd', {id: 'ctl-v-th'}, '—'),
        h('dt', {text: 'Nem sınırı'}), h('dd', {id: 'ctl-v-rh'}, '—'), h('dt', {text: 'Isıtırken nem'}), h('dd', {id: 'ctl-v-pol'}, '—'),
        h('dt', {text: 'Manuel öncelik'}), h('dd', {id: 'ctl-v-mp'}, '—')),
      h('p', {class: 'field-hint', text: 'Isıtırken gereksiz havalandırma yapılmaz; başlama eşiği etkin hedefin en az 2 °C üstünde tutulur.'}))));

  // ---- PID
  const termRow = (k, label) => h('div', {class: 'term'}, h('span', {text: label}),
    (() => { const s = svgEl('svg', {viewBox: '-100 0 200 10', preserveAspectRatio: 'none', class: 'bar', id: 'pid-t-' + k});
      s.append(svgEl('rect', {class: 'bar-fill', x: '0', y: '0', width: '0', height: '10'}), svgEl('line', {class: 'bar-mark', x1: '0', x2: '0', y1: '0', y2: '10'})); return s; })(),
    h('span', {class: 'num', id: 'pid-v-' + k}, '—'));
  const form = h('form', {class: 'form-grid', novalidate: '', id: 'pid-form'});
  const pf = [['pid_mode', 'Algoritma', 'select', ['P', 'PI', 'PID', 'ONOFF']], ['pid_kp', 'Kp (%/°C)', 'number', '0.5', '200', '0.5'],
    ['pid_ki', 'Ki (%/(°C·dk))', 'number', '0', '20', '0.05'], ['pid_kd', 'Kd (%·dk/°C)', 'number', '0', '60', '0.5'],
    ['pid_deadband', 'Ölü bant (°C)', 'number', '0', '1', '0.05'], ['setpoint_ramp_c_per_min', 'Hedef rampası (°C/dk)', 'number', '0', '2', '0.05']];
  pf.forEach(([k, l, type, a, b, c]) => {
    const inp = type === 'select' ? h('select', {id: 'pf-' + k, name: k}, ...a.map(o => h('option', {value: o, text: o})))
      : h('input', {type: 'number', id: 'pf-' + k, name: k, min: a, max: b, step: c, inputmode: 'decimal', required: ''});
    inp.addEventListener('input', () => { form.dataset.dirty = '1'; inp.closest('.field').classList.add('changed'); });
    form.append(h('div', {class: 'field'}, h('label', {for: inp.id, text: l}), inp));
  });
  const pidMsg = h('div', {class: 'cmd-msg'});
  const pidSave = h('button', {type: 'submit', class: 'primary', 'data-icon': 'save', 'data-text': ''}, 'Katsayıları uygula');
  const pidRevert = h('button', {type: 'button', 'data-icon': 'undo'}, 'Geri al');
  form.append(h('div', {class: 'full btn-row'}, pidRevert, pidSave), h('div', {class: 'full'}, pidMsg));
  pidRevert.addEventListener('click', () => { delete form.dataset.dirty; $$('.changed', form).forEach(f => f.classList.remove('changed')); loadPidForm(true); });
  form.addEventListener('submit', async e => {
    e.preventDefault();
    const bad = $$('input', form).find(i => !i.checkValidity());
    if (bad) { bad.reportValidity(); bad.focus(); showMsg({msg: pidMsg}, 'critical', 'Kaydedilmedi: alanı düzeltin.'); return; }
    const body = {};
    const diffs = [];
    pf.forEach(([k, l]) => {
      const el = $('#pf-' + k), v = el.tagName === 'SELECT' ? el.value : Number(el.value);
      body[k] = v;
      if (String(v) !== String(pidCfg[k])) diffs.push(l.split(' (')[0] + ' ' + pidCfg[k] + ' → ' + v);
    });
    if (!diffs.length) { showMsg({msg: pidMsg}, 'info', 'Değişiklik yok.'); return; }
    const ok = await confirmDlg('PID katsayıları', diffs.join(' · ') + '. Çıkış anında sabit kalır (bumpless). Uygulansın mı?', 'Uygula');
    if (!ok) return;
    pidSave.setAttribute('aria-busy', 'true');
    try {
      const r = await api('/api/settings', body);
      toast(r && r.message || 'Kaydedildi');
      delete form.dataset.dirty;
      $$('.changed', form).forEach(f => f.classList.remove('changed'));
      Object.assign(pidCfg, body);
      showMsg({msg: pidMsg}, null, '');
    } catch (err) {
      showMsg({msg: pidMsg}, 'critical', 'Kaydedilemedi: ' + err.message + ' Değerler formda duruyor.');
    }
    pidSave.setAttribute('aria-busy', 'false');
  });
  let pidCfg = {};
  async function loadPidForm(force) {
    try {
      const s = await api('/api/settings');
      pf.forEach(([k]) => { pidCfg[k] = s[k]; });
      if (!form.dataset.dirty || force) pf.forEach(([k]) => { $('#pf-' + k).value = s[k]; });
    } catch (e) { /* bir sonraki girişte denenir */ }
  }
  pPid.append(h('div', {class: 'grid2'},
    h('section', {class: 'panel'}, h('h3', {text: 'Canlı PID'}),
      h('dl', {class: 'kv'}, h('dt', {text: 'Hata (SP − PV)'}), h('dd', {id: 'pid-err'}, '—'),
        h('dt', {text: 'PID çıkışı / talep'}), h('dd', {id: 'pid-out'}, '—'), h('dt', {text: 'Doyum'}), h('dd', {id: 'pid-sat'}, '—'),
        h('dt', {text: 'Kademe'}), h('dd', {id: 'pid-stage'}, '—')),
      h('h4', {class: 'group-heading', text: 'Terim katkıları (%)'}),
      h('div', {class: 'terms'}, termRow('p', 'P'), termRow('i', 'I'), termRow('d', 'D'))),
    h('section', {class: 'panel'}, h('h3', {text: 'Katsayılar'}), form,
      h('p', {class: 'field-hint', text: 'Ki ve Kd dakika tabanlıdır; kontrol periyodu değişse de davranış korunur. Değişiklik yönetici yetkisi ister.'}))));
  onEnter.control = () => loadPidForm(false);

  pageUpdaters.control = (d, st) => {
    if (!d) return;
    setRadio(modes, d.operating_mode, st);
    if (!sp.inp.dataset.dirty && document.activeElement !== sp.inp) sp.inp.value = d.temperature_setpoint.toFixed(1);
    sp.btn.disabled = st;
    setText($('#ctl-spe'), fmt.t(d.setpoint_effective) + ' °C');
    setText($('#ctl-src'), PROFILE_TR[d.setpoint_source] || d.setpoint_source);
    syncNum(man, d.manual_heat_demand, st, d.operating_mode !== 'MANUAL');
    en.cb.checked = onoff(d.controller_enable);
    en.cb.disabled = st;
    lockBtn.disabled = st;
    setText($('#ctl-lock-st'), d.local_lock === 'ON' ? 'Yerel kilit etkin · kalan ' + fmt.dur(d.local_lock_remaining_s) : 'Yerel kilit kapalı');
    setRadio(prof, d.profile, st);
    setText($('#ctl-pa'), (PROFILE_TR[d.profile_active] || d.profile_active) + ' · ' + fmt.t(d.setpoint_effective) + ' °C');
    setText($('#ctl-pg'), d.programs_enabled !== 'ON' ? 'KAPALI' : d.time_valid !== 'ON' ? 'Saat bekleniyor' : (d.program_active && d.program_active !== '—' ? d.program_active + (d.program_until ? ' → ' + String(d.program_until).slice(11, 16) : '') : 'Etkin program yok'));
    setText($('#ctl-sn'), onoff(d.sched_night) ? 'AÇIK (Suite Programs)' : 'KAPALI');
    setText($('#ctl-sa'), onoff(d.sched_away) ? 'AÇIK (Suite Programs)' : 'KAPALI');
    setText($('#ctl-af'), d.setpoint_source === 'ANTIFREEZE' ? 'ETKİN · hedef ' + fmt.t(d.setpoint_frost) + ' °C' : 'Bekliyor (T1 < ' + fmt.t(d.frost_guard_temperature) + ' °C)');
    boostBtn.disabled = st;
    boostBtn.lastChild.textContent = d.boost === 'ON' ? 'Boost iptal' : 'Boost başlat';
    setText($('#ctl-boost-st'), d.boost === 'ON' ? 'kalan ' + d.boost_remaining_min + ' dk' : d.boost_minutes + ' dk · ' + fmt.t(d.setpoint_boost) + ' °C');
    const rows = $('#ctl-prof-rows');
    const pr = [['DAY', d.temperature_setpoint], ['NIGHT', d.setpoint_night], ['AWAY', d.setpoint_away], ['FROST', d.setpoint_frost], ['BOOST', d.setpoint_boost]];
    if (rows.children.length !== pr.length) { rows.textContent = ''; pr.forEach(() => rows.append(h('tr', null, h('td', {'data-label': 'Profil'}), h('td', {'data-label': 'Hedef', class: 'num'}), h('td', {'data-label': 'Kaynak'}), h('td', {'data-label': 'Durum'})))); }
    pr.forEach(([k, v], i) => {
      const c = rows.children[i].children;
      setText(c[0], PROFILE_TR[k]);
      setText(c[1], fmt.t(v) + ' °C');
      setText(c[2], k === 'DAY' ? 'Ana hedef' : k === 'BOOST' ? 'Süreli (' + d.boost_minutes + ' dk)' : (k === 'FROST' ? 'Güvenlik' : 'Profil'));
      setText(c[3], d.profile_active === k ? 'ETKİN' : '—');
    });
    psp.forEach(n => syncNum(n, d[n.inp.id.slice(2)], st));
    // havalandırma
    vsw.cb.checked = onoff(d.ventilation_fan_manual);
    vsw.cb.disabled = st;
    setText($('#ctl-v-req'), onoff(d.ventilation_fan_manual) ? 'AÇIK' : 'KAPALI');
    setText($('#ctl-v-eff'), onoff(d.ventilation_fan_active) ? 'ÇALIŞIYOR' : 'KAPALI');
    const vw = $('#ctl-v-why'); if (vw.dataset.r !== d.ventilation_fan_reason) { vw.dataset.r = d.ventilation_fan_reason; vw.textContent = ''; vw.append(reasonEl(d.ventilation_fan_reason)); }
    setText($('#ctl-v-st'), VSTATE_TR[d.ventilation_state] || d.ventilation_state);
    const src = $('#ctl-v-src');
    const S = d.vent_sources || [];
    const all = [['TEMP_HIGH', 'Sıcaklık yüksek'], ['HUMIDITY_HIGH', 'Nem yüksek'], ['MANUAL', 'Manuel istek'], ['SCHEDULED', 'Periyodik'], ['OVERTEMP', 'Aşırı sıcaklık tahliyesi']];
    const key = S.join(',');
    if (src.dataset.k !== key) {
      src.dataset.k = key;
      src.textContent = '';
      all.forEach(([k, l]) => src.append(h('div', {class: 'alarm-row ' + (S.includes(k) ? (k === 'OVERTEMP' ? 'critical' : 'warn') : 'info')},
        icon(S.includes(k) ? 'check' : 'pause'), h('span', {class: 'txt'}, h('b', {text: l})), h('span', {text: S.includes(k) ? 'İSTİYOR' : 'pasif'}))));
    }
    setText($('#ctl-v-th'), fmt.t(d.ventilation_start_effective) + ' / ' + fmt.t(d.ventilation_start_effective - (d.ventilation_start_temperature - d.ventilation_stop_temperature)) + ' °C');
    setText($('#ctl-v-rh'), d.humidity_high_limit + ' % (histerezis ' + d.humidity_hysteresis + ' %)');
    setText($('#ctl-v-pol'), {INHIBIT: 'Engelle', ALLOW: 'İzin ver', ALLOW_ABOVE_SP: 'Hedefe yakınsa izin ver'}[d.humidity_vent_while_heating] || '—');
    setText($('#ctl-v-mp'), d.manual_vent_priority === 'VENT_WINS' ? 'Havalandırma kazanır (ısıtma durur)' : 'Isıtma kazanır');
    // PID
    setText($('#pid-err'), d.pid_error === null ? '—' : (d.pid_error >= 0 ? '+' : '') + d.pid_error.toFixed(2) + ' °C');
    setText($('#pid-out'), d.pid_output.toFixed(1) + ' % / ' + d.heat_demand.toFixed(1) + ' %');
    const satEl = $('#pid-sat');
    const sk = d.pid_saturation + d.anti_windup_active + d.pid_tracking;
    if (satEl.dataset.k !== sk) {
      satEl.dataset.k = sk;
      satEl.textContent = '';
      satEl.append(h('span', {class: 'badge ' + (d.pid_saturation === 'NONE' ? 'ok' : 'warn'), text: d.pid_saturation === 'NONE' ? 'YOK' : d.pid_saturation === 'HIGH' ? 'ÜST' : 'ALT'}), ' ',
        onoff(d.anti_windup_active) ? h('span', {class: 'badge warn', text: 'ANTI-WINDUP'}) : '', ' ',
        onoff(d.pid_tracking) ? h('span', {class: 'badge', text: 'İZLEME'}) : '');
    }
    setText($('#pid-stage'), d.power_stage + ' · R1 ' + d.r1_duty.toFixed(0) + ' % · R2 ' + d.r2_duty.toFixed(0) + ' %');
    [['p', d.pid_p], ['i', d.pid_i], ['d', d.pid_d]].forEach(([k, v]) => {
      const s = $('#pid-t-' + k), r = s.querySelector('rect');
      const c = Math.max(-100, Math.min(100, v || 0));
      r.setAttribute('x', c < 0 ? c.toFixed(1) : '0');
      r.setAttribute('width', Math.abs(c).toFixed(1));
      r.setAttribute('class', 'bar-fill' + (c < 0 ? ' neg' : ''));
      setText($('#pid-v-' + k), (v || 0).toFixed(1));
    });
  };
};

// ================================================================= PROGRAMLAR (ADR-009)
const DAY_TR = ['Pzt', 'Sal', 'Çar', 'Per', 'Cum', 'Cmt', 'Paz'];
const KIND_TR = {WEEKLY: 'Haftalık', DATE_RANGE: 'Tarih aralığı', ONCE: 'Tek sefer'};
const ACTION_TR = {SETPOINT: 'Sıcaklık hedefi', PROFILE: 'Profil', HEATING_OFF: 'Isıtmayı durdur', VENTILATE: 'Havalandır'};
const PROG_ERR_TR = {NAME: 'Ad 1–23 bayt olmalı.', DAYS: 'En az bir gün seçin.', START: 'Başlangıç saati geçersiz.',
  END: 'Bitiş saati başlangıçtan farklı olmalı.', DURATION: 'Süre aralık dışında.', DATE_ORDER: 'Bitiş tarihi başlangıçtan önce olamaz.',
  DATE_SPAN: 'Tarih aralığı en çok 366 gün olabilir.', SETPOINT: 'Hedef 5–30 °C ve 0.5 adımında olmalı.',
  PROFILE: 'Profil Gece, Uzakta veya Donma olmalı.', SAFETY_MARGIN: 'Hedef, aşırı sıcaklık limitinin en az 10 °C altında olmalı.',
  TOO_MANY: 'En çok 16 program tanımlanabilir.'};
const hhmm = m => String(Math.floor(m / 60) % 24).padStart(2, '0') + ':' + String(m % 60).padStart(2, '0');
const toMin = s => { const [a, b] = String(s || '').split(':').map(Number); return a * 60 + b; };
// Gün numarası (1970-01-01'den yerel gün) ↔ YYYY-MM-DD; takvim UTC ile hesaplanır (saat dilimi etkisi yok)
const dayOf = s => Math.floor(Date.UTC(+s.slice(0, 4), +s.slice(5, 7) - 1, +s.slice(8, 10)) / 86400000);
const dateOf = d => new Date(d * 86400000).toISOString().slice(0, 10);
const wd = d => ((d % 7) + 7 + 3) % 7;   // 0 = Pazartesi
function progLen(p) { return p.end === 'ALL_DAY' ? 1440 : p.end === 'END_TIME' ? (toMin(p.end_time) - toMin(p.start) + 1440) % 1440 : +p.duration; }
function occursOn(p, d) {
  if (p.kind === 'WEEKLY') return (p.days >> wd(d)) & 1;
  if (p.kind === 'DATE_RANGE') return d >= dayOf(p.date_from) && d <= dayOf(p.date_to) && (!p.days || ((p.days >> wd(d)) & 1));
  return d === dayOf(p.date_from);
}
function progWhen(p) {
  let days = '';
  if (p.kind === 'WEEKLY' || (p.kind === 'DATE_RANGE' && p.days)) {
    const sel = DAY_TR.filter((x, i) => (p.days >> i) & 1);
    days = p.days === 127 ? 'Her gün' : p.days === 31 ? 'Hafta içi' : p.days === 96 ? 'Hafta sonu' : sel.join(', ');
  }
  const range = p.kind === 'DATE_RANGE' ? p.date_from + ' → ' + p.date_to : p.kind === 'ONCE' ? p.date_from : '';
  const time = p.end === 'ALL_DAY' ? 'tüm gün' : p.start + (p.end === 'END_TIME' ? '–' + p.end_time + (toMin(p.end_time) <= toMin(p.start) ? ' (+1)' : '') : ' · ' + fmtDur(+p.duration));
  return [range, days, time].filter(Boolean).join(' · ');
}
function fmtDur(m) { const hh = Math.floor(m / 60), mm = m % 60; return (hh ? hh + ' sa ' : '') + (mm ? mm + ' dk' : (hh ? '' : '0 dk')).trim(); }
function progWhat(p) {
  if (p.action === 'SETPOINT') return fmt.t(p.setpoint) + ' °C';
  if (p.action === 'PROFILE') return (PROFILE_TR[p.profile] || p.profile) + ' profili';
  if (p.action === 'HEATING_OFF') return 'Isıtma durur (donma koruması sürer)';
  return 'Havalandırma isteği';
}
function localClock(min) {
  if (min === null || min === undefined || min < 0) return '—';
  const d = Math.floor(min / 1440);
  return DAY_TR[wd(d)] + ' ' + dateOf(d).slice(8, 10) + '.' + dateOf(d).slice(5, 7) + ' ' + hhmm(min % 1440);
}

builders.programs = sec => {
  let st = null;          // son /api/programs
  const enabled = h('input', {type: 'checkbox', id: 'pg-enabled'});
  const enMsg = h('div', {class: 'cmd-msg'});
  enabled.addEventListener('change', () => sendCmd('programs_enabled', enabled.checked ? 'ON' : 'OFF', {btns: [enabled], msg: enMsg, onDone: load}));
  const holdBtn = h('button', {type: 'button', 'data-icon': 'pause', 'data-text': ''}, 'Etkin programı atla');
  const holdMsg = h('div', {class: 'cmd-msg'});
  holdBtn.addEventListener('click', async () => {
    const name = st && st.active.climate >= 0 ? st.list[st.active.climate].name : '';
    if (!(await confirmDlg('Programı atla', '“' + name + '” bu oluşumun sonuna (' + localClock(st.active.until) + ') kadar uygulanmayacak; sonraki oluşum normal çalışır. Devam edilsin mi?', 'Atla'))) return;
    sendCmd('program_hold', 'PRESS', {btns: [holdBtn], msg: holdMsg, onDone: load});
  });
  const addBtn = h('button', {type: 'button', class: 'primary', 'data-icon': 'plus', 'data-text': ''}, 'Program ekle');
  addBtn.addEventListener('click', () => openEditor(-1));
  const tl = svgEl('svg', {class: 'chart week', viewBox: '0 0 1000 250', role: 'img', 'aria-labelledby': 'pg-tl-cap'});
  sec.append(sectionHead('Programlar'),
    h('div', {id: 'pg-notice'}),
    h('div', {class: 'grid2'},
      h('section', {class: 'panel'}, h('h3', {text: 'Şu an'}),
        h('dl', {class: 'kv'}, h('dt', {text: 'İklim programı'}), h('dd', {id: 'pg-now'}, '—'), h('dt', {text: 'Havalandırma programı'}), h('dd', {id: 'pg-vnow'}, '—'),
          h('dt', {text: 'Sonraki değişim'}), h('dd', {id: 'pg-next'}, '—'), h('dt', {text: 'Etkin hedef'}), h('dd', {id: 'pg-sp'}, '—')),
        h('div', {class: 'btn-row'}, holdBtn), holdMsg),
      h('section', {class: 'panel'}, h('h3', {text: 'Modül'}),
        h('div', {class: 'field toggle'}, enabled, h('label', {for: 'pg-enabled', text: 'Yerel programlar etkin'})), enMsg,
        h('p', {class: 'field-hint', text: 'Öncelik: Boost › açık profil seçimi › yerel program › Suite “Uzakta” › Suite “Gece” › Gündüz. Çakışan programlarda tek sefer › tarih aralığı › haftalık; aynı türde daha geç başlayan kazanır. Donma koruması ve güvenlik kilitleri her programın üstündedir.'}),
        h('p', {class: 'field-hint', id: 'pg-clock'}, '—'))),
    h('section', {class: 'panel'}, h('h3', {text: 'Bu hafta'}), h('p', {class: 'sr-only', id: 'pg-tl-cap', text: 'Haftalık program zaman çizelgesi'}),
      h('div', {class: 'table-wrap'}, tl),
      h('div', {class: 'legend', id: 'pg-legend'})),
    h('section', {class: 'panel'}, h('div', {class: 'section-head'}, h('h3', {text: 'Program listesi'}), addBtn),
      h('div', {class: 'prog-list', id: 'pg-list'}),
      h('p', {class: 'field-hint', text: 'En çok 16 program. Liste bütünüyle doğrulanır; hatalı kayıtta hiçbir değişiklik uygulanmaz. Programlar yalnız cihaz saati eşitlendiğinde çalışır.'})));

  // ---- zaman çizelgesi
  function drawWeek() {
    tl.textContent = '';
    if (!st) return;
    const now = st.now, today = Math.floor(now / 1440), monday = today - wd(today);
    const L = 64, R = 12, top = 20, rowH = 30, W = 1000;
    const X = m => L + (W - L - R) * m / 1440;
    for (let hr = 0; hr <= 24; hr += 3) {
      const x = X(hr * 60);
      tl.append(svgEl('line', {x1: x, x2: x, y1: top - 4, y2: top + 7 * rowH, class: 'wk-grid'}));
      const t = svgEl('text', {x, y: 12, 'text-anchor': hr === 0 ? 'start' : hr === 24 ? 'end' : 'middle'});
      t.textContent = String(hr).padStart(2, '0') + ':00';
      tl.append(t);
    }
    for (let r = 0; r < 7; r++) {
      const d = monday + r, y = top + r * rowH;
      const lab = svgEl('text', {x: 4, y: y + 19, class: d === today ? 'wk-today' : ''});
      lab.textContent = DAY_TR[r] + ' ' + dateOf(d).slice(8, 10) + '.' + dateOf(d).slice(5, 7);
      tl.append(lab);
      tl.append(svgEl('rect', {x: L, y: y + 3, width: W - L - R, height: rowH - 6, class: 'gt-bg'}));
    }
    // oluşumları çiz: her program × (önceki gün … pazar), gün sınırında böl
    st.list.forEach((p, i) => {
      if (!p.enabled) return;
      const len = progLen(p);
      for (let d = monday - 7; d < monday + 7; d++) {
        if (!occursOn(p, d)) continue;
        let s = d * 1440 + (p.end === 'ALL_DAY' ? 0 : toMin(p.start)), e = s + len;
        const occS = s;
        while (s < e) {
          const day = Math.floor(s / 1440), r = day - monday, segEnd = Math.min(e, (day + 1) * 1440);
          if (r >= 0 && r < 7) {
            const y = top + r * rowH, vent = p.action === 'VENTILATE';
            const rect = svgEl('rect', {x: X(s - day * 1440).toFixed(1), y: vent ? y + rowH - 11 : y + 4, width: Math.max(2, X(segEnd - day * 1440) - X(s - day * 1440)).toFixed(1),
              height: vent ? 7 : rowH - 16, class: 'wk-' + p.action.toLowerCase() + ((st.active.climate === i || st.active.vent === i) && now >= occS && now < e ? ' wk-active' : '')});
            const tt = svgEl('title', {});
            tt.textContent = p.name + ' · ' + progWhat(p);
            rect.append(tt);
            tl.append(rect);
            if (!vent && X(segEnd - day * 1440) - X(s - day * 1440) > p.name.length * 6.2 + 10) {
              const tx = svgEl('text', {x: X(s - day * 1440) + 4, y: y + 16, class: 'wk-label'});
              tx.textContent = p.name;
              tl.append(tx);
            }
          }
          s = segEnd;
        }
      }
    });
    if (st.time_valid) {
      const r = today - monday, x = X(now % 1440);
      tl.append(svgEl('line', {x1: x, x2: x, y1: top + r * rowH, y2: top + (r + 1) * rowH, class: 'wk-now'}));
    }
    const lg = $('#pg-legend');
    if (!lg.children.length) {
      [['setpoint', 'Sıcaklık hedefi'], ['profile', 'Profil'], ['heating_off', 'Isıtma durur'], ['ventilate', 'Havalandırma (alt şerit)']].forEach(([k, l]) => {
        const s = svgEl('svg', {viewBox: '0 0 26 8'});
        s.append(svgEl('rect', {x: '0', y: '0', width: '26', height: '8', class: 'wk-' + k}));
        lg.append(h('span', null, s, l));
      });
    }
  }
  // ---- liste
  function renderList() {
    const box = $('#pg-list');
    box.textContent = '';
    if (!st.list.length) { box.append(h('p', {class: 'dim', text: 'Henüz program yok.'})); return; }
    st.list.forEach((p, i) => {
      const active = st.active.climate === i || st.active.vent === i;
      const edit = h('button', {type: 'button', 'data-icon': 'pencil'}, p.name + ' düzenle');
      const toggle = h('button', {type: 'button', 'data-icon': p.enabled ? 'pause' : 'play'}, p.name + (p.enabled ? ' duraklat' : ' başlat'));
      const del = h('button', {type: 'button', 'data-icon': 'trash', class: 'danger'}, p.name + ' sil');
      edit.addEventListener('click', () => openEditor(i));
      toggle.addEventListener('click', () => { const l = clone(); l[i].enabled = !l[i].enabled; save(l, p.enabled ? '“' + p.name + '” duraklatıldı' : '“' + p.name + '” başlatıldı'); });
      del.addEventListener('click', async () => {
        if (!(await confirmDlg('Programı sil', '“' + p.name + '” silinecek. Devam edilsin mi?', 'Sil', true))) return;
        const l = clone(); l.splice(i, 1); save(l, 'Program silindi');
      });
      box.append(h('article', {class: 'prog' + (p.enabled ? '' : ' off') + (active ? ' on' : '')},
        h('div', {class: 'prog-head'}, h('span', {class: 'badge wk-b-' + p.action.toLowerCase(), text: KIND_TR[p.kind]}),
          h('b', {class: 'prog-name', text: p.name}),
          active ? h('span', {class: 'badge ok', text: 'ŞU AN ETKİN'}) : null, p.enabled ? null : h('span', {class: 'badge', text: 'DURAKLATILDI'})),
        h('div', {class: 'prog-when', text: progWhen(p)}),
        h('div', {class: 'prog-what'}, h('span', {class: 'dim', text: ACTION_TR[p.action] + ': '}), progWhat(p)),
        h('div', {class: 'prog-acts'}, edit, toggle, del)));
    });
    iconize(box);
  }
  const clone = () => JSON.parse(JSON.stringify(st.list));
  async function save(list, okMsg) {
    try {
      await api('/api/programs', {list});
      toast(okMsg || 'Programlar kaydedildi');
      await load();
      return true;
    } catch (e) {
      toast('Kaydedilmedi: ' + ((e.body && PROG_ERR_TR[e.body.code]) || e.message) + (e.body && e.body.index >= 0 ? ' (' + (list[e.body.index] || {}).name + ')' : ''), true);
      return false;
    }
  }

  // ---- düzenleyici diyaloğu
  const dlg = h('dialog', {id: 'pg-dlg', 'aria-labelledby': 'pg-dlg-t', class: 'wide'});
  document.body.append(dlg);
  function openEditor(idx) {
    const p = idx >= 0 ? st.list[idx] : {name: '', enabled: true, kind: 'WEEKLY', days: 31, date_from: dateOf(Math.floor(st.now / 1440)), date_to: dateOf(Math.floor(st.now / 1440) + 7),
      start: '06:30', end: 'END_TIME', end_time: '08:30', duration: 60, action: 'SETPOINT', setpoint: 22, profile: 'NIGHT'};
    dlg.textContent = '';
    const nm = h('input', {id: 'pe-name', maxlength: '23', required: '', value: p.name, autocomplete: 'off'});
    const en = h('input', {type: 'checkbox', id: 'pe-en'});
    en.checked = p.enabled;
    const kind = radioGroup('pe-kind', [['WEEKLY', 'Haftalık'], ['DATE_RANGE', 'Tarih aralığı'], ['ONCE', 'Tek sefer']], 'Tekrar');
    $$('input', kind).forEach(i => { i.checked = i.value === p.kind; });
    const days = h('fieldset', {class: 'days', id: 'pe-days'}, h('legend', {text: 'Günler'}),
      ...DAY_TR.map((d, i) => { const c = h('input', {type: 'checkbox', id: 'pe-d' + i}); c.checked = !!((p.days >> i) & 1); return h('label', {class: 'radio-choice'}, c, h('span', {text: d})); }),
      h('span', {class: 'day-quick'}, ...[['Hafta içi', 31], ['Hafta sonu', 96], ['Her gün', 127]].map(([t, m]) => {
        const b = h('button', {type: 'button'}, t);
        b.addEventListener('click', () => DAY_TR.forEach((x, i) => { $('#pe-d' + i).checked = !!((m >> i) & 1); }));
        return b;
      })));
    const df = h('input', {type: 'date', id: 'pe-df', value: p.date_from || '', min: '2000-01-01', max: '2199-12-31'});
    const dt = h('input', {type: 'date', id: 'pe-dt', value: p.date_to || '', min: '2000-01-01', max: '2199-12-31'});
    const start = h('input', {type: 'time', id: 'pe-start', value: p.start || '06:00', step: '60'});
    const endKind = radioGroup('pe-endk', [['END_TIME', 'Bitiş saati'], ['DURATION', 'Süre'], ['ALL_DAY', 'Tüm gün']], 'Bitiş');
    $$('input', endKind).forEach(i => { i.checked = i.value === p.end; });
    const et = h('input', {type: 'time', id: 'pe-et', value: p.end_time || '08:00', step: '60'});
    const durH = h('input', {type: 'number', id: 'pe-dh', min: '0', max: '168', value: String(Math.floor((+p.duration || 60) / 60)), inputmode: 'numeric'});
    const durM = h('input', {type: 'number', id: 'pe-dm', min: '0', max: '59', value: String((+p.duration || 60) % 60), inputmode: 'numeric'});
    const act = h('select', {id: 'pe-act'}, ...Object.entries(ACTION_TR).map(([k, v]) => h('option', {value: k, text: v})));
    act.value = p.action;
    const sp = h('input', {type: 'number', id: 'pe-sp', min: '5', max: '30', step: '0.5', value: String(p.setpoint ?? 22), inputmode: 'decimal'});
    const prof = h('select', {id: 'pe-prof'}, ...[['NIGHT', 'Gece'], ['AWAY', 'Uzakta'], ['FROST', 'Donma']].map(([k, v]) => h('option', {value: k, text: v})));
    prof.value = p.profile || 'NIGHT';
    const err = h('div', {class: 'cmd-msg', id: 'pe-err'});
    const preview = h('p', {class: 'field-hint', id: 'pe-prev'});
    const f = (id, label, input, hint) => h('div', {class: 'field', id: 'pf-' + id}, h('label', {for: input.id, text: label}), input, hint ? h('small', {class: 'field-hint', text: hint}) : null);
    const saveBtn = h('button', {type: 'submit', class: 'primary', 'data-icon': 'save', 'data-text': ''}, 'Programı kaydet');
    const cancel = h('button', {type: 'button'}, 'Vazgeç');
    const form = h('form', {novalidate: '', class: 'form-grid'},
      f('name', 'Ad', nm, '1–23 karakter; listede ve olay günlüğünde görünür.'),
      h('div', {class: 'field toggle'}, en, h('label', {for: 'pe-en', text: 'Etkin'})),
      h('div', {class: 'field full'}, h('span', {class: 'lbl', text: 'Tekrar'}), kind),
      h('div', {class: 'full', id: 'pf-days'}, days, h('small', {class: 'field-hint', id: 'pe-days-hint'})),
      f('df', 'Başlangıç tarihi', df), f('dt', 'Bitiş tarihi', dt),
      h('div', {class: 'field full'}, h('span', {class: 'lbl', text: 'Bitiş'}), endKind),
      f('start', 'Başlangıç saati', start),
      f('et', 'Bitiş saati', et, 'Başlangıçtan önceyse ertesi güne taşar (ör. 22:00 → 06:00).'),
      h('div', {class: 'field', id: 'pf-dur'}, h('span', {class: 'lbl', text: 'Süre'}),
        h('div', {class: 'row'}, durH, h('span', {class: 'dim', text: 'sa'}), durM, h('span', {class: 'dim', text: 'dk'})),
        h('small', {class: 'field-hint', id: 'pe-dur-hint'})),
      f('act', 'Eylem', act), f('sp', 'Hedef sıcaklık (°C)', sp, '5–30 °C, 0.5 adım'), f('prof', 'Profil', prof),
      h('div', {class: 'full'}, preview), h('div', {class: 'full'}, err),
      h('div', {class: 'full dlg-acts'}, cancel, saveBtn));
    dlg.append(h('div', {class: 'dlg-head'}, h('h2', {id: 'pg-dlg-t', text: idx >= 0 ? 'Programı düzenle' : 'Yeni program'}),
      (() => { const x = h('button', {type: 'button', 'data-icon': 'x'}, 'Kapat'); x.addEventListener('click', () => dlg.close()); return x; })()), form);
    iconize(dlg);
    const read = () => {
      const k = $('input:checked', kind).value, ek = $('input:checked', endKind).value;
      let mask = 0;
      DAY_TR.forEach((x, i) => { if ($('#pe-d' + i).checked) mask |= 1 << i; });
      return {name: nm.value.trim(), enabled: en.checked, kind: k, days: mask, date_from: df.value, date_to: k === 'ONCE' ? df.value : dt.value,
        start: start.value, end: ek, end_time: et.value, duration: (+durH.value || 0) * 60 + (+durM.value || 0), action: act.value,
        setpoint: +sp.value, profile: prof.value};
    };
    const sync = () => {
      const v = read();
      $('#pf-days').hidden = v.kind === 'ONCE';
      setText($('#pe-days-hint'), v.kind === 'DATE_RANGE' ? 'Aralık içinde yalnız seçili günler; hiçbiri seçili değilse her gün.' : 'En az bir gün seçin.');
      $('#pf-df').hidden = v.kind === 'WEEKLY';
      setText($('#pf-df label'), v.kind === 'ONCE' ? 'Tarih' : 'Başlangıç tarihi');
      $('#pf-dt').hidden = v.kind !== 'DATE_RANGE';
      $('#pf-start').hidden = v.end === 'ALL_DAY';
      $('#pf-et').hidden = v.end !== 'END_TIME';
      $('#pf-dur').hidden = v.end !== 'DURATION';
      setText($('#pe-dur-hint'), v.kind === 'ONCE' ? 'En çok 7 gün.' : 'En çok 24 saat.');
      $('#pf-sp').hidden = v.action !== 'SETPOINT';
      $('#pf-prof').hidden = v.action !== 'PROFILE';
      setText(preview, 'Özet: ' + progWhen(v) + ' · ' + progWhat(v));
    };
    form.addEventListener('input', sync);
    form.addEventListener('change', sync);
    sync();
    cancel.addEventListener('click', () => dlg.close());
    form.addEventListener('submit', async e => {
      e.preventDefault();
      const v = read();
      const bad = m => { showMsg({msg: err}, 'critical', m); return false; };
      const okLocal = (() => {
        if (!v.name || new TextEncoder().encode(v.name).length > 23) return bad(PROG_ERR_TR.NAME);
        if (v.kind === 'WEEKLY' && !v.days) return bad(PROG_ERR_TR.DAYS);
        if (v.kind !== 'WEEKLY' && !v.date_from) return bad('Tarih seçin.');
        if (v.kind === 'DATE_RANGE' && (!v.date_to || v.date_to < v.date_from)) return bad(PROG_ERR_TR.DATE_ORDER);
        if (v.end !== 'ALL_DAY' && !v.start) return bad(PROG_ERR_TR.START);
        if (v.end === 'END_TIME' && (!v.end_time || v.end_time === v.start)) return bad(PROG_ERR_TR.END);
        if (v.end === 'DURATION' && (v.duration < 1 || v.duration > (v.kind === 'ONCE' ? 10080 : 1440))) return bad(PROG_ERR_TR.DURATION + (v.kind === 'ONCE' ? ' (1 dk–7 gün)' : ' (1 dk–24 sa)'));
        if (v.action === 'SETPOINT' && !(v.setpoint >= 5 && v.setpoint <= 30 && Number.isInteger(v.setpoint * 2))) return bad(PROG_ERR_TR.SETPOINT);
        return true;
      })();
      if (!okLocal) return;
      const l = clone();
      if (idx >= 0) l[idx] = v; else l.push(v);
      if (l.length > 16) { bad(PROG_ERR_TR.TOO_MANY); return; }
      saveBtn.setAttribute('aria-busy', 'true');
      if (await save(l, idx >= 0 ? '“' + v.name + '” güncellendi' : '“' + v.name + '” eklendi')) dlg.close();
      saveBtn.setAttribute('aria-busy', 'false');
    });
    if (dlg.showModal) dlg.showModal(); else dlg.setAttribute('open', '');
    nm.focus();
  }

  async function load() {
    try { st = await api('/api/programs'); } catch (e) { toast('Programlar alınamadı: ' + e.message, true); return; }
    enabled.checked = st.enabled;
    const n = $('#pg-notice');
    n.textContent = '';
    if (!st.time_valid) n.append(h('div', {class: 'notice warn'}, icon('warn'), h('span', {text: 'Saat bekleniyor: cihaz saati eşitlenene kadar programlar çalışmaz (NTP/RTC).'})));
    else if (!st.enabled) n.append(h('div', {class: 'notice warn'}, icon('warn'), h('span', {text: 'Yerel programlar kapalı. Hedef, profil seçimi ve Suite istekleriyle belirlenir.'})));
    const c = st.active.climate, v = st.active.vent;
    setText($('#pg-now'), c >= 0 ? st.list[c].name + ' · ' + progWhat(st.list[c]) + ' · ' + hhmm(st.active.until % 1440) + '’a kadar' : (st.active.held ? 'Atlandı (oluşum sonuna kadar)' : 'Yok'));
    setText($('#pg-vnow'), v >= 0 ? st.list[v].name + ' · ' + hhmm(st.active.vent_until % 1440) + '’a kadar' : 'Yok');
    setText($('#pg-next'), st.next_change >= 0 ? localClock(st.next_change) : 'Önümüzdeki 8 günde değişim yok');
    setText($('#pg-clock'), st.time_valid ? 'Cihaz saati: ' + localClock(st.now) + ' (UTC+' + (st.tz_offset_min / 60) + ')' : 'Cihaz saati eşitlenmedi.');
    holdBtn.disabled = c < 0;
    renderList();
    drawWeek();
  }
  let last = 0;
  pageUpdaters.programs = d => {
    if (d) setText($('#pg-sp'), fmt.t(d.setpoint_effective) + ' °C · ' + (PROFILE_TR[d.setpoint_source] || d.setpoint_source));
    if (Date.now() - last > 10000 && !dlg.open) { last = Date.now(); load(); }
  };
  onEnter.programs = () => { last = Date.now(); load(); };
};

// ================================================================= ÇIKIŞLAR
builders.outputs = sec => {
  sec.append(sectionHead('Çıkışlar'));
  const note = h('div', {class: 'notice info'}, icon('info'), h('span', {text: 'Gösterilen durum komutlanan çıkıştır (fiziksel geri bildirim yok). R1/R2 için doğrudan kumanda yalnız yerel servis modunda vardır.'}));
  const tb = h('tbody');
  const table = h('table', {class: 'out'}, h('thead', null, h('tr', null,
    ...['Çıkış', 'İstek', 'Etkin', 'Neden', 'Bugün', 'Anahtarlama', 'Test'].map((x, i) => h('th', {text: x, class: i === 6 ? 'svc-col' : null})))), tb);
  OUT.forEach((o, i) => {
    const reqCell = h('td', {'data-label': 'İstek'});
    if (o.req) {
      const msg = h('div', {class: 'cmd-msg'});
      const btn = h('button', {type: 'button', 'data-icon': 'power', 'data-text': '', id: 'out-btn-' + o.k}, 'İsteği değiştir');
      const ui = {btns: [btn], msg};
      btn.addEventListener('click', () => sendCmd(o.req, onoff(D && D[o.req]) ? 'OFF' : 'ON', ui));
      reqCell.append(h('div', {class: 'state-badge req'}, h('span', {class: 'rq', text: '—'})), btn, msg);
    } else reqCell.append(h('span', {class: 'rq', text: '—'}));
    const testBtn = h('button', {type: 'button', 'data-icon': 'wrench', 'data-text': '', id: 'out-test-' + o.k}, 'Test');
    const tmsg = h('div', {class: 'cmd-msg'});
    testBtn.addEventListener('click', async () => {
      const on = !onoff(D && D['svc_test_' + o.k]);
      try { await api('/api/service/test', {out: i, on}); } catch (e) { showMsg({msg: tmsg}, 'critical', e.message); }
    });
    tb.append(h('tr', {id: 'out-row-' + o.k},
      h('td', {'data-label': 'Çıkış'}, h('span', {class: 'card-head'}, h('span', {class: 'no', text: o.no}), icon(o.ico), h('span', {text: o.name}))),
      reqCell,
      h('td', {'data-label': 'Etkin'}, h('span', {class: 'state'}, h('span', {class: 'dot'}), h('span', {class: 'st', text: '—'}))),
      h('td', {'data-label': 'Neden', class: 'why'}),
      h('td', {'data-label': 'Bugün', class: 'num today'}, '—'),
      h('td', {'data-label': 'Anahtarlama', class: 'num sw'}, '—'),
      h('td', {'data-label': 'Test', class: 'svc-col'}, testBtn, tmsg)));
  });
  sec.append(note, h('section', {class: 'panel'}, h('div', {class: 'table-wrap'}, table)),
    h('p', {class: 'field-hint', text: 'Kurallar: R1 veya R2 açıkken ısıtıcı fanı kapanamaz; rezistans fan 3 s çalıştıktan sonra açılır; her kapanıştan sonra fan soğutma süresince çalışır.'}));
  pageUpdaters.outputs = (d, st) => {
    if (!d) return;
    const svc = d.controller_state === 'SERVICE';
    $$('.svc-col', sec).forEach(c => { c.hidden = !svc; });
    OUT.forEach(o => {
      const row = $('#out-row-' + o.k);
      const on = onoff(d[o.k + '_active']);
      const cell = $('.state', row);
      cell.classList.toggle('on', on && !st);
      cell.classList.toggle('off', !on && !st);
      setText($('.st', row), st ? 'BİLİNMİYOR' : (on ? 'ÇALIŞIYOR' : 'KAPALI'));
      const rq = $('.rq', row);
      if (o.req) setText(rq, onoff(d[o.req]) ? 'AÇIK' : 'KAPALI');
      else setText(rq, 'OTO (' + d[o.k + '_duty'].toFixed(0) + ' %)');
      const b = $('#out-btn-' + o.k); if (b) b.disabled = st;
      const why = $('.why', row), r = d[o.k + '_reason'];
      const rem = r === 'POST_COOL' ? d.post_cool_remaining_s : null;
      const k = r + rem;
      if (why.dataset.k !== k) { why.dataset.k = k; why.textContent = ''; why.append(reasonEl(r, rem)); }
      setText($('.today', row), fmt.hm(d[o.k + '_minutes_today']));
      setText($('.sw', row), fmt.int(d[o.k + '_switch_count']));
      const tb2 = $('#out-test-' + o.k);
      tb2.lastChild.textContent = onoff(d['svc_test_' + o.k]) ? 'Testi bitir' : 'Test';
    });
  };
};

// ================================================================= TRENDLER
const WINS = [[300, '5 dk'], [900, '15 dk'], [3600, '1 sa'], [21600, '6 sa'], [86400, '24 sa']];
builders.trends = sec => {
  let win = Number(lsGet('scada-trend-win')) || 900;
  const btns = WINS.map(([s, l]) => {
    const b = h('button', {type: 'button', 'aria-pressed': s === win ? 'true' : 'false', 'data-win': String(s)}, l);
    b.addEventListener('click', () => { win = s; lsSet('scada-trend-win', String(s)); $$('[data-win]', sec).forEach(x => x.setAttribute('aria-pressed', x === b ? 'true' : 'false')); load(); });
    return b;
  });
  const svg = svgEl('svg', {class: 'chart', viewBox: '0 0 1000 420', role: 'img', 'aria-labelledby': 'tr-cap'});
  const legend = h('div', {class: 'legend'});
  [['s-t1', 'Kulübe sıcaklığı', ''], ['s-sp', 'Etkin hedef', '6 4'], ['s-rh', 'Nem (sağ eksen)', '1 3'], ['s-dem', 'Isı talebi', '']].forEach(([c, l, da]) => {
    const s = svgEl('svg', {viewBox: '0 0 26 8'});
    s.append(svgEl('line', {x1: '0', y1: '4', x2: '26', y2: '4', class: 'ln-' + c}));
    legend.append(h('span', null, s, l));
  });
  const table = h('table', {class: 'out'}, h('thead', null, h('tr', null, ...['Zaman', 'T1 °C', 'Hedef °C', 'Nem %', 'Talep %', 'R1', 'R2', 'HF', 'VF'].map(x => h('th', {text: x})))), h('tbody', {id: 'tr-tbl'}));
  sec.append(sectionHead('Trendler'),
    h('section', {class: 'panel'},
      h('div', {class: 'trend-tools'}, h('div', {class: 'radio-group', role: 'group', 'aria-label': 'Zaman penceresi'}, ...btns),
        h('span', {class: 'dim', id: 'tr-info'}, '—')),
      h('p', {class: 'sr-only', id: 'tr-cap'}, 'Kulübe sıcaklığı, etkin hedef, nem, ısı talebi ve çıkış durumları'),
      svg, legend,
      h('details', null, h('summary', {text: 'Son 20 örnek (tablo)'}), h('div', {class: 'table-wrap'}, table))),
    h('p', {class: 'field-hint', text: 'Cihaz 1 sa (5 s) ve 24 sa (60 s) halkası tutar; halkalar RAM’dedir ve yeniden başlatmada sıfırlanır. Uzun dönem geçmiş MQTT Suite historian’dadır.'}));
  let data = null;
  async function load() {
    try { data = await api('/api/trend?win=' + win); draw(); } catch (e) { setText($('#tr-info'), 'Trend alınamadı: ' + e.message); }
  }
  function draw() {
    const ser = data;
    svg.textContent = '';
    const W = 1000, L = 44, R = 44, top = 10, tH = 230, dTop = 262, dH = 70, gTop = 350, gH = 60;
    const n = ser.t.length;
    if (!n) { setText($('#tr-info'), 'Veri yok'); return; }
    const t0 = ser.t[0], t1 = ser.t[n - 1], span = Math.max(1, t1 - t0);
    const X = t => L + (W - L - R) * (t - t0) / span;
    const temps = ser.T.concat(ser.SP).filter(v => v !== null);
    let lo = Math.floor(Math.min(...temps) - 1), hi = Math.ceil(Math.max(...temps) + 1);
    if (hi - lo < 4) { hi = lo + 4; }
    const Y = v => top + tH - tH * (v - lo) / (hi - lo);
    const Yrh = v => top + tH - tH * v / 100;
    const g = svgEl('g', {class: 'grid'});
    const steps = 4;
    for (let i = 0; i <= steps; i++) {
      const v = lo + (hi - lo) * i / steps, y = Y(v);
      g.append(svgEl('line', {x1: L, x2: W - R, y1: y, y2: y}));
      const tl = svgEl('text', {x: L - 6, y: y + 3, 'text-anchor': 'end'}); tl.textContent = v.toFixed(0) + '°'; svg.append(tl);
      const tr = svgEl('text', {x: W - R + 6, y: Yrh(100 * i / steps) + 3}); tr.textContent = (100 * i / steps).toFixed(0) + '%'; svg.append(tr);
    }
    [dTop, dTop + dH].forEach(y => g.append(svgEl('line', {x1: L, x2: W - R, y1: y, y2: y})));
    svg.prepend(g);
    const tickN = 5;
    for (let i = 0; i <= tickN; i++) {
      const t = t0 + span * i / tickN;
      const tx = svgEl('text', {x: X(t), y: 418, 'text-anchor': i === 0 ? 'start' : i === tickN ? 'end' : 'middle'});
      tx.textContent = ser.clock ? fmt.clock(t) : '−' + Math.round((t1 - t) / 60) + ' dk';
      svg.append(tx);
    }
    const path = (vals, Yf, step) => {
      let d = '', pen = false, prevY = null;
      for (let i = 0; i < n; i++) {
        const v = vals[i];
        if (v === null || v === undefined) { pen = false; continue; }
        const x = X(ser.t[i]), y = Yf(v);
        if (!pen) { d += 'M' + x.toFixed(1) + ' ' + y.toFixed(1); pen = true; }
        else if (step) d += 'H' + x.toFixed(1) + 'V' + y.toFixed(1);
        else d += 'L' + x.toFixed(1) + ' ' + y.toFixed(1);
        prevY = y;
      }
      return d;
    };
    svg.append(svgEl('path', {d: path(ser.RH, Yrh), class: 'ln-s-rh'}));
    svg.append(svgEl('path', {d: path(ser.SP, Y, true), class: 'ln-s-sp'}));
    svg.append(svgEl('path', {d: path(ser.T, Y), class: 'ln-s-t1'}));
    // uç nokta vurgusu
    const lastT = ser.T[n - 1];
    if (lastT !== null) {
      svg.append(svgEl('circle', {cx: X(t1), cy: Y(lastT), r: '4', class: 'pt-t1'}));
    }
    // talep alanı
    let da = 'M' + X(t0) + ' ' + (dTop + dH);
    for (let i = 0; i < n; i++) da += 'L' + X(ser.t[i]).toFixed(1) + ' ' + (dTop + dH - dH * (ser.D[i] || 0) / 100).toFixed(1);
    da += 'L' + X(t1) + ' ' + (dTop + dH) + 'Z';
    svg.append(svgEl('path', {d: da, class: 'ar-dem'}));
    const dl = svgEl('text', {x: L - 6, y: dTop + 9, 'text-anchor': 'end'}); dl.textContent = '100%'; svg.append(dl);
    const dl2 = svgEl('text', {x: L, y: dTop - 4}); dl2.textContent = 'ISI TALEBİ'; svg.append(dl2);
    // Gantt: 4 çıkış
    const rows = [['R1', 1], ['R2', 2], ['HF', 4], ['VF', 8]];
    rows.forEach(([lab, bit], r) => {
      const y = gTop + r * (gH / 4);
      const tl = svgEl('text', {x: L - 6, y: y + 11, 'text-anchor': 'end'}); tl.textContent = lab; svg.append(tl);
      svg.append(svgEl('rect', {x: L, y: y + 2, width: W - L - R, height: gH / 4 - 4, class: 'gt-bg'}));
      let start = null;
      for (let i = 0; i <= n; i++) {
        const on = i < n && (ser.B[i] & bit);
        if (on && start === null) start = i;
        if (!on && start !== null) {
          const x1 = X(ser.t[start]), x2 = i < n ? X(ser.t[i]) : X(t1);
          svg.append(svgEl('rect', {x: x1.toFixed(1), y: y + 2, width: Math.max(1, x2 - x1).toFixed(1), height: gH / 4 - 4, class: bit < 4 ? 'gt-heat' : 'gt-fan'}));
          start = null;
        }
      }
    });
    setText($('#tr-info'), n + ' örnek · çözünürlük ' + ser.res + ' s · ' + (ser.boot_note || ''));
    const tbl = $('#tr-tbl');
    tbl.textContent = '';
    for (let i = Math.max(0, n - 20); i < n; i++) {
      tbl.append(h('tr', null, ...[ser.clock ? fmt.clock(ser.t[i]) : '−' + Math.round((t1 - ser.t[i])) + ' s', fmt.t(ser.T[i]), fmt.t(ser.SP[i]), fmt.t(ser.RH[i]),
        (ser.D[i] || 0).toFixed(0), ser.B[i] & 1 ? 'ON' : '—', ser.B[i] & 2 ? 'ON' : '—', ser.B[i] & 4 ? 'ON' : '—', ser.B[i] & 8 ? 'ON' : '—'].map((x, j) =>
        h('td', {'data-label': ['Zaman', 'T1', 'Hedef', 'Nem', 'Talep', 'R1', 'R2', 'HF', 'VF'][j], text: x}))));
    }
  }
  let last = 0;
  pageUpdaters.trends = () => { if (Date.now() - last > 5000) { last = Date.now(); load(); } };
  onEnter.trends = () => { last = Date.now(); load(); };
};

// ================================================================= ALARMLAR
builders.alarms = sec => {
  const ackAll = h('button', {type: 'button', 'data-icon': 'check', 'data-text': '', class: 'primary'}, 'Tümünü onayla');
  const reset = h('button', {type: 'button', 'data-icon': 'unlock', 'data-text': '', class: 'danger'}, 'Kilidi sıfırla');
  const msg = h('div', {class: 'cmd-msg'});
  sec.append(sectionHead('Alarmlar'),
    h('section', {class: 'panel'}, h('h3', {text: 'Aktif alarmlar'}),
      h('div', {class: 'toolbar'}, ackAll, reset, h('span', {class: 'dim', id: 'al-sum'})), msg,
      h('div', {class: 'alarm-list', id: 'al-active'}),
      h('p', {class: 'field-hint', text: 'Onay koşulu ve kilidi değiştirmez; cihazda kalıcıdır ve MQTT’ye yayınlanır. Kilit yalnız koşul temizken sıfırlanır.'})),
    h('section', {class: 'panel'}, h('h3', {text: 'Geçmiş'}), h('div', {class: 'alarm-list', id: 'al-hist'}),
      h('p', {class: 'field-hint', text: 'RAM’de son 50 geçiş; kritik alarmlar kalıcı halkada (son 32).'})));
  ackAll.addEventListener('click', async () => {
    try { await api('/api/alarms/ack', {code: 'ALL'}); toast('Alarmlar onaylandı'); load(); } catch (e) { showMsg({msg}, 'critical', e.message); }
  });
  reset.addEventListener('click', async () => {
    const ok = await confirmDlg('Kilit sıfırlama', 'Koşulu temizlenmiş güvenlik kilitleri kaldırılacak; ısıtma yeniden başlayabilir. Devam edilsin mi?', 'Kilidi sıfırla', true);
    if (!ok) return;
    try { const r = await api('/api/alarms/reset', {code: 'ALL'}); toast(r.message || 'Kilit sıfırlandı'); load(); }
    catch (e) { showMsg({msg}, 'critical', e.message); }
  });
  function row(a, hist) {
    const cls = SEV_CLASS[a.sev] || 'info';
    const r = h('div', {class: 'alarm-row ' + (a.state === 'cleared_unacknowledged' || a.state === 'latched' ? cls : cls)},
      icon(a.state === 'cleared_unacknowledged' ? 'check' : 'warn'),
      h('span', {class: 'txt'}, h('b', {text: (SEV_TR[a.sev] || '') + ' · ' + (ALARM_TR[a.code] || a.code)}),
        ' · ' + (a.since ? fmt.clock(a.since) : '—') + ' · ' + (hist ? (a.text || '') : (ASTATE_TR[a.state] || a.state)) + (a.cond ? ' · ' + a.cond : '')),
      h('span', {class: 'acts'}));
    if (!hist && (a.state === 'active_unacknowledged' || a.state === 'cleared_unacknowledged')) {
      const b = h('button', {type: 'button', 'data-icon': 'check', 'data-text': ''}, 'Onayla');
      b.addEventListener('click', async () => { try { await api('/api/alarms/ack', {code: a.code}); load(); } catch (e) { toast(e.message, true); } });
      $('.acts', r).append(b);
    }
    return r;
  }
  async function load() {
    try {
      const d = await api('/api/alarms');
      const act = $('#al-active'), hs = $('#al-hist');
      act.textContent = '';
      hs.textContent = '';
      if (!d.active.length) act.append(h('div', {class: 'alarm-row info'}, icon('check'), h('span', {class: 'txt', text: 'Aktif alarm yok.'}), h('span')));
      d.active.forEach(a => act.append(row(a)));
      d.history.forEach(a => hs.append(row(a, true)));
      if (!d.history.length) hs.append(h('p', {class: 'dim', text: 'Kayıt yok.'}));
      reset.disabled = !d.active.some(a => a.latched);
      setText($('#al-sum'), d.active.length + ' aktif · ' + d.active.filter(a => a.state.indexOf('unack') >= 0).length + ' onaysız');
      iconize(sec);
    } catch (e) { showMsg({msg}, 'critical', 'Alarmlar alınamadı: ' + e.message); }
  }
  let last = 0, lastKey = '';
  pageUpdaters.alarms = d => {
    const k = d ? d.alarm + d.active_alarm_count + d.unacked_alarm_count : '';
    if (k !== lastKey || Date.now() - last > 5000) { lastKey = k; last = Date.now(); load(); }
  };
  onEnter.alarms = load;
};

// ================================================================= OLAYLAR
builders.events = sec => {
  const srcSel = h('select', {id: 'ev-src', 'aria-label': 'Kaynak'}, h('option', {value: '', text: 'Tüm kaynaklar'}),
    ...['STATE', 'SAFETY', 'CONTROLLER', 'OUTPUT', 'ALARM', 'COMMAND', 'CONFIG', 'NET', 'SYSTEM', 'SERVICE'].map(s => h('option', {value: s, text: s})));
  const sevSel = h('select', {id: 'ev-sev', 'aria-label': 'Önem'}, h('option', {value: '', text: 'Tüm önemler'}),
    ...[['WARNING', 'Uyarı ve üstü'], ['CRITICAL', 'Yalnız kritik']].map(([v, t]) => h('option', {value: v, text: t})));
  const list = h('div', {class: 'ev-list', id: 'ev-list', role: 'list'});
  sec.append(sectionHead('Olaylar'),
    h('section', {class: 'panel'},
      h('div', {class: 'toolbar'}, srcSel, sevSel, h('span', {class: 'dim', id: 'ev-info'})),
      h('p', {class: 'field-hint', text: 'Son 200 olay RAM’de; uyarı ve üstü olaylar kalıcı halkada (son 64). Pencere içi zaman-oransal anahtarlamalar olay değildir, sayaçlara yansır.'}),
      list));
  let all = [];
  const rank = {DEBUG: 0, INFO: 1, WARNING: 2, CRITICAL: 3};
  function render() {
    const s = srcSel.value, v = sevSel.value;
    list.textContent = '';
    all.filter(e => (!s || e.src === s) && (!v || rank[e.sev] >= rank[v])).slice(-120).reverse().forEach(e => {
      list.append(h('div', {class: 'ev', role: 'listitem'},
        h('span', {class: 'badge k-' + e.src, text: e.src}),
        h('span', {class: 'dim', text: '#' + e.seq + ' · ' + (e.ts ? fmt.clock(e.ts) : '+' + fmt.dur(e.up)) + (e.prev ? ' · önceki oturum' : '')}),
        h('span', {class: 'msg', text: (e.sev === 'CRITICAL' ? 'KRİTİK · ' : e.sev === 'WARNING' ? 'UYARI · ' : '') + e.msg})));
    });
  }
  srcSel.addEventListener('change', render);
  sevSel.addEventListener('change', render);
  async function load() {
    try { const d = await api('/api/events'); all = d.events; setText($('#ev-info'), d.events.length + ' olay · üzerine yazılan ' + d.overwritten); render(); }
    catch (e) { setText($('#ev-info'), 'Olaylar alınamadı: ' + e.message); }
  }
  let last = 0;
  pageUpdaters.events = () => { if (Date.now() - last > 3000) { last = Date.now(); load(); } };
  onEnter.events = load;
};

// ================================================================= OTURUM
builders.login = sec => {
  const u = h('input', {id: 'lg-user', autocomplete: 'username', required: '', maxlength: '32'});
  const p = h('input', {id: 'lg-pass', type: 'password', autocomplete: 'current-password', maxlength: '128'});
  const rem = h('input', {type: 'checkbox', id: 'lg-rem'});
  const msg = h('div', {class: 'cmd-msg'});
  const f = h('form', {class: 'form-grid', novalidate: ''},
    h('div', {class: 'field'}, h('label', {for: 'lg-user', text: 'Kullanıcı adı'}), u),
    h('div', {class: 'field'}, h('label', {for: 'lg-pass', text: 'Parola'}), p),
    h('div', {class: 'field toggle full'}, rem, h('label', {for: 'lg-rem', text: 'Beni hatırla (14 gün)'})),
    h('div', {class: 'full btn-row'}, h('button', {type: 'submit', class: 'primary', 'data-icon': 'key', 'data-text': ''}, 'Giriş yap'),
      h('button', {type: 'button', id: 'lg-out', 'data-icon': 'logout', 'data-text': ''}, 'Çıkış yap')), h('div', {class: 'full'}, msg));
  f.addEventListener('submit', async e => {
    e.preventDefault();
    if (!u.value) { u.reportValidity(); return; }
    try { const r = await api('/api/login', {user: u.value, password: p.value, remember: rem.checked}); p.value = ''; toast(r.message || 'Giriş yapıldı'); refreshSession(); }
    catch (err) { showMsg({msg}, 'critical', err.message); }
  });
  $('#lg-out', f).addEventListener('click', async () => { try { await api('/api/logout', {}); toast('Çıkış yapıldı'); refreshSession(); } catch (e) { toast(e.message, true); } });
  async function refreshSession() {
    try { const s = await api('/api/session'); setText($('#lg-state'), s.user ? 'Oturum: ' + s.user + ' · rol ' + s.role + ' · ' + s.expires : 'Oturum açık değil'); } catch (e) { /* */ }
  }
  sec.append(sectionHead('Oturum'), h('section', {class: 'panel'}, h('h3', {text: 'Giriş'}), h('p', {class: 'dim', id: 'lg-state'}, '—'), f,
    h('details', null, h('summary', {text: 'Parolamı unuttum'}), h('p', {class: 'field-hint', text: 'Kurtarma sorusu tanımlıysa cevapla kısa ömürlü bilet alınır ve yalnız web parolası değiştirilir. Tanımlı değilse cihazdaki servis düğmesi 10 s basılı tutularak web parolası silinir; ayarlar ve güvenlik limitleri korunur.'}))),
    h('div', {class: 'notice info'}, icon('info'), h('span', {text: 'Bağlantı şifrelenmiyor (yerel HTTP). Cihazı yalnız güvenilir yerel ağda kullanın.'})));
  onEnter.login = refreshSession;
};

// ================================================================= AYARLAR
// Bildirimsel alan tanımı: [ad, etiket, tür, seçenekler]. Sınırlar CONFIGURATION_MODEL ile aynıdır; sunucu yeniden doğrular.
const IPV4 = '((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)\\.){3}(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)';
const SEL = (...o) => o.map(x => Array.isArray(x) ? x : [x, x]);
const DEF = {
  net: [
    ['Cihaz kimliği', [
      ['adN', 'Cihaz adı (görünen)', 'text', {req: 1, ml: 32, hint: 'Yalnız web arayüzünde görünür; MQTT keşif adı sabittir.'}],
      ['mdns', 'mDNS adı', 'text', {req: 1, ml: 63, pat: '[A-Za-z0-9]([A-Za-z0-9\\-]*[A-Za-z0-9])?', hint: 'Harf, rakam ve tire; “.local” eki eklenir.'}]]],
    ['IP yapılandırması', [
      ['staticEnabled', 'Statik IP kullan', 'checkbox', {hint: 'Kapalıyken adres DHCP ile alınır. Statik bağlantı kurulamazsa cihaz DHCP’ye döner.'}],
      ['staticIP', 'IP adresi', 'ip', {req: 1, dep: 'staticEnabled'}], ['gateway', 'Ağ geçidi', 'ip', {req: 1, dep: 'staticEnabled'}],
      ['subnet', 'Alt ağ maskesi', 'ip', {req: 1, dep: 'staticEnabled'}],
      ['dns1', 'Birincil DNS', 'ip', {dep: 'staticEnabled', hint: 'Boşsa ağ geçidi kullanılır.'}], ['dns2', 'İkincil DNS', 'ip', {dep: 'staticEnabled'}]]]],
  mqtt: [
    ['Broker', [
      ['mqtt_host', 'Broker adresi', 'text', {ml: 63, hint: 'Boş = MQTT kapalı. Yerel kontrol broker olmadan çalışır.'}],
      ['mqtt_port', 'Broker portu', 'number', {min: 1, max: 65535, req: 1}],
      ['mqtt_user', 'Kullanıcı adı', 'text', {ml: 64}],
      ['mqtt_password', 'Yeni MQTT parolası', 'password', {ml: 128, off: 'clearMqttPassword', hint: 'Boş bırakılırsa kayıtlı parola korunur.'}],
      ['clearMqttPassword', 'Kayıtlı MQTT parolasını sil', 'checkbox', {ui: 1}]]],
    ['Topic ve yayın', [
      ['mqtt_base', 'Kök topic', 'text', {req: 1, ml: 96, pat: '[^+#\\s]+', hint: 'Tam taban: <kök>/<SLUG>. SLUG değişimi taşınma sihirbazıyla yapılır.'}],
      ['slug', 'SLUG (cihaz kimliği)', 'text', {ro: 1}],
      ['state_active_s', 'Isıtırken yayın (s)', 'number', {min: 1, max: 30}], ['state_idle_s', 'Boşta yayın (s)', 'number', {min: 10, max: 59, hint: 'Suite nokta geçerlilik süresinden kısa olmalı (< 60 s).'}],
      ['diag_interval_s', 'Tanı yayını (s)', 'number', {min: 30, max: 300}],
      ['discovery_enabled', 'Otomatik keşif (Home Assistant / Studio)', 'checkbox', {hint: 'Kapatılınca yayımlanmış keşif kayıtları silinir.'}],
      ['history_discovery_enabled', 'Günlük ısıtma dakikası keşfi', 'checkbox', {hint: 'Suite TÜKETİM “min” birimini desteklemediği için varsayılan kapalı.'}]]],
    ['Uzak yetkiler', [
      ['remote_config_enabled', 'MQTT’den konfigürasyon yazımına izin ver', 'checkbox', {hint: 'Güvenlik limitleri hiçbir koşulda uzaktan yazılamaz. Anonim broker’da açılamaz.'}],
      ['pid_remote_tuning', 'MQTT’den PID ayarına izin ver', 'checkbox', {dep: 'remote_config_enabled'}],
      ['remote_manual_allowed', 'MQTT’den MANUEL moda izin ver', 'checkbox'],
      ['service_channel_enabled', 'MQTT servis kanalı (alarm reset, tanı, reboot)', 'checkbox', {hint: 'Servis jetonu gerektirir; çıkış testi ve fabrika ayarı MQTT’den yapılamaz.'}]]]],
  io: [
    ['Sensör sürücüleri', [
      ['t1_driver', 'T1 sıcaklık sürücüsü', 'select', {opts: SEL('AUTO', 'SHT4X', 'SHT3X', 'BME280', 'BME680', 'AHT20', 'DS18B20'), hint: 'Değişiklik yeniden başlatma gerektirir.'}],
      ['rh1_driver', 'RH1 nem sürücüsü', 'select', {opts: SEL('AUTO', 'SHT4X', 'SHT3X', 'BME280', 'BME680', 'AHT20')}],
      ['t2_enabled', 'T2 hava çıkış sensörü takılı', 'checkbox', {hint: 'T2 yoksa aşırı çıkış sıcaklığı (S2) ve sıcaklıkla post-cool devre dışıdır.'}],
      ['sensor_model', 'Algılanan sensör', 'text', {ro: 1}]]],
    ['Kalibrasyon ve örnekleme', [
      ['t1_offset', 'T1 ofseti (°C)', 'number', {min: -5, max: 5, step: 0.1}], ['rh1_offset', 'RH1 ofseti (%)', 'number', {min: -10, max: 10, step: 0.1}],
      ['t2_offset', 'T2 ofseti (°C)', 'number', {min: -10, max: 10, step: 0.1, dep: 't2_enabled'}],
      ['sensor_interval_s', 'Örnekleme aralığı (s)', 'number', {min: 1, max: 10}],
      ['sensor_filter_tau_s', 'Filtre zaman sabiti (s)', 'number', {min: 0, max: 120}],
      ['sensor_stuck_s', 'Takılı değer süresi (s)', 'number', {min: 300, max: 7200}]]]],
  ctrl: [
    ['Isıtma çıkışı', [
      ['output_driver_r', 'Rezistans sürücüsü', 'select', {opts: SEL(['SSR_ZC', 'SSR (sıfır geçişli)'], ['SSR_RANDOM', 'SSR (rastgele)'], ['RELAY', 'Röle / kontaktör']), hint: 'Röle: pencere ≥ 300 s, min açık ≥ 60 s zorunlu. Değişiklik yeniden başlatma gerektirir.'}],
      ['tp_window_s', 'Zaman-oransal pencere (s)', 'number', {min: 5, max: 1800}],
      ['heater_min_on_s', 'Asgari açık süre (s)', 'number', {min: 1, max: 900}], ['heater_min_off_s', 'Asgari kapalı süre (s)', 'number', {min: 1, max: 900}],
      ['stage2_on', 'Kademe 2 açılış (%)', 'number', {min: 40, max: 90}], ['stage2_off', 'Kademe 2 kapanış (%)', 'number', {min: 10, max: 85, hint: 'Açılışın en az 5 % altında olmalı.'}],
      ['stage_min_dwell_s', 'Kademe asgari kalış (s)', 'number', {min: 0, max: 1800}],
      ['heater_power_w_r1', 'R1 gücü (W)', 'number', {min: 0, max: 5000, hint: '0: eşit güç'}], ['heater_power_w_r2', 'R2 gücü (W)', 'number', {min: 0, max: 5000}],
      ['lead_rotation', 'Lider rotasyonu', 'select', {opts: SEL(['DAILY', 'Günlük'], ['OFF', 'Kapalı'])}]]],
    ['Fan ve soğutma', [
      ['fan_prestart_s', 'Fan ön çalışma (s)', 'number', {min: 0, max: 30}],
      ['post_cool_mode', 'Soğutma yöntemi', 'select', {opts: SEL(['TIME', 'Süre'], ['TEMPERATURE', 'T2 sıcaklığı'], ['HYBRID', 'Süre + T2']), hint: 'T2 yöntemleri T2 sensörü gerektirir.'}],
      ['post_cool_seconds', 'Soğutma süresi (s)', 'number', {min: 30, max: 600}],
      ['post_cool_min_s', 'Asgari soğutma (s)', 'number', {min: 10, max: 120}], ['post_cool_max_s', 'Azami soğutma (s)', 'number', {min: 60, max: 1800}],
      ['post_cool_safe_temp', 'Güvenli T2 (°C)', 'number', {min: 25, max: 70}]]],
    ['Talep koşullandırma', [
      ['control_interval_s', 'Kontrol periyodu (s)', 'number', {min: 1, max: 30}],
      ['min_heat_demand', 'Asgari talep (%)', 'number', {min: 0, max: 20}], ['max_heat_demand', 'Azami talep (%)', 'number', {min: 20, max: 100}],
      ['demand_slew_pct_per_min', 'Talep artış hızı (%/dk)', 'number', {min: 1, max: 100}],
      ['pid_setpoint_weight', 'Setpoint ağırlığı b', 'number', {min: 0, max: 1, step: 0.1}], ['onoff_hysteresis', 'ONOFF histerezisi (°C)', 'number', {min: 0.2, max: 2, step: 0.1}]]],
    ['Havalandırma', [
      ['ventilation_start_temperature', 'Başlama sıcaklığı (°C)', 'number', {min: 15, max: 40, step: 0.5}],
      ['ventilation_stop_temperature', 'Durma sıcaklığı (°C)', 'number', {min: 14, max: 39, step: 0.5, hint: 'Başlamanın en az 1 °C altında olmalı.'}],
      ['vent_sp_margin', 'Hedef payı (°C)', 'number', {min: 1, max: 10, step: 0.5}],
      ['humidity_vent_enabled', 'Neme göre havalandır', 'checkbox'],
      ['humidity_high_limit', 'Nem üst sınırı (%)', 'number', {min: 40, max: 95, dep: 'humidity_vent_enabled'}],
      ['humidity_hysteresis', 'Nem histerezisi (%)', 'number', {min: 2, max: 20, dep: 'humidity_vent_enabled'}],
      ['humidity_low_limit', 'Nem alt sınırı (%)', 'number', {min: 10, max: 60}],
      ['humidity_vent_while_heating', 'Isıtırken nem havalandırması', 'select', {opts: SEL(['INHIBIT', 'Engelle'], ['ALLOW', 'İzin ver'], ['ALLOW_ABOVE_SP', 'Hedefe yakınsa'])}],
      ['manual_vent_priority', 'Manuel havalandırma önceliği', 'select', {opts: SEL(['VENT_WINS', 'Havalandırma kazanır'], ['HEAT_WINS', 'Isıtma kazanır'])}],
      ['vent_heat_cap', 'Havalandırırken azami talep (%)', 'number', {min: 0, max: 50}],
      ['manual_vent_timeout_min', 'Manuel istek süresi (dk)', 'number', {min: 0, max: 720, hint: '0: sınırsız'}],
      ['ventilation_periodic_min', 'Periyodik havalandırma (dk/sa)', 'number', {min: 0, max: 60, hint: '0: kapalı'}],
      ['vent_min_on_s', 'Fan asgari açık (s)', 'number', {min: 10, max: 600}], ['vent_min_off_s', 'Fan asgari kapalı (s)', 'number', {min: 10, max: 600}],
      ['heat_vent_changeover_s', 'Isıtma → havalandırma geçişi (s)', 'number', {min: 0, max: 600}],
      ['vent_heat_changeover_s', 'Havalandırma → ısıtma geçişi (s)', 'number', {min: 0, max: 600}],
      ['ota_vent_state', 'Güncelleme sırasında havalandırma', 'select', {opts: SEL(['LAST', 'Son durumda kalsın'], ['OFF', 'Kapalı'])}]]],
    ['Süreli istekler', [
      ['boost_minutes', 'Boost süresi (dk)', 'number', {min: 10, max: 240}],
      ['sched_timeout_h', 'Program isteği zaman aşımı (sa)', 'number', {min: 0, max: 48, hint: '0: kapalı'}],
      ['manual_timeout_h', 'MANUEL mod zaman aşımı (sa)', 'number', {min: 0, max: 48, hint: '0: kapalı'}]]]],
  safety: [
    ['Güvenlik limitleri', [
      ['cabin_overtemp_limit', 'Kulübe aşırı sıcaklık (°C)', 'number', {min: 30, max: 60, hint: 'En yüksek hedefin ≥ 10 °C, havalandırma başlamasının ≥ 5 °C üstünde olmalı.'}],
      ['overtemp_reset_hysteresis', 'Aşırı sıcaklık reset histerezisi (°C)', 'number', {min: 1, max: 10}],
      ['heater_outlet_limit', 'Hava çıkışı limiti (°C)', 'number', {min: 50, max: 150, dep: 't2_enabled_ro'}],
      ['max_continuous_heating_min', 'Doyumda kesintisiz ısıtma (dk)', 'number', {min: 30, max: 1440, hint: 'Talep üst sınırda bu süre kalırsa kilit (sensör yerinden çıkması vb.).'}],
      ['sensor_stale_s', 'Sensör bayatlık süresi (s)', 'number', {min: 5, max: 60}],
      ['unexpected_rise_c_per_10min', 'Rezistans kapalıyken artış uyarısı (°C/10 dk)', 'number', {min: 0.5, max: 10, step: 0.1}],
      ['max_rise_c_per_10min', 'Isıtırken azami artış (°C/10 dk)', 'number', {min: 1, max: 20, step: 0.5}]]],
    ['Donma koruması', [
      ['antifreeze_enabled', 'Donma koruması etkin', 'checkbox', {hint: 'OFF ve HAVALANDIRMA modlarında da çalışır. Sensör arızasında ısıtma yapılamaz.'}],
      ['frost_guard_temperature', 'Devreye girme (°C)', 'number', {min: 2, max: 10, step: 0.5, dep: 'antifreeze_enabled'}],
      ['frost_exit_hysteresis', 'Çıkış histerezisi (°C)', 'number', {min: 0.5, max: 3, step: 0.5, dep: 'antifreeze_enabled'}],
      ['setpoint_frost', 'Donma hedefi (°C)', 'number', {min: 4, max: 12, step: 0.5}]]],
    ['Servis ve yeniden başlatma', [
      ['service_timeout_min', 'Servis modu zaman aşımı (dk)', 'number', {min: 5, max: 120}],
      ['service_test_max_s', 'Çıkış testi azami süre (s)', 'number', {min: 10, max: 300}],
      ['restart_storm_limit', 'Aşırı yeniden başlatma sınırı', 'number', {min: 3, max: 10}],
      ['restart_storm_window_min', 'Aşırı yeniden başlatma penceresi (dk)', 'number', {min: 10, max: 120}]]]],
  access: [
    ['Erişim', [
      ['user', 'Web kullanıcı adı', 'text', {req: 1, ml: 32}],
      ['guestRead', 'Misafirler durum okuyabilir', 'checkbox'],
      ['session_hours', 'Oturum süresi (sa)', 'number', {min: 1, max: 24}]]],
    ['OTA parolası', [
      ['otaPw', 'Yeni OTA parolası', 'password', {minl: 8, ml: 64, off: 'clearOtaPassword', hint: 'Bu cihazda parolasız OTA kapalıdır; parola tanımlanmadan güncelleme yapılamaz. Yalnız özet saklanır.'}],
      ['clearOtaPassword', 'OTA parolasını kaldır (OTA kapanır)', 'checkbox', {ui: 1}]]]],
  maint: []
};
const SECTIONS = [['net', 'Ağ', 'wifi'], ['mqtt', 'MQTT', 'antenna'], ['io', 'Sensörler', 'sensor'], ['ctrl', 'Kontrol', 'sliders'],
  ['safety', 'Güvenlik', 'shield'], ['access', 'Erişim', 'key'], ['maint', 'Bakım', 'warn']];

builders.settings = sec => {
  const form = h('form', {id: 'settings-form', novalidate: ''});
  const tablist = h('div', {class: 'tabs-v', role: 'tablist', 'aria-orientation': 'vertical', 'aria-label': 'Ayar bölümleri'});
  const panelsBox = h('div');
  let baseline = {}, values = {}, saving = false;
  const fields = {};        // ad → {el, input, def, sec}
  SECTIONS.forEach(([id, label, ico]) => {
    const tab = h('button', {type: 'button', role: 'tab', id: 'tab-' + id, 'aria-controls': 'panel-' + id, 'aria-selected': 'false', tabindex: '-1'},
      icon(ico), h('span', {class: 'tab-label', text: label}), h('span', {class: 'tab-count', hidden: true, 'aria-label': '0 değişiklik'}));
    tablist.append(tab);
    const panel = h('div', {role: 'tabpanel', id: 'panel-' + id, 'aria-labelledby': 'tab-' + id, class: 'tabpanel', hidden: true});
    panelsBox.append(panel);
  });
  const savebar = h('div', {class: 'savebar', id: 'savebar'}, h('p', {class: 'dirty-text', id: 'dirty-text', text: 'Kaydedilmemiş değişiklik yok'}),
    h('div', {class: 'savebar-actions'},
      h('button', {type: 'button', id: 'revert', 'data-icon': 'undo', disabled: ''}, 'Geri al'),
      h('button', {type: 'submit', form: 'settings-form', id: 'save', class: 'primary', 'data-icon': 'save', 'data-text': '', disabled: ''}, 'Ayarları kaydet')));
  sec.append(sectionHead('Cihaz ayarları'), form, h('div', {class: 'settings'}, tablist, h('div', null, panelsBox, savebar)));

  function field([name, label, type, o = {}], secId) {
    let input;
    const id = 'f-' + name;
    if (type === 'checkbox') input = h('input', {type: 'checkbox', id, name, form: 'settings-form'});
    else if (type === 'select') input = h('select', {id, name, form: 'settings-form'}, ...o.opts.map(([v, t]) => h('option', {value: v, text: t})));
    else input = h('input', {id, name, form: 'settings-form', type: type === 'ip' ? 'text' : type,
      inputmode: type === 'ip' || type === 'number' ? 'decimal' : null, pattern: type === 'ip' ? IPV4 : (o.pat || null),
      min: o.min, max: o.max, step: type === 'number' ? (o.step || 1) : null, maxlength: o.ml, minlength: o.minl,
      required: o.req ? '' : null, readonly: o.ro ? '' : null, autocomplete: type === 'password' ? 'new-password' : 'off'});
    const hintId = o.hint ? id + '-hint' : null;
    if (hintId) input.setAttribute('aria-describedby', hintId);
    const hint = o.hint ? h('small', {class: 'field-hint', id: hintId, text: o.hint}) : null;
    const wrap = type === 'checkbox'
      ? h('div', {class: 'field full', 'data-name': name}, h('div', {class: 'field toggle'}, input, h('label', {for: id, text: label})), hint)
      : h('div', {class: 'field', 'data-name': name}, h('label', {for: id, text: label}), input, hint);
    fields[name] = {wrap, input, type, o, sec: secId, label};
    return wrap;
  }
  function valueOf(f) {
    if (f.type === 'checkbox') return f.input.checked;
    if (f.type === 'number') return f.input.value === '' ? '' : Number(f.input.value);
    return f.input.value;
  }
  function setValue(f, v) {
    if (f.type === 'checkbox') f.input.checked = !!v;
    else if (f.type === 'password') f.input.value = '';
    else f.input.value = v === undefined || v === null ? '' : v;
  }
  function applyDeps() {
    Object.values(fields).forEach(f => {
      if (f.o.dep) {
        const src = fields[f.o.dep];
        const on = src ? src.input.checked : (f.o.dep === 't2_enabled_ro' ? !!values.t2_enabled : true);
        f.wrap.hidden = !on;
        f.input.disabled = !on;
      }
      if (f.o.off) {
        const c = fields[f.o.off];
        if (c && c.input.checked) { f.input.value = ''; f.input.disabled = true; } else f.input.disabled = saving;
      }
    });
  }
  function isChanged(f, name) {
    if (f.o.ui) return f.input.checked;
    if (f.type === 'password') return f.input.value !== '';
    return String(valueOf(f)) !== String(baseline[name]);
  }
  function refreshDirty() {
    const counts = {};
    let n = 0;
    Object.entries(fields).forEach(([name, f]) => {
      if (f.o.ro) return;
      const ch = isChanged(f, name);
      f.wrap.classList.toggle('changed', ch);
      if (ch) { n++; counts[f.sec] = (counts[f.sec] || 0) + 1; }
    });
    SECTIONS.forEach(([id]) => {
      const b = $('#tab-' + id + ' .tab-count');
      const c = counts[id] || 0;
      b.hidden = !c;
      b.textContent = c;
      b.setAttribute('aria-label', c + ' değişiklik');
    });
    setText($('#dirty-text'), n ? n + ' alanda kaydedilmemiş değişiklik' : 'Kaydedilmemiş değişiklik yok');
    savebar.classList.toggle('is-dirty', n > 0);
    $('#save').disabled = !n || saving;
    $('#revert').disabled = !n || saving;
    return n;
  }
  function selectTab(id, focus) {
    SECTIONS.forEach(([k]) => {
      const on = k === id;
      const t = $('#tab-' + k);
      t.setAttribute('aria-selected', on ? 'true' : 'false');
      t.tabIndex = on ? 0 : -1;
      $('#panel-' + k).hidden = !on;
    });
    if (focus) $('#tab-' + id).focus();
    history.replaceState(null, '', (useHash ? '#settings' : location.pathname) + (useHash ? '' : '#' + id));
    lsSet('scada-settings-tab', id);
  }
  tablist.addEventListener('click', e => { const b = e.target.closest('[role=tab]'); if (b) selectTab(b.id.slice(4)); });
  tablist.addEventListener('keydown', e => {
    const ids = SECTIONS.map(s => s[0]);
    const i = ids.indexOf(document.activeElement.id.slice(4));
    let j = -1;
    if (e.key === 'ArrowDown' || e.key === 'ArrowRight') j = (i + 1) % ids.length;
    if (e.key === 'ArrowUp' || e.key === 'ArrowLeft') j = (i - 1 + ids.length) % ids.length;
    if (e.key === 'Home') j = 0;
    if (e.key === 'End') j = ids.length - 1;
    if (j >= 0) { e.preventDefault(); selectTab(ids[j], true); }
  });
  sec.addEventListener('input', e => { if (e.target.form === form || e.target.getAttribute('form') === 'settings-form') { applyDeps(); refreshDirty(); } });
  sec.addEventListener('change', e => { if (e.target.getAttribute('form') === 'settings-form') { applyDeps(); refreshDirty(); } });
  window.addEventListener('beforeunload', e => { if (built.settings && refreshDirty()) { e.preventDefault(); e.returnValue = ''; } });

  function render(d) {
    values = d;
    SECTIONS.forEach(([id]) => {
      const p = $('#panel-' + id);
      p.textContent = '';
      (DEF[id] || []).forEach(([title, list]) => {
        const grid = h('div', {class: 'form-grid'}, ...list.map(fd => field(fd, id)));
        p.append(h('section', {class: 'panel'}, h('h3', {text: title}), grid));
      });
    });
    // Güvenlik bölümü notu
    $('#panel-safety').prepend(h('div', {class: 'notice warn'}, icon('warn'),
      h('span', {text: 'Güvenlik limitleri yalnız bu yerel arayüzden ve yönetici rolüyle değiştirilir; MQTT’den yazılamaz. Yazılım korumaları termik kesici, sigorta ve RCD’nin yerine geçmez.'})));
    renderAccessExtras($('#panel-access'), d);
    renderMaint($('#panel-maint'), d);
    Object.entries(fields).forEach(([name, f]) => { if (!f.o.ui) setValue(f, d[name]); else f.input.checked = false; });
    baseline = {};
    Object.entries(fields).forEach(([name, f]) => { baseline[name] = f.type === 'password' ? '' : valueOf(f); });
    applyDeps();
    refreshDirty();
    iconize(sec);
  }
  $('#revert').addEventListener('click', () => {
    Object.entries(fields).forEach(([name, f]) => { if (f.o.ui) f.input.checked = false; else if (f.type === 'password') f.input.value = ''; else setValue(f, baseline[name]); });
    applyDeps(); refreshDirty(); toast('Değişiklikler geri alındı');
  });
  form.addEventListener('submit', async e => {
    e.preventDefault();
    // gizli sekmedeki ilk geçersiz alan
    const bad = Object.values(fields).find(f => !f.input.disabled && !f.o.ro && !f.input.checkValidity());
    if (bad) {
      selectTab(bad.sec);
      bad.input.reportValidity();
      bad.input.focus();
      toast('Kaydedilmedi: “' + bad.label + '” alanını düzeltin', true);
      return;
    }
    const body = {};
    Object.entries(fields).forEach(([name, f]) => {
      if (f.o.ro || f.o.ui || f.input.disabled && !(f.o.off && fields[f.o.off].input.checked)) return;
      if (f.type === 'password') {
        if (f.o.off && fields[f.o.off].input.checked) body[name] = '';
        else if (f.input.value) body[name] = f.input.value;
        return;
      }
      body[name] = valueOf(f);
    });
    saving = true;
    const save = $('#save');
    save.setAttribute('aria-busy', 'true');
    save.lastChild.textContent = 'Kaydediliyor…';
    refreshDirty();
    try {
      const r = await api('/api/settings', body);
      Object.entries(body).forEach(([k, v]) => { if (fields[k] && fields[k].type !== 'password') baseline[k] = v; values[k] = v; });
      Object.values(fields).forEach(f => { if (f.type === 'password') f.input.value = ''; if (f.o.ui) f.input.checked = false; });
      savebar.classList.remove('is-error');
      toast(r && r.message || 'Kaydedildi');
    } catch (err) {
      savebar.classList.add('is-error');
      setText($('#dirty-text'), 'Kaydedilemedi · değişiklikler formda duruyor');
      toast(err.message, true);
      const fld = err.body && err.body.field && fields[err.body.field];
      if (fld) { selectTab(fld.sec); fld.input.focus(); }
    }
    saving = false;
    save.setAttribute('aria-busy', 'false');
    save.lastChild.textContent = 'Ayarları kaydet';
    applyDeps();
    if (!savebar.classList.contains('is-error')) refreshDirty();
  });

  // ---- Erişim: ayrı formlar
  function renderAccessExtras(p, d) {
    const ota = h('p', {class: 'field-hint'}, 'OTA parolası durumu: ', h('b', {id: 'ota-state', text: d.otaPasswordSet ? 'Tanımlı · OTA açık' : 'Tanımlı değil · OTA kapalı'}));
    p.children[1] && p.children[1].append(ota);
    const pw = [['pw-old', 'Mevcut parola', 'current-password'], ['pw-new', 'Yeni parola', 'new-password'], ['pw-new2', 'Yeni parola tekrar', 'new-password']]
      .map(([id, l, ac]) => h('div', {class: 'field'}, h('label', {for: id, text: l}), h('input', {type: 'password', id, maxlength: '128', autocomplete: ac})));
    const pwMsg = h('div', {class: 'cmd-msg'});
    const pwForm = h('form', {class: 'form-grid', novalidate: ''}, ...pw,
      h('div', {class: 'full btn-row'}, h('button', {type: 'submit', class: 'primary', 'data-icon': 'key', 'data-text': ''}, 'Parolayı kaydet')), h('div', {class: 'full'}, pwMsg));
    pwForm.addEventListener('submit', async e => {
      e.preventDefault();
      const a = $('#pw-new').value, b = $('#pw-new2').value;
      if (a !== b) { showMsg({msg: pwMsg}, 'critical', 'Yeni parolalar eşleşmiyor.'); return; }
      if (a && a.length < 8) { showMsg({msg: pwMsg}, 'critical', 'Parola en az 8 karakter olmalı.'); return; }
      if (!a && !(await confirmDlg('Web parolası', 'Web parola koruması kaldırılsın mı?', 'Korumayı kaldır', true))) return;
      try { const r = await api('/api/password', {oldPassword: $('#pw-old').value, password: a}); $$('input', pwForm).forEach(i => { i.value = ''; }); showMsg({msg: pwMsg}, null, ''); toast(r.message || 'Parola kaydedildi; bütün oturumlar kapatıldı'); }
      catch (err) { showMsg({msg: pwMsg}, 'critical', err.message); }
    });
    const pin = h('input', {type: 'password', id: 'svc-pin', inputmode: 'numeric', pattern: '\\d{4,8}', maxlength: '8', autocomplete: 'new-password'});
    const pinMsg = h('div', {class: 'cmd-msg'});
    const pinForm = h('form', {class: 'form-grid', novalidate: ''},
      h('div', {class: 'field'}, h('label', {for: 'svc-pin', text: 'Yeni servis PIN’i (4–8 rakam)'}), pin,
        h('small', {class: 'field-hint', text: d.servicePinSet ? 'Durum: tanımlı. Servis modu açılabilir.' : 'Durum: tanımlı değil. Servis modu kapalı.'})),
      h('div', {class: 'full btn-row'}, h('button', {type: 'submit', class: 'primary', 'data-icon': 'save', 'data-text': ''}, 'PIN’i kaydet')), h('div', {class: 'full'}, pinMsg));
    pinForm.addEventListener('submit', async e => {
      e.preventDefault();
      if (!pin.checkValidity() || !pin.value) { pin.reportValidity(); return; }
      try { await api('/api/service/pin', {pin: pin.value}); pin.value = ''; toast('Servis PIN’i kaydedildi'); } catch (err) { showMsg({msg: pinMsg}, 'critical', err.message); }
    });
    p.append(h('section', {class: 'panel'}, h('h3', {text: 'Web parolası'}),
      h('p', {class: 'field-hint', text: 'Web parolası OTA parolasından ve servis PIN’inden bağımsızdır. Bağlantı şifrelenmez (yerel HTTP).'}), pwForm),
      h('section', {class: 'panel'}, h('h3', {text: 'Servis PIN’i'}), pinForm));
  }

  // ---- Bakım: kırmızı alan
  function renderMaint(p, d) {
    const act = (label, ico, danger, title, text, path, body, okText) => {
      const b = h('button', {type: 'button', class: danger ? 'danger' : '', 'data-icon': ico, 'data-text': ''}, label);
      b.addEventListener('click', async () => {
        if (!(await confirmDlg(title, text, okText || label, danger))) return;
        try { const r = await api(path, body || {}); toast(r && r.message || 'İstek alındı'); } catch (err) { toast(err.message, true); }
      });
      return b;
    };
    const svcPin = h('input', {type: 'password', id: 'svc-enter-pin', inputmode: 'numeric', maxlength: '8', autocomplete: 'off'});
    const svcBtn = h('button', {type: 'button', 'data-icon': 'wrench', 'data-text': ''}, 'Servis moduna gir');
    svcBtn.addEventListener('click', async () => {
      const on = D && D.controller_state === 'SERVICE';
      if (!on && !(await confirmDlg('Servis modu', 'Isıtma talebi sıfırlanır ve soğutma tamamlandıktan sonra çıkış testi izni verilir. Donma koruması servis süresince devre dışıdır. Devam edilsin mi?', 'Servis moduna gir', true))) return;
      try { await api(on ? '/api/service/exit' : '/api/service/enter', {pin: svcPin.value}); svcPin.value = ''; toast(on ? 'Servis modundan çıkıldı' : 'Servis modu etkin'); } catch (err) { toast(err.message, true); }
    });
    const cnt = h('select', {id: 'cnt-sel'}, ...[['all', 'Tüm sayaçlar'], ['r1', 'R1'], ['r2', 'R2'], ['heater_fan', 'Isıtıcı fanı'], ['ventilation_fan', 'Havalandırma fanı']].map(([v, t]) => h('option', {value: v, text: t})));
    const cntBtn = h('button', {type: 'button', class: 'danger', 'data-icon': 'timer', 'data-text': ''}, 'Sayaçları sıfırla');
    cntBtn.addEventListener('click', async () => {
      if (!(await confirmDlg('Sayaç sıfırlama', (cnt.value === 'all' ? 'Bütün çıkışların' : cnt.options[cnt.selectedIndex].text + ' çıkışının') + ' çalışma saati ve anahtarlama sayacı sıfırlanacak. Önceki değer olay günlüğüne yazılır. Çıkışlar etkilenmez.', 'Sıfırla', true))) return;
      try { await api('/api/service/reset-counters', {out: cnt.value}); toast('Sayaçlar sıfırlandı'); } catch (err) { toast(err.message, true); }
    });
    const curSsid = h('dd', {id: 'cur-ssid', class: 'mono'}, d.ssid || 'Tanımlı değil (AP kurulum modu)');
    const wBtn = h('button', {type: 'button', 'data-icon': 'wifi', 'data-text': ''}, 'Ağ tara ve değiştir');
    wBtn.addEventListener('click', openWifiDialog);
    const wResetOut = h('div', {class: 'cmd-msg', id: 'wreset-out'});
    const wReset = h('button', {type: 'button', class: 'danger', 'data-icon': 'wifioff', 'data-text': ''}, 'Wi-Fi bilgilerini sil');
    wReset.addEventListener('click', () => resetWifiFlow(wReset, wResetOut));
    const otaFile = h('input', {type: 'file', id: 'ota-file', accept: '.bin'});
    const otaPw = h('input', {type: 'password', id: 'ota-pw', maxlength: '64', autocomplete: 'off'});
    const otaBtn = h('button', {type: 'button', class: 'danger', 'data-icon': 'upload', 'data-text': ''}, 'Firmware yükle');
    otaBtn.addEventListener('click', async () => {
      if (!otaFile.files.length) { toast('Önce .bin dosyası seçin', true); return; }
      if (!(await confirmDlg('Firmware güncelleme', 'Rezistanslar kapatılır, soğutma tamamlanır, sonra imaj yazılır. Yeni imaj öz testi geçemezse önceki sürüme dönülür. Devam edilsin mi?', 'Güncellemeyi başlat', true))) return;
      try { const r = await api('/api/ota/begin', {password: otaPw.value, size: otaFile.files[0].size}); toast(r.message || 'Güncelleme hazırlanıyor'); } catch (err) { toast(err.message, true); }
    });
    p.append(h('section', {class: 'panel red-zone', 'aria-labelledby': 'rz-h'},
      h('h3', {id: 'rz-h', text: '⚠ Kırmızı alan'}),
      h('h4', {class: 'group-heading', text: 'Servis modu'}),
      h('div', {class: 'form-grid'}, h('div', {class: 'field'}, h('label', {for: 'svc-enter-pin', text: 'Servis PIN’i'}), svcPin,
        h('small', {class: 'field-hint', text: 'Çıkış testleri Çıkışlar sayfasında açılır: tek seferde tek rezistans, en çok 120 s, interlock’lar etkin.'})),
        h('div', {class: 'field'}, h('span', {class: 'lbl', text: 'İşlem'}), svcBtn)),
      h('h4', {class: 'group-heading', text: 'Kablosuz bağlantıyı değiştir'}),
      h('dl', {class: 'kv'}, h('dt', {text: 'Kayıtlı ağ'}), curSsid, h('dt', {text: 'Parola'}), h('dd', {text: d.passSet ? 'Kayıtlı' : 'Yok (açık ağ)'})),
      h('div', {class: 'btn-row'}, wBtn),
      h('p', {class: 'field-hint', text: 'Yeni ağ seçildiğinde cihaz yeniden başlamadan geçiş yapar ve bu sayfayla bağlantı kesilir. Bağlanamazsa 20–40 sn sonra kurulum ağı açılır ve kayıtlı ağ 5 dakikada bir yeniden denenir.'}),
      h('h4', {class: 'group-heading', text: 'Firmware'}),
      h('div', {class: 'form-grid'}, h('div', {class: 'field'}, h('label', {for: 'ota-file', text: 'İmaj dosyası'}), otaFile),
        h('div', {class: 'field'}, h('label', {for: 'ota-pw', text: 'OTA parolası'}), otaPw),
        h('div', {class: 'full btn-row'}, otaBtn)),
      h('h4', {class: 'group-heading', text: 'Sayaçlar ve cihaz'}),
      h('div', {class: 'btn-row'}, cnt, cntBtn),
      h('div', {class: 'btn-row'},
        act('Yeniden başlat', 'reboot', false, 'Yeniden başlatma', 'Rezistanslar kapatılıp soğutma tamamlandıktan sonra cihaz yeniden başlatılsın mı?', '/api/reboot'),
        wReset,
        act('Fabrika ayarlarına dön', 'factory', true, 'Fabrika ayarları', 'Ağ, parola, ayarlar, kilitli olmayan alarmlar ve sayaçlar silinecek; güvenlik limitleri varsayılana döner. Fabrika ayarlarına dönülsün mü?', '/api/factory-reset')),
      wResetOut,
      h('p', {class: 'field-hint', text: 'Üç işlem ayrıdır: yeniden başlatma yalnız cihazı yeniden açar; Wi-Fi silme yalnız kablosuz bilgileri siler; fabrika ayarları bütün ayarları siler. Bağlantı sorununda önce Wi-Fi ağını değiştirin.'}),
      h('p', {class: 'field-hint', text: 'Yeniden başlatmada rezistanslar donanım pull-down’ları ile kapalı kalır; mod ve ayarlar korunur.'})));
  }

  async function load() {
    try { render(await api('/api/settings')); }
    catch (e) { panelsBox.prepend(h('div', {class: 'notice critical'}, icon('warn'), h('span', {text: 'Ayarlar alınamadı: ' + e.message}))); }
    const want = (!useHash && location.hash.slice(1)) || lsGet('scada-settings-tab') || 'net';
    selectTab(SECTIONS.some(s => s[0] === want) ? want : 'net');
  }
  onEnter.settings = () => { if (!Object.keys(fields).length) load(); };
};

// ================================================================= BAŞLATMA
function init() {
  // menü: ikon + görünür metin
  $$('nav.menu a').forEach(a => {
    a.prepend(icon(a.dataset.icon));
    a.addEventListener('click', e => {
      if (e.ctrlKey || e.metaKey || e.shiftKey) return;
      e.preventDefault();
      go(a.dataset.route, true);
      $('#main').focus({preventScroll: true});
    });
  });
  iconize(document);
  // tema
  $('#btn-theme').addEventListener('click', () => {
    const t = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', t);
    lsSet('scada-tema', t);
  });
  // kimlik şeridi: masaüstünde açık, telefonda kapalı
  const idp = $('#identity'), ib = $('#btn-info');
  const setId = open => { idp.classList.toggle('open', open); ib.setAttribute('aria-expanded', open ? 'true' : 'false'); };
  setId(!window.matchMedia('(max-width:600px)').matches);
  ib.addEventListener('click', () => setId(!idp.classList.contains('open')));
  // sistem durumu aç/kapa tercihi
  const dg = $('#diag');
  dg.open = lsGet('scada-diag') === '1';
  dg.addEventListener('toggle', () => { lsSet('scada-diag', dg.open ? '1' : '0'); if (D) renderDiag(); });
  // diyalog
  $('#dlg-ok').addEventListener('click', () => closeDlg(true));
  $('#dlg-cancel').addEventListener('click', () => closeDlg(false));
  $('#dlg-x').addEventListener('click', () => closeDlg(false));
  $('#dlg').addEventListener('cancel', e => { e.preventDefault(); closeDlg(false); });
  window.addEventListener('popstate', () => go(routeFromLocation(), false));
  window.addEventListener('hashchange', () => { if (useHash) go(routeFromLocation(), false); });
  go(routeFromLocation(), false);
  poll();
  setInterval(poll, POLL_MS);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
