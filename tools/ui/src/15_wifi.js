
// ================================================================= AP KURULUM + Wi-Fi DİYALOĞU
// SCADA ailesi sözleşmesi (4chRelayModule / Flowmeter ESP32; scada-ui-design §8.6 “Wi-Fi diyaloğu”):
//   GET /scan → {pending, networks:[{ssid, rssi, secure, channel}]} (pending ise ~700 ms, en çok 20 deneme)
//   POST /api/settings {ssid, pass} → yalnız kablosuz kimlik; cihaz yeniden başlamadan yeni ağa geçer
//   POST /api/reset-wifi → kimlik silinir, statik IP kapanır, kurulum AP'si açılır
// /api/data: ap_mode, ap_name, ap_ip, wifi_ssid, net_note
let wifiDlg = null, wifiSelected = '', wifiScanning = false;

function wifiDialog() {
  if (wifiDlg) return wifiDlg;
  const status = h('p', {id: 'wifi-status', role: 'status', class: 'field-hint'});
  const list = h('div', {id: 'networks', class: 'wifi-list', role: 'list', 'aria-label': '2.4 GHz ağlar'});
  const rescan = h('button', {type: 'button', id: 'wifi-rescan', 'data-icon': 'refresh', 'data-text': ''}, 'Yeniden tara');
  const pw = h('input', {type: 'password', id: 'wifi-password', maxlength: '64', autocomplete: 'new-password', required: ''});
  const open = h('input', {type: 'checkbox', id: 'wifi-open'});
  const sel = h('h3', {id: 'wifi-selected'});
  const err = h('div', {class: 'cmd-msg', id: 'wifi-err'});
  const save = h('button', {type: 'submit', id: 'wifi-save', class: 'primary', 'data-icon': 'save', 'data-text': ''}, 'Ağı kaydet');
  const cancel = h('button', {type: 'button'}, 'Vazgeç');
  const form = h('form', {id: 'wifi-form', hidden: '', novalidate: ''}, sel,
    h('div', {class: 'field'}, h('label', {for: 'wifi-password', text: 'Wi-Fi parolası'}), pw),
    h('div', {class: 'field toggle'}, open, h('label', {for: 'wifi-open', text: 'Açık ağ (parolasız)'})),
    h('p', {class: 'field-hint', text: 'Kaydetme yalnız Wi-Fi adı ve parolasını değiştirir. Cihaz yeni ağa geçerken bu bağlantı kesilebilir; kontrol çalışmaya devam eder.'}),
    err, h('div', {class: 'dlg-acts'}, cancel, save));
  const close = h('button', {type: 'button', 'data-icon': 'x'}, 'Kapat');
  const dlg = h('dialog', {id: 'wifi-dialog', 'aria-labelledby': 'wifi-dlg-t'},
    h('div', {class: 'dlg-head'}, h('h2', {id: 'wifi-dlg-t', text: 'Wi-Fi ağı seç'}), close),
    h('p', {class: 'field-hint', text: 'ESP32 yalnız 2.4 GHz ağları görür. Aynı ad ve güvenlik türündeki ağlar en güçlü sinyalle listelenir.'}),
    h('div', {class: 'btn-row'}, rescan), status, list, form);
  document.body.append(dlg);
  iconize(dlg);
  close.addEventListener('click', () => dlg.close());
  cancel.addEventListener('click', () => dlg.close());
  dlg.addEventListener('close', () => { pw.value = ''; setText(err, ''); });
  rescan.addEventListener('click', scanWifi);
  open.addEventListener('change', () => { pw.disabled = open.checked; if (open.checked) pw.value = ''; });
  form.addEventListener('submit', async e => {
    e.preventDefault();
    setText(err, '');
    if (!open.checked && (pw.value.length < 8 || pw.value.length > 64)) { showMsg({msg: err}, 'critical', 'Parola 8–64 karakter olmalı (açık ağ için kutuyu işaretleyin).'); return; }
    save.setAttribute('aria-busy', 'true');
    try {
      const r = await api('/api/settings', {ssid: wifiSelected, pass: open.checked ? '' : pw.value});
      dlg.close();
      toast((r && r.message) || 'Wi-Fi kaydedildi. Cihaz “' + wifiSelected + '” ağına geçiyor.');
      if (D && D.ap_mode) toast('Telefonunuzu normal Wi-Fi ağınıza geri bağlayın; cihaz ' + (D.mdns || 'kulube-iklim') + '.local veya IP adresiyle açılır.');
    } catch (ex) { showMsg({msg: err}, 'critical', ex.message); }
    finally { save.setAttribute('aria-busy', 'false'); }
  });
  wifiDlg = dlg;
  return dlg;
}

