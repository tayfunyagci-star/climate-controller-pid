# Tanı ve Gözlemlenebilirlik

## 1. Tanı değerleri

| Değer | Birim | Kaynak / hesap | Kalıcı | Yayın |
|---|---|---|---|---|
| `uptime` | s | Monoton saat | — | diag, state |
| `reset_reason` | enum | `esp_reset_reason()` → POWER_ON, SW, PANIC, INT_WDT, TASK_WDT, BROWNOUT, DEEPSLEEP, OTA | Son 8 boot | diag |
| `boot_count` / `fault_boot_count` | adet | Her boot; fault = PANIC/WDT/BROWNOUT | ✓ | diag |
| `free_heap` / `min_heap` | B | `heap_caps` (internal) | — | diag |
| `largest_free_block` | B | Parçalanma göstergesi | — | diag |
| `task_stack_min[]` | B | Görev başına yığın yüksek su işareti | — | `/api/diag` |
| `wifi_rssi`, `wifi_channel`, `ip` | dBm / — | STA | — | diag |
| `wifi_reconnects`, `mqtt_reconnects` | adet | Boot'tan beri | — | diag |
| `mqtt_status` | enum | DISABLED, CONNECTING, CONNECTED, BACKOFF, AUTH_FAIL | — | web |
| `mqtt_publish_fail` | adet | Yayın hataları | — | diag |
| `sensor_error_count`, `sensor_crc_errors`, `sensor_timeouts`, `sensor_error_rate_10m` | | SensorManager | toplam ✓ | diag |
| `control_loop_ms` / `control_loop_max_ms` | ms | ControlTask çalışma süresi (son / boot'tan beri max, reset edilebilir) | — | diag |
| `control_jitter_max_ms` | ms | Periyot sapması | — | diag |
| `safety_loop_max_ms` | ms | | — | diag |
| `cmd_queue_max`, `cmd_rejected_busy` | adet | Arbiter | — | diag |
| `output_switch_count[R1,R2,HF,VF]` | adet | OutputManager, gerçek GPIO geçişi | ✓ (uint64) | diag |
| `output_on_seconds[R1,R2,HF,VF]` | s → `*_hours` | OutputManager | ✓ (uint64) | diag |
| `heating_minutes_today`, `duty_cycle_1h/24h`, `heating_cycles_1h`, `stage2_ratio_24h`, `heating_efficiency_index` | | HPM | günlük pencere ✓ | diag |
| `time_valid`, `time_source` | | | — | diag |
| `fs_errors`, `config_rev`, `config_save_fail` | | Storage | — | diag |
| `event_overwritten`, `event_dropped_mqtt` | adet | EventLog | — | diag |
| `fw_version`, `fw_build`, `hw_rev`, `idf_version`, `partition` | | Build kimliği | — | diag, web başlık |

## 2. Preventive maintenance

| Sayaç | Kullanım | Eşik (öneri) |
|---|---|---|
| `r1_switch_count`, `r2_switch_count` | Röle/SSR ömrü | RELAY: `relay_life_cycles` %80 → `RELAY_LIFE_WARNING` |
| `r1_hours`, `r2_hours` | Rezistans ömrü, lider rotasyonu doğrulaması (fark > %20 → rotasyon kontrolü) | Üretici verisi |
| `heater_fan_hours`, `ventilation_fan_hours` | Fan rulman bakımı | ör. 20 000 sa |
| `heater_fan_switch_count` | Fan motoru start sayısı | |
| `heating_efficiency_index` trend | Kapasite düşüşü | [SAFETY_DESIGN §6](SAFETY_DESIGN.md) |

**DESIGN DECISION — sayaç kalıcılığı:** RAM'de birikir; `counters.bin` nesil kontrollü, **15 dk'da bir** (değişiklik varsa) ve kontrollü reboot/OTA öncesi yazılır → ≤ 96 yazma/gün, kesintide ≤ 15 dk kayıp (NFR-08). Sayaç reseti servis işlemidir, olay günlüğüne önceki değerle yazılır.

## 3. Tanı erişimi

| Yol | İçerik | Yetki |
|---|---|---|
| Web › Sistem durumu (`details`) | Özet | Kullanıcı |
| `/api/diag` | Tam liste + görev yığınları | Kullanıcı |
| MQTT `B/diag/state` | §1 (görev yığınları hariç) | Broker ACL |
| Seri konsol (115200) | Boot logu, reset nedeni, çökme özeti | Fiziksel |
| Servis › Tanı paketi indir | diag + config (sırsız) + son 200 olay + kalıcı olaylar (JSON) | Yönetici |

- Çökme sonrası core dump (flash bölümü) **FUTURE ENHANCEMENT**; v1'de panic nedeni ve PC adresi RTC belleğinde tutulup sonraki boot'ta olaya yazılır.
- Sırlar, oturum token'ları ve parolalar hiçbir tanı çıktısında bulunmaz.
