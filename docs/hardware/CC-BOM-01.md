# CC-BOM-01 · Malzeme Listesi (BOM)

Durum: rev A · 28.10.2026 · donanım F2.1 (ESP32 DevKit V1) + F2.4 (WS2812B durum şeridi) · şema [CC-SCH-01 rev B](CC-SCH-01.html) · pin haritası `src/app/pins.h`

Excel sürümü: [CC-BOM-01.xlsx](CC-BOM-01.xlsx). Bu belgeyle aynı içeriktedir; ayrıca fiyat, tedarikçi ve durum alanlı sipariş listesi ile formüllü güç bütçesi içerir. `python3 tools/bom/build_xlsx.py` ile üretilir; bu belge değişince betikteki satırlar da güncellenir.

Referanslar ve değerler CC-SCH-01 rev B'den alınmıştır. Şemada çizili olmayan WS2812B şerit katı (§1.2) `pins.h` ve `cc_ledstrip.h`'ye göre eklenmiştir; bu kattaki referanslar öneridir. Pano ve kurulum malzemeleri (§3) şema notlarından ve [SAFETY_DESIGN §5](../SAFETY_DESIGN.md)'ten türetilmiştir; adetleri pano yerleşimine göre değişir.

> **Şebeke gerilimi.** §2 ve §3'teki parçalar 230 V tarafındadır. Kurulum ve enerjilendirme yetkili elektrikçi tarafından yapılır; RCBO, STB ve PE bağlantıları ölçülmeden enerji verilmez (checklist 10). ARM hattı olmadığından SSR kısa devre arızasını yalnız F3 sınırlar.

## 1. SELV mantık kartı (CC-SCH-01 Sayfa 1)

### 1.1 Şemadaki parçalar

| Ref. | Parça / değer | Kılıf / anma | Görev | Adet |
|---|---|---|---|---|
| U1 | Mean Well IRM-10-5 | PCB modül, 5 V 2 A 10 W | AC/DC besleme, çift izolasyonlu (Class II); girişi Sayfa 2 F1 sonrası | 1 |
| U2 | ESP32 DevKit V1 (DOIT, 30 pin) | ESP32-WROOM-32, 4 MB flash, PSRAM yok | MCU; kart üstü AMS1117 3.3 V, USB-UART, BOOT (GPIO0), mavi LED (GPIO2) | 1 |
| U3 | 2 kanal 5 V röle modülü | Optokuplörlü, JD-VCC jumper'lı; röle SRD-05VDC-SL-C, kontak 10 A 250 VAC | HF (IN1, GPIO32) ve VF (IN2, GPIO33) fanları, aktif-LOW. **Jumper sökülü:** VCC = 3.3 V, JD-VCC = 5 V | 1 |
| B1 | DHT22 / AM2302 | 3.3–5 V, tek hat | T1 + RH1 sensörü, GPIO4 | 1 |
| Q1, Q2 | BC337-40 | NPN, TO-92, 45 V 800 mA | SSR1/SSR2 low-side sürücü | 2 |
| D1 | SS34 | Schottky 3 A 40 V, SMA | USB bağlıyken kart 5 V hattından U1'e geri beslemeyi engeller. Delikli kartta eşdeğer: 1N5822 | 1 |
| R1 | 4.7 kΩ | ¼ W %1 | DHT22 DATA pull-up → 3.3 V | 1 |
| R2 | 100 Ω | ¼ W | DATA seri koruma | 1 |
| R3, R5 | 1 kΩ | ¼ W | Q1/Q2 taban direnci | 2 |
| R4, R6 | 10 kΩ | ¼ W | Taban pull-down: reset/boot anında SSR kapalı | 2 |
| R7, R8 | 10 kΩ | ¼ W | Röle IN pull-up → 3.3 V: reset/boot anında röle bırakılmış | 2 |
| C1 | 470 µF 16 V | Elektrolitik, 105 °C | +5V tamponu (D1 çıkışı) | 1 |
| C2, C4, C5 | 100 nF 50 V | Seramik X7R | C2 +5V; C4 U3 VCC pinine yakın; C5 **sensör ucunda**, B1 VCC–GND | 3 |
| C3 | 10 µF 16 V | Seramik X5R veya tantal | U3 VCC dekuplajı | 1 |
| J1 | 4 pin klemens / JST-XH | 1 VCC · 2 DATA · 3 NC · 4 GND | DHT22 kablosu | 1 |
| J2, J3 | 2 pin klemens | 5.08 mm | SSR1/SSR2 girişi (A1+ / A2−) | 2 |

