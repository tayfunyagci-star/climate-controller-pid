# F2 Donanımlı Test (HIL) Prosedürü

Durum: F2 · 25.09.2026 · **Karta yükleme yalnız kullanıcının açık talimatıyla.** İlk HIL'de şebeke bağlanmaz: SSR girişlerine LED (seri direnç) takılır, röle modülünün kontak tarafı boştur.

## 1. Düzenek

| Hat | Bağlantı (HIL-1) | Not |
|---|---|---|
| GPIO5 → R1 sürücü | NPN/MOSFET + SSR girişi **veya** 1 kΩ + LED | Tabanda 10 kΩ pull-down |
| GPIO6 → R2 sürücü | aynı | |
| GPIO7 → HF röle IN | Röle modülü (JD-VCC 5 V, VCC 3.3 V, jumper sökülü) | IN'de 10 kΩ pull-up → 3.3 V |
| GPIO15 → VF röle IN | aynı | |
| GPIO4 ↔ DHT22 DATA | 4.7 kΩ pull-up → 3.3 V, 100 nF | Kablo ≤ 20 m |
| GPIO0 | Kartın BOOT butonu | |
| UART0 (USB-UART) | Seri konsol 115200 | `pio device monitor` |

Şebeke tarafı (HIL-2, lamba/düşük güçlü yük) ancak HIL-1 maddeleri geçtikten ve termik kesici/sigorta/RCD/PE (checklist 10) kurulduktan sonra yapılır.

## 2. Yükleme

```
pio test -e native            # önce: 16 paket / 189 test
pio run -e esp32-s3-hil       # HIL imajı (sim/hang komutları)
pio run -e esp32-s3-hil -t upload     # yalnız açık talimatla
pio device monitor
```

Üretim imajı (`esp32-s3-devkitc-1`) `sim` ve `hang` komutlarını içermez.

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

Her adımın sonucu ve seri log çıktısı `docs/CHANGELOG.md` F2 bölümüne işlenir. Geçmeyen madde varsa HIL-2'ye geçilmez.

## 4. Bilinen sınırlar

- **ARM hattı yok:** SafetyTask takılırsa R hattı TWDT süresince (≤ 5 s) son seviyede kalabilir. Bağımsız, elle resetli termik kesici bu yüzden enerjilendirmeden önce zorunludur.
- **Güç kesintisi:** `heater_was_on` ve kilitli safety bitleri F2'de RTC belleğindedir; yalnız yazılım/WDT resetlerinden sağ çıkar. Güç kesintisine dayanıklı kopya F3'te (StorageTask) eklenir.
- **Wi-Fi kimliği** F2'de seri konsoldan NVS'e yazılır; web kurulumu F4'te.
