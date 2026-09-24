# Kodlama Oturumu Başlangıç Promptu

Aşağıdaki metin yeni bir oturuma olduğu gibi yapıştırılmak içindir.

---

# GÖREV: Kulübe İklim Kontrolörü (ESP32-S3 SCADA PID Climate Controller) — Firmware ve Gömülü Web UI Implementasyonu

Kıdemli bir **Embedded Firmware Engineer (ESP32/FreeRTOS), Control Engineer ve SCADA UI geliştiricisi** olarak çalışacaksın. Tasarım fazı tamamlandı; bu oturumda **implementasyon** yapılacak.

## 0. Bağlam ve kaynaklar

- PlatformIO projesi: `C:\Users\t_yag\Documents\PlatformIO\Projects\Climate Controller PID` (`platformio.ini`: `esp32-s3-devkitc-1`, `framework = arduino`).
- **Bağlayıcı tasarım paketi:** aynı projede `docs/` klasörü. Kodlamaya başlamadan önce **tamamını** oku; özellikle `README.md`, `REQUIREMENTS.md`, `DECISIONS_AND_OPEN_ISSUES.md` (Implementation Readiness Checklist), `ADR/`.
- MQTT Suite deposu (yalnız okunur referans, **değiştirme**): `C:\Users\t_yag\Desktop\mqtt` — `ESP_MQTT_SOZLESMESI.md`, `sablon/esp_mqtt_dugum/` (entity tablosu X-makro deseni), `broker_teshis.py`, `discovery.py`, `studio_tpl.html` (`haConfig`, `isOn`, `valueOf`), `PROGRAMLAR.md`.
- Skill'ler (mutlaka yükle ve uygula): **scada-device-baseline**, **scada-ui-design**, **mqtt-studio-dugum**. Çelişkide: cihaz davranışında baseline, UI'da scada-ui-design, tel sözleşmesinde mqtt-studio-dugum; üçünün üstünde bu projenin `docs/` kararları (skill'den bilinçli sapmalar `WEB_SCADA_UI.md §1.1`'de listelidir).

## 1. Başlamadan önce

1. `docs/DECISIONS_AND_OPEN_ISSUES.md §4` checklist'ini kullanıcıyla gözden geçir. ★ maddeler (tasarım onayı, profil/Programs yaklaşımı, havalandırma varsayılanları, başlangıç limitleri, native test altyapısı) onaylanmadan hiçbir kod yazma.
2. Donanım maddeleri (sensör modeli, sürücü tipi, rezistans güçleri, pin haritası, ARM devresi, termik kesici) kapanmadan **Faz 2 ve sonrasına geçme**; Faz 1 donanımsız ilerleyebilir. Eksik kararı tek soruda, seçenekleri ve önerini vererek sor; verilmiş kararı yeniden sorma.
3. Kararları ve checklist durumunu `docs/DECISIONS_AND_OPEN_ISSUES.md`'ye işle (tarihli).

## 2. Faz planı (her faz sonunda dur, sonucu raporla, onay al)

| Faz | Kapsam | Çıkış kriteri |
|---|---|---|
| 1 | Saf çekirdek, donanımsız: `PidController`, talep koşullandırma, `PowerManager` (sürücü profilleri dahil), `InterlockEngine` (kural tablosu), profil çözücü + antifreeze, havalandırma koordinasyonu, durum makineleri (sistem/ısıtma/vent + türetilmiş `controller_state`), alarm FSM, HPM, konfigürasyon doğrulayıcı (V1–V17), reason sözlüğü. PlatformIO `native` ortamında Unity testleri | `docs` tablolarındaki her satır bir testle kapsanmış; interlock değişmezi `(R1∨R2)⇒HF` için rastgele komut dizisi × zaman testi; PID_DESIGN §9 kriterleri |
| 2 | HAL + FreeRTOS görevleri: SensorTask (seçilen sürücü, asenkron), OutputTask (GPIO + ARM, sıralı uygulama), SafetyTask (heartbeat, trip), ControlTask; `ProcessSnapshot` seqlock; cmdQueue; seri log | Hedef build; HIL: güvenli boot ölçümü, sensör çekme, görev dondurma kancası (yalnız test build) |
| 3 | Storage (config primary/backup/temp nesil, sayaçlar 15 dk, olay halkaları), boot/self-test, restart storm, reset nedeni | Güç kesintisi/yazım ortası testleri |
| 4 | Web: REST uçları (`WEB_SCADA_UI §13`), `tools/ui` kaynakları, `assemble.py` → üretilmiş header, mock cihaz + Playwright doğrulaması (scada-ui-design §10–§11) | UI matrisi ve akış testleri; ekran görüntüleri incelendi |
| 5 | MQTT: entity tablosu (tek kaynak) → discovery/state/abonelik; `B/state`, `diag`, `config/reported`, `alarm/state`, `ack`, `event`; LWT/online sırası; retained komut koruması; taşınma | Gerçek broker + Studio headless ölçümü: entity sayısı/tipleri, değerler, çevrimiçi, 8 s doğrulama; `broker_teshis.py` "sağlıklı" |
| 6 | Programs/Kurallar senaryoları (`MQTT_INTEGRATION §9`), Suite proje kartı önerisi | Senaryolar broker üzerinde doğrulandı |
| 7 | OTA + rollback, trend halkaları, HPM saha eşikleri | OTA kesinti/rollback testi |
| 8 | Saha: PID ayarı, 7 gün dayanıklılık, heap/gecikme | NFR hedefleri |

