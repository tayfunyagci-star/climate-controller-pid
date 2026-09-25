# Değişiklik Günlüğü

## F1 — Saf çekirdek + native testler (25.09.2026)

### Onaylar

- Implementation Readiness Checklist ★ maddeleri (1, 2, 3, 4, 12) kullanıcı tarafından 25.09.2026'da onaylandı: tasarım paketi ve ADR-001…008, ADR-005 profil önceliği, havalandırma varsayılanları (INHIBIT, VENT_WINS), başlangıç limitleri (40 °C, 240 dk, 10 s, 4 °C), native test altyapısı.

### Eklenenler

- `lib/core/` — saf C++17 çekirdek. Arduino/ESP-IDF başlığı yok, dinamik bellek yok, `delay()` yok. Zamanlayıcılar doygun biriktiricilerdir; uint32 taşmasına dayanıklıdır.

| Modül | Dosya | Kapsam |
|---|---|---|
| Ortak tipler, ad tabloları | `cc_types.h`, `cc_names.cpp`, `cc_events.h` | Kapalı reason sözlüğü, enum → tel adı, olay halkası (200) |
| Konfigürasyon + doğrulayıcı | `cc_config.*`, `cc_config_validate.*` | X-makro alan tablosu (aralık, adım, bayraklar), V1–V17, `setField` (uzak yazım politikası, ret ≠ kıstırma) |
| Sensör rol filtresi | `cc_sensor.*` | Doğrulama, medyan-3, EMA, kalite (GOOD…DISABLED), yaş, takılı değer, yoğuşma, 10 dk hata oranı |
| PID | `cc_pid.*` | P/PI/PID/ONOFF, b, deadband, filtreli türev, koşullu entegrasyon + geri hesaplama, hold, tracking, bumpless |
| Talep koşullandırma | `cc_demand.*` | Min talep + 2 % histerezis, yalnız artışta slew, max/cap, MANUAL + antifreeze max |
| Profil / setpoint | `cc_profile.*` | Öncelik tablosu, BOOST süresi, `sched_timeout_h`, yerel kilit, rampa, antifreeze bekçisi |
| PowerManager | `cc_power.*` | Kademe 55/45 + dwell, eşit/farklı güç eşlemesi, zaman-oransal pencere, min darbe, lider rotasyonu, uygulanan talep |
| Interlock + OutputGuard | `cc_interlock.*` | I-1…I-9, prestart, post-cool (TIME/TEMPERATURE/HYBRID, timeout), boot post-cool, min ON/OFF; OutputGuard ikinci kontrol + sıralı uygulama + sayaçlar |
| Havalandırma | `cc_vent.*` | Kaynaklar (TEMP_HIGH, HUMIDITY_HIGH, MANUAL, SCHEDULED, OVERTEMP), §5.2 koordinasyon, §5.3 eşik payı, changeover, OTA |
| Durum makineleri | `cc_state.*` | Sistem (BOOT…OTA), ısıtma zinciri, `controller_state` türetimi, restart fırtınası |
| Safety | `cc_safety.*` | S1–S4, S6, S7, S9, S13, S17, S19; kilitler, reset kuralları, heartbeat, ARM izni |
| Alarm FSM | `cc_alarm.*` | 20 alarmlık katalog, tüm geçişler, onDelay/offDelay, occurrence, SERVICE bastırma, kalıcılık dışa/içe aktarma |
| HPM | `cc_hpm.*` | 10 dk regresyon, duty 1 sa/24 sa, döngü, günlük dakika, kademe 2 oranı, verim indeksi, 6 kural |
| Birleştirici | `cc_core.*` | `ClimateCore`: control/output/safety aşamaları, komut arbitrasyonu, servis, OTA hazırlığı, olaylar, `CoreSnapshot` |

- `test/native/` — 13 Unity paketi, 162 test. `support/sim.h`: deterministik PRNG, kulübe termal modeli, çıkış değişmezi denetçisi.
  - Özellik testleri: InterlockEngine üzerinde 200 tohum × 2000 s; ClimateCore üzerinde 24 tohum × 3 sa (rastgele komut, kaynak, sensör kopması, aşırı sıcaklık, servis, OTA, boot post-cool). Her tikte `(R1∨R2)⇒HF`, prestart ≥ `fan_prestart_s`, HF kapanışından önce post-cool ≥ `post_cool_seconds` denetlenir.
