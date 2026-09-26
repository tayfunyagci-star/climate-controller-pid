# Wi-Fi ilk kurulum ve kurtarma (eski `scada-wifi-onboarding`)

Firmware'in gerçek durum geçişlerini kullanıcıya anlaşılır, dürüst ve kesintiden sonra devam edilebilir bir deneyime dönüştür. Varsayılan çıktı Türkçe, kod içermeyen tasarım şartnamesi veya doğrudan kopyalanabilir tasarım promptudur.

## Kapsam

- Kaynakları salt okunur incele; firmware, UI, ayarlar ve cihaz durumunu değiştirme. Derleme, test, simülasyon, yükleme veya gerçek ağ işlemi çalıştırma.
- Kullanıcı ayrıca isterse tasarım belgesini dosyaya yaz. Proje talimatı günlük güncellemesi istiyorsa yalnız tamamlanan analiz ve kalan işleri kaydet.
- Kullanıcı uygulama isterse tasarım çıktısından ayrı bir uygulama görevi olarak ele al; bu beceri kendi başına uygulama yetkisi vermez.
- Gizli parolaları kaynaklardan çıktıya taşıma. Referanstaki fabrika AP parolası ürünün bilinen kurulum bilgisidir; kullanıcıya ait ağ parolaları değildir.

## İş akışı

1. Proje erişilebiliyorsa önce `SCADA_PROJECT_LOG.md` ve geçerli proje talimatlarını oku. Günlükteki iddiaları kaynak kodla karşılaştır; eski yorumları çalışan koddan üstün tutma.
2. `references/wifi-firmware-referansi.md` dosyasını oku. Bu, 25 Eylül 2026 tarihli OneCH v6.2.0 inceleme kaydıdır; sonraki firmware sürümlerinin garantisi değildir.
3. Şu kaynakları veya projedeki karşılıklarını incele:
   - `src/main.cpp`: açılış, AP/STA başlatma, ağ görevi, tarama, ağ kaydı, reboot, Wi-Fi sıfırlama, yetkilendirme, durum API'si.
   - `include/settings_validation.h`: veri sınırları ve doğrulama.
   - `tools/ui/index.html`, `app.js`, `app.css`, `theme.js`: mevcut ekranlar, olaylar, hata yönetimi, tema ve tipografi.
   - Gerekirse `platformio.ini`: hedef donanım ve derleme bağlamı; çalıştırma yapma.
4. Ağ görevini, proses döngüsünü, ISR'ı, mutex sınırlarını ve LittleFS kayıt yolunu yalnız kurulum davranışını açıklayacak derinlikte haritala. Süreleri, reboot tetikleyicisini, AP kapanma koşulunu ve güvenli çıkış davranışını çıkar.
5. Her bulguyu üç sınıftan birine yerleştir: **kodda doğrulandı**, **belgelenmiş fakat doğrulanmadı**, **tasarım önerisi**. Kaynak yoksa referans profilini varsayım olarak kullan ve bu sınırlamayı başta belirt. Kaynakları görmüş gibi davranma.
6. `references/wifi-tasarim-sartnamesi.md` içindeki ekran ve bileşen gereksinimlerini oku. Kullanıcının güncel tercihleri ve mevcut proje tasarım sistemiyle uyumlu hale getir.
7. Mevcut davranışı kısa özetle; ardından istenen kapsamda kendi başına anlaşılabilir tasarım promptunu veya şartnameyi üret. Yalnız eksik bilgi ilerlemeyi gerçekten engelliyorsa soru sor.

## Tasarımın temel kuralları

- **Kaydedildi ≠ bağlandı.** Başarılı kayıt yanıtını bağlantı başarısı gibi sunma. Yeniden başlatma komutunun kabulünü de reboot tamamlandı diye gösterme.
- **Yanıt alınamadı ≠ işlem yapılmadı.** Ağ kopması sırasında sonuç belirsizse tekrar yazma/reboot döngüsü yerine erişimi kontrol ettir.
- AP kapanınca eski sayfa yeni IP'yi bilemez. Hedef adresi ve geri dönüş talimatlarını kayıt öncesinde göster; bağlantı kesildiğinde de okunabilir tut.
- Telefonun Wi-Fi ağını tarayıcıdan değiştirebildiğini, captive portalın otomatik açılacağını veya farklı origin üzerindeki cihazı otomatik sorgulayabildiğini varsayma.
- AP modu ilk kurulum ile kurtarmayı tek başına ayırmaz. Cihazdan gelmeyen kesin hata nedeni, kalan süre, SSID veya yeni IP üretme.
- Wi-Fi, internet, MQTT ve saat eşitlemesi ayrı durumlar olsun. MQTT eksikliği Wi-Fi kurulumu başarısızlığı değildir.
- AP modu süreç kilidi değildir. Reboot sırasında yazılımın verdiği kapatma komutunu fiziksel vana kapandı diye sunma; kontak geri bildirimi bulunmayabilir.
- Mevcut kodun bloklayan ağ beklemelerini saklama. Önerilen deneyim non-blocking proses mimarisini, çekirdekler arası kilitleri ve deferred-save yaklaşımını korumalıdır.
- Parola, SSID ve diğer hassas alanları URL, günlük, toast veya kalıcı tarayıcı depolamasına koyma. SSID'yi güvenilmeyen metin olarak ele al.

## Çıktı sözleşmesi

İsteğin boyutuna uygun şekilde şu içeriği teslim et:

1. Mevcut davranış ve kaynak referansları; incelemenin statik olduğunu belirt.
2. Ürün amacı, kullanıcı profili ve tasarım dili.
3. Ekran akışı; gerekirse Mermaid durum şeması.
4. Her ekranın yerleşimi, birincil/ikincil eylemi, tam Türkçe metinleri ve hata/boş/yükleniyor durumları.
5. Durum → kanıt/tetikleyici → kullanıcı mesajı → eylem → sonraki durum tablosu.
6. Mobil/masaüstü, açık/koyu tema, erişilebilirlik ve bileşen davranışları.
7. **Mevcut firmware ile yapılabilir**, **yalnız UI geliştirmesi**, **firmware/API desteği gerekir** ayrımı.

Kullanıcı özellikle “tasarım promptu” isterse 2–7. maddeleri başka bir araca tek seferde verilebilecek bağımsız bir promptta birleştir. Kullanıcı sadece belirli ekranı isterse tüm ürünü yeniden tasarlama; ortak bağlantı doğrulama kurallarını o ekrana uygula.

## Son editoryal kontrol

Bu bir test çalıştırma adımı değildir. Yazdığın metni şu açılardan gözden geçir:

- Başarı mesajı gerçek cihaz kanıtına bağlı mı?
- Ağ kopması sonrası kullanıcı ne yapacağını, hangi adrese gideceğini biliyor mu?
- Hata nedenleri, süreler ve otomatik kurtarma yetenekleri kaynakla destekleniyor mu?
- Wi-Fi silme, reboot ve fabrika sıfırlama ayrılmış mı?
- Referans profilindeki tarihsel bilgiler güncel doğrulanmış bilgi gibi sunulmuş mu?
- Yeni API gereksinimleri ve erişilemeyen bilgiler açıkça işaretlenmiş mi?

## Örnek kullanım

- “Firmware'in AP → Wi-Fi seç → reset akışını incele ve tasarım promptu hazırla.”
- “Kurulumdan sonra telefon bağlantısı kopuyor; kullanıcıya gösterilecek ekranları tasarla.”
- “Bu firmware için Wi-Fi kurtarma deneyimini mobil ve koyu/açık tema olarak tarif et.”
