
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