- `platformio.ini` — `[env:native]` (Unity, `-std=gnu++17 -Werror`); ESP32 env'inde `gnu++17`.
- `src/main.cpp` — F1 hedef derleme doğrulaması. Çekirdeği derler, GPIO sürmez.

### Doğrulama durumu

| Kontrol | Sonuç |
|---|---|
| Native testler, bulut sandbox'ında g++ 13.3 + Unity (git) ile doğrudan derleme | 13/13 paket, 162/162 test geçti |
| Aynı testler, `-fsanitize=address,undefined` ile | Geçti, bellek/UB hatası yok |
| `pio test -e native` | **Çalıştırılmadı.** Sandbox'ta PlatformIO kayıt sunucusu (api.registry.platformio.org) erişim politikasınca engelli. Kullanıcı makinesinde çalıştırılacak. |
| `pio run -e esp32-s3-devkitc-1` (RAM/flash raporu) | **Çalıştırılmadı.** Aynı neden. |
| HIL / kart | Kapsam dışı (F2) |

### Tasarımdan sapmalar ve yorumlar (gerekçeli)

1. **S7 `HEATING_TIMEOUT` yorumu — kullanıcı tarafından onaylandı (25.09.2026).** Sayaç yalnız talep etkin üst sınırda (doyumda) kesintisiz kaldığında birikir. Belgedeki "sürekli ısıtma" düz okunursa kışın PI kararlı rejimde talep sürekli > 0 olur ve 240 dk sonra her gün kilit oluşur. Doyum yorumu şu durumu yakalar: yerinden çıkmış sensör → PID sürekli %100 talep → kulübe ısınıyor ama ölçülmüyor.
2. **Yeni kodlar.** `Reason::MODE_OFF`: OFF modunda manuel havalandırma isteği engellenir. `FailsafeReason::TEMP_RISE`: S9 kritik durumu; §4 tablosunda karşılığı yoktu. ENTITY_MODEL §5 ve STATE_MACHINE §4'e eklendi.
3. **Isıtma izninde T1 kalitesi** GOOD **veya UNCERTAIN** kabul edilir. SENSOR_ARCHITECTURE §4.2'ye göre UNCERTAIN'da son GOOD değer `sensor_stale_s` boyunca kontrolde kullanılır. STATE_MACHINE §2 yalnız GOOD diyordu. Antifreeze ve SELF_TEST yalnız GOOD ister.
4. **RELAY profili** ayrı bir sabit kademe algoritması kullanmaz. Aynı kademeli zaman-oransal algoritma uzun pencere (≥ 300 s) ve min darbe (≥ 60/120 s) ile çalışır. Sonuç: ≤ 6 anahtarlama/sa/kanal (test edildi). ADR-002'ye not düşüldü.
5. **Boot'ta min OFF sayaçları dolmuş sayılır.** Çıkışlar bilinmeyen süredir kapalıdır. Yeniden başlatma döngüsünü RESTART_STORM sınırlar. Bu olmasaydı röle profilinde boot sonrası ilk ısıtma 180 s gecikirdi.
6. **AUTO → MANUAL:** `manual_heat_demand` son talebin 5 %'lik adıma yuvarlanmış değeriyle başlar. Adım dışı değer config doğrulamasını bozardı; en çok 2.5 % sapma olur. MANUAL → AUTO tam bumpless yapılır (≤ 2 %, test edildi).
7. **PID integratör aralığı** [−100, 100] %. Kazanç değişiminde bumpless için negatif I gerekir (`I = u − P − D`). Çıktı yine 0…100 % kıstırılır. ONOFF → PI geçişinde de genel ilke uygulanır: `I = u − P − D`.
8. **A11:** Kontrol görevi donduğunda FAILSAFE, heartbeat sınırı (3 periyot = 6 s, SYSTEM_ARCHITECTURE §4.1) + ≤ 1 s içinde gelir. REQUIREMENTS A11'deki "≤ 1 s" algılamadan itibaren ölçülmüştür.
9. **`vent_min_forced_s`** konfigürasyonda tanımlı değil. FORCED çıkışında `vent_min_on_s` kullanılır.
10. **HPM kuralları** (`EXCESSIVE_DUTY`, `LONG_HEATING_DURATION`, `FREQUENT_CYCLING`, `CAPACITY_DEGRADATION`) alarm kataloğunda yok. HPM bayrağı olarak üretilir; olay/diag yayını F5'te bağlanacak. `HEATING_PERFORMANCE_LOW` alarm olarak bağlıdır.
11. **Flicker koruması.** İki rezistans aynı tikte devreye girmez; takipçinin açılışı 100 ms ertelenir. Takipçinin penceresi, lider modüle ediliyorsa yarım pencere kaydırılarak başlar.
12. **Boot'ta sensör** ilk geçerli örneğe kadar UNCERTAIN (değer `null`), `sensor_stale_s` sonra STALE olur. SELF_TEST 10 s içinde GOOD bekler.
13. **Alarm gecikmeleri.** Safety kaynaklı alarmlarda (OVERTEMPERATURE vb.) gecikmeyi Safety uygular; alarm onDelay 0'dır. offDelay değerleri katalogdaki gibidir.
14. **Alarm reset.** MQTT'den reddedilir (`REJECTED_POLICY`); servis kanalı F5'te. Yerel reset koşul sürerken `REJECTED_STATE` döner.

