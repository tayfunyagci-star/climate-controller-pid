# Kararlar, Açık Konular ve Implementation Readiness

## 1. Yönetici özeti

1. **Genel mimari:** Ağdan bağımsız saf proses çekirdeği (Sensor → Climate → PID → heat_demand → PowerManager → Interlock/Safety → Output) + ayrı servisler (Web, MQTT, Storage, OTA). FreeRTOS'ta Safety en yüksek öncelikli bağımsız görev; GPIO, MQTT istemcisi ve flash'ın her birinin tek sahibi var. Üç ortogonal durum makinesi (sistem, ısıtma zinciri, havalandırma) + deterministik tek `controller_state`.
2. **Kritik safety/interlock kararları:** `R1∨R2 ⇒ HeaterFan` tablo tabanlı interlock + OutputManager'da ikinci kontrol; fan prestart 3 s ve her kapanışta post-cool; rezistans sürme yolu kanal GPIO ∧ `HEATER_ARM` (∧ önerilen dinamik sinyal); safety limitleri uzaktan yazılamaz; kilitli alarmlar reboot'la kaybolmaz; MQTT/Wi-Fi kaybı kontrolü etkilemez; bağımsız termik kesici/sigorta/RCD/izolasyon/PE zorunlu.
3. **PID/Power Manager:** AUTO ve PID birleşik (algoritma `pid_mode`, varsayılan PI); koşullu entegrasyon + geri hesaplamalı anti-windup, deadband, min talep, slew, rampa, bumpless. PowerManager kademeli zaman-oransal (0–50 % R1, 50–100 % R1 + R2), 55/45 % histerezis, sürücü profiline göre pencere (SSR 20 s / röle 600 s), lider rotasyonu.
4. **Web SCADA:** scada-ui-design standardı; 8 sayfa (Genel Bakış, Kontrol, Trendler, Çıkışlar, Alarmlar, Olaylar, Ayarlar, Oturum), requested/effective/neden gösterimi, 1 s polling, cihaz RAM'inde 1 sa + 24 sa trend halkası, servis işlemleri kırmızı alanda PIN korumalı.
5. **MQTT Suite entegrasyonu:** `ESP_MQTT_SOZLESMESI.md` aynen; `B = mqttsuite/climate/<slug>`; düz JSON, `"ON"/"OFF"`, `null` geçersiz; yalnız Suite'in tanıdığı bileşenler (climate yok); switch state = istek, binary = etkin; profil istek anahtarlarıyla Programs uyumu; `B/ack` ret nedenleri.
6. **Suite uyumsuzlukları:** `climate` bileşeni yok (M1), ad tabanlı entity kimliği (M2), TÜKETİM yalnız L/gal ve tek metrik (M3), 8 s doğrulama ile etkin/istek farkı (M4), cihaz alarm olayı için entity kanalı yok (M5), 1024 B tampon yetersiz (M6), Programs yalnız AÇ/KAPAT (M7), `ack` tüketilmiyor (M8), historian enum desteği doğrulanmadı (M9), buton şablon anahtarı (M10) — ayrıntı [MQTT_INTEGRATION §10](MQTT_INTEGRATION.md).
7. **Donanım öncesi açık konular:** sensör modeli ve yerleşimi, SSR/röle tipi ve anma değerleri, fan tipi/sürücüsü, T2 varlığı, ARM/dinamik sinyal devresi, fan geri bildirimi, termik kesici, kart ve pinler, RTC.
8. **Implementasyon öncesi kararlar:** §4 checklist'teki "Kodlamadan önce" maddeleri.
9. **Geliştirme sırası:** Saf çekirdek + native test → HAL/görevler/HIL güvenlik → Storage/boot → Web → MQTT → Programs/Suite kartı → OTA/trend → saha ayarı → genişlemeler ([EXPANSION_ROADMAP §2](EXPANSION_ROADMAP.md)).

## 2. Karar kaydı

