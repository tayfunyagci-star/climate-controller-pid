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

## F2 — HAL + FreeRTOS görevleri + seri konsol (25.09.2026)

### Donanım kararları (kullanıcı onayı 25.09.2026)

| Konu | Karar |
|---|---|
| T1/RH1 (OI-H1) | DHT22/AM2302, GPIO4, 4.7 kΩ pull-up; örnekleme 2 s |
| T2 (OI-H2) | Yok → post-cool yalnız süre (TIME); S2/S6 devre dışı |
| Rezistans (OI-H3) | 2 × 1000 W, eşit (`heater_power_w_r1/r2 = 1000`) |
| R sürücü (OI-H4) | Sıfır geçişli SSR, `SSR_ZC` profili; GPIO5/6 aktif-HIGH, NPN/MOSFET low-side, 10 kΩ pull-down |
| Fan sürücü (OI-H5) | 5 V aktif-LOW optokuplörlü röle modülü; GPIO7 (HF), GPIO15 (VF); JD-VCC 5 V / VCC 3.3 V, 10 kΩ pull-up |
| ARM hattı (OI-H6) | **Yok** — sapma 1 |
| Kart (OI-H9) | ESP32-S3-DevKitC-1 N8R8/N8R2, 8 MB flash; PSRAM kullanılmıyor; GPIO35–37 boş |
| Saat (OI-H10, checklist 16) | NTP: `pool.ntp.org` (konsoldan değiştirilebilir) + 2. sunucu ağ geçidi; RTC yok |
| Buton/LED (OI-H11) | BOOT butonu (GPIO0, yalnız giriş) + dahili WS2812 (GPIO48; v1.1 kartta GPIO38) |
| Framework / MQTT (OI-S1/S2) | Arduino-ESP32 2.0.17 (espressif32@6.9.0) + IDF API; MQTT F5'te esp-mqtt |
| Bölüm tablosu (checklist 13) | `partitions_8mb_ota.csv`: nvs 20 KB, otadata, app0/app1 3 MB, LittleFS 1.875 MB, coredump 64 KB |
| İlk HIL | SSR girişlerinde LED, şebeke bağlı değil ([HIL.md](HIL.md)) |

### Eklenenler

- `src/app/pins.h` — pin haritası ve polarite tablosu (cc::Out sırası).
- `src/app/hal_outputs.*` — önce pasif seviye yazımı, sonra çıkış yönü; her çevrimde dört pin yeniden yazılır; acil yol yalnız R1/R2'yi pasife çeker.
- `src/app/hal_dht22.*` — RMT RX (1 µs tık, 300 µs boşta eşiği) ile darbe yakalama; başlatma darbesi 2–3 ms RTOS uykusu; yakalama başlatma + hat bırakma görev geçişsiz (`vTaskSuspendAll`). Meşgul bekleme ve `delay()` yok.
- `lib/core/src/cc_dht.*` — DHT22 çerçeve çözücü (saf, native testli): son 40 bit, bit genişliği ve LOW aralığı denetimi, checksum → `CRC_ERROR`, yanıt yok → `TIMEOUT`, aralık dışı → `BUS_ERROR`; geçersiz ölçüm NaN.
- `src/app/tasks.*` — Safety (250 ms, öncelik 10), Output (100 ms, 9), Control (`control_interval_s`, 7), Sensor (≥ 2 s, 6); hepsi çekirdek 1, tek öncelik mirasçı mutex. Safety ve Output TWDT'ye abone (5 s, panik → reset), çekirdek 1 idle izleniyor.
- `ClimateCore::baseTick()` / `publish()` / `controlPeriodMs()` — `tick()` bunlara bölündü (davranış değişmedi); hedefte görevler aşamaları ayrı çağırır.
- `src/app/boot_state.*` — reset nedeni; RTC_NOINIT bloğunda `heater_was_on`, hatalı boot sayacı, kilitli safety bitleri, acil yol işareti.
- `src/app/net_clock.*` — Wi-Fi istasyonu (kimlik NVS `net` alanında, parola yazdırılmaz), SNTP; saat yalnız en az bir eşitleme + makul epoch ile geçerli.
- `src/app/console.*` — seri konsol: `status`, `watch`, `set <id> <değer>` (LOCAL_SERVICE), `ack`, `reset`, `service`, `test`, `recovery`, `wifi`, `ntp`, `reboot` (ısıtma/post-cool sürerken reddedilir); olay günlüğü akışı; durum LED'i. HIL imajında `sim`, `hang`.
- `src/app/core_api.h` — Arduino makroları (`PI`, `HIGH`, `LOW`, `DISABLED`, `OUTPUT`) çekirdek numaralandırıcılarıyla çakışıyordu; başlıklar makrolar geçici kaldırılarak içerilir. Liste Arduino-ESP32 2.0.17 makro kümesiyle çekirdek tanımlayıcılarının kesişimidir (`g++ -dM -E`). **F1'deki `src/main.cpp` bu çakışma nedeniyle hedefte derlenmiyordu; F1 raporundaki "pio run çalıştırılmadı" notu bu hatayı gizlemişti.**
- `platformio.ini` — platform sürümü sabit (`espressif32@6.9.0`), özel bölüm tablosu, `esp32-s3-hil` ortamı (`-DCC_HIL=1`).
- `test/native/test_dht` (9 test), `test/native/test_tasking` (5 test: F2 konfigürasyonu, bölünmüş görevlerle boot/ısıtma, control heartbeat, 2 s sensör kaybı, 16 tohum × 2 sa rastgele faz + gecikme altında çıkış değişmezleri).
- `docs/HIL.md` — H1–H15 donanımlı test listesi.

