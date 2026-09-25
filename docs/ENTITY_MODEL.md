# Entity Modeli

Eşleme kaynakları: `ENTITY_TAXONOMY.md` (§2 `kind` ↔ `entity_type`), `ENTITY_WIDGET_MAPPING.md` (widget uyumu), `OBJECT_TAXONOMY.md` (nesne dalları). Sütun kısaltmaları: **Bil.** HA bileşeni, **Kind** Studio `kind`, **ET/DT** entity_type/data_type, **Yaz** yazılabilir, **Topic** state kaynağı (`S`=`B/state`, `C`=`B/config/reported`, `D`=`B/diag/state`), **Grup** `KART` (Suite ana kart), `DETAY` (detay ekran), `AYAR` (konfigürasyon), `TANI` (diagnostic).

## 1. Başlangıç listesinin değerlendirmesi

| Prompt önerisi | Karar | Neden |
|---|---|---|
| `temperature_setpoint` hem sensor hem number | Tek **number** | Aynı alanın iki entity'si Suite'te çift kayıt üretir; number zaten okunur |
| `pid_output` + `heat_demand` | İkisi de | Ham PID ile sınırlanmış talep farkı tanı için gerekli |
| `uptime`, `wifi_rssi` | `B/diag/state`, `TANI` | Proses snapshot'ını şişirmez, 60 s yeterli |
| `sensor_ok` | binary, device_class yok | `problem` sınıfı ON=sorun anlamına gelir; `sensor_ok` ON=iyi |
| `alarm_state` | sensor (enum string) | Suite select dışında enum'u `sensor`/string olarak gösterir |
| `controller_enable` | Korunur; `operating_mode=OFF`'tan farkı: enable=OFF kontrolör + antifreeze dahil **tüm otomatiği** durdurur. **DESIGN DECISION:** OFF yalnız yerel web'den ve onayla; MQTT'den yalnız ON kabul edilir (OFF → `REJECTED_POLICY`) | Bakım/sezon kapatma; uzaktan donma korumasının kapatılmasını önler |
| Eklenenler | `setpoint_effective`, `profile`, `profile_active`, `boost`, `sched_night`, `sched_away`, `*_reason`, `heating_phase`, `ventilation_state`, `failsafe_reason`, `active_alarm_count`, `alarm_ack`, `local_lock`, `power_stage`, `temperature_quality` | Requested/effective modeli, Programs entegrasyonu, alarm yönetimi |
| R1/R2 manuel switch | **Yayınlanmaz** | Service-only ([OUTPUT_AND_INTERLOCKS §8](OUTPUT_AND_INTERLOCKS.md)) |

## 2. Proses entity'leri (`B/state`)