| ID | Karar | Belge |
|---|---|---|
| D-01 | PID yalnız `heat_demand` üretir | ADR-001 |
| D-02 | Kademeli zaman-oransal güç yönetimi, sürücü profiline bağlı | ADR-002 |
| D-03 | Requested/effective/reason; switch state = istek | ADR-003 |
| D-04 | Ayrık entity'ler, HA `climate` yok | ADR-004 |
| D-05 | Profil çözümü: BOOST › açık seçim › AWAY › NIGHT › DAY; Programs için `sched_*` anahtarları | ADR-005 |
| D-06 | Web canlı veri HTTP polling 1 s | ADR-006 |
| D-07 | Kalıcılık ayrımı (config / sayaç / olay), sayaç 15 dk | ADR-007 |
| D-08 | Donanım ARM hattı + bağımsız donanım korumaları | ADR-008 |
| D-09 | AUTO ve PID birleşik; `pid_mode` ayarı | CONTROL_ARCHITECTURE §2 |
| D-10 | SERVICE yalnız yerel; R1/R2 doğrudan kumandası yalnız serviste | OUTPUT_AND_INTERLOCKS §8 |
| D-11 | Antifreeze tüm normal modlarda; ölçümsüz ısıtma yok | CONTROL_ARCHITECTURE §3.4 |
| D-12 | Isıtırken havalandırma varsayılan inhibit; istisnalar ayarlı | CONTROL_ARCHITECTURE §5 |
| D-13 | Aralık dışı komut reddedilir, kıstırılmaz | MQTT_INTEGRATION §6 |
| D-14 | `remote_config_enabled`, servis kanalı, `pid_remote_tuning` varsayılan kapalı | CONFIGURATION_MODEL |
| D-15 | Güç dönüşünde mod persist (AUTO) | SYSTEM_ARCHITECTURE §5 |
| D-16 | Kilitli alarmlar reboot'ta korunur | ALARM_AND_EVENTS §3 |
| D-17 | Parolasız OTA yok | CONFIGURATION_MODEL §2.10 |
| D-18 | `controller_enable=OFF` yalnız yerel | ENTITY_MODEL §1 |
| D-19 | `sched_*` istekleri `sched_timeout_h` ile kendiliğinden düşer | CONTROL_ARCHITECTURE §3.3 |
| D-20 | Boot sonrası `heater_was_on` ise post-cool | OUTPUT_AND_INTERLOCKS §6 |
| D-24 | Kurulum/kurtarma deneyimi `scada-wifi-onboarding` ilkelerine göre: kayıt ≠ bağlantı, devir (AP bağlandıktan sonra ≤ 120 s açık), deneme sayacı + sonuç + neden sınıfı API'de, sıfırlama kapsamları ayrı | [NETWORK.md §3](NETWORK.md), CHANGELOG F2.3 |
| D-23 | Ağ bağlantı yaşam döngüsü SCADA ailesiyle aynı: `SCADA_AP_<id>` kurulum AP'si, captive portal, 20 s deneme, statik → DHCP, AP + 5 dk deneme, Wi-Fi değişimi yeniden başlatmasız | [NETWORK.md](NETWORK.md) |
| D-22 | F2 donanımı: ESP32 DevKit V1 (F2.1), DHT22, T2 yok, 2 × 1000 W SSR_ZC, aktif-LOW fan röleleri, ARM yok (sapma), NTP, 4 MB bölüm tablosu | [CHANGELOG F2](CHANGELOG.md), [HIL.md](HIL.md) |
| D-21 | Yerel program modülü: WEEKLY/DATE_RANGE/ONCE, durumsuz değerlendirme, öncelik BOOST › açık profil › program › Suite | [ADR-009](ADR/ADR-009-local-programs.md), [PROGRAMS.md](PROGRAMS.md) |

## 3. Açık konular

### 3.1 Donanım

| ID | Konu | Etki | Öneri |
|---|---|---|---|
| OI-H1 | Sıcaklık/nem sensörü modeli | Sürücü, doğruluk, CRC | SHT4x (T1/RH1) |
| OI-H2 | T2 sensörü var mı | Post-cool, S2, S6 | DS18B20 prob, önerilir |
| OI-H3 | Rezistans güçleri, eşit mi | PowerManager | Etiket değeri → `heater_power_w` |
| OI-H4 | R sürücüsü: SSR_ZC / RELAY / kontaktör | Pencere, ömür | SSR_ZC + soğutucu |
| OI-H5 | Fan tipleri, sürücü, fail-on seçeneği | Post-cool güvenilirliği | Elektrik tasarımcısı |
| OI-H6 | `HEATER_ARM` ve dinamik (charge-pump) sinyal devresi | SR-04, ADR-008 | Uygulanması önerilir |
| OI-H7 | Fan / çıkış geri bildirimi (akım trafosu, RPM) | S6, S10 | FUTURE; en az HF için önerilir |
| OI-H8 | Termik kesici tipi ve eşik | L0 | Elle resetli |
| OI-H9 | Kart (ESP32-S3-DevKitC-1 varyantı, PSRAM), pin haritası, strapping | Boot güvenliği | Pin tablosu donanım belgesinde |
| OI-H10 | RTC modülü | Saat, rotasyon, günlük geçmiş | NTP yeterliyse gerek yok |
| OI-H11 | Durum LED'i / servis butonu | UI LED bölümü, kurtarma | Buton önerilir |
| OI-H12 | Muhafaza içi sensör yerleşimi | Öz ısınma | SENSOR_ARCHITECTURE §8 |

