# Gereksinimler

Kimlik biçimi: `FR` işlevsel, `SR` güvenlik, `NFR` işlevsel olmayan, `IR` entegrasyon, `UR` arayüz. Her gereksinimin kabul kriteri (AC) aynı satırdadır veya §7'de toplanmıştır. "Zorunlu" = v1; "İsteğe bağlı" = donanım varsa; "Gelecek" = v1 dışı.

## 1. Kapsam ve varsayımlar

- **ASSUMPTION:** Tek kulübe, tek bölge; R1 ve R2 yaklaşık eşit güçte (her biri ≤ 2 kW mertebesi). Eşit olmadıkları durum `heater_power_w` ile desteklenir.
- **ASSUMPTION:** MCU ESP32-S3 (çift çekirdek, ≥ 512 KB SRAM, 8 MB flash). PSRAM zorunlu değildir.
- **ASSUMPTION:** Çıkışlarda fiziksel geri bildirim (akım, RPM, hava akışı) v1'de yoktur; durum "komutlanan çıkış" anlamındadır.
- **ASSUMPTION:** Cihaz yerel LAN'da, internet olmadan çalışır; saat kaynağı yerel NTP/RTC olabilir veya hiç olmayabilir.
- **OPEN ISSUE:** Sensör, SSR/röle, fan tipi, T2 varlığı ve kart seçilmedi ([DECISIONS_AND_OPEN_ISSUES.md](DECISIONS_AND_OPEN_ISSUES.md)).

## 2. İşlevsel gereksinimler — kontrol

| ID | Gereksinim | Öncelik | Kabul kriteri |
|---|---|---|---|
| FR-01 | T1'i `setpoint_effective` çevresinde tutan kapalı döngü kontrol | Zorunlu | Kararlı rejimde ortalama hata ≤ 0.3 °C, salınım genliği ≤ ±0.5 °C (saha ayarı sonrası ölçülür) |
| FR-02 | Kontrol algoritması P / PI / PID modlarını destekler; varsayılan PI | Zorunlu | `pid_mode` değişimi bumpless (çıkış sıçraması ≤ 2 %) |
| FR-03 | PID çıkışı yalnız `heat_demand %` üretir; GPIO'ya erişimi yoktur | Zorunlu | Kod incelemesi: kontrol modülü çıkış sürücü API'sine bağımlı değil |
| FR-04 | Power Manager talebi R1/R2 ve sürücü profiline göre çıkışlara çevirir | Zorunlu | [OUTPUT_AND_INTERLOCKS §3](OUTPUT_AND_INTERLOCKS.md) tablosuna göre simülasyon testi |
| FR-05 | Anti-windup, deadband, çıkış sınırı, minimum talep, setpoint rampası, türev filtresi, çıkış eğim sınırı | Zorunlu | Her mekanizma için birim testi ([PID_DESIGN.md](PID_DESIGN.md) §9) |
| FR-06 | Çalışma modları: `OFF`, `AUTO`, `MANUAL`, `VENT_ONLY`; `SERVICE` yalnız yerel | Zorunlu | Mod geçiş matrisi testleri |
| FR-07 | Profiller: `DAY`, `NIGHT`, `AWAY`, `FROST` + süreli `BOOST` | Zorunlu | Profil çözüm tablosu testleri ([CONTROL_ARCHITECTURE §3](CONTROL_ARCHITECTURE.md)) |
| FR-08 | Antifreeze koruması `OFF` ve `VENT_ONLY` modlarında da çalışır | Zorunlu | T1 < `frost_guard_temperature` iken ısıtma FROST setpoint'iyle başlar |
| FR-09 | Ventilation Fan sıcaklık, nem, manuel ve Suite isteğiyle çalışır; koordinasyon tablosuna uyar | Zorunlu | [CONTROL_ARCHITECTURE §5](CONTROL_ARCHITECTURE.md) tablosunun her satırı test edilir |
| FR-10 | Deterministik durum makinesi ve tek `controller_state` raporu | Zorunlu | [STATE_MACHINE.md](STATE_MACHINE.md) geçiş tablosunun tamamı testle kapsanır |
| FR-11 | Heating Performance Monitor: sıcaklık artış hızı, düşük performans, beklenmeyen artış, döngü sıklığı | Zorunlu (uyarı düzeyi) | Sentetik profillerle alarm üretimi |

