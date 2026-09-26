# Durum LED şeridi (WS2812B) — bütün SCADA cihazları

Her SCADA cihazında durum göstergesi **WS2812B adreslenebilir LED şerididir**. Kart üstü tek renk LED (varsa) yalnız yardımcı kalp atışıdır; durum bilgisinin asıl taşıyıcısı şerittir. Şeridi olmayan eski kart, profilde `donanımda yok` olarak işaretlenir; LED sekmesi o zaman çıkarılır.

## 1. Sabit sıra

İlk dört LED ailenin ortak sözleşmesidir; sıra, anlam ve anahtarlar hiçbir cihazda değişmez. Cihaza özgü LED'ler 5'ten başlar.

| LED | Anahtar | Durum 0 | Durum 1 | Durum 2 | Varsayılan renk (0/1/2) |
|---|---|---|---|---|---|
| 1 Durum | `cls` | Normal — **sabit** | Uyarı — **yanıp söner** | Alarm / güvenli durum — **yanıp söner** | yeşil / turuncu / kırmızı |
| 2 Ağ | `clw` | Bağlantı yok | Wi-Fi (STA) bağlı | Yalnız AP kurulum modu | kırmızı / yeşil / mavi |
| 3 MQTT | `clq` | Kesik | Bağlı | Tanımsız (broker boş / MQTT kapalı) | kırmızı / yeşil / sönük |
| 4 mDNS | `clm` | Yok (başlatılmadı/başarısız) | Hazır | Devre dışı | kırmızı / yeşil / sönük |
| 5+ | cihaza özgü | Profilde tanımlı (en çok 3 durum) | | | |

Cihaza özgü örnekler: röle kartında kanal başına `clr` (Kapalı, AUTO açık, MANUEL açık); iklim kontrolöründe LED5 `clr` Isıtma (Kapalı, 1 kademe, 2 kademe), LED6 `clf` Fan (Kapalı, Isıtıcı fanı, Havalandırma).

## 2. Durum seçimi

- LED1: en yüksek etkin alarm önemi KRİTİK veya proses güvenli durumdaysa 2; UYARI ise 1; bilgi seviyesi ve alarm yoksa 0. Onaylanmış ama süren alarm yanıp sönmeye devam eder (koşul sürüyor).
- LED2: STA bağlıysa 1 (devirde AP de açık olabilir); yalnız kurulum AP'si açıksa 2; ikisi de değilse 0. Wi-Fi bağlı ≠ internet ≠ MQTT.
- LED3: MQTT istemcisi yoksa/broker tanımsızsa 2; bağlıysa 1; tanımlı ama kopuksa 0. Keşif (HA discovery) fiziksel LED değildir.
- LED4: mDNS adı kapatılabiliyorsa ve kapalıysa 2; yayında ise 1; değilse 0.
- Cihaza özgü LED'ler **uygulanan** (interlock sonrası) çıkış durumunu gösterir, istek durumunu değil.
- Yanıp sönme yalnız LED1'dedir (1 Hz, %50). Başka LED'e yanıp sönme eklemek cihaz profilinde gerekçelendirilir.

## 3. Ayarlar ve API

- Sekme `led` (Güvenlik ile Erişim arası). Paneller: **LED durumu (canlı)** → **LED parlaklığı** (`ledB`, %0–100, varsayılan düşük, ör. 20) → **LED renkleri** (grup kartları, `arayuz-tasarimi.md` §8.4 paleti).
- Renk anahtarı `<grup><0..2>` = `#rrggbb`. `GET /api/settings` bütün LED anahtarlarını ve sürücü durumunu (`ledOk`) döndürür; `POST` LED bölümünün alanlarını ayrı kaydeder (bölüm yalıtımı).
- `GET /api/data`: `led_states` (LED başına durum indeksi dizisi) ve `led_ok`. UI canlı şeridi bu indekslerle ve **kayıtlı** renklerle çizer; tema fiziksel rengi değiştirmez.
- Renkler ve parlaklık kalıcıdır (NVS/FS, ayrı ad alanı). Kayıt hatasında önceki değer kalır.

## 4. Donanım ve sürücü

- Veri hattı: GPIO → 330 Ω seri direnç → DIN. Şerit 5 V'tan beslenir; 3.3 V MCU için DIN'de 74AHCT1G125 (veya eşdeğeri) seviye çevirici önerilir. 5 V–GND arası ≥ 470 µF + 100 nF. Boot strap pinleri kullanılmaz.
- Akım bütçesi: LED başına tam beyaz ≈ 60 mA; parlaklık sınırı ve varsayılan düşük parlaklık besleme bütçesine göre seçilir.
- ESP32: RMT TX (IDF 4.4 legacy sürücüde `rmt_write_sample` + çevirmen). Çerçeve RMT belleğine sığacak kadar blok ayrılır (ör. 6 LED × 24 bit = 144 girdi → 3 blok) ki Wi-Fi kesmeleri bit zamanlamasını bozmasın. Önceki çerçeve bitmediyse yeni çerçeve atlanır. Diğer RMT kullanıcılarıyla (ör. DHT RX) kanal/blok çakışması pin haritasında yazılır.
- ESP8266: UART1 veya I2S DMA tabanlı sürücü; bit-bang yalnız kesmeler kısa süre kapatılabiliyorsa.
- LED işi proses bütçesini tüketmez: 50 ms civarı tempoda, çekirdek/proses kilidi kısa zaman aşımıyla denenir; alınamazsa son girdi kullanılır. HTTP/MQTT handler'ı şeridi doğrudan sürmez.
- Durum seçimi ve renk/parlaklık/yanıp sönme hesabı saf fonksiyondur ve native testle doğrulanır; sürücü yalnız bayt yazar.

## 5. Doğrulama

- Native: her LED'in durum tablosu, yanıp sönme fazı, parlaklık ölçeği (%0 → sönük, %100 → aynen), renk ayrıştırma ve anahtar eşlemesi.
- UI: canlı şerit, palet akışları, LED bölümü kaydının yalnız LED anahtarlarını göndermesi.
- Kartta: her durumun rengi, alarmda yanıp sönme, AP modunda LED2, reboot'ta şeridin sönük başlaması. Yapılmadıysa açık bırak.