### Açık kalanlar (F1)

- `pio test -e native` ve `pio run` kullanıcı makinesinde çalıştırılıp sonuç bu dosyaya eklenecek.
- Kalıcılık (alarm/safety kilitleri, sayaçlar, `controller_enable`) F3'te.

## F4 önizleme — gömülü web arayüzü kaynakları (25.09.2026)

Kullanıcı isteğiyle F2/F3'ten önce UI kaynakları ve sahte cihazlı önizleme hazırlandı. Firmware tarafı (REST uçları, `assemble.py`, `ui_generated.h`) F4'te yazılacak.

### Eklenenler

- `tools/ui/index.html`, `app.css`, `theme.js`, `app.js` (kaynak: `tools/ui/src/*.js`, sıralı birleştirme). scada-ui-design token'ları, 10/11/12/14 px ölçeği, iki tema, inline stil/betik yok (dinamik ölçüler SVG öznitelikleriyle).
- Sayfalar: Genel Bakış, Kontrol (İklim · Profiller · Havalandırma · PID), Trendler (5 dk–24 sa, SVG, Gantt, tablo alternatifi), Çıkışlar (istek/etkin/neden, servis test sütunu), Alarmlar, Olaylar, Ayarlar (Ağ · MQTT · Sensörler · Kontrol · Güvenlik · Erişim · Bakım), Oturum.
- Komut akışı: gönderiliyor → onay bekleniyor (`seq` + alan eşleşmesi, 5 s) → onaylandı / reddedildi / zaman aşımı; `OVERRIDDEN` uyarı notu; bayat veride kumanda kapalı.
- `tools/ui/test/mock_device.js` — sahte cihaz: çekirdeğin sadeleştirilmiş JS kopyası (PI, kademe, prestart, post-cool, havalandırma, S1/S3, alarmlar, olaylar, trend), önizleme paneli (hız, sensör çekme, aşırı sıcaklık, broker, komut reddi, çevrimdışı, dış sıcaklık).
- `tools/ui/test/build_test_html.py` → `test/ui_test.html` (file://) ve `test/ui_preview.html` (Artifact gövdesi).

### Doğrulama

- Playwright (Chromium, headless): 2 tema × 1280/390 px, 8 sayfa gezildi. JS/konsol hatası yok (yalnız sandbox'ta erişilemeyen Google Fonts), yatay taşma yok, görünür yazılar 10/11/12/14 px (gösterge değerleri hariç). Ekran görüntüleri incelendi.
- §10 matrisinin tamamı (768 px, 320 px, %200 zoom, kontrast ölçümü), CSP başlıklı sunucu testi ve §11 akış testleri **yapılmadı** → F4.

### Tasarımdan sapmalar / eklemeler

1. REST eklemeleri (WEB_SCADA_UI §13'e işlenecek): `/api/cmd` için `local_lock_min` kimliği; `/api/session`, `/api/service/{enter,exit,test,pin,reset-counters}`, `/api/wifi`, `/api/ota/begin`, `/api/reboot`, `/api/reset-wifi`, `/api/factory-reset`. `/api/data` alanlarına `vent_sources`, `*_minutes_today`, `*_switch_count`, `svc_test_*`, `service_remaining_s`, `local_lock_remaining_s`, `stage2_on/off`, kimlik ve bağlantı alanları eklendi.
2. Ayarlarda LED bölümü yok (donanımda yok). Programlar sayfası ilk önizlemede yoktu; ADR-009 ile eklendi (aşağıda).
3. Önizlemede fontlar Google Fonts'tan yüklenir; cihazda IBM Plex WOFF2 yerel olacak (checklist 15 açık).

## Yerel program modülü — ADR-009 (25.09.2026)

Kullanıcı isteği: haftanın günleri, belirli tarihler, belirli saatler, belirli sıcaklıklar ve süreli programlar. Tasarım: [PROGRAMS.md](PROGRAMS.md), karar: [ADR-009](ADR/ADR-009-local-programs.md). OI-S4 kapandı.

### Çekirdek (F1b)

- `lib/core/src/cc_schedule.{h,cpp}`: takvim (Hinnant, 2000–2199), 16 program, WEEKLY / DATE_RANGE / ONCE × END_TIME / DURATION / ALL_DAY × SETPOINT / PROFILE / HEATING_OFF / VENTILATE, durumsuz değerlendirme, iki kanal (iklim, havalandırma), öncelik ONCE › DATE_RANGE › WEEKLY › geç başlayan › düşük indeks, atla (program + oluşum başlangıcı), 8 gün içinde sonraki değişim, P1–P9 doğrulaması.
- `ClimateCore`: `setClock(valid, epoch_utc)` (`setTimeInfo` yerine; yerel gün dönümü buradan), `setPrograms` (yalnız yerel kaynak), `setProgramsEnabled`, `holdProgram`, `clearHold`; komutlar `programs_enabled`, `program_hold`; olaylar `PROGRAM_START/END/HOLD`, `PROGRAMS_CHANGED`; `ProfileActive::PROGRAM`, `SetpointSource::PROGRAM`; HEATING_OFF → AUTO'da yalnız antifreeze talebi; VENTILATE → havalandırma kaynağı `SCHEDULED`; limit düşürülürken programları bozan konfigürasyon reddedilir (P8 / V5 eşi).
- Öncelik zinciri: BOOST › açık profil › yerel program › Suite `sched_away` › `sched_night` › DAY. Antifreeze ve Safety üstte.
- Testler: `test/native/test_schedule` 13 test (takvim, pencereler, gece yarısı taşması, öncelik, kanallar, atla, sonraki değişim, doğrulama, ClimateCore entegrasyonu). Bulut ortamında g++ 13 + Unity ile **14 süit / 175 test geçti**, ASan/UBSan temiz. `pio test -e native` kullanıcı makinesinde çalıştırılmadı (PlatformIO registry sandbox'ta erişilemez).

### UI (F4 önizleme)

- Menüye **Programlar** (`/programs`, takvim ikonu) eklendi; menü 9 öğe, ≤1000 px ve telefonda 3×3.
- Sayfa: Şu an (iklim/havalandırma programı, bitiş, sonraki değişim, etkin hedef, “Etkin programı atla”), Modül anahtarı (`programs_enabled`), haftalık SVG zaman çizelgesi (şimdi çizgisi, etkin oluşum vurgusu, havalandırma alt şerit), program kartları (düzenle/duraklat/sil), düzenleyici diyaloğu (tekrar türü, günler + hızlı seçim, tarih, bitiş türü, süre, eylem, hedef/profil, canlı özet, istemci doğrulaması; sunucu hatası `code` + `index` ile Türkçe gösterilir).
- Genel Bakış profil satırı ve Kontrol › Profiller'de etkin yerel program; Suite satırları “Suite gece / Suite uzakta” diye adlandırıldı.
- Sahte cihaz: çekirdek değerlendirmesinin JS eşi, 7 örnek program, `/api/programs` GET/POST, program olayları, HEATING_OFF ve VENTILATE etkisi. Önizleme saati Cuma 25.09.2026 06:40 (UTC+3).
- Playwright: 2 tema × 1280/390 px, 9 sayfa, yatay taşma ve JS hatası yok; program ekleme (gün yok → hata, 35 °C → hata, geçerli → kaydedildi), atla ve Genel Bakış akışı denendi.

### Faz planına etkisi

| Faz | İş |
|---|---|
| F1b | Çekirdek + testler — **tamam** |
| F2 | Saat kaynağı (NTP/RTC) ve `TIME_INVALID`; checklist 16 F2 öncesine çekildi |
| F3 | `programs.json` atomik kalıcılık, `programs_enabled` ve atlama kaydının persist'i |
| F4 | `/api/programs` GET/POST, `/api/cmd` kimlikleri, sayfa — UI önizlemede hazır |
| F5 | MQTT: `programs_enabled` (switch), `program_active`, `program_until` (sensor), `program_hold` (button) |
| F8 | HIL: gün dönümü, saat düzeltmesi, kesinti sonrası doğru program |
