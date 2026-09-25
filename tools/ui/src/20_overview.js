
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
  sec.append(sectionHead('Genel Bakış'));
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