### 3.2 Yazılım / entegrasyon

| ID | Konu | Öneri |
|---|---|---|
| OI-S1 | Framework: Arduino-ESP32 üzerinde IDF bileşenleri mi, saf ESP-IDF mi | Arduino + IDF API (mevcut `platformio.ini`), httpd ve esp-mqtt IDF'den |
| OI-S2 | MQTT kütüphanesi (PubSubClient QoS0 vs esp-mqtt QoS1) | esp-mqtt |
| OI-M3 | Suite TÜKETİM birim/metrik genişletmesi | Suite geliştirme talebi |
| OI-M4 | Suite onayının cihaza aktarılması | Suite geliştirme talebi |
| OI-S3 | MANUAL modun güç dönüşü davranışı | AUTO |
| OI-S4 | Yerel haftalık program | **Kapandı** → D-21 / ADR-009 |
| OI-S5 | MQTT TLS | Seçenek olarak v1.1 |
| OI-S6 | Flash encryption / secure boot | FUTURE |
| OI-S7 | Donma riskinde sensör arızası (FROST_RISK_NO_SENSOR) için yedek strateji | İkinci sensör |
| OI-S8 | Başlangıç PID katsayıları | Saha adım cevabı |
| OI-S9 | Operatör rolü ayrı parola mı | v1 tek admin + opsiyonel operatör |

## 4. Implementation Readiness Checklist

