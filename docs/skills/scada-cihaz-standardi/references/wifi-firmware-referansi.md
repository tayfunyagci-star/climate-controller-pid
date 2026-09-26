# OneCH SCADA — kaynak incelemesi referansı

İnceleme tarihi: 2026-09-25. Sürüm: v6.2.0 kaynak tabanı. Bu kayıt çalıştırılmış test veya donanım doğrulaması değildir. Başka projeye uygularken değerleri yeniden doğrula.

## Dosya ve fonksiyon haritası

| Kaynak | İlgili sorumluluk |
|---|---|
| `src/main.cpp`: `setup`, `networkTask` | Donanım/FS açılışı, STA/AP seçimi, HTTP başlangıcı |
| `startAPMode`, `startStationMode`, `wifiTryConnect` | AP oluşturma, STA bekleme, statik IP/DHCP yolu |
| `handleScan`, `handleSetWifi` | Ağ listesi, doğrulama, atomik kayıt, reboot planı |
| `handleResetWifi`, `handleReboot`, `handleFactoryReset` | Birbirinden farklı sıfırlama kapsamları |
| `handleApiData`, `setupRoutes` | UI veri sözleşmesi ve yetki sınırları |
| `tools/ui/app.js`: `scanWifi`, `wifi-form` submit | Tarama polling'i, seçim, kaydetme bildirimi |
| `tools/ui/index.html`: `ap-setup`, `wifi-dialog` | Dashboard üstündeki AP kartı ve Wi-Fi dialogu |
| `tools/ui/app.css` | Lacivert/teal kimlik, iki tema, kompakt tipografi |

## Doğrulanan davranışlar

1. Açılışta LittleFS otomatik format edilmez. Ayarlar yüklenir; kayıtlı vana polaritesine göre kapalı çıkış uygulanır. ISR akış darbelerini sayar; Core 1 proses kontrolünü, Core 0 ağ/HTTP/MQTT ve deferred kayıt işlerini yürütür. Ağın ilk bağlantı beklemesi Core 0'dadır; bu sırada proses döngüsü ayrı çalışır.
2. SSID boşsa STA denenmeden AP açılır: `SCADA_AP_<ID>`, fabrika AP parolası `12345678`, varsayılan kurulum IP'si `192.168.4.1`. AP başlatma bir kez yeniden denenir; yine başarısızsa alarm üretilir. Tarayıcı AP hiç açılmadığında bu alarmı gösteremez; USB/seri teşhis ayrı destek yoludur.
3. Normal ağ adresi özel mDNS adı veya `scada-<ID>.local` olur. HTTP sunucusu STA/AP başlatıldıktan sonra dinlemeye alınır.
4. `/scan` asenkron tarama sonucu ya da `status: scanning` döndürür. UI yaklaşık 700 ms aralıkla en fazla 20 sorgu yapar; istek süreleri toplam bekleme süresini etkiler. Sonuçlarda SSID, RSSI ve secure vardır. UI boş SSID'leri eler, SSID+güvenlik türüne göre tekilleştirir, en güçlü RSSI'ye göre sıralar. Mevcut API bant/kanal döndürmez; donanım 2.4 GHz'dir.
5. `/api/set-wifi`: SSID 1–32 bayt; parola boş veya 8–64 bayt. Arduino String uzunluğu bayt ölçer; tarayıcı maxlength ile UTF-8 bayt sınırı aynı değildir. Genel 64 uzunluk kontrolü, her 64 karakterli parolanın ağ yığını tarafından geçerli kabul edildiği garantisi değildir.
6. Ağ bilgileri önce settings adayına yazılır ve atomik LittleFS kaydı denenir. Başarısızlık HTTP 507 üretir, önceki ağ korunur. Başarıda RAM güncellenir, `otaActive=true` olur ve yaklaşık 1 sn sonrası için reboot planlanır. API kayıt onayı döndürür; bağlantı denemesi reboot sonrasındadır.
7. Yeni ağ seçimi mevcut statik IP tercihini korur. STA bağlantısı 20 sn denenir. Statik IP etkin ve bağlantı başarısızsa DHCP ile ek 20 sn deneme yapılır. Bu yol, Wi-Fi bağlantısı kurulmasına rağmen yanlış IP nedeniyle cihaz erişilemiyorsa mutlaka devreye girecek bir erişilebilirlik testi değildir.
8. STA başarısızsa AP açılır. Yeni kaydedilmiş SSID/parola tutulur; eski ağa otomatik rollback yoktur.
9. AP'de kayıtlı SSID varsa 5 dk sonra AP+STA üzerinden arka plan denemesi yapılır. Başarıda yalnız STA'ya geçilir, AP kapanır; ağ servisleri ve NTP başlatılır. 20 sn başarısızlıkta AP'ye dönülür, sonraki deneme için 5 dk beklenir. SSID yoksa bu döngü başlamaz. Bunu bütün çalışma zamanı Wi-Fi kopmalarının otomatik AP'ye dönüş garantisi olarak genelleme.
10. Wi-Fi sıfırlama SSID/parolayı siler, statik IP kullanımını kapatır ve reboot planlar. Diğer ayarlar korunur. Reboot ve fabrika sıfırlama ayrı endpoint'lerdir; fabrika sıfırlama çok daha geniş veri kaybı yaratır.
11. Web parolası tanımlıysa ağ tarama/değiştirme korumalıdır. Misafir okuma tercihi dashboard erişimini etkileyebilir. AP parolası, web parolası ve hedef Wi-Fi parolası birbirinden ayrıdır.
12. AP HTTP bilinmeyen yolları köke yönlendirebilir; kaynak incelemesinde DNS captive portal servisi görülmedi. Otomatik portal açılması garanti değildir.
13. `otaActive`/depolama duruşu proses kontrolünde çıkışı kapatma isteği üretir. AP modu tek başına vana kilidi değildir. Vana durumu yazılım çıkış komutudur; fiziksel kontak geri bildirimi yoktur.

