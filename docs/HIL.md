# F2 Donanımlı Test (HIL) Prosedürü

Durum: F2.1 · 25.09.2026 · hedef ESP32 DevKit V1 · şema [hardware/CC-SCH-01.html](hardware/CC-SCH-01.html) · **Karta yükleme yalnız kullanıcının açık talimatıyla.** İlk HIL'de şebeke bağlanmaz: SSR girişlerine LED (seri direnç) takılır, röle modülünün kontak tarafı boştur.

## 1. Düzenek

| Hat | Bağlantı (HIL-1) | Not |
|---|---|---|
| GPIO25 → R1 sürücü | NPN + SSR girişi **veya** 1 kΩ + LED | Tabanda 10 kΩ pull-down |
| GPIO26 → R2 sürücü | aynı | |
| GPIO32 → HF röle IN | Röle modülü (JD-VCC 5 V, VCC 3.3 V, jumper sökülü) | IN'de 10 kΩ pull-up → 3.3 V |
| GPIO33 → VF röle IN | aynı | |
| GPIO4 ↔ DHT22 DATA | 4.7 kΩ pull-up → 3.3 V, 100 nF | Kablo ≤ 20 m |
| GPIO0 / GPIO2 | Kartın BOOT butonu / mavi durum LED'i | |
| UART0 (USB-UART, GPIO1/3) | Seri konsol 115200 | `pio device monitor` |

Şebeke tarafı (HIL-2, lamba/düşük güçlü yük) ancak HIL-1 maddeleri geçtikten ve termik kesici/sigorta/RCD/PE (checklist 10) kurulduktan sonra yapılır.

## 2. Yükleme

```
pio test -e native            # önce: 16 paket / 189 test
pio run -e esp32dev-hil        # HIL imajı (sim/hang komutları)
pio run -e esp32dev-hil -t upload     # yalnız açık talimatla
pio device monitor
```

Üretim imajı (`esp32dev`) `sim` ve `hang` komutlarını içermez.

## 3. Test listesi

