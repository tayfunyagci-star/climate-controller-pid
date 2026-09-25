
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
      h('p', {class: 'field-hint', text: 'Yeni ağ seçildiğinde cihaz yeniden başlamadan geçiş yapar. Bağlanamazsa 20 s sonra kurulum AP’si açılır ve kayıtlı ağ 5 dakikada bir yeniden denenir.'}),
      h('h4', {class: 'group-heading', text: 'Firmware'}),
      h('div', {class: 'form-grid'}, h('div', {class: 'field'}, h('label', {for: 'ota-file', text: 'İmaj dosyası'}), otaFile),
        h('div', {class: 'field'}, h('label', {for: 'ota-pw', text: 'OTA parolası'}), otaPw),
        h('div', {class: 'full btn-row'}, otaBtn)),
      h('h4', {class: 'group-heading', text: 'Sayaçlar ve cihaz'}),
      h('div', {class: 'btn-row'}, cnt, cntBtn),
      h('div', {class: 'btn-row'},
        act('Yeniden başlat', 'reboot', false, 'Yeniden başlatma', 'Rezistanslar kapatılıp soğutma tamamlandıktan sonra cihaz yeniden başlatılsın mı?', '/api/reboot'),
        act('Wi-Fi bilgilerini sil ve AP başlat', 'wifioff', true, 'Wi-Fi sıfırlama', 'Yalnız Wi-Fi adı ve parolası silinecek, statik IP kapatılacak. Cihaz yeniden başlamadan AP kurulum moduna geçer; kontrol çalışmaya devam eder. Devam edilsin mi?', '/api/reset-wifi'),
        act('Fabrika ayarlarına dön', 'factory', true, 'Fabrika ayarları', 'Ağ, parola, ayarlar, kilitli olmayan alarmlar ve sayaçlar silinecek; güvenlik limitleri varsayılana döner. Fabrika ayarlarına dönülsün mü?', '/api/factory-reset')),
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