| id | Ad (sabit) | Bil. | Kind | ET/DT | Yaz | Birim / aralık | device_class | Grup |
|---|---|---|---|---|---|---|---|---|
| `temperature` | Kulübe sıcaklığı | sensor | sensor | number/float | — | °C | temperature | KART |
| `humidity` | Kulübe nemi | sensor | sensor | number/float | — | % | humidity | KART |
| `temperature_setpoint` | Sıcaklık hedefi | number | number | number/float | ✓ | °C 5–30 / 0.5 | temperature | KART |
| `setpoint_effective` | Etkin hedef | sensor | sensor | number/float | — | °C | temperature | KART |
| `setpoint_source` | Hedef kaynağı | sensor | sensor | string | — | DAY…ANTIFREEZE | — | DETAY |
| `heat_demand` | Isı talebi | sensor | sensor | number/float | — | % | — | KART |
| `pid_output` | PID çıkışı | sensor | sensor | number/float | — | % | — | DETAY |
| `r1_duty` / `r2_duty` | R1 / R2 oranı | sensor | sensor | number/float | — | % | — | DETAY |
| `power_stage` | Güç kademesi | sensor | sensor | number/integer | — | 0–2 | — | DETAY |
| `temperature_rate` | Sıcaklık değişim hızı | sensor | sensor | number/float | — | °C/sa | — | DETAY |
| `r1_active` / `r2_active` | R1 / R2 çalışıyor | binary_sensor | binary | binary/boolean | — | ON/OFF | heat | KART |
| `heater_fan_active` | Isıtıcı fanı çalışıyor | binary_sensor | binary | binary/boolean | — | | running | KART |
| `ventilation_fan_active` | Havalandırma çalışıyor | binary_sensor | binary | binary/boolean | — | | running | KART |
| `heating_active` | Isıtma aktif | binary_sensor | binary | binary/boolean | — | | heat | DETAY |
| `ventilation_active` | Havalandırma aktif | binary_sensor | binary | binary/boolean | — | | running | DETAY |
| `heater_fan_manual` | Isıtıcı fanı isteği | switch | switch | binary/boolean | ✓ | ON/OFF | — | DETAY |
| `ventilation_fan_manual` | Havalandırma isteği | switch | switch | binary/boolean | ✓ | ON/OFF | — | KART |
| `r1_reason`, `r2_reason`, `heater_fan_reason`, `ventilation_fan_reason` | … nedeni | sensor | sensor | string | — | reason kodu | — | DETAY |
| `operating_mode` | Çalışma modu | select | select | enum/string | ✓ | OFF, AUTO, MANUAL, VENT_ONLY | — | KART |
| `controller_enable` | Kontrolör etkin | switch | switch | binary/boolean | ✓ (bkz. §1) | | — | DETAY |
| `controller_state` | Kontrolör durumu | sensor | sensor | string | — | [STATE_MACHINE §5](STATE_MACHINE.md) | — | KART |
| `heating_phase`, `ventilation_state`, `heating_reason`, `failsafe_reason` | … | sensor | sensor | string | — | | — | DETAY |
| `manual_heat_demand` | Manuel ısı talebi | number | number | number/float | ✓ | % 0–100 / 5 | — | DETAY |
| `profile` | Profil | select | select | enum/string | ✓ | DAY, NIGHT, AWAY, FROST | — | DETAY |
| `profile_active` | Etkin profil | sensor | sensor | string | — | + BOOST | — | KART |
| `boost` | Boost | switch | switch | binary/boolean | ✓ | | — | DETAY |
| `boost_remaining_min` | Boost kalan | sensor | sensor | number/integer | — | dk | duration | DETAY |
| `sched_night` | Gece programı isteği | switch | switch | binary/boolean | ✓ | | — | DETAY (Programs hedefi) |
| `sched_away` | Uzakta programı isteği | switch | switch | binary/boolean | ✓ | | — | DETAY (Programs hedefi) |
| `sensor_ok` | Sensör sağlıklı | binary_sensor | binary | binary/boolean | — | | — | KART |
| `temperature_quality`, `humidity_quality` | … kalitesi | sensor | sensor | string | — | GOOD… | — | TANI |
| `t2` | Hava çıkış sıcaklığı | sensor | sensor | number/float | — | °C | temperature | DETAY (**yalnız `t2_enabled`**) |
| `overtemperature` | Aşırı sıcaklık | binary_sensor | binary | binary/boolean | — | | problem | DETAY |
| `alarm` | Alarm var | binary_sensor | binary | binary/boolean | — | | problem | KART |
| `alarm_state` | Alarm seviyesi | sensor | sensor | string | — | NORMAL, INFO, WARNING, CRITICAL | — | KART |
| `active_alarm_count`, `unacked_alarm_count` | … | sensor | sensor | number/integer | — | | — | DETAY |
| `alarm_ack` | Alarmları onayla | button | button | button/— | invoke | PRESS | — | DETAY |
| `local_lock` | Yerel kilit | binary_sensor | binary | binary/boolean | — (yalnız web ayarlar) | | lock | DETAY |
| `last_command_source` | Son komut kaynağı | sensor | sensor | string | — | | — | TANI |

## 3. Konfigürasyon entity'leri (`B/config/reported`, `ent_cat: config`)

Remote writable olan alanlar number/select/switch olarak yayınlanır; diğerleri yalnız web'de. Tam liste ve aralıklar: [CONFIGURATION_MODEL.md](CONFIGURATION_MODEL.md).