| # | Adım | Beklenen | Gözlem |
|---|---|---|---|
| H1 | Güç ver, reset anında osiloskop/LED ile R1/R2/HF/VF hatlarını izle | Hiçbir LED/röle yanıp sönmez (pull dirençleri + ilk iş pasif yazım) | ☐ |
| H2 | `status` | `sys=RUN` (≤ 10 s), `T1` gerçek oda sıcaklığı, `DHT22 son=OK`, hata sayısı artmıyor | ☐ |
| H3 | `wifi <ssid> <parola>` → 30 s sonra `status` | `wifi=bagli`, `saat=gecerli`; parola hiçbir çıktıda görünmez | ☐ |
| H4 | `sim 18` (SP 21) | HF → ≥ 3 s sonra R1; talep > 55 % ise R2; olaylarda HEATING_START, STAGE2_ON | ☐ |
| H5 | `sim 23` | R1/R2 kapanır, HF 60 s post-cool, sonra OFF; `POST_COOL_START/END` | ☐ |
| H6 | Isıtırken `set heater_fan_manual OFF` | `OVERRIDDEN (HEATER_INTERLOCK)`, HF açık kalır | ☐ |
| H7 | Isıtırken DHT22 kablosunu çek (sim off iken) | ≤ 10 s içinde R OFF, `FAILSAFE SENSOR_FAULT`, HF post-cool, T1 `nan` | ☐ |
| H8 | `sim 45` 10 s | OVERTEMPERATURE kilidi, R OFF, HF+VF zorlanır; `sim 20` sonrası `reset` ile çözülür, koşul sürerken `reset` reddedilir | ☐ |
| H9 | Isıtırken `hang control` | ≤ 7 s içinde `INTERNAL_FAULT`, R OFF, HF post-cool | ☐ |
| H10 | Isıtırken `hang output` | ≤ 1 s içinde `ACIL: OUTPUT_TASK_STALL`, R hatları LOW, yeniden başlatma; boot'ta `reset=SOFTWARE hatali_boot=1`, HF boot post-cool | ☐ |
| H11 | Isıtırken `hang safety` | ≤ 5 s içinde TWDT paniği, reset; R hattı bu sürede eski seviyede kalabilir (ARM yok — artık risk) | ☐ |
| H12 | 5 kez `hang output` art arda | 5. boot'ta `RECOVERY` (restart fırtınası), ısıtma kilitli; `recovery` komutuyla çıkılır | ☐ |
| H13 | `service on`, `test 0 on` | Servis testi yalnız interlock'la: HF prestart sonra R1; `test 1 on` aynı anda reddedilir | ☐ |
| H14 | Wi-Fi erişim noktasını kapat | Kontrol etkilenmez; `WIFI_OFFLINE` alarmı; saat geçerli kalır | ☐ |
| H15 | 24 sa çalıştır, `status` | Heap düşüşü yok, `kilit zaman asimi=0`, görev azami süreleri < 5 ms, DHT hata oranı < % 1 | ☐ |
| H16 | NVS boş kart (`pio run -t erase` sonrası) ilk açılış | `SCADA_AP_<id>` yayında, LED yavaş; telefonla bağlanınca kurulum sayfası kendiliğinden açılır (açmazsa 192.168.4.1) | ☐ |
| H17 | Kurulum sayfası → **Wi-Fi ağı seç** → ağ + parola → **Kaydet ve bağlan** | Yeniden başlatma yok; pencere üç aşamayı cihaz verisiyle ilerletir; “Cihaz Wi-Fi ağına bağlandı” + IP/mDNS + kapanışa kalan süre; **Kurulumu bitir** ile AP kapanır (dokunulmazsa 120 s); olaylarda `NET_WIFI_CHANGED`, `NET_CONNECTED`, `NET_AP_OFF` | ☐ |
| H18 | Yanlış parola kaydet | ≤ 20 s sonra “bağlanamadı” + `AUTH` sınıfı metni (konsolda neden kodu, beklenen 15/202/204); AP açık kalır; **Parolayı yeniden gir** çalışır; 5 dk sonra arka plan denemesi | ☐ |
| H19 | Router'ı kapat (cihaz bağlıyken), 2 dk sonra aç | 15 s sonra deneme, başarısızsa AP açılır; router dönünce en geç 5 dk içinde bağlanır, AP kapanır; kontrol hiç etkilenmez | ☐ |
| H20 | Ayarlar › Ağ: statik IP'yi başka alt ağa ayarla, kaydet | Statik deneme 20 s → DHCP ile bağlanır; genel uyarı “Statik IP ile bağlanılamadı; DHCP ile alınan adres …” | ☐ |
| H21 | Bakım › **Wi-Fi bilgilerini sil**; ayrıca BOOT butonu 10 s | Onay metni kapsamı ve kurulum ağı adresini verir; her ikisinde yeniden başlatmadan AP açılır; sayfada kalıcı yönerge, “veri bayat” alarm yağmuru yok; ısıtma/fan çıkışları değişmez | ☐ |
| H22 | `otapass <parola>`, `ota`, `platformio.ini` espota satırlarını aç, `pio run -t upload` | Hazırlıksız yükleme iptal + hazırlık; ısıtma durup post-cool bitince yükleme kabul edilir; yeni imaj açılır | ☐ |
| H23 | 5 GHz-yalnız / olmayan ağ adını “Ağım görünmüyor” ile gir | `NOT_FOUND` metni; AP açık kalır | ☐ |
| H24 | Normal ağdan (Bakım) başka ağa geç | Sayfa bağlantısı kesilince tek “Ağ değişikliği sürüyor” notu; pencerede yeni adres + kurulum ağı dönüş yönergesi; yeni ağda cihaz açılır | ☐ |
| H25 | Kayıtlı ağ kapalıyken açılış → AP (kurtarma) kartı; router'ı aç → **Kayıtlı ağı şimdi dene** | Kart “kayıtlı ağa bağlanamadı” + sonraki deneme süresi; deneme başarılı, telefon AP'deyken devir uygulanır | ☐ |
| H26 | Kurulum ağında kaydet; STA farklı kanaldaysa telefonun AP bağlantısını izle | Kısa kopma olursa sayfa sakin not gösterir, telefon AP'ye dönünce sonucu alır; dönmezse “Kurulum tamamlandı” yönergeleri geçerli | ☐ |

Her adımın sonucu ve seri log çıktısı `docs/CHANGELOG.md` F2 bölümüne işlenir. Geçmeyen madde varsa HIL-2'ye geçilmez.

## 4. Bilinen sınırlar

- **ARM hattı yok:** SafetyTask takılırsa R hattı TWDT süresince (≤ 5 s) son seviyede kalabilir. Bağımsız, elle resetli termik kesici bu yüzden enerjilendirmeden önce zorunludur.
- **Güç kesintisi:** `heater_was_on` ve kilitli safety bitleri F2'de RTC belleğindedir; yalnız yazılım/WDT resetlerinden sağ çıkar. Güç kesintisine dayanıklı kopya F3'te (StorageTask) eklenir.
- **Web oturumu yok (F4):** AP ve LAN'daki herkes komut gönderebilir; UI “Web parolası tanımlı değil” uyarısını gösterir. Bağlantı yaşam döngüsü: [NETWORK.md](NETWORK.md).