async function scanWifi() {
  if (wifiScanning) return;
  wifiScanning = true;
  const list = $('#networks'), status = $('#wifi-status');
  $('#wifi-rescan').disabled = true;
  list.textContent = '';
  $('#wifi-form').hidden = true;
  setText(status, '2.4 GHz ağlar taranıyor…');
  try {
    let d;
    for (let i = 0; i < 20; i++) {
      d = await api('/scan');
      if (!d.pending) break;
      await new Promise(r => setTimeout(r, 700));
    }
    if (d.pending) throw new Error('Tarama zaman aşımı. Yeniden tarayın.');
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
      const b = h('button', {type: 'button', class: 'wifi-row'}, sig, h('span', {class: 'ssid', text: n.ssid}),
        h('span', {class: 'num dim', text: n.rssi + ' dBm'}), n.secure ? icon('lock') : h('span', {class: 'dim', text: 'açık'}));
      b.addEventListener('click', () => {
        wifiSelected = n.ssid;
        setText($('#wifi-selected'), n.ssid);
        const pw = $('#wifi-password'), op = $('#wifi-open');
        pw.value = ''; op.checked = !n.secure; pw.disabled = !n.secure;
        $('#wifi-form').hidden = false;
        if (n.secure) pw.focus();
      });
      list.append(h('div', {role: 'listitem'}, b));
    });
    setText(status, rows.length ? rows.length + ' ağ bulundu. Bir ağ seçin.' : '2.4 GHz ağ bulunamadı.');
  } catch (e) { setText(status, e.message); }
  finally { wifiScanning = false; $('#wifi-rescan').disabled = false; }
}

function openWifiDialog() {
  const d = wifiDialog();
  if (d.showModal) d.showModal(); else d.setAttribute('open', '');
  scanWifi();
}

// Genel Bakış'ın başındaki AP kurulum paneli (kurulum ağında açılan sayfa)
function apSetupPanel() {
  const btn = h('button', {type: 'button', id: 'ap-connect', class: 'primary', 'data-icon': 'wifi', 'data-text': ''}, 'Wi-Fi seç ve bağlan');
  btn.addEventListener('click', openWifiDialog);
  return h('section', {class: 'panel ap-setup', id: 'ap-setup', hidden: '', 'aria-labelledby': 'ap-h'},
    h('span', {class: 'eyebrow', text: 'AP · KURULUM MODU'}),
    h('h3', {id: 'ap-h', text: 'Cihazı Wi-Fi ağına bağlayın'}),
    h('p', {id: 'ap-why', text: 'Cihaz şu anda kendi kurulum ağını yayınlıyor.'}),
    h('dl', {class: 'kv'}, h('dt', {text: 'Kurulum ağı'}), h('dd', {id: 'ap-name', class: 'mono'}, 'SCADA_AP'),
      h('dt', {text: 'Kurulum adresi'}), h('dd', {id: 'ap-ip', class: 'mono'}, '192.168.4.1')),
    h('ol', {class: 'steps'}, h('li', {text: 'Telefon veya tabletinizi yukarıdaki ağa bağlayın (parola etiketinde yazar).'}),
      h('li', {text: '2.4 GHz Wi-Fi ağınızı seçip parolasını girin.'}),
      h('li', {text: 'Kaydettikten sonra normal Wi-Fi ağınıza geri dönün.'})),
    h('div', {class: 'btn-row'}, btn, h('a', {href: useHash ? '#settings' : '/settings#maint', class: 'btn-link', text: 'Diğer cihaz ayarları'})),
    h('p', {class: 'field-hint', text: 'AP bağlantısında internet olmaması normaldir. Kontrol ve güvenlik işlevleri kurulum sırasında çalışmaya devam eder.'}));
}
function updateApPanel(d) {
  const p = $('#ap-setup');
  if (!p || !d) return;
  p.hidden = !d.ap_mode;
  if (!d.ap_mode) return;
  setText($('#ap-name'), d.ap_name || 'SCADA_AP');
  setText($('#ap-ip'), d.ap_ip || '192.168.4.1');
  setText($('#ap-why'), d.wifi_ssid ? '“' + d.wifi_ssid + '” ağına bağlanılamadı; kurulum ağı açıldı. Cihaz 5 dakikada bir kayıtlı ağı yeniden dener.' : 'Cihaz şu anda kendi kurulum ağını yayınlıyor.');
}