## Mevcut arayüzün sınırları

- AP kurulum kartı kontrol sayfasının üstündedir; bağımsız adımlı kurulum deneyimi yoktur.
- Wi-Fi dialogunda tek seçenekli 2.4 GHz seçicisi, ağ listesi, parola alanı ve açık ağ kutusu vardır.
- Ağ seçildiğinde açık ağ bilgisi form durumuna otomatik yansıtılmaz; parola alanı yeniden zorunlu olur.
- Kaydetme sonrası dialog kapanır ve yaklaşık 5 sn'lik toast gösterilir. Kalıcı reboot/ağ geçiş ekranı ve gerçek STA başarı doğrulaması yoktur.
- Kaydetme isteği 6 sn içinde yanıtlanmazsa sonuç belirsiz olabilir; otomatik tekrar yazma doğru değildir.
- `is_ap_mode` ilk kurulum/kurtarma nedenini ayırt etmez. `apName`, `mdns`, `ip`, `wifi` gibi mevcut alanlar kullanılabilir; kesin hata nedeni ve kurtarma geri sayımı varsayılmamalıdır.
- Yeni adrese geçişte eski origin'deki oturum/yerel depolama paylaşılmayabilir; yönetim işlemleri için yeniden oturum gerekebilir.

## Tarihsel görsel profil

Lacivert koyu yüzeyler, teal vurgu; açık temada krem/beyaz yüzeyler. IBM Plex Sans gövde ve IBM Plex Mono etiketler. Kompakt metin ölçeği 10/11/12 px, en büyük başlık 14 px; grafik sayıları istisnadır. Mobil alanların 12 px olması iOS yakınlaştırmasına neden olabilir. Daha büyük kurulum formu metni önerisini açık tasarım istisnası olarak sun.
