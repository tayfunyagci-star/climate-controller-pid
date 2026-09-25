# Konfigürasyon Modeli

Sütunlar: **Tür** (f=float, i=int, b=bool, e=enum, s=string, x=sır), **Kalıcı** (P=flash), **R** yeniden başlatma gerekir, **SC** güvenlik-kritik, **RW** uzaktan (MQTT) yazılabilir (`remote_config_enabled` açıkken). Tüm varsayılanlar başlangıç tasarım değeridir.

## 1. Kalıcılık ilkeleri

- **DESIGN DECISION:** Konfigürasyon tek şemalı belge (`config.json`, `schema_version`, `config_rev`, CRC32) olarak primary/backup/temp üçlüsüyle nesil kontrollü atomik yazılır (baseline §4). Sayaçlar (`counters.bin`) ve olaylar ayrı dosyalardır; farklı yazım sıklıkları birbirini etkilemez.
- Aday konfigürasyonun **tamamı** doğrulanmadan hiçbir alan RAM'e uygulanmaz; hata önceki çalışma ayarını korur.
- Operasyonel değerler (`temperature_setpoint`, `operating_mode`, `profile`, `manual_heat_demand`) persist edilir ama yazım **ertelenir** (5 s deferred, azami 60 s flush) — slider benzeri hızlı değişimler flash'ı aşındırmaz.
- Yarım/bozuk kayıt fabrika sıfırlaması tetiklemez: son doğrulanmış kopya → güvenli varsayılan + `CONFIGURATION_ERROR` + ısıtma kilidi.
- Sırlar (`x`) GET/MQTT/yedekte dönmez; bkz. [SECURITY.md](SECURITY.md).

## 2. Alanlar

### 2.1 System

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `device_display_name` | s | ≤ 32 bayt | `Kulübe İklim` | P | — | — | — |
| `slug` | s | `[a-z0-9_]{3,32}` | `kulube_iklim_<mac3>` | P | MQTT yeniden bağlanır (taşınma) | — | — |
| `tz_offset_min` | i | −720…840 | 180 | P | — | — | — |
| `time_source` | e | NONE, NTP, RTC | NTP | P | — | — | — |
| `ntp_server` | s | host/IPv4 | `""` (gateway) | P | — | — | — |
| `restart_storm_limit` | i | 3–10 | 5 | P | — | ✓ | — |
| `restart_storm_window_min` | i | 10–120 | 30 | P | — | ✓ | — |

### 2.2 Climate

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `temperature_setpoint` | f | 5.0–30.0 / 0.5 | 21.0 | P (ertelenmiş) | — | — | ✓ (operasyonel) |
| `setpoint_night` | f | 5–30 | 18.0 | P | — | — | ✓ |
| `setpoint_away` | f | 5–25 | 12.0 | P | — | — | ✓ |
| `setpoint_frost` | f | 4–12 | 5.0 | P | — | ✓ | ✓ |
| `setpoint_boost` | f | 10–30 | 23.0 | P | — | — | ✓ |
| `boost_minutes` | i | 10–240 | 30 | P | — | — | ✓ |
| `setpoint_ramp_c_per_min` | f | 0–2.0 | 0.2 | P | — | — | ✓ |
| `operating_mode` | e | OFF, AUTO, MANUAL, VENT_ONLY | AUTO | P (ertelenmiş) | — | — | ✓ (operasyonel) |
| `profile` | e | DAY, NIGHT, AWAY, FROST | DAY | P | — | — | ✓ (operasyonel) |
| `sched_timeout_h` | i | 0–48 (0=kapalı) | 16 | P | — | — | ✓ |
| `manual_timeout_h` | i | 0–48 | 8 | P | — | — | ✓ |
| `remote_manual_allowed` | b | | true | P | — | ✓ | — |
| `antifreeze_enabled` | b | | true | P | — | ✓ | — |
| `frost_guard_temperature` | f | 2–10 | 4.0 | P | — | ✓ | — |
| `frost_exit_hysteresis` | f | 0.5–3 | 1.0 | P | — | ✓ | — |

