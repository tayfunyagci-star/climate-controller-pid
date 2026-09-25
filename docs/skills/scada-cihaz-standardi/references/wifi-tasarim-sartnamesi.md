# Kurulum ekranları ve UI/UX şartnamesi

Bu belge hedef tasarımı tarif eder; burada önerilen ekranlar mevcut firmware'de uygulanmış kabul edilmez.

## Deneyim amacı

Yerinde kurulum yapan operatöre her aşamada cihazın ne yaptığı, kullanıcının ne yapacağı ve sorun halinde nasıl döneceği açık olmalı. Mobil öncelikli, masaüstünde dar içerik paneli olan bir akış kullan. Normal kontrol paneli kurulum eyleminin önüne geçmesin.

## A — Kurulum ağına erişim

Henüz cihaz sayfasını açamayan kişiye gereken bilgiyi yalnız o sayfaya koyma. Ürün etiketi veya kısa başlangıç kartı tasarımına da dahil et:

- AP adı, fabrika kurulum parolası, `http://192.168.4.1`.
- “Bu ağda internet bağlantısı olmaması normaldir. Kuruluma devam etmek için bağlı kalın.”
- Tarayıcı sayfası açılmazsa adresi elle yazma; telefon başka ağa geçtiyse kurulum ağına geri bağlanma.
- Tarayıcının telefonun Wi-Fi ayarını değiştirmesini veya portalın kendiliğinden açılmasını vaat etme.

## B — Kurulum başlangıcı

Üstte cihaz adı/kimliği, tema düğmesi, “Kurulum ağına bağlı” rozeti. Başlık: “Cihazınızı Wi-Fi ağına bağlayın.” Birincil eylem “Wi-Fi ağı seç”; ikincil “Cihaz panelini aç”. Paneli açmak kurulum tamamlandı sayılmaz. AP nedenini bilmiyorsan “İlk açılış” deme. Yetki gerekirse web oturum ekranına anlaşılır geçiş ver; Wi-Fi parolasını giriş parolası gibi isteme.

## C — Ağ tarama ve seçme

- “Yalnızca 2.4 GHz ağlar desteklenir.” Sabit bilgi kullan; tek seçenekli bant seçicisi kullanma.
- Tarama sırasında belirsiz süreli yükleniyor göstergesi, “Ağlar aranıyor…” metni ve tekrar taramayı engelleyen durum.
- Ağ satırı: SSID, sinyal ikonu + metin, açık/parolalı etiketi. RSSI ikincil ayrıntı olsun.
- En güçlü sinyal üstte; aynı SSID ve güvenlik türü tek satır. Seçili satırı onay ikonu ile belirt.
- “Yeniden tara”, “Ağım görünmüyor”. Boş liste, zaman aşımı, oturum gereksinimi ve iletişim kopmasını ayrı göster.
- Yardımda 2.4 GHz, mesafe ve yeniden tarama yönergeleri. Elle SSID girişini önerirsen UI geliştirmesi olarak işaretle; gizli ağ desteğini firmware üzerinde doğrula.

## D — Ağ bilgileri

Seçili SSID ve “Başka ağ seç”. Parolalı ağda etiketli parola alanı, göster/gizle düğmesi ve alan yanında hata. Açık ağda algılanan durumu forma uygula; “Bu ağ parola istemiyor” de. Kullanıcıya güvenlik türünü gereksiz yere tekrar seçtirme.

UTF-8 bayt sınırını karakter sınırıyla karıştırma. SSID/parolayı sessizce kırpma veya baş/son boşluklarını otomatik silme. Parolayı kalıcı depolama; dialog kapanınca temizle.

Statik IP etkinse “Mevcut sabit IP ayarları bu ağda da kullanılacak.” bilgisini ver. Ayrıntıları açılır alana koy; ilk kurulumda gelişmiş ağ ayarlarını zorunlu yapma.

Birincil eylem: **“Kaydet ve yeniden başlat”**.

Eylem öncesi: “Cihaz yeniden başlatılacak ve seçtiğiniz ağa bağlanmayı deneyecek. Bu sırada bağlantınız kesilecek.” Reboot sırasında sulama çıkışına etkisini kısa ve doğru anlat; fiziksel kapanma onayı verme.

## E — Kaydetme ve geçiş

Kaydetmeden önce seçilen ağı, mDNS adresini, AP adı/adresini ve telefonun hedef ağa dönmesi gerektiğini göster. Böylece cihazın hızlı reboot'u yönergeleri okumayı engellemez.

Gönderimde eylemi devre dışı bırak. HTTP 400 için alan hatası; 401 için oturum; 507 için kalıcı kayıt hatası sun. 409 yalnız gerçekten backend tarafından döndürülüyorsa cihaz meşgul durumu olarak kullan.

Kayıt onayında kalıcı geçiş kartı:

**Ayarlar kaydedildi**

“Cihaz yeniden başlatılıyor. Telefonunuzu seçtiğiniz Wi-Fi ağına bağlayın, ardından cihaz panelini açın.”

Üç aşama: Ayarlar kaydedildi → Cihaz yeniden başlatılıyor → Bağlantı doğrulaması bekleniyor. Yalnız kanıtlanan aşamaları tamamlandı göster. Zaman geçmesini başarı kanıtı sayma; gerçek yüzde veya kesin bitiş geri sayımı uydurma.

Yanıt gelmeden koparsa: “İşlem sonucu doğrulanamadı; cihaz yeniden başlamış olabilir.” Cihazı bulma yönergeleri ver; aynı kayıt isteğini otomatik tekrarlama.

Beklenen kopma sırasında genel offline toast yağmurunu bastır. Kontrol komutlarını devre dışı tut; son bilinen veriyi canlı gibi gösterme. Yönergeler sayfa üzerinde bağlantısız da okunabilsin.

