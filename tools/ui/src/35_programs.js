
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
