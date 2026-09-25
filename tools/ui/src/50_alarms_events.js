
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