### 2.3 PID

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `pid_mode` | e | P, PI, PID, ONOFF | PI | P | — | — | ✓ + `pid_remote_tuning` |
| `pid_kp` | f | 0.5–200 | 20.0 | P | — | — | ✓ + `pid_remote_tuning` |
| `pid_ki` | f | 0–20 (%/°C·dk) | 1.0 | P | — | — | ✓ + `pid_remote_tuning` |
| `pid_kd` | f | 0–60 (%·dk/°C) | 0.0 | P | — | — | ✓ + `pid_remote_tuning` |
| `pid_setpoint_weight` | f | 0–1 | 1.0 | P | — | — | — |
| `pid_deadband` | f | 0–1.0 °C | 0.1 | P | — | — | — |
| `control_interval_s` | i | 1–30 | 2 | P | — | — | — |
| `min_heat_demand` | f | 0–20 % | 5 | P | — | — | — |
| `max_heat_demand` | f | 20–100 % | 100 | P | — | — | ✓ |
| `demand_slew_pct_per_min` | f | 1–100 | 10 | P | — | — | — |
| `onoff_hysteresis` | f | 0.2–2.0 °C | 0.5 | P | — | — | — |
| `pid_remote_tuning` | b | | false | P | — | — | — |

### 2.4 Heating (Power Manager / çıkış)

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `output_driver_r` | e | SSR_ZC, SSR_RANDOM, RELAY | SSR_ZC | P | ✓ | ✓ | — |
| `output_driver_fan` | e | RELAY, SSR_ZC | RELAY | P | ✓ | ✓ | — |
| `output_active_high[4]` | b | | true | P | ✓ | ✓ | — |
| `tp_window_s` | i | SSR 5–120 / RELAY 300–1800 | 20 / 600 | P | — | ✓ | — |
| `heater_min_on_s` / `heater_min_off_s` | i | SSR 1–60 / RELAY 60–900 | 1/1 · 120/180 | P | — | ✓ | — |
| `stage2_on` | f | 40–90 % | 55 | P | — | — | — |
| `stage2_off` | f | 10…(`stage2_on` − 5) % | 45 | P | — | — | — |
| `stage_min_dwell_s` | i | 0–1800 | 120 | P | — | — | — |
| `heater_power_w_r1` / `_r2` | i | 0–5000 (0 = eşit) | 0 | P | — | — | — |
| `lead_rotation` | e | OFF, DAILY | DAILY | P | — | — | — |
| `fan_prestart_s` | i | 0–30 | 3 | P | — | ✓ | — |
| `post_cool_mode` | e | TIME, TEMPERATURE, HYBRID | TIME | P | — | ✓ | — |
| `post_cool_seconds` | i | 30–600 | 60 | P | — | ✓ | ✓ (alt sınır korumalı) |
| `post_cool_safe_temp` | f | 25–70 °C | 40 | P | — | ✓ | — |
| `post_cool_min_s` / `post_cool_max_s` | i | 10–120 / 60–1800 | 20 / 600 | P | — | ✓ | — |
| `relay_life_cycles` | i | 10 000–10 000 000 | 100 000 | P | — | — | — |

### 2.5 Ventilation

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `ventilation_start_temperature` | f | 15–40 | 26.0 | P | — | — | ✓ |
| `ventilation_stop_temperature` | f | 14–39 | 24.0 | P | — | — | ✓ |
| `vent_sp_margin` | f | 1–10 | 2.0 | P | — | — | — |
| `humidity_vent_enabled` | b | | true | P | — | — | ✓ |
| `humidity_high_limit` | f | 40–95 % | 75 | P | — | — | ✓ |
| `humidity_hysteresis` | f | 2–20 % | 5 | P | — | — | ✓ |
| `humidity_low_limit` | f | 10–60 % | 30 | P | — | — | ✓ |
| `humidity_vent_while_heating` | e | INHIBIT, ALLOW, ALLOW_ABOVE_SP | INHIBIT | P | — | — | ✓ |
| `manual_vent_priority` | e | VENT_WINS, HEAT_WINS | VENT_WINS | P | — | — | ✓ |
| `vent_heat_cap` | f | 0–50 % | 0 | P | — | — | — |
| `manual_vent_timeout_min` | i | 0–720 | 120 | P | — | — | ✓ |
| `ventilation_periodic_min` | i | 0–60 dk/sa | 0 | P | — | — | ✓ |
| `vent_min_on_s` / `vent_min_off_s` | i | 10–600 | 60 / 60 | P | — | — | — |
| `heat_vent_changeover_s` / `vent_heat_changeover_s` | i | 0–600 | 120 / 60 | P | — | — | — |
| `ota_vent_state` | e | LAST, OFF | LAST | P | — | — | — |

### 2.6 Safety