| id | Bil. | Birim / aralık | Remote writable |
|---|---|---|---|
| `setpoint_night`, `setpoint_away`, `setpoint_frost`, `setpoint_boost` | number | °C | ✓ |
| `boost_minutes` | number | dk 10–240 | ✓ |
| `humidity_high_limit`, `humidity_low_limit` | number | % | ✓ |
| `ventilation_start_temperature`, `ventilation_stop_temperature` | number | °C | ✓ |
| `pid_kp`, `pid_ki`, `pid_kd` | number | [PID_DESIGN §2](PID_DESIGN.md) | ✓ (**remote_config_enabled** + `pid_remote_tuning`) |
| `pid_mode` | select | P, PI, PID, ONOFF | ✓ (aynı koşul) |
| `post_cool_seconds` | number | s 30–600 | ✓ |
| `max_heat_demand` | number | % 20–100 | ✓ |
| `humidity_vent_while_heating` | select | INHIBIT, ALLOW, ALLOW_ABOVE_SP | ✓ |
| `antifreeze_enabled` | switch | | ✗ (yalnız yerel; kapatma güvenlik etkili) |

`humidity_low_limit`: v1'de nemlendirici çıkış yok; yalnız `HUMIDITY_LOW` INFO alarmı üretir (FUTURE: nemlendirme).

## 4. Tanı entity'leri (`B/diag/state`, `ent_cat: diagnostic`)

`uptime` (s, duration), `wifi_rssi` (dBm, signal_strength), `free_heap`, `min_heap` (B), `reset_reason` (string), `mqtt_reconnects`, `sensor_error_count`, `control_loop_max_ms`, `r1_hours`, `r2_hours`, `heater_fan_hours`, `ventilation_fan_hours` (h, duration), `r1_switch_count`, `r2_switch_count`, `heater_fan_switch_count`, `ventilation_fan_switch_count`, `heating_minutes_today`, `duty_cycle_24h`, `fw_version`. Hepsi `sensor`, salt okunur. Ayrıntı: [DIAGNOSTICS.md](DIAGNOSTICS.md).

## 5. Requested / effective eşlemesi

| Çıkış | Requested (istek) | Effective (etkin) | Reason |
|---|---|---|---|
| R1 | (dahili, PowerManager) | `r1_active` | `r1_reason` |
| R2 | (dahili) | `r2_active` | `r2_reason` |
| Heater Fan | `heater_fan_manual` (switch) | `heater_fan_active` | `heater_fan_reason` |
| Ventilation Fan | `ventilation_fan_manual` (switch) + otomatik | `ventilation_fan_active` | `ventilation_fan_reason` |
| Setpoint | `temperature_setpoint` (number) | `setpoint_effective` | `setpoint_source` |
| Profil | `profile`, `sched_*`, `boost` | `profile_active` | `setpoint_source` |
| Isı talebi | `manual_heat_demand` (MANUAL) | `heat_demand` | `heating_reason` |

**Reason sözlüğü:** `NONE`, `HEATER_INTERLOCK`, `POST_COOL`, `PREPURGE`, `FAN_PRESTART`, `BOOT_POST_COOL`, `SAFETY_LOCKOUT`, `OVERTEMPERATURE`, `OVERTEMPERATURE_LOCKOUT`, `SENSOR_FAULT`, `ANTIFREEZE_INHIBIT`, `HEATING_PRIORITY`, `VENT_PRIORITY`, `MIN_ON_TIME`, `MIN_OFF_TIME`, `CHANGEOVER_DELAY`, `LOCAL_LOCK`, `SERVICE_ONLY`, `SERVICE_TEST`, `OTA`, `CONTROLLER_DISABLED`, `AUTO_DEMAND` (istek OFF ama otomatik kural ON), `MODE_OFF` (OFF modunda manuel havalandırma isteği engellendi; F1, 25.09.2026). Sözlük kapalıdır; yeni kod firmware sürüm notunda ilan edilir.

## 6. Studio widget eşlemesi

`ENTITY_WIDGET_MAPPING.md` tablosu uyarınca:

| Entity | Varsayılan widget (`tur` / `bicim`) | Alternatif |
|---|---|---|
| `temperature`, `setpoint_effective` | `deger` / `normal` (büyük sayı) | `analoggosterge`, `bolgeligosterge` (0–40 °C, bölge: <5 mavi, >35 kırmızı) |
| `humidity` | `halkagosterge` (ring) | `deger` |
| `temperature_setpoint` | `degerkomutu` / `kutu` (sayısal giriş) | `knob` (dokunmatik ekranda yanlış dokunma riski: önerilmez) |
| `heat_demand`, `r1_duty`, `r2_duty` | `ilerleme` / `linear` | `lineargosterge` |
| `r1_active`, `r2_active`, `*_fan_active` | `durum` / `rozet` | `etiket`+`ikon` |
| `operating_mode`, `profile` | `buton` / `enum` | — (`twochoice` yalnız iki seçenekli entity'ler için; burada uygun değil) |
| `ventilation_fan_manual`, `boost` | `anahtar` / `tgl` | `buton` / `pwr` |
| `controller_state`, `alarm_state`, `profile_active` | `durum` / `metin` | `rozet` |
| `alarm` | `durum` / `rozet` | |
| `alarm_ack` | `buton` / `cmd` | |
| `temperature` trend | `grafik` (son 60 örnek; uzun dönem historian) | |

## 7. MQTT Suite SCADA kartı

### 7.1 Ana kart (bilgi yoğunluğu: "bir bakışta")

```text
┌ Kulübe İklim 01 ─────────────────────── ● Çevrimiçi ┐
│ 21.8 °C           Hedef 22.0 °C (Etkin 21.6 · DAY)  │
│ Nem 48 %                                            │
│ Isı talebi  ████████░░░░░░░░  43 %                  │
│ [● R1 ÇALIŞIYOR] [○ R2 KAPALI]                      │
│ [● ISITICI FANI] [○ HAVALANDIRMA]                   │
│ Mod: AUTO ▾         Durum: ▲ ISITIYOR               │
│ ✓ Sensör   ✓ MQTT   ✓ Alarm yok                     │
└─────────────────────────────────────────────────────┘
```

| Kart bölgesi | Entity | Nesne |
|---|---|---|
| Birincil | `temperature` | Değer / Sayısal / Metin (büyük) |
| Hedef | `temperature_setpoint` (komut), `setpoint_effective`, `profile_active` | Değer komutu / Sayısal giriş + Değer |
| Nem | `humidity` | Değer |
| Talep | `heat_demand` | Değer / Doğrusal |
| Çıkışlar | `r1_active`, `r2_active`, `heater_fan_active`, `ventilation_fan_active` | Değer / Durum / Rozet |
| Mod | `operating_mode` | Buton / Seçenekli (enum) |
| Durum | `controller_state` | Değer / Durum / Metin |
| Sağlık | `sensor_ok`, avail (MQTT), `alarm`/`alarm_state` | Değer / Durum / Bağlantı + Rozet |

**DESIGN DECISION:** Ana kartta tek kumandalar `temperature_setpoint` ve `operating_mode`dur. Havalandırma isteği, boost, profil, onay ve reason alanları detay ekranındadır (scada-ui-design: kontrol ile izleme ayrılır, ana kart teşhis bilgisiyle boğulmaz). Kart, Suite Projeler'de mantıksal noktalar üzerinden de kurulabilir (`iklim.kulube.sicaklik`, `iklim.kulube.hedef`, …) — kaynak değişiminde bağlama korunur (SCADA_PROJECTS.md).

### 7.2 Detay ekranı

Gruplar: **Kontrol** (setpoint, mod, profil, boost, manuel talep, sched_*), **Çıkışlar** (dört çıkış × istek/etkin/neden tablosu), **Havalandırma** (manuel istek, ventilation_state, eşikler), **PID** (pid_output, heat_demand, duty'ler, kademe), **Alarm** (alarm_state, sayılar, `alarm_ack`), **Tanı** (diag entity'leri).

### 7.3 Suite proje alarm kuralı önerileri

| Kural | Nokta | Tür | Önem |
|---|---|---|---|
| Cihaz alarmı | `alarm` | `binary_active` | Cihazın `alarm_state`'ine göre iki kural: CRITICAL/WARNING |
| Sıcaklık düşük | `temperature` | `low` 5 °C, hist. 0.5, onDelay 300 s | critical |
| Cihaz çevrimdışı | herhangi nokta | `offline` | warning |
| Veri bayat | `temperature` | `stale` | warning |

## 8. Genişleme için ayrılmış anahtarlar

`outdoor_temperature`, `co2`, `voc_index`, `pressure`, `t3`, `r3_active`, `r3_duty`, `r3_reason`, `cooling_active`, `door_open`, `window_open`, `occupancy`, `heater_current`, `heater_power_w`, `energy_kwh_today`, `heater_fan_rpm`, `zone_<n>_*`. Bu adlar başka anlamda kullanılmaz ([EXPANSION_ROADMAP.md](EXPANSION_ROADMAP.md)).
