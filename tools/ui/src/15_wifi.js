
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