Yalnız yerel web, yönetici rolü, onay diyaloğu. Hiçbiri uzaktan yazılamaz.

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC |
|---|---|---|---|---|---|---|
| `cabin_overtemp_limit` | f | 30–60 °C | 40 | P | — | ✓ |
| `overtemp_reset_hysteresis` | f | 1–10 °C | 3 | P | — | ✓ |
| `heater_outlet_limit` | f | 50–150 °C | 80 | P | — | ✓ |
| `max_continuous_heating_min` | i | 30–1440 | 240 | P | — | ✓ |
| `sensor_stale_s` | i | 5–60 | 10 | P | — | ✓ |
| `unexpected_rise_c_per_10min` | f | 0.5–10 | 1.5 | P | — | ✓ |
| `max_rise_c_per_10min` | f | 1–20 | 5 | P | — | ✓ |
| `service_timeout_min` | i | 5–120 | 30 | P | — | ✓ |
| `service_test_max_s` | i | 10–300 | 120 | P | — | ✓ |

### 2.7 Sensors

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `t1_driver` / `rh1_driver` | e | AUTO, SHT4X, SHT3X, BME280, BME680, AHT20, DS18B20 (yalnız T) | AUTO | P | ✓ | ✓ | — |
| `t2_enabled` | b | | false | P | ✓ | ✓ | — |
| `t2_driver`, `*_address` | e / i | | DS18B20 / otomatik | P | ✓ | ✓ | — |
| `t1_offset`, `rh1_offset`, `t2_offset` | f | ±5 / ±10 / ±10 | 0 | P | — | ✓ | — |
| `sensor_interval_s` | i | 1–10 | 1 | P | — | — | — |
| `sensor_filter_tau_s` | i | 0–120 | 10 | P | — | — | — |
| `sensor_stuck_s` | i | 300–7200 | 1800 | P | — | — | — |

### 2.8 MQTT

| Anahtar | Tür | Aralık | Varsayılan | Kalıcı | R | SC | RW |
|---|---|---|---|---|---|---|---|
| `mqtt_host` / `mqtt_port` | s / i | ≤ 63 / 1–65535 | `""` / 1883 | P | yeniden bağlan | — | — |
| `mqtt_user` / `mqtt_password` | s / x | ≤ 64 / ≤ 128 | | P | yeniden bağlan | — | — |
| `mqtt_base` | s | ≤ 96, `+ #` yok | `mqttsuite/climate` | P | taşınma | — | — |
| `discovery_enabled` | b | | true | P | — | — | — |
| `history_discovery_enabled` | b | | false | P | — | — | — |
| `state_active_s` / `state_idle_s` | i | 1–30 / 10–60 | 5 / 30 | P | — | — | — |
| `diag_interval_s` | i | 30–300 | 60 | P | — | — | — |
| `remote_config_enabled` | b | | **false** | P | — | ✓ | — |
| `service_channel_enabled` | b | | false | P | — | ✓ | — |
| `service_token` | x | 16–64 | — | P | — | ✓ | — |
| `mqtt_tls` (FUTURE) | b | | false | P | ✓ | — | — |

### 2.9 Network

Skill kataloğu (scada-ui-design §8.4 Ağ): `adN`→`device_display_name`, `mdns` (`kulube-iklim`), `staticEnabled`, `staticIP`, `gateway`, `subnet`, `dns1`, `dns2`, `ssid`, `pass` (x), ~~`ap_policy`~~ (kaldırıldı, D-23: AP davranışı aile standardında sabit — [NETWORK.md](NETWORK.md)). Hepsi P, SC değil, RW değil.

### 2.10 Web UI / Erişim

| Anahtar | Tür | Varsayılan | Not |
|---|---|---|---|
| `web_user` | s | `admin` | |
| `web_password_hash` | x | tanımsız (kurulumda zorunlu önerisi) | PBKDF2-HMAC-SHA256, tuz, iterasyon hedefte ölçülür |
| `operator_password_hash` | x | tanımsız | Operatör rolü (FUTURE: tek kullanıcı + rol) |
| `guest_read` | b | false | |
| `session_hours` / `remember_days` | i | 8 / 14 | |
| `service_pin_hash` | x | tanımsız → servis modu kapalı | |
| `local_lock_max_min` | i | 1440 | |
| `ota_password_hash` | x | tanımsız → OTA kapalı | **DESIGN DECISION:** Bu cihazda parolasız OTA varsayılanı yok (rezistans yükü) |

### 2.11 Diagnostics

