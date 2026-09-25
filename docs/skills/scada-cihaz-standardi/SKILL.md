---
name: scada-cihaz-standardi
description: "SCADA/IoT uç cihazlarının (ESP32, ESP8266, OneCH vb.) ortak standardı tek pakette: firmware mimarisi, proses güvenliği, ağ/MQTT/kalıcılık/OTA, Scale krem/lacivert/teal gömülü web arayüzü ve bölüm bölüm kaydedilen ayarlar sayfası, WS2812B durum LED şeridi (LED1 durum, LED2 ağ, LED3 MQTT, LED4 mDNS, sonrakiler cihaza özgü) ve Wi-Fi ilk kurulum/AP/kurtarma deneyimi. Yeni SCADA cihazı geliştirirken, mevcut cihazı standarda taşırken, web arayüzü/ayarlar/LED/Wi-Fi kurulum akışı tasarlarken veya gözden geçirirken kullan. scada-device-baseline, scada-ui-design ve scada-wifi-onboarding'in yerini alır."
---

# SCADA cihaz standardı

Bu paket üç eski yeteneği birleştirir ve aile genelinde yeni iki kuralı ekler: **ayarların bölüm bölüm kaydı** ve **WS2812B durum LED şeridi**. Paket tek başına taşınabilir; başka depo veya araç gerektirmez.

## Hangi belgeyi oku

| İş | Oku |
|---|---|
| Firmware mimarisi, proses/çıkış güvenliği, kalıcılık, ağ, MQTT, erişim, OTA, MCU uyarlaması, ayar kataloğu | `references/cihaz-temeli.md` |
| Web arayüzü görünümü, token'lar, tipografi, bileşenler, ayarlar sayfası iskeleti ve alan kataloğu, UI testi | `references/arayuz-tasarimi.md` |
| Durum LED şeridi: LED sırası, durum seçimi, ayar anahtarları, donanım ve sürücü | `references/durum-ledleri.md` |
| İlk kurulum, AP modu, Wi-Fi seçimi, STA bağlantısı, kurtarma ekranları ve metinleri | `references/wifi-kurulum.md` (+ `wifi-firmware-referansi.md`, `wifi-tasarim-sartnamesi.md`) |

Birden çok alana dokunan işte ilgili bütün belgeleri oku. Çelişkide: cihaz davranışında `cihaz-temeli.md`, görünüm/etkileşimde `arayuz-tasarimi.md`, LED'de `durum-ledleri.md`, kurulum akışı metinlerinde `wifi-kurulum.md` geçerlidir. Kullanıcının açık proje tercihi ve çalışan haberleşme sözleşmesi hepsinden önce gelir.

## Aile genelinde değişmez kurallar

1. **Ayarlar bölüm bölüm kaydedilir.** Her sekme kendi formu, kaydet çubuğu ve “<Bölüm> ayarlarını kaydet” düğmesidir. Kayıt yalnız o bölümün alanlarını doğrular ve gönderir; başka bölümdeki geçersiz/eksik/henüz desteklenmeyen alan (ör. broker adresi) ağ veya statik IP kaydını engelleyemez, taslağı da silinmez. Tek genel “Ayarları kaydet” düğmesi yoktur. Sunucu gövdede olmayan alanı korur ve yalnız ilgili modülü yazar.
2. **Durum LED'i WS2812B şerididir.** Sıra bütün cihazlarda sabit: LED1 Durum (alarm yoksa yeşil sabit, uyarı/alarmda yanıp söner), LED2 Ağ (yok / Wi-Fi / AP), LED3 MQTT (kesik / bağlı / tanımsız), LED4 mDNS (yok / hazır / devre dışı), LED5+ cihaza özgü fonksiyonel LED'ler. Ayarlar'da LED sekmesi canlı şerit durumu, parlaklık ve durum başına renk içerir.
3. **Cihaz adı düzenlenebilir ve görünür.** `adN` Ağ › Cihaz kimliği'ndedir; kayıtta üst başlık ve tarayıcı sekmesi anında güncellenir. SLUG/client ID gibi salt okunur kimlikler “cihaz adı” diye etiketlenmez.
4. **Kaydedildi ≠ bağlandı.** Ağ kaydı yanıtı bağlantı başarısı değildir; sonuç cihaz kanıtıyla (deneme sayacı, sonuç, IP) gösterilir.
5. Yerel proses ağdan bağımsızdır; LED, ağ ve flash işi proses bütçesini tüketmez; HTTP/MQTT handler'ı çıkışı veya LED'i doğrudan sürmez.
6. Üst çubuk ile genel uyarı çerçeveleri arasında boşluk bırakılır (12 px).
7. **OTA parolasız da çalışır; parolasız durum kalıcı uyarıdır.** Parola Ayarlar › Erişim'deki ayrı formdan tanımlanır veya onayla kaldırılır; değişiklik mümkünse yeniden başlatmadan geçerli olur. Yüklemeden önce güvenli duruş her durumda zorunludur.
8. Olmayan özelliği yapılmış gibi raporlama; yapılmayan build/test/HIL adımını açıkça yaz.

## Tamamlanma

`cihaz-temeli.md` §12, `arayuz-tasarimi.md` §12 ve `durum-ledleri.md` §5 kontrol listelerinin ilgili maddeleri işaretlenmeden iş tamamlanmış sayılmaz.