## 3. Güvenlik gereksinimleri

| ID | Gereksinim | Kabul kriteri |
|---|---|---|
| SR-01 | `R1 ∨ R2 ⇒ HeaterFan` hiçbir komutla ihlal edilemez | Tüm kaynak × komut kombinasyonlarında ihlal 0 (özellik tabanlı test) |
| SR-02 | Heater Fan, rezistanstan en az `fan_prestart_s` önce çalışır | Zaman damgalı çıkış izi |
| SR-03 | Rezistanslar kapandıktan sonra POST_COOL tamamlanmadan Heater Fan kapanmaz (FAILSAFE dahil, OTA hariç değil) | Çıkış izi |
| SR-04 | Boot, reset, OTA ve konfigürasyon hatasında rezistanslar OFF; donanım ARM hattı pasif | Elektriksel ölçüm (saha) + birim testi |
| SR-05 | Sensör kaybı/bayatlığında rezistanslar ≤ `sensor_stale_s` içinde OFF | Fault injection |
| SR-06 | T1 ≥ `cabin_overtemp_limit` ⇒ rezistanslar OFF, kilit, havalandırma açılır | Fault injection |
| SR-07 | Sürekli ısıtma `max_continuous_heating_min` aşılırsa kilit | Hızlandırılmış zaman testi |
| SR-08 | Safety Manager kontrol görevinden bağımsız görevde, daha yüksek öncelikte çalışır ve kontrol görevinin heartbeat kaybında çıkışları güvenli duruma alır | Görev dondurma testi |
| SR-09 | Yazılım tek güvenlik katmanı değildir: bağımsız termik kesici, sigorta, izolasyon, topraklama, uygun sürücü şartı belgelenir | Donanım tasarım incelemesi |
| SR-10 | R1/R2 doğrudan kumandası yalnız yerel Service modunda, interlock'lar altında | Normal UI ve MQTT'de bu uç yok; servis testi |
| SR-11 | Kilitli güvenlik alarmı koşul sürerken sıfırlanamaz | Test |

## 4. Entegrasyon gereksinimleri (MQTT Suite)

| ID | Gereksinim | Kabul kriteri |
|---|---|---|
| IR-01 | Topic/discovery/availability `ESP_MQTT_SOZLESMESI.md` ile uyumlu | Studio'da tüm entity'ler keşfedilir, cihaz çevrimiçi; `broker_teshis.py` "sağlıklı" |
| IR-02 | Tüm `value_template` değerleri `{{ value_json.<alan> }}` biçiminde, düz JSON | Discovery yüklerinin regex testi |
| IR-03 | Yalnız Suite'in tanıdığı bileşenler: `sensor`, `binary_sensor`, `switch`, `number`, `select`, `button` | `discovery.parse` hiçbir kayıtta ValueError üretmez |
| IR-04 | Kabul edilen komut ≤ 1 s içinde state'e yansır (Suite 8 s penceresi) | Broker izi |
| IR-05 | Reddedilen komutta state değişmez; `B/ack` nedeni yayınlar | Test |
| IR-06 | Retained komut uygulanmaz (abonelik sonrası 2 s pencere) | Test |
| IR-07 | Programs, profil istek anahtarları üzerinden AÇ/KAPAT semantiğiyle kullanılabilir | [MQTT_INTEGRATION §9](MQTT_INTEGRATION.md) senaryoları |
| IR-08 | Discovery `name` ve `dev.name` sabittir (Suite entity kimliği ad tabanlı) | Görünen ad değişikliği discovery'yi değiştirmez |
| IR-09 | Keşif kapatıldığında retained discovery kayıtları boş yükle silinir | Test |

## 5. Arayüz gereksinimleri