### Doğrulama

| Kontrol | Sonuç |
|---|---|
| Native (bulut, g++ 13 + Unity) | 16 paket / 189 test geçti |
| Aynı testler ASan/UBSan | Geçti |
| ESP32-S3 hedef derleme (bulut, arduino-cli + Arduino-ESP32 2.0.17 + xtensa-esp32s3 gcc 8.4.0, `-Wall -Wextra`) | Üretim ve HIL imajı uyarısız derlendi. Üretim: flash 740 625 B, statik RAM 58 464 B |
| Bölüm tablosu (`gen_esp32part.py --flash-size 8MB`) | Geçerli |
| `pio test -e native`, `pio run` (PlatformIO) | **Çalıştırılmadı** (registry sandbox'ta erişilemez); kullanıcı makinesinde doğrulanacak |
| HIL | **Yapılmadı** — karta yükleme açık talimat bekliyor |

### Tasarımdan sapmalar

1. **HEATER_ARM hattı yok (SR-04, ADR-008 karşılanmıyor) — kullanıcı kararı.** Tek GPIO'nun takılı kalması rezistansı açık tutabilir. Telafi: (a) OutputTask 1 s çalışmazsa veya SafetyTask çekirdek kilidini 1 s alamazsa acil yol R hatlarını doğrudan pasife çeker ve yeniden başlatır (GPIO tek sahiplik kuralının belgeli tek istisnası); (b) SafetyTask takılırsa TWDT ≤ 5 s içinde panik reset → pinler yüksek empedans → pull-down SSR'yi kapatır; (c) elle resetli bağımsız termik kesici enerjilendirmeden önce zorunlu. Artık risk: SafetyTask donduğunda ≤ 5 s. GPIO17 ileride ARM için ayrıldı.
2. **Görevlere gerçek dt verilir.** Geç kalan görev yetişme patlaması yapmaz; aşamaya son çalışmadan beri geçen süre verilir. Sabit periyot + yetişme çağrıları, post-cool'u fiziksel olarak gecikme kadar kısaltıyordu (native `test_tasking` bu hatayı yakaladı). SYSTEM_ARCHITECTURE §4'e not düşüldü.
3. **ProcessSnapshot seqlock yerine kilit altında kopya.** F2 okuyucusu yalnız konsol; web/MQTT (F4/F5) için seqlock çift tampon ertelendi.
4. **Wi-Fi kimliği ve NTP sunucusu F2'de NVS'e doğrudan yazılıyor** (StorageTask tek sahipliği F3'te). NVS, config dosyalarından ayrı alan (`net`).
5. **Sensör sürücüsü konfigürasyonda seçilmiyor;** DHT22 derleme zamanında sabit. `sensor_interval_s < 2` ise SensorTask 2 s uygular; F3 doğrulayıcısına "DHT22 ⇒ ≥ 2 s" kuralı eklenecek.
6. **Güç kesintisine dayanıklılık:** `heater_was_on`, hatalı boot sayacı ve kilitli safety bitleri F2'de RTC belleğinde (yalnız yazılım/WDT resetleri). Restart fırtınası penceresi yaklaşık: RUN'da 30 dk kesintisiz çalışma sayacı sıfırlar.

### Açık kalanlar (F2)

- Kullanıcı makinesinde `pio test -e native`, `pio run -e esp32-s3-devkitc-1`, `pio run -e esp32-s3-hil`.
- HIL H1–H15 (açık talimatla yükleme sonrası).
- Checklist 10 (termik kesici, sigorta/MCB, RCD, PE) — HIL-2 ve enerjilendirme öncesi.

## F2.1 — Hedef kart: ESP32 DevKit V1 (25.09.2026)

Kullanıcı kararı: elde ESP32 DevKit V1 (DOIT, ESP32-WROOM-32, 4 MB flash, PSRAM yok) var; hedef ESP32-S3-DevKitC-1'den buna alındı. Çekirdek ve görev modeli değişmedi (ESP32 de çift çekirdek; görevler çekirdek 1'de).

| Konu | ESP32-S3 (F2) | ESP32 DevKit V1 (F2.1) | Gerekçe |
|---|---|---|---|
| DHT22 | GPIO4 | GPIO4 | Boot etkisi yok |
| R1 / R2 (aktif-HIGH) | GPIO5 / GPIO6 | **GPIO25 / GPIO26** | Klasik ESP32'de GPIO5 strapping (boot'ta PWM), GPIO6 flash hattı |
| HF / VF (aktif-LOW) | GPIO7 / GPIO15 | **GPIO32 / GPIO33** | GPIO7 flash hattı, GPIO15 strapping (boot'ta PWM) |
| Durum LED'i | WS2812 GPIO48 (RMT) | **Mavi LED GPIO2** (tek renk, desenli) | DevKit V1'de RGB LED yok; GPIO2 strapping, yalnız boot sonrası sürülür |
| BOOT butonu | GPIO0 | GPIO0 | |
| Yedek ARM / I²C / 1-Wire | 17 / 8-9 / 16 | **13 / 21-22 / 18** | GPIO14 boot'ta PWM üretir |
| Konsol | UART0 GPIO43/44 | UART0 GPIO1/3 | |
| Flash / bölümler | 8 MB, app 3 MB × 2 | **4 MB**: nvs 20 KB, otadata, app 1.75 MB × 2, LittleFS 384 KB, coredump 64 KB (`partitions_4mb_ota.csv`) | |
| PlatformIO | `esp32-s3-devkitc-1`, `esp32-s3-hil` | **`esp32dev`**, **`esp32dev-hil`** (board `esp32doit-devkit-v1`) | |

Kaçınılan pinler: strapping 0/2/5/12/15 (0 yalnız buton girişi, 2 yalnız LED), flash 6–11, UART0 1/3, yalnız giriş 34–39, boot'ta PWM üreten 14.

### Doğrulama

| Kontrol | Sonuç |
|---|---|
| ESP32 hedef derleme (bulut, arduino-cli + Arduino-ESP32 2.0.17 + xtensa-esp32 gcc 8.4.0, `-Wall -Wextra`), üretim ve HIL | Uyarısız. Üretim: flash 784 733 B (app bölümünün % 43'ü), statik RAM 58 856 B |
| Bölüm tablosu (`gen_esp32part.py --flash-size 4MB`) | Geçerli |
| Native testler | Değişmedi (çekirdek dokunulmadı) — 16 paket / 189 test |
| Devre şeması | CC-SCH-01 rev B (DevKit V1 pinleri, VIN girişi, AMS1117, GPIO2 LED) |
| `pio run` / HIL | **Yapılmadı** — kullanıcı makinesinde ve açık talimatla |

### Not

- 4 MB flash'ta F4 web varlıkları (≈ 33 KB gzip UI + ≈ 105 KB font) LittleFS yerine firmware içine gömülür (baseline §11); LittleFS 384 KB yalnız config/olay/program dosyaları içindir.

## F2.2 — AP kurulum ve Wi-Fi bağlantı senaryoları (SCADA ailesi) (25.09.2026)

Kullanıcı isteği: ilk açılışta AP modu, sonra Wi-Fi bağlantısı; bağlantı senaryoları diğer SCADA projeleriyle aynı davranış ve aynı arayüzlerle; `platformio.ini`'de OTA parametreleri yorum olarak. Ayrıntı: [NETWORK.md](NETWORK.md) (D-23).

### Referans alınan uygulamalar (salt okuma)

- `4chRelayModule/include/network.h` (`wifiTick`, `beginWifiAttempt`, `startAP`) ve `web_api.h` (`/scan {pending, networks}`, `/api/reset-wifi`, Wi-Fi diyaloğu → `/api/settings {ssid, pass}`).
- `Flowmeter ESP32/src/main.cpp` (`SCADA_AP_<chipId>`, `12345678`, 192.168.4.1, statik → DHCP düşüşü, 5 dk Auto-Recovery, captive yönlendirme) ve `tools/ui` AP kurulum paneli.

### Eklenenler

- `lib/core/src/cc_netfsm.*` — bağlantı yaşam döngüsü durum makinesi (saf) + `parseIpv4`, `validStaticIpv4`; `test/native/test_netfsm` 9 test.
- `src/app/net_manager.*` — NetTask (çekirdek 0, öncelik 3): FSM, AP + captive DNS, STA (statik/DHCP), mDNS, ArduinoOTA (parolalı), SNTP; ayarlar NVS `net` alanında. `net_clock.*` kaldırıldı.
- `src/app/web.*` — WebServer: gzip UI varlıkları (CSP, immutable önbellek), `/api/data`, `/api/cmd`, `/scan`, `/api/settings` (GET tüm konfigürasyon; POST Ağ bölümü + Wi-Fi), `/api/reset-wifi`, `/api/reboot`, `/api/events`, `/api/alarms` (+ack/reset), `/api/programs` (GET), captive yönlendirme; kalan uçlar `501` (F4).
- `tools/ui/assemble.py` → `include/ui_generated.h` (gzip 9, mtime 0, `?v=` içerik özeti; 165 KB → 48.6 KB); `tools/prebuild.py` PlatformIO ön derleme adımı.
- UI: Genel Bakış'ta **AP · KURULUM MODU** paneli, aile standardı Wi-Fi diyaloğu (tarama, sinyal, kilit, açık ağ, yeniden tara), Bakım › “Kablosuz bağlantıyı değiştir”, genel uyarılar (AP modu, statik → DHCP notu). Ağ bölümünden `ap_policy` alanı kaldırıldı (davranış aile standardında sabit). Sahte cihaza AP modu, `/scan`, Wi-Fi kaydı, Wi-Fi silme eklendi.
- Konsol: `wifi` / `ntp` web ile aynı doğrulama yolundan; `otapass`, `ota`; durum satırında ağ fazı, AP adı, OTA durumu. BOOT butonu 10 s → Wi-Fi sil + AP. Boştayken AP'de LED yavaş yanıp söner.
- Çekirdek: olay kodları `NET_AP_ON/OFF`, `NET_CONNECTED`, `NET_DISCONNECTED`, `NET_DHCP_FALLBACK`, `NET_WIFI_CHANGED`, `NET_WIFI_CLEARED`, `OTA_START`, `OTA_FAIL`; `noteEvent()`, `guard()`, `serviceTestOn()` erişimcileri.
- `platformio.ini`: `espressif32@6.13.0` (Flowmeter ESP32 ile aynı), `lib_deps ArduinoJson 7.4.3`, `extra_scripts`, espota parametreleri yorum satırı olarak.

### Doğrulama

| Kontrol | Sonuç |
|---|---|
| Native (bulut, g++ 13 + Unity) | 17 paket / 198 test geçti; ASan/UBSan temiz |
| ESP32 hedef derleme (arduino-cli, Arduino-ESP32 2.0.17, ArduinoJson 7.4.3, `-Wall -Wextra`) | Üretim ve HIL imajı uyarısız; üretim flash 963 473 B (1.75 MB app bölümünün % 52'si), statik RAM 69 796 B |
| UI (Playwright, 2 tema × 1280/390 px) | AP paneli, tarama (bekleme → liste, 5 GHz ve yinelenen ağ elenir), kısa parola reddi, kaydet → AP kapanır, Bakım sayfası; yatay taşma ve JS hatası yok |
| `pio run`, HIL H16–H22 | **Yapılmadı** — kullanıcı makinesinde ve açık talimatla |

### Tasarımdan sapmalar

1. **AP adı ve politikası aile standardına alındı (D-23).** SYSTEM_ARCHITECTURE §6'daki `KulubeIklim-XXXX` ve CONFIGURATION_MODEL'deki `ap_policy` (FIRST_SETUP_ONLY / ON_WIFI_FAIL) yerine `SCADA_AP_<id>`, ortak parola ve “bağlanamazsa AP + 5 dk deneme” davranışı. Kullanıcı kararı: diğer SCADA projeleriyle aynı.
2. **AP açıkken Genel Bakış gizlenmez.** Referans UI AP modunda gösterge panelini gizler; bu cihazda AP, Wi-Fi kaybında da açıldığı ve kontrol sürdüğü için panel üstte, proses göstergeleri altta kalır.
3. **Wi-Fi değişimi yeniden başlatmadan uygulanır** (4chRelayModule ile aynı; Flowmeter yeniden başlatır). Kontrol görevleri etkilenmez.
4. **OTA parolasız açılmaz** (D-17) — 4chRelayModule'ün parolasız varsayılanı alınmadı.
5. **Oturum yok (F4).** Yazma uçları yalnız `X-SCADA` başlığıyla korunur; SECURITY §2'deki parola/oturum F4'te.
6. Ağ ayarları NVS'e doğrudan yazılır (StorageTask tek sahipliği F3'te; F2 sapma 4'ün devamı).