### 1.2 WS2812B durum şeridi (F2.4; CC-SCH-01 rev B'de çizili değil)

| Ref. | Parça / değer | Kılıf / anma | Görev | Adet |
|---|---|---|---|---|
| LS1 | WS2812B şerit, 6 LED | 5 V; 60 LED/m şeritten 6 LED'lik parça veya 6 adet WS2812B modül zinciri | LED1 durum · LED2 ağ · LED3 MQTT · LED4 mDNS · LED5 ısıtma · LED6 fan | 1 |
| U4 | 74AHCT1G125 | SOT-23-5. Delikli kartta eşdeğer: 74HCT125N veya 74AHCT125N (DIP-14); 74HC125 olmaz | 3.3 V → 5 V seviye çevirici (önerilir); OE → GND, VCC = +5V | 1 |
| R13 | 330 Ω | ¼ W | Veri hattı seri direnci: U4 çıkışı (U4 yoksa GPIO27) → R13 → DIN; şeride yakın | 1 |
| C8 | 470 µF 16 V | Elektrolitik | Şerit girişinde +5V–GND | 1 |
| C9 | 100 nF 50 V | Seramik X7R | Şerit girişinde +5V–GND | 1 |
| C10 | 100 nF 50 V | Seramik X7R | U4 VCC dekuplajı (yalnız U4 varsa) | 1 |
| J4 | 3 pin klemens / JST-XH | +5V · DIN · GND | Şerit bağlantısı | 1 |

U4 olmadan da çoğu şerit çalışır, ancak 3.3 V veri seviyesi WS2812B'nin VIH = 0.7 · VDD sınırındadır (`pins.h`). Delikli kartta 74HCT125N veya 74AHCT125N kullanılır; ikisinin bacak düzeni aynıdır, HCT daha kolay bulunur. Kullanılmayan üç kapının A girişleri GND'ye, OE'leri +5V'a bağlanır. T'siz 74HC125 olmaz: 5 V'ta giriş eşiği 3.5 V'tur, 3.3 V'u güvenilir okumaz. HCT/AHCT'de eşik 2.0 V'tur ve +5V hattının ≈ 4.6 V olması (4.5–5.5 V aralığında) sorun değildir.

### 1.3 Kart montajı

| Kalem | Özellik | Adet |
|---|---|---|
| Delikli pertinaks / prototip kartı | Çift yüz, ör. 9 × 15 cm; U1'in şebeke pinleri ile SELV arasında ≥ 6 mm açıklık (CC-SCH-01 not 4) | 1 |
| Dişi pin header 1 × 15, 2.54 mm | DevKit V1 soketi | 2 |
| M3 ara parça + vida/somun | Kart montajı | 4 |

## 2. Şebeke tarafı (CC-SCH-01 Sayfa 2)

| Ref. | Parça / değer | Kılıf / anma | Görev | Adet |
|---|---|---|---|---|
| X1 | Giriş klemensi L / N / PE | DIN ray, 2.5 mm²; PE yeşil-sarı | Şebeke girişi | 1 takım (3 kutup) |
| QF1 | RCBO 1P+N C16, 30 mA, tip A | DIN ray | Ana aşırı akım + kaçak akım koruması; sonrası toplam ≈ 9.2 A | 1 |
| F1 | Sigorta T 1 A 5×20 mm + DIN ray yuvası | 250 V | U1 beslemesi | 1 |
| F2 | MCB 1P C10 | DIN ray | Rezistans dalı (2 × 4.35 A) | 1 |
| F3 | STB güvenlik termostatı | Elle resetli, 16 A 250 VAC, ≈ 70 °C; kapiler hazneli veya elle resetli bimetal (KSD301 tipi, 16 A) | Rezistans dalına seri, yazılımdan bağımsız. Algılayıcı ısıtıcı çıkış havasında (CC-SCH-01 not 7) | 1 |
| F4 | Sigorta T 2 A 5×20 mm + DIN ray yuvası | 250 V | Fan dalı | 1 |
| SSR1, SSR2 | Sıfır geçişli SSR 25 A | Giriş 3–32 VDC, çıkış ≥ 240 VAC; ör. Crydom D2425, Carlo Gavazzi RM1A23D25 | R1/R2 rezistans anahtarı (`SSR_ZC`) | 2 |
| — | SSR soğutucusu | Rth ≤ 3 K/W, termal macun veya ped ile | SSR başına ≈ 5–6 W kayıp (4.35 A); rezistans hava akışının dışında | 2 |
| RV1, RV2 | S14K275 varistör | 275 VAC, 14 mm disk (TDK/EPCOS SIOV) | SSR çıkışına paralel aşırı gerilim koruması | 2 |
| R11, R12 | 100 Ω | 1 W, darbe dayanımlı | Fan RC snubber direnci | 2 |
| C6, C7 | 100 nF X2 | 275 VAC, MKP | Fan RC snubber kondansatörü | 2 |
| E1, E2 | Rezistans 1000 W 230 V | Ör. kanatlı boru rezistans; metal gövde PE terminalli | R1 / R2 ısıtıcı (4.35 A) | 2 |
| M1 | Fan 230 VAC | Rezistans üzerinden üfleme; tip ve debi açık konu (OI-H5) | Heater Fan (HF), U3 K1 kontağı | 1 |
| M2 | Fan 230 VAC | Kulübe havalandırması | Ventilation Fan (VF), U3 K2 kontağı | 1 |

