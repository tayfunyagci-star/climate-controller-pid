
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
