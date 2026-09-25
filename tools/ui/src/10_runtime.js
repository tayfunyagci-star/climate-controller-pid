
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
  const title = (D.device_name || 'Kulübe İklim') + ' · ' + ($('#' + currentRoute) ? $('#' + currentRoute).dataset.title : '');
  if (document.title !== title) document.title = title;
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
    if (D.mqtt_status === 'AUTH_FAIL') list.push(['warn', 'MQTT broker kimlik bilgilerini reddetti. Ayarlar › MQTT bölümünden kullanıcı adı ve parolayı denetleyin.']);
    if (D.ota_password_set === false) list.push(['warn', 'OTA parolasız açık: aynı ağdaki herkes firmware yükleyebilir. Ayarlar › Erişim › OTA parolası bölümünden parola belirleyin.']);
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