**Kural:** "Kodlamadan önce" sütunu ✗ olan madde kapanmadan faz 2 (donanım görevleri) kodlamasına geçilmez. Faz 1 (saf çekirdek, host'ta) yalnız ★ işaretli maddeleri gerektirir.

| # | Madde | Sahip | Kodlamadan önce | Durum |
|---|---|---|---|---|
| 1 ★ | Bu tasarım paketinin kullanıcı tarafından onayı (özellikle ADR-001…008) | Kullanıcı | Zorunlu | ☑ 25.09.2026 |
| 2 ★ | Profil çözüm önceliği ve Programs yaklaşımı (ADR-005) onayı | Kullanıcı | Zorunlu | ☑ 25.09.2026 |
| 3 ★ | Havalandırma koordinasyon varsayılanları (INHIBIT, VENT_WINS) onayı | Kullanıcı | Zorunlu | ☑ 25.09.2026 |
| 4 ★ | Başlangıç limitleri (40 °C, 240 dk, 10 s stale, 4 °C frost) onayı | Kullanıcı | Zorunlu | ☑ 25.09.2026 |
| 5 | Sensör modeli seçildi (OI-H1) ve T2 kararı (OI-H2) | Kullanıcı | Zorunlu | ☑ 25.09.2026 — DHT22, T2 yok |
| 6 | R ve fan sürücü tipi, anma değerleri (OI-H4, OI-H5) | Elektrik tasarımı | Zorunlu | ◐ 25.09.2026 — SSR_ZC + aktif-LOW röle modülü; SSR/soğutucu anma değerleri elektrik tasarımında |
| 7 | Rezistans güçleri (OI-H3) | Kullanıcı | Zorunlu | ☑ 25.09.2026 — 2 × 1000 W |
| 8 | Pin haritası, polarite, pull-down'lar, strapping kontrolü (OI-H9) | Donanım | Zorunlu | ☑ 25.09.2026 — `src/app/pins.h` (DevKit V1, F2.1), şema CC-SCH-01; donanımda ölçüm HIL H1 |
| 9 | `HEATER_ARM` devresi kararı (OI-H6) | Donanım | Zorunlu | ☑ 25.09.2026 — **ARM yok** (sapma, CHANGELOG F2 #1) |
| 10 | Bağımsız termik kesici, sigorta/MCB, RCD, PE, izolasyon tasarımı (SAFETY_DESIGN §5) | Elektrikçi | Zorunlu (enerjilendirmeden önce) | ☐ |
| 11 | Framework ve MQTT kütüphanesi (OI-S1, OI-S2) | Geliştirici | Zorunlu | ☑ 25.09.2026 — Arduino-ESP32 2.0.17 + IDF, esp-mqtt |
| 12 ★ | Native test altyapısı (PlatformIO `native` ortamı) | Geliştirici | Zorunlu | ◐ 25.09.2026 — `[env:native]` + 13 Unity paketi eklendi; `pio test -e native` kullanıcı makinesinde doğrulanacak |
| 13 | Flash bölüm tablosu (OTA×2, LittleFS, NVS, coredump payı) | Geliştirici | Zorunlu | ☑ 25.09.2026 — `partitions_4mb_ota.csv` (F2.1) |
| 14 | Suite test ortamı: broker, Studio, `broker_teshis.py` erişimi | Kullanıcı | Faz 5 öncesi | ☐ |
| 15 | Web UI font/ikon varlıkları (IBM Plex WOFF2 + OFL) | Geliştirici | Faz 4 öncesi | ☐ |
| 16 | Saat kaynağı kararı (NTP sunucusu / RTC) (OI-H10) — yerel programlar için zorunlu (ADR-009) | Kullanıcı | **Faz 2 öncesi** | ☑ 25.09.2026 — NTP (pool.ntp.org + ağ geçidi) |
| 17 | HIL test düzeneği (rezistans yerine güvenli yük/lamba, sensör simülasyonu) | Geliştirici | Faz 2 öncesi | ◐ 25.09.2026 — [HIL.md](HIL.md), HIL imajında `sim`/`hang`; uygulama bekliyor |
| 18 | Suite geliştirme talepleri kaydı (M3, M4, M8) | Kullanıcı | Hayır | ☐ |

## 5. Gelecek geliştirmeler (özet)

T2 zorunlu hâle getirme, akım/RPM geri bildirimi, dış sıcaklık ve PID ileri besleme, otomatik PID ayarı, program önceden ısıtma (optimum start), WebSocket, MQTT TLS, imzalı OTA, core dump, HMAC denetim kaydı, çoklu bölge, soğutma/ısı pompası, nemlendirme, CO₂/VOC havalandırma — bkz. [EXPANSION_ROADMAP.md](EXPANSION_ROADMAP.md).

## 5. Karar günlüğü (implementasyon)

| Tarih | Karar | Not |
|---|---|---|
| 25.09.2026 | ★ 1, 2, 3, 4 onaylandı; F1 başlatıldı | Kullanıcı onayı ("Devam") |
| 25.09.2026 | F1 çekirdek + native testler tamamlandı | Ayrıntı ve sapmalar: [CHANGELOG.md](CHANGELOG.md) |
| 25.09.2026 | S7 yorumu onaylandı: `max_continuous_heating_min` sayacı yalnız talep doyumdayken birikir | CHANGELOG F1 madde 1; SAFETY_DESIGN §3 S7 notu |
| 25.09.2026 | F4 UI önizlemesi (sahte cihaz) hazırlandı | Kullanıcı isteği; CHANGELOG “F4 önizleme” |
| 25.09.2026 | Yerel program modülü eklendi (D-21, ADR-009); çekirdek F1b'de uygulandı, UI önizlemede | Kullanıcı isteği; checklist 16 F2 öncesine çekildi |
| 25.09.2026 | F2 donanım kararları onaylandı (D-22), ARM hattı olmaması sapma olarak kabul edildi; F2 uygulandı | Kullanıcı onayı; CHANGELOG F2 |
| 25.09.2026 | Hedef kart ESP32 DevKit V1 olarak değişti; pin haritası ve 4 MB bölüm tablosu yenilendi | Kullanıcı kararı; CHANGELOG F2.1 |
| 25.09.2026 | AP kurulum ve bağlantı senaryoları aile standardına alındı (D-23); web sunucusunun kurulum/veri/komut kısmı F4'ten öne çekildi | Kullanıcı isteği; CHANGELOG F2.2 |
| 25.09.2026 | Firmware ilk kurulum ve Wi-Fi kurtarma akışı `scada-wifi-onboarding`'e göre yeniden tasarlandı ve uygulandı (D-24) | Kullanıcı isteği; CHANGELOG F2.3 |