Sayfa 2'deki K1/K2, U3 modülünün röleleridir; ayrı parça değildir. M1 + M2 toplam akımı F4 (T 2 A) ve röle kontağı (10 A, endüktif yükte daha düşük) altında kalmalıdır.

**SSR seçimi:** Piyasadaki "SSR-25DA" etiketli ucuz kopyalarda iç triyak anma değerinin altında olabilir. Marka/distribütör kaynaklı SSR kullanın; SSR'nin tipik arıza modu kısa devredir ve bu donanımda ARM hattı yoktur.

## 3. Pano ve kurulum malzemeleri (şemada yok)

| Kalem | Özellik | Adet |
|---|---|---|
| Pano | Metal, PE terminalli, en az IP54, DIN ray montaj plakalı; SSR soğutucuları ve mantık kartı için yer (ör. 300 × 400 × 150 mm) | 1 |
| DIN ray | TS35 | ≈ 1 m |
| PE barası | ≥ 6 çıkış; pano, E1/E2 ve M1/M2 gövdeleri tek baraya yıldız bağlanır | 1 |
| Ray klemensi | 2.5 mm² geçiş (gri/mavi) ve PE (yeşil-sarı); E1, E2, M1, M2 çıkışları (L/N/PE) ve N dağıtımı, köprü elemanlarıyla | ≈ 12–16 |
| Kablo rakoru | M20 (şebeke, rezistans, fan), M16 (DHT22) | ≈ 6 |
| Kablo, 1.5 mm² | H05VV-F / H07V-K; giriş ve rezistans dalı (CC-SCH-01 not 5) | gerektiği kadar |
| Kablo, 0.75 mm² | Fan dalı | gerektiği kadar |
| Kablo, 0.25–0.5 mm² | SELV iç bağlantılar | gerektiği kadar |
| Sensör kablosu | 4 damarlı, ekranlı; 3.3 V beslemede ≤ 5 m (daha uzunsa CC-SCH-01 not 3) | 1 |
| Kablo yüksüğü | 0.5 / 0.75 / 1.5 / 2.5 mm² | 1 takım |
| Yalıtkan bariyer | Şebeke ile SELV arasında ≥ 6 mm açıklık; SSR giriş/çıkış ve röle kontak tarafı ayrı klemens bloklarında | gerektiği kadar |
| Yedek sigorta | T 1 A ve T 2 A, 5×20 mm | 2 + 2 |
| Micro-USB kablo | Programlama ve seri konsol (115200) | 1 |

## 4. HIL test malzemesi (sahaya gitmez)

| Ref. | Parça / değer | Görev | Adet |
|---|---|---|---|
| R9, R10 | 1 kΩ ¼ W | HIL-1: J2/J3 yerine +5V → R → LED → Q1/Q2 kollektörü | 2 |
| LED1, LED2 | 3 mm LED | HIL-1 SSR komut göstergesi (WS2812B LED1–LED6 ile karıştırılmasın) | 2 |
| — | Akkor veya halojen lamba 25–60 W 230 V + duy | HIL-2 düşük güçlü yük ([HIL.md](../HIL.md)). LED lamba kullanılmaz: SSR kaçak akımı kapalıyken de yakar | 2 |

## 5. Sipariş özeti

Üretim için, değer bazında toplam (§4 hariç):