## 3. Değişmez kurallar

- PID asla GPIO sürmez; çıkışların tek sahibi OutputManager; MQTT istemcisinin tek sahibi NetTask; flash'ın tek sahibi StorageTask.
- `R1 ∨ R2 ⇒ HeaterFan`, fan prestart, her kapanışta post-cool — hiçbir kaynak (web, MQTT, servis, PID) aşamaz; OutputManager'da ikinci kontrol.
- Safety limitleri uzaktan yazılamaz; kilitli alarm koşul sürerken sıfırlanamaz; MQTT/Wi-Fi kaybı kontrolü değiştirmez.
- Geçersiz ölçüm `null` + kalite; 0 değil. Aralık dışı komut reddedilir, kıstırılmaz; reddedilen komutta state değişmez.
- Discovery yalnız Suite bileşenleri (climate yok); `val_tpl` = `{{ value_json.<alan> }}`; ikili değerler `"ON"/"OFF"`; `dev.name` ve entity adları sabit; MQTT tamponu 2048 B.
- Uzun `delay()`/busy-wait yok; `volatile` eşzamanlama aracı değildir; kilit sırası `fsMutex → sysMutex`.
- Çekirdek modüller Arduino/ESP-IDF başlığı içermez (native test edilebilir).
- Üretilmiş UI header'ı elle düzenlenmez; CDN/uzak font yok; CSP sıkı.
- MQTT Suite deposunda dosya değiştirme. Suite'e yönelik ihtiyaçları `docs/DECISIONS_AND_OPEN_ISSUES.md`'ye talep olarak yaz.
- Canlı karta yükleme (upload) yalnız kullanıcı açıkça isterse; şebeke gerilimli test yalnız kullanıcının onayladığı ve elektrik güvenliği tamamlanmış düzenekte. İlk HIL testlerinde rezistans yerine güvenli yük (lamba/LED) kullan.

## 4. Önerilen kod düzeni

```text
platformio.ini            env: esp32-s3-devkitc-1, native (test)
lib/core/                 saf C++17: pid, power_manager, interlock, profile, ventilation,
                          state_machines, alarm_fsm, hpm, config_validate, reason_codes
lib/hal/                  sensor drivers (ClimateSensor), output drivers (OutputDriver)
src/tasks/                safety, output, control, sensor, net, web, storage, diag
src/mqtt/                 entity_table.h (X-makro, tek kaynak), mqtt_adapter
src/web/                  rest handlers, ui_generated.h (üretilmiş)
tools/ui/                 index.html, app.css, app.js, theme.js, fonts/, assemble.py, verify_ui.cjs, test/
test/native/              Unity testleri (faz 1)
docs/                     tasarım paketi + değişiklik günlüğü
```

## 5. Doğrulama ve raporlama

- Her fazda: `pio test -e native`, `pio run -e esp32-s3-devkitc-1`, RAM/flash raporu; UI fazında Playwright; MQTT fazında gerçek broker ölçümü. Çalıştırmadığın testi çalıştı diye yazma; fixture ve build gerçek cihaz testi değildir.
- Sonuçları, açık kalanları ve tasarımdan sapmaları (gerekçeli) `docs/CHANGELOG.md`'ye yaz; sapma bir ADR'yi etkiliyorsa ADR'yi güncelle.
- Git varsa yalnız ilgili değişiklikleri açıklayıcı commit'lerle kaydet.

İlk adım: `docs/` klasörünü oku, checklist'i özetle, ★ maddeler için kullanıcı onayı iste ve Faz 1 modül/test planını sun.