## F — Cihaza yeniden erişim

“Cihaz panelini aç”, “Adresi kopyala”, “Cihaza ulaşamıyorum”. Özel mDNS veya `scada-<ID>.local` adresi kullan. `192.168.4.1` yalnız AP dönüş adresidir. DHCP adresi bilinmiyorsa uydurma.

Kopyalama HTTP/güvensiz bağlamda kullanılamıyorsa seçilebilir düz metin sun. Tarayıcılar arası farklı origin/CORS ve oturum sınırlarını hesaba kat; eski AP sayfasının yeni cihaz adresini okuyabildiğini varsayma.

mDNS çözülmezse modem/router istemci listesinden IP'yi bulma yönergesi ver. Kimlik bilgisi farklı cihaza aitse başarı kabul etme; hedef sayfada cihaz kimliğini göster.

## G — Başarı

Hedef cihazdan güncel veriyle STA bağlantısı doğrulanınca “Cihaz Wi-Fi ağına bağlandı.” de. Cihaz adı, doğrulanmış erişim adresi ve mevcut bağlantı bilgilerini göster. “Kontrol paneline git” ve “Diğer ayarları tamamla” eylemleri sun.

MQTT, internet ve saat eşitlemesi ayrı durumlar olsun. Web yönetim parolası oluşturmayı sonraki adım olarak öner; kaynak desteklemiyorsa zorunlu kurulum aşaması yapma.

## H — Erişim yok / AP kurtarma

Başlık: “Cihaza henüz ulaşılamıyor.”

1. Telefonun seçilen Wi-Fi ağına bağlı olduğunu kontrol et.
2. Cihaz adresini yeniden aç.
3. Kurulum ağı tekrar görünüyorsa ona bağlan.
4. `192.168.4.1` üzerinden ağ bilgilerini kontrol et.

Yanlış parola, zayıf sinyal, erişim noktasının kapalı olması veya ağ yapılandırması olası nedenlerdir. Kesin telemetri yoksa bunlardan birini teşhis olarak sunma. Yeni ağ başarısız olunca eski ağa otomatik dönüş varmış gibi anlatma.

AP kurtarma denemesi başarıya ulaşınca AP'nin kapanabileceğini açıkla. Firmware zamanlama verisi sunmuyorsa canlı geri sayım ekleme. İlk kurulum ile kurtarmayı ayıracak yeni alan önerisini API ihtiyacı olarak işaretle.

## Sıfırlama kapsamları

Reboot, Wi-Fi silme ve fabrika sıfırlamayı ayrı tasarla. Wi-Fi onay metni:

“Wi-Fi adı ve parolası silinecek, sabit IP kullanımı kapatılacak. Cihaz kurulum ağıyla yeniden başlayacak. Diğer cihaz ayarları korunacak.”

Fabrika sıfırlaması bağlantı sorununda ilk öneri olmasın. Sonuçların kapsamını onay ekranında açıkla; tehlikeli işlemleri ana kurulum eylemiyle yan yana eşit ağırlıkta sunma.

## Bileşen sistemi

Aşama göstergesi, bağlantı rozeti, seçilebilir ağ satırı, parola alanı, kalıcı durum kartı, alan hatası, adres/kopyalama, açılır yardım, sıfırlama onayı için normal/odak/seçili/devre dışı/yükleniyor/hata/başarı durumlarını tarif et.

- Lacivert/teal tema, açık temada krem/beyaz; mevcut token isimleri esas alınsın.
- IBM Plex Sans gövde, Mono etiket/adres/sayı. İnternet/CDN olmadan yerel varlıklar.
- 10/11/12 px kompakt ölçek ve 14 px başlık referanstır. Mobil formda 16 px gibi okunabilirlik önerisini proje ölçeğine açık istisna olarak belirt.
- En az 44×44 px dokunma alanı, görünür klavye odağı, dialogda odak yönetimi ve kapanışta tetikleyiciye dönüş.
- Durum duyuruları için canlı bölge; alan hatasını alanla ilişkilendirme; yalnız renge dayanmayan durumlar.
- Uzun SSID/UTF-8 metinde taşma olmaması, mobil klavyede ana eylemin erişilebilirliği, azaltılmış hareket tercihi.
- Tek sütun mobil, dar panel masaüstü; adım başına belirgin tek ana eylem.
- Katı CSP ile uyumlu yerel varlıkları gözet; tasarım için satır içi script veya dış servis gerektirme.

## Teslim tablosu kalıbı

| Durum | Kanıt/tetikleyici | Kullanıcı metni | Kullanıcı eylemi | Sonraki durum | Destek |
|---|---|---|---|---|---|
| Kaydediliyor | Ağ kaydı isteği gönderildi | Ayarlar kaydediliyor… | Bekle | Onay/hata/belirsiz | Mevcut API + UI |
| Kayıt onaylandı | Başarılı API yanıtı | Ayarlar kaydedildi | Hedef Wi-Fi'ye geç | Erişim bekleniyor | Mevcut API + yeni ekran |
| Sonuç belirsiz | Yanıt alınmadan kopma | İşlem sonucu doğrulanamadı | Cihaz adresini aç | STA veya AP | Yeni UI |
| Bağlandı | Hedef cihazın güncel STA verisi | Cihaz Wi-Fi ağına bağlandı | Panele git | Kontrol | Hedef origin'de doğrulama |

Bu tabloyu gerçek inceleme bulgularıyla tamamla. API gereksinimlerine örnek: kurulum/kurtarma nedeni, deneme sonucu kodu, denemenin kalan süresi. Bunları mevcut alanmış gibi kullanma.
