# Genişleme Noktaları ve Yol Haritası

İlke: v1 mimarisi genişlemeye **kapalı değildir** ama genişleme için v1'e ölü kod eklenmez. Genişleme noktaları arayüz (rol tablosu, sürücü profili, interlock kural tablosu, entity tablosu) üzerinden tanımlıdır.

## 1. Genişleme noktaları

| Genişleme | Arayüz etkisi | Entity (ayrılmış) | Mimari hazırlık |
|---|---|---|---|
| Dış sıcaklık | Sensör rolü `T_ext` | `outdoor_temperature` | HPM verim normalizasyonu, antifreeze öngörüsü, PID ileri besleme (FUTURE) |
| CO₂ | `ClimateSensor` yeteneği `CO2`; rol `CO2` | `co2` | Havalandırma istek kaynağı `CO2_HIGH` (tablo satırı) |
| VOC | Rol `VOC` (BME680 gaz) | `voc_index` | Vent kaynağı |
| Basınç | Rol `P` | `pressure` | Yalnız izleme |
| İkinci iklim sensörü | Rol `T1b/RH1b` | `t3`, … | Yedekli ölçüm: medyan/ortalama, tutarsızlık alarmı; donma riskinde sensör arızasına dayanıklılık |
| Hava çıkış sensörü (T2) | v1'de isteğe bağlı | `t2` | Post-cool TEMPERATURE, S2, S6 |
| Akım ölçümü | `OutputDriver.feedback()` | `heater_current`, `heater_power_w` | `OUTPUT_FAULT`, açık devre rezistans tespiti, gerçek enerji |
| Fan RPM / akış anahtarı | Fan sürücü geri bildirimi | `heater_fan_rpm` | `HEATER_FAN_FAULT` doğrudan |
| Kapı / pencere | Dijital giriş rolü | `door_open`, `window_open` | Profil çözümüne "açıklık" girişi: ısıtmayı `open_delay_s` sonra durdur |
| Doluluk | Dijital giriş | `occupancy` | Profil kaynağı (AWAY ↔ DAY) |
| Enerji ölçümü | Sayaç | `energy_kwh_today` | Günlük geçmiş `kWh` (Suite TÜKETİM birim genişletmesiyle) |
| 3. rezistans kademesi | PowerManager kademe listesi N; interlock tablosu | `r3_active`, `r3_duty`, `r3_reason` | Kademeli eşleme N kanala genelleşir |
| Soğutma çıkışı | Talep işareti: `demand ∈ [−100, 100]` (split-range) | `cooling_active` | PID çıkış aralığı ve PowerManager ayrımı zaten var; ısıtma/soğutma arası ölü bant ve changeover |
| Isı pompası | Ayrı sürücü profili (min run/off 3–5 dk, defrost girişi) | — | Minimum süre kuralları interlock tablosunda |
| Çoklu bölge | `Zone` nesnesi: sensör rolleri + kontrolör + çıkış grubu | `zone_<n>_*` | ClimateController örneklenebilir tasarlanır (global durum yok); MQTT'de bölge başına entity öneki |
| Nemlendirme | Çıkış + `humidity_low_limit` | `humidifier_active` | Havalandırma koordinasyon tablosu genişler |
| Yerel program önceden ısıtma (optimum start), yaz saati, MQTT'den liste yazımı | Program modülü (ADR-009 uygulandı) | — | Öğrenilen ısınma hızı |
| WebSocket canlı veri | Web katmanı | — | Polling yolu korunur (ADR-006) |
| Core dump, HMAC denetim | Diag, EventLog | — | |

## 2. Önerilen geliştirme sırası

| Faz | İçerik | Çıkış kriteri |
|---|---|---|
| 0 | Donanım kararları ([DECISIONS_AND_OPEN_ISSUES §4](DECISIONS_AND_OPEN_ISSUES.md) checklist) | Checklist "kodlamadan önce" maddeleri kapalı |
| 1 | Saf çekirdek (host'ta): PID, PowerManager, InterlockEngine, profil çözücü, config doğrulayıcı, alarm FSM, HPM + native testler | Tüm tablolar testle kapsanmış; interlock değişmez testi |
| 1b | Yerel program çekirdeği (`cc_schedule`, ADR-009) + testler | **Tamam** (25.09.2026) |
| 2 | HAL + görev iskeleti: SensorTask (1 sürücü), OutputTask (GPIO + ARM), SafetyTask, ControlTask; saat kaynağı (NTP/RTC → `setClock`); seri log | HIL: güvenli boot ölçümü, sensör çekme, görev dondurma |
| 3 | Storage (config, sayaç, olay, `programs.json`) + boot/self-test | Kesinti testleri (yazım ortasında güç) |
| 4 | Web (REST + UI, `/api/programs`; skill test kiti, mock cihaz) | scada-ui-design §10/§11 |
| 5 | MQTT (discovery tablosu, state, komut, ack; program entity'leri) | `broker_teshis.py` sağlıklı; Studio'da entity/tip/değer/online ölçümü; 8 s doğrulama |
| 6 | Programs/Kurallar senaryoları, Suite proje kartı | MQTT_INTEGRATION §9 |
| 7 | OTA + rollback, trend, HPM ayarı | OTA kesinti testi |
| 8 | Saha: PID ayarı, 7 gün dayanıklılık, heap/gecikme | NFR hedefleri |
| 9+ | T2, akım ölçümü, dış sıcaklık, program önceden ısıtma … | Özellik bazlı |