| Grup | Değer | Referanslar | Adet |
|---|---|---|---|
| Direnç ¼ W | 100 Ω | R2 | 1 |
| | 330 Ω | R13 | 1 |
| | 1 kΩ | R3, R5 | 2 |
| | 4.7 kΩ %1 | R1 | 1 |
| | 10 kΩ | R4, R6, R7, R8 | 4 |
| Direnç 1 W | 100 Ω | R11, R12 | 2 |
| Kondansatör | 100 nF 50 V X7R | C2, C4, C5, C9, C10 | 5 |
| | 10 µF 16 V | C3 | 1 |
| | 470 µF 16 V elektrolitik | C1, C8 | 2 |
| | 100 nF X2 275 VAC | C6, C7 | 2 |
| Yarı iletken | BC337-40 | Q1, Q2 | 2 |
| | SS34 (veya 1N5822) | D1 | 1 |
| | 74AHCT1G125 (veya 74HCT125N / 74AHCT125N) | U4 | 1 |
| | S14K275 | RV1, RV2 | 2 |
| Klemens | 2 pin | J2, J3 | 2 |
| | 3 pin | J4 | 1 |
| | 4 pin | J1 | 1 |
| Modül | IRM-10-5, ESP32 DevKit V1, 2 kanal röle modülü, DHT22, WS2812B şerit (6 LED) | U1, U2, U3, B1, LS1 | 1'er |
| Şebeke | RCBO C16 30 mA A, MCB C10, STB, 2 × sigorta yuvası, 2 × SSR 25 A + soğutucu | QF1, F2, F3, F1/F4, SSR1/SSR2 | §2 |
| Yük | 2 × 1000 W rezistans, 2 × 230 VAC fan | E1, E2, M1, M2 | §2 |

İsteğe bağlı yedek: 2 × 470 Ω ¼ W. U3'teki opto seri direnci 3.3 V sürüşte röleyi çektirmeye yetmezse 1 kΩ yerine takılır (CC-SCH-01 not 1).

## 6. +5V güç bütçesi (U1: 2 A)

| Tüketici | Akım |
|---|---|
| U2 ESP32 DevKit V1 (Wi-Fi gönderim tepesi, AMS1117 üzerinden) | ≈ 250 mA |
| U3 röle bobinleri (SRD-05VDC, 2 × ≈ 72 mA) | ≈ 150 mA |
| SSR girişleri (2 × ≤ 15 mA) | ≤ 30 mA |
| LS1 WS2812B (6 × 60 mA, beyaz %100) | ≤ 360 mA; varsayılan %20 parlaklıkta ≈ 72 mA |
| U3 opto, DHT22, pull-up'lar | ≈ 10 mA |
| **Toplam (en kötü durum)** | **≈ 0.8 A** (U1 anmasının ≈ %40'ı) |

D1 üzerinde ≈ 0.4 V düşüm olur ve +5V hattı ≈ 4.6 V'ta kalır. WS2812B (3.5–5.3 V), SRD-05VDC (çekme ≤ 3.75 V) ve AMS1117 (3.3 V çıkış için ≈ 1 V düşüm payı) bu gerilimde çalışır. HIL-1'de VIN pini yük altında ölçülmelidir (≥ 4.4 V).

## 7. CC-SCH-01 rev B ile farklar

1. WS2812B şerit katı (LS1, U4, R13, C8–C10, J4) şemada çizili değil. F2.4'te `pins.h`'e eklendi; GPIO27 kullanılır. Şema rev C'de çizilmeli.
2. D1 (SS34) ve U4 (74AHCT1G125) yüzey montajlıdır. Kart delikli pertinaksa 1N5822 ve 74HCT125N (veya 74AHCT125N) kullanılır.
3. Sayfa 2'deki PE notu "U1 FG" diyor; ancak IRM-10-5 Class II modüldür ve FG pini yoktur. U1'in PE'ye bağlanacak ucu yok.
4. DHT22 kablo sınırı [HIL.md](../HIL.md)'de ≤ 20 m, şema notunda (3.3 V beslemede) ≤ 5 m olarak geçiyor. Bu listede şema notu esas alınmıştır.

## 8. v1 kapsamı dışında kalanlar

Pinleri ayrılmış, parçası bu listeye dahil edilmemiş genişlemeler:

| Genişleme | Pin | Örnek parça | Açık konu |
|---|---|---|---|
| HEATER_ARM hattı (+ dinamik güvenlik sinyali) | GPIO13 | Lojik VE / ikinci transistör katı | OI-H6, [ADR-008](../ADR/ADR-008-hardware-arm-line.md) |
| T2 hava çıkış sensörü | GPIO18 (1-Wire) | DS18B20 su geçirmez prob + 4.7 kΩ | OI-H2 |
| RTC | GPIO21/22 (I²C) | DS3231 modül | OI-H10 (v1: NTP) |
| Fan geri bildirimi | — | Akım trafosu veya RPM çıkışlı fan | OI-H7 |