| Anahtar | Tür | Varsayılan |
|---|---|---|
| `event_mqtt_enabled` | b | true |
| `event_debug` | b | false (DEBUG olayları RAM'e) |
| `trend_enabled` | b | true |

## 3. Güvenlik limitleri ile kontrol limitlerinin ayrımı

| | Kontrol limitleri | Güvenlik limitleri |
|---|---|---|
| Örnek | setpoint, vent eşikleri, max_heat_demand | cabin_overtemp_limit, heater_outlet_limit, max_continuous_heating_min, sensor_stale_s |
| Kullanan | ClimateController / PID | SafetyManager (ayrı görev, ayrı kopya) |
| Yazma | Web, MQTT (izinliyse) | Yalnız yerel web, yönetici, onay |
| Etki | Hedef/davranış | Kilit, çıkış kesme |
| Doğrulama | Kendi aralığı + ilişkiler | Kendi aralığı + kontrol limitlerinin **üzerinde** güvenlik payı |

SafetyManager limitlerin kendi RAM kopyasını tutar; yalnız doğrulanmış tam konfigürasyon commit edildiğinde kopya güncellenir.

## 4. Doğrulama kuralları (ilişkiler)

| # | Kural | Hata kodu |
|---|---|---|
| V1 | `ventilation_stop_temperature ≤ ventilation_start_temperature − 1.0` | `INVALID_RELATION` (alan: stop) |
| V2 | `humidity_low_limit ≤ humidity_high_limit − humidity_hysteresis − 5` | `INVALID_RELATION` |
| V3 | `setpoint_frost ≤ setpoint_away ≤ setpoint_night ≤ temperature_setpoint`, `setpoint_boost ≥ temperature_setpoint` — **uyarı**, ret değil | `WARN_PROFILE_ORDER` |
| V4 | `frost_guard_temperature < setpoint_frost` | `INVALID_RELATION` |
| V5 | `cabin_overtemp_limit ≥ max(setpoint_boost, temperature_setpoint) + 10` | `INVALID_SAFETY_MARGIN` |
| V6 | `cabin_overtemp_limit ≥ ventilation_start_temperature + 5` | `INVALID_SAFETY_MARGIN` |
| V7 | `stage2_off ≤ stage2_on − 5` | `INVALID_RELATION` |
| V8 | Sürücü profiline göre pencere ve min on/off aralığı ([OUTPUT_AND_INTERLOCKS §4.2](OUTPUT_AND_INTERLOCKS.md)) | `INVALID_DRIVER_TIMING` |
| V9 | `heater_min_on_s + heater_min_off_s ≤ tp_window_s` | `INVALID_DRIVER_TIMING` |
| V10 | `post_cool_min_s ≤ post_cool_seconds ≤ post_cool_max_s` | `INVALID_RELATION` |
| V11 | `post_cool_mode ≠ TIME ⇒ t2_enabled` | `REQUIRES_T2` |
| V12 | `pid_mode = PID ⇒ pid_kd > 0` uyarısı; `PI ⇒ pid_ki > 0` zorunlu | `INVALID_PID` |
| V13 | PID kararlılık kaba sınırı: `pid_kp · 1 °C ≤ 200 %`, `pid_ki ≤ pid_kp` (Ti ≥ 1 dk) | `INVALID_PID` |
| V14 | `control_interval_s ≥ sensor_interval_s` | `INVALID_RELATION` |
| V15 | `sensor_stale_s ≥ 3 · sensor_interval_s` | `INVALID_RELATION` |
| V16 | `state_idle_s < 60` (Suite nokta geçerlilik süresinden kısa) | `INVALID_RANGE` |
| V17 | Tek MQTT `/set` komutu ilişki ihlali oluşturuyorsa (ör. yalnız start düşürüldü) reddedilir; UI'da çoklu alan kaydı atomik doğrulanır | `REJECTED_RELATION` |

- **DESIGN DECISION:** Ret, kıstırma değildir. Kullanıcı tek MQTT komutuyla ilişkiyi bozamaz; ilişkili iki alanı değiştirmek için doğru sırayla iki komut gönderilir veya web formu kullanılır.
- Doğrulama modülü saf fonksiyondur (`validate(candidate, current) → [errors], [warnings]`); web, MQTT ve boot yüklemesi aynı fonksiyonu kullanır.

## 5. Yedekleme ve geri yükleme

- Yedek: `schema_version`, `config_rev`, fw sürümü, sırsız tüm alanlar (JSON). Sırlar "tanımlı/tanımsız" bayrağıyla.
- Geri yükleme: şema göçü → tam doğrulama → fark önizlemesi (UI) → onay → atomik commit. Güvenlik limitleri farkı ayrıca vurgulanır.
- Farklı `hw_rev` yedeği: sürücü/pin alanları atlanır, uyarı verilir.