| ID | Gereksinim | Kabul kriteri |
|---|---|---|
| UR-01 | Gömülü web HMI scada-ui-design standardına uyar (token, tipografi 10/11/12/14 px, iki tema) | scada-ui-design §10 matrisi |
| UR-02 | Ana ekranda T1, setpoint, nem birincil; ısıtma, havalandırma, kontrolör, sağlık grupları | Wireframe uyumu |
| UR-03 | Her çıkış requested / effective / reason gösterir | Çıkışlar sayfası ekran testleri |
| UR-04 | Veri yaşı görünür; bayat veride kumanda kapanır | 4 s stale testi |
| UR-05 | Riskli işlemler görünür metinli onay diyaloğu ister | Akış testi |
| UR-06 | MQTT bağlantısı olmadan web arayüzü tam çalışır | Broker kapalı test |
| UR-07 | Trend: 5 dk / 15 dk / 1 sa / 6 sa / 24 sa pencereleri | Trend sayfası testi |

## 6. İşlevsel olmayan gereksinimler

| ID | Gereksinim | Hedef |
|---|---|---|
| NFR-01 | Kontrol döngüsü jitter | ≤ 50 ms (2 s periyotta), ağ yükü altında ölçülür |
| NFR-02 | Safety değerlendirme periyodu | 250 ms |
| NFR-03 | Heap tabanı | 24 saat testte min heap ≥ 40 KB, düşüş eğilimi yok |
| NFR-04 | Flash aşınması | Normal çalışmada ≤ 100 yazma/gün (sayaç + olay); 10 yıl ömür hedefi |
| NFR-05 | Boot'tan kontrole geçiş | Sensör hazırsa ≤ 10 s (ağ beklenmez) |
| NFR-06 | Web yanıt süresi | `/api/data` ≤ 100 ms, 2 eşzamanlı istemci |
| NFR-07 | Offline teslim | UI varlıkları cihazdan; CDN/uzak font yok |
| NFR-08 | Güç kesintisinde kayıp | Ayar kaybı yok; sayaçlarda ≤ 15 dk birikim kaybı |
| NFR-09 | Gözlemlenebilirlik | [DIAGNOSTICS.md](DIAGNOSTICS.md) listesinin tamamı yayınlanır |

## 7. Uçtan uca kabul senaryoları

| # | Senaryo | Beklenen |
|---|---|---|
| A1 | AUTO, T1=18, SP=22 | Heater Fan ON → 3 s sonra R1 → talep > 55 % ise R2 kademesi; `controller_state=HEATING` |
| A2 | T1 setpoint'e ulaşır | Talep düşer, R2 → R1 sırayla kapanır, POST_COOL 60 s, Heater Fan OFF, `IDLE` |
| A3 | UI'dan R1 aktifken Heater Fan OFF isteği | `heater_fan_requested=OFF`, `effective=ON`, `reason=HEATER_INTERLOCK` |
| A4 | Sensör kablosu çekilir | ≤ `sensor_stale_s` içinde R1/R2 OFF, POST_COOL, `FAILSAFE/SENSOR_FAULT`, CRITICAL alarm |
| A5 | T1 > `cabin_overtemp_limit` | R1/R2 OFF, Ventilation ON (`reason=OVERTEMPERATURE`), kilit; T1 düşünce alarm `cleared_unacknowledged`, reset gerekir |
| A6 | Broker kapatılır | Kontrol sürer; `MQTT_OFFLINE` uyarısı yerel; broker dönünce tam state + discovery + online |
| A7 | Wi-Fi kesilir, 24 saat | Kontrol ve web AP/yerel erişim profili uyarınca sürer; heap sabit |
| A8 | OTA başlatılır | Rezistanslar OFF, POST_COOL tamamlanır, sonra yazım; yeni imaj self-test geçemezse rollback |
| A9 | Programs 22:00 `sched_night` ON | `profile_active=NIGHT`, `setpoint_effective=18`; 06:00 OFF → DAY |
| A10 | `ventilation_stop_temperature ≥ start` gönderilir | Komut reddedilir, `B/ack` `INVALID_RELATION`, state değişmez |
| A11 | Kontrol görevi dondurulur (test kancası) | Safety görevi ≤ 1 s içinde R1/R2 OFF, `INTERNAL_FAULT` |
| A12 | Güç kesilip gelir | Boot'ta çıkışlar OFF; ayarlar korunur; `WATCHDOG_RESET` değil `POWER_ON` reset nedeni |
