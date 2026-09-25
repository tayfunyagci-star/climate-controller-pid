# Cihaz temeli (eski `scada-device-baseline`)

Bu dosya tek başına taşınabilir; başka bir depo, referans klasörü veya belirli bir AI aracına ihtiyaç duymaz. Yeni projelerde temel al; mevcut projelerde kullanıcının açık tercihlerini ve çalışan haberleşme sözleşmesini koruyarak uyarla. Donanımda bulunmayan sensör, çıkış veya ağ arabirimini varmış gibi gösterme. WS2812B durum LED şeridi ailenin standart donanımıdır (`durum-ledleri.md`); yalnız eski kartta yoksa `donanımda yok` olarak işaretlenir.

Kaynak: kullanıcının sağladığı `scada-platform` belgesinin mimari ilkeleri ve `4chRelayModule` projesinin 2026-09-21 tarihinde incelenen UI/firmware kaynakları. Görsel standart, bu projede son CSS kurallarıyla etkin olan **Scale krem / lacivert / teal** profilidir. Eski siyah/hardal ana tema, neumorfik kabuk ve bütün arayüzü monospace yapma kuralı uygulanmaz. Amber yalnız uyarı anlamında kalır.

## 1. Uygulama sırası ve proje profili

1. Proje talimatı, günlük, kaynak ve testleri oku. MCU, SDK, pinler, polarite, proses, MQTT tüketicileri ve web/OTA tercihlerini belirle. Yalnız uygulamayı engelleyen eksik bilgiyi sor; verilmiş kararı tekrar onaya sunma.
2. Kararları **ortak standart / MCU uyarlaması / cihaz-proses tercihi** olarak ayır. Referans cihazın dört kanalı, pinleri, IP'si ve limitleri evrensel varsayılan değildir.
3. Özellik kataloğunda her maddeyi `uygulanıyor / donanımda yok / kapsam dışı` olarak işaretle. Olmayan özelliği yapılmış gibi raporlama.
4. Düzenlenebilir kaynakları değiştir; üretilmiş firmware UI header'ını elle düzenleme. İlgili davranış, tarayıcı ve hedef derleme doğrulamalarını tamamla.
5. Sonucu ve açık sınırları günlüğe yaz; Git varsa yalnız ilgili değişiklikleri açıklayıcı commit'e al. Canlı yükleme talebi yoksa build/test ile bitir; bu skill tek başına cihaz yükleme yetkisi vermez.

| Profil alanı | Kaydedilecek karar |
|---|---|
| Hedef | Tam MCU/kart modeli, SDK/framework sürümü, çekirdek sayısı, RTOS |
| Bütçe | RAM/heap, flash/OTA bölümleri, FS, görev yığınları, UI/font payı |
| I/O | Kanal kimliği/adedi, pin/sürücü, aktif seviye, boot strap, pull dirençleri, fiziksel geri bildirim |
| Proses | Güvenli durum, açılış/restore, manuel/bakım/arıza/OTA öncelik tablosu |
| Modüller | Sensör, sayaç, takvim, WS2812B durum şeridi (LED adedi, veri pini, cihaza özgü LED'ler), Wi-Fi/Ethernet, keşif, audit ve OTA desteği |
| Kontrat | Kalıcı kimlik, DHCP/statik/kurtarma, Wi-Fi bantları; API, mevcut topic'ler, indeks tabanı, retain/QoS, tüketici stale süresi |
| Erişim/zaman | Web ve OTA politikası ayrı; misafir okuma, fiziksel kurtarma, taşıma şifrelemesi, saat kaynağı/zaman dilimi |
| Kanıt | Yerel test, MCU build, cihaz/saha ölçümü ve sınanmayan durumlar |

## 2. Mimari, zaman bütçesi ve kilitler

- Yerel proses/interlock merkez, internet, MQTT veya web arayüzü olmadan çalışır. Ağ kaybındaki güvenli durum proses profilidir; her cihazı zorunlu kapalıya götürme.
- Çıkışların tek sahibi proses katmanıdır. HTTP/MQTT doğrulanmış isteği sınırlı kuyruğa bırakır; handler doğrudan GPIO sürmez. Kuyruk dolu/kilitli/OTA durumları görünür ret üretir.
- Ağ, flash ve LED işi proses bütçesini tüketmez: tek çekirdekte kısa tick/durum makineleri, RTOS'ta görev/olay kuyrukları. Uzun `delay()`/busy-wait yoktur; RTOS'ta görevi uyutan süreli bekleme uygundur. Bütün gecikme API'lerini yasaklayıp idle görevini aç bırakma.
- Süre hesabı taşmaya dayanıklı monoton saat farkıdır. DNS/TCP/MQTT/collector timeout ve backoff ölçülür; WDT beslemek bloklayan çağrıyı non-blocking yapmaz.
- Thread-safe olmayan MQTT/TCP istemcisinin tek sahibi ağ katmanıdır. Diğer görevler kuyruk/atomik bildirim kullanır. **`volatile` tek başına eşzamanlama veya bellek sıralaması sağlamaz.**
- RAM ve FS kilitlerini ayır. Birlikte alınmaları gerekiyorsa sıra `fsMutex → sysMutex`; ters sıra yoktur. Tercihen RAM snapshot al, RAM kilidini bırak, FS kilidiyle yaz. Flash I/O sırasında RAM kilidi; ağ çağrısı sırasında RAM/FS kilidi tutulmaz.
- Recursive mutex yalnız yeniden giriş gerekiyorsa kullanılır. ISR'da bloklayan mutex, dosya, JSON veya ağ çağrısı yoktur.
- Bağlantı kontrolü/yeniden deneme hız sınırlıdır. `connected()` için 250 ms, reconnect için 15 sn başlangıç örnekleridir; kütüphane ve ölçüme göre ayarlanır.
- Snapshot kaydında nesil sayacı: değişiklik nesli artırır; başarılı yazım sonunda yalnız mevcut nesil snapshot nesline eşitse dirty temizlenir. Yazım sırasında gelen değişikliği kaydedilmiş sayma. Başarı zaman damgası ve sayaç tabanı da tutarlı güncellenir.

## 3. Çıkış güvenliği ve proses

- Boot/reset/OTA elektriksel güvenli seviyesini belirle; uygunsa latch'i OUTPUT öncesi hazırla. Kısa yanlış GPIO seviyesini güvenli sayma; pull direnci, enable hattı veya sürücü devresiyle çöz ve donanımda ölç.
- Polarite ayardan geliyorsa yükleme sonrası güvenli seviyeyi kesin yeniden uygula. `istenen == mevcut` eski elektrik seviyesini kalıcı bırakmamalı. Çalışma zamanı değişimini proses sahibine devret.
- Manuel, bakım, arıza, genel kilit, program, uzak istek ve OTA önceliği tablo/testle belirlenir. Eski belgedeki tek sıralamayı her cihaza kopyalama.
- Referans röle profili: OTA bütün çıkışları kapatır; fiziksel MANUEL bakımda ON olabilir; bakım AUTO'yu kapatır. Bu yalnız o cihazın kararıdır.
- Süre/hacim, iletişim kaybı, dry-run, ortam ve sensör/giriş koruması ilgili donanımda uygulanır. Geçersiz ölçümü sıfır/sağlıklı veri gibi yayımlama; geçerlilik ve yaş ayrıdır.
- Sensör var/yok bayrağı gerçek tespitten gelir. ADC doygunluğu, açık/kısa devre ve kopuk kablo değerlendirilir; algılanamayan durumun sınırını belirt.
- Fiziksel geri bildirim yoksa durum **komutlanan çıkış** anlamındadır. GPIO ON fiziksel kontak, akış veya motor çalışması kanıtı değildir.
- LATCH/TACTILE varsa debounce/basış/bırakış tanımlıdır. Referans: LATCH konuma göre manuel, TACTILE her kararlı basışta manuel aktif/pasif; bırakmada değişim yok. Başka giriş tipine zorla taşıma.
- Alarmı okuma, arızayı giderme ve kilit sıfırlama ayrıdır. Koşul sürerken reset korumayı devre dışı bırakamaz.

## 4. Kalıcılık, sayaç ve takvim

- Ayar, sık değişen sayaç ve geçmişi ayır. Şema sürümü/tür/aralık/bütünlük doğrulaması yap; checksum sır saklama veya saldırgana karşı kimlik doğrulama değildir.
- Dosya kaydı: beklenen boyutu önceden ölç → geçici dosyaya yaz/kapat → yeniden açıp beklenen/yazılan/diskteki boyutları ve payload bütünlüğünü doğrula → önceki sağlam kopyayı koruyarak devreye al. Sadece `yazılan == diskteki` yetmez; ikisi de eksik olabilir.
- Rename hatasında tek sağlam `.tmp` silinmez. Primary/backup/temporary kurtarma ve nesil seçimi deterministiktir; kesinti noktaları test edilir. NVS/transaction hedefinde eşdeğer dayanıklılığı onun garantileriyle kur; dosya adları zorunlu mimari değildir.
- Yarım kayıt/açılamayan FS otomatik format veya fabrika sıfırlaması başlatmaz. Son doğrulanmış ayarı kurtar; geçerli kayıt yoksa güvenli çıkış + görünür kurulum/hata modu. Silme ayrı açık kullanıcı işlemidir.
- Deferred save yanında azami flush aralığı vardır; sürekli proses kaydı sonsuza kadar ertelemez. Güç kesintisinde kaybolabilecek aralığı belirt. Kritik ağ/erişim ayarı kalıcı kayıt doğrulanmadan başarılı sayılmaz.
- Ayar işlemlerini serileştir veya sürüm kontrolü kullan. Adayın tamamını doğrula, kalıcı commit/RAM aktivasyonunu tutarlı yap. Hata önceki çalışma ayarını korur; hiçbir alan kısmen uygulanmaz.
- Reboot/OTA öncesi flush sonucunu değerlendir. Kayıt hatası sonrası sessizce reboot planlama; gerekli güvenli duruş kayıt başarısından bağımsızdır.
- Boots/faultBoots/reset nedeni ve storage tanısını aşınmayı sınırlayan kalıcılıkla tut. RAM'de alarm olmaması reset olmadığını kanıtlamaz.
- Göçte yeni kopya doğrulanmadan eski sayaç/alanı kaldırma. Downgrade uyumluluğunu belgele; kaynak snapshot'ı canlı cihaz ayar yedeği değildir.
- Uzun ömürlü sayaçta uygun 64 bit birikim. Toplam resetinde seans/hedef/limit başlangıç referanslarını aynı işlemde kaydır; negatif farkla korumanın kapanmasını önle. Reset çıkış komutu değildir.
- Süre monoton, takvim geçerli duvar saatiyle çalışır. Yerel LAN NTP/RTC gibi desteklenen kaynak kullan; internet zorunlu değildir. İlk saat doğrulanmadan program yürütme, belirsizlik görünür olsun.
- Gün farkını `tm_yday + 365` ile hesaplama; takvim gün serisi kullan. Artık yıl/yıl sonu, ileri/geri saat ve dizi sınırları test edilir; geri saatte negatif indeksle geçmiş kaydırılmaz.
- **Günlük geçmiş (tüketim/sayaç/çalışma süresi):** cihaz yalnız son N kapanmış günü (referans N=7) kayan pencere olarak flash'ta tutar; uzun dönem arşiv cihazın işi değildir, abonenindir (§5 "Günlük geçmiş yayını"). Pencere `[0]=dün` sırasıyla, takvim gün serisiyle kaydırılır; bugün ayrı sayaçtadır. Kayıt başlangıç günü (`since`) yalnız **yeni/silinmiş** dosya sisteminde ilk geçerli saatle damgalanır; alanı olmayan eski kayıt geriye uyumlu okunur (bilinmiyor = alan yok). Toplam sayaç reseti geçmişi değiştirmez.
- Haftalık program: ad, etkinlik, hedef kanal(lar), günler, başlangıç işlemi/saati, süre veya bitiş saati; ekle/düzenle/sil/etkinleştir. Gece yarısı/hafta geçişi, çakışma, boot uzlaştırması ve değişikliğin aktif çıkışa etkisi tanımlıdır. Kapasite, zaman dilimi ve bitiş eylemi profildedir.

## 5. Ağ, kurulum ve MQTT

### Ağ ve kurtarma

- Kalıcı cihaz kimliğini görünen cihaz/mDNS adından ayır. Arabirime göre DHCP/statik IP, maske/gateway/DNS sun. IP görünürdür; mDNS tek erişim yolu değildir.
- Wi-Fi yapılandırmasını bağlantı öncesi uygula; Arduino `config/begin` sırasını kullanılan core'a göre doğrula. IPv4, bitişik maske, gateway alt ağı, ağ/broadcast ve çakışan adresler sunucuda doğrulanır.
- Bağlantı denemesi süreli durum makinesidir. Başarısız statik denemede DHCP kurtarma + alarm bulunur. Association yanlış gateway/alt ağı kanıtlamaz; erişilebilirlik ölçülmüyorsa her hatalı statik adresin otomatik kurtulacağını iddia etme.
- İlk bağlantı ve reconnect aynı idempotent servis yolunu kullanır: mDNS/OTA/HTTP ve abonelikler yeniden işler. Yalnız boot'ta başlatılan servis bırakma.
- AP varsa adı/adresi ve kurulum adımları görünür. AP erişimi/parolası/açık kalma politikası profildedir; Ethernet cihazına Wi-Fi menüsü ekleme.
- Tarama yükleme/hata/boş liste durumları, RSSI/güvenlik ve desteklenen bantları gösterir. SSID'den bant tahmin etme. Aynı isimleri birleştirirken güvenlik/BSSID farkını yanlış ağa bağlanacak biçimde kaybetme.
- Wi-Fi modalında ağ + parola/açık ağ seçilir; yalnız Wi-Fi kaydedilir, diğer taslaklar korunur. İptalde parola temizlenir, hatada modal açık kalır. Bağlantı yalnız kaydet eylemiyle değişir.

### MQTT sözleşmesi

- Client ID sabit ve cihazlar arasında benzersizdir; reconnect'te rastgele ek yok. Görünen isim değişince kalıcı kimlik değişmez.
- Yeni projede örnek topic ailesi: `<base>/state` retained JSON, `<base>/cmd/<nesne>`, `<base>/result/<nesne>`, `<base>/avail` retained online/offline. Mevcut topic ve entity kimliklerini izinsiz değiştirme.
- LWT + bağlantıda online + periyodik availability birlikte uygulanır. Örnek LWT QoS1/availability 30 sn; periyot/QoS desteğini taşıyıcı ve tüketiciyle doğrula. Publish dönüşü broker ACK'i değildir.
- Broker/kök topic değişimi: eski bağlı adrese offline dene → temiz disconnect → yeni bağlantı/LWT → abonelik → tam durum/keşif/online. Ulaşılamayan eski broker'da temizliği garanti etme.
- Ayrık değişim hızlı, ölçüm deadband ile, aktif proses periyodik, boşta heartbeat ile yayımlanır. Referans aktif 2 sn/boşta 60 sn; heartbeat tüketicinin stale süresinden kısa olmalı. Availability ayrı zamanlayıcıdır.
- Başarısız yayında dirty/referans gönderilmiş sayılmaz; reconnect tam durum yayımlar. Aynı duruma komut da sonuç/durum üretir. Sonuç kabul/neden/hedef/kaynak ve mümkünse ilişkilendirme kimliği taşır; kuyruğa kabul uygulamadan ayrılır.
- **Günlük geçmiş yayını** (günlük değer tutan her cihazda standart):
  - Ayrı retained topic `<base>/history/daily` (birden çok ölçüde `<base>/history/<metrik>_daily`). `/state`'e eklenmez: günde bir değişen seri sık durum mesajına bindirilmez.
  - Payload: `{"v":1,"unit":"L","end":"YYYY-MM-DD","days":[..N..],"sum":<N gün toplamı>,"since":"YYYY-MM-DD","ts":<epoch>}`. `days[0]` = `end` günü (dün), `days[i]` = `end`'den i gün önce, yalnız kapanmış günler. Tarih AÇIK verilir, abone hesaplamaz. Değerler cihazda birime çevrilir ve yuvarlanır; bozuk (NaN/negatif) değer 0'a indirgenip JSON bozulmaz. `since` bilinmiyorsa alan yazılmaz. Alan eklenirse `v` artar.
  - Tetikleyiciler: gün devri, broker (yeniden) bağlantısı, birim değişimi ve **saatte bir** (son başarılı yayından). Saat geçerli değilse yayın yok, bayrak bekler.
  - Proses görevi yalnız bekleyen-yayın bayrağı kurar; yayını MQTT istemcisinin sahibi ağ katmanı yapar. Bayrak snapshot'tan önce temizlenir, başarısız yayında geri kurulur (yayın sırasında gelen gün devri ezilmez); başarısız deneme hız sınırlı (ör. 5 sn). Snapshot RAM kilidiyle, publish kilit dışında.
  - Keşif: aynı `dev.ids` ile bir sensör; `stat_t` = `json_attr_t` = geçmiş topic'i, `val_tpl` = `{{ value_json.sum }}` (düz alan; tüketici ifade/filtre çözmeyebilir). Keşif kapatılınca bu kayıt da temizlenir; kök topic değişince eski adresteki retained geçmiş boş retained ile silinir.
  - Bugünkü değer geçmiş topic'inde değil, `/state` içindeki günlük alanla deadband'e göre akış oldukça yayınlanır.
  - Payload üretimi ve gün serisi ↔ tarih dönüşümü donanımdan bağımsız saf başlıkta tutulur ve native testle sınanır (artık yıl, 2100, yıl sonu gidiş-dönüş, tampon taşması, en kötü payload boyutu). Referans uygulama: OneCH v6.2.0 `include/history_publish.h`.
  - Abonenin uyacağı birleştirme sözleşmesi (tarih anahtarlı upsert, `since` öncesini yok sayma, birim kayıt başına, boşluk ≠ 0) `mqtt-studio-dugum` yeteneğindedir.
- Eylem komutları özellikle reset/toggle retained değildir. Callback retain bayrağı sunmuyorsa retained komut reddi varmış gibi davranma; idempotent hedef-değer, komut ID/sırası veya destekleyen istemciyle tekrar politikasını belirle.
- Yeni abonelikteki retained kayıt ve süre boyunca gözlenen yayınları ayır. Retained online tek başına canlılık, offline fiziksel duruş kanıtı değildir. MQTT sürümü/abonelik seçenekleri nedeniyle retain bayrağını evrensel canlılık testi sayma.
- HA keşfi: sabit `unique_id`, tüm entity'lerde availability, desteklenen kısa anahtarlar, gerçek paket boyutuna yeterli tampon. Sabit 1024 baytın daima yeteceğini varsayma. Tüketici kısa/uzun anahtarları ve `~` kısayolunu desteklemeli.
- `homeassistant/status` sonrasında hız sınırlı keşif tekrarı (ör. 10 sn); keşif kapanınca eski retained config temizliği. Ad bazlı eşleştiren tüketicinin sınırını belirt.
- Sayaç reseti/LED parlaklığı varsa web ile aynı doğrulama/proses yolu, HA button/number keşfi. Reset sonucunun kalıcılık zamanı, parlaklık aralığı ve kayıt hatasında geri alma tanımlıdır.

## 6. Scale web arayüzü standardı

### Temiz tema token'ları

Aşağıdaki katman yeni arayüzün temelidir. Referans CSS'in eski siyah/hardal bloklarını ve tarihsel override yığınını kopyalama. Bileşen renkleri semantik token kullanır; fiziksel LED renk seçimi UI paletinden bağımsızdır.

```css
:root {
  --navy:#132a4c; --navy-2:#24406b; --navy-deep:#0e1f3b;
  --teal:#1d8f74; --teal-2:#3fb897; --teal-soft:#9eebd1;
  --cream:#fbf7f1; --card:#ffffff; --mist:#f4f1ea;
  --line:#e6e1d6; --line-dark:#d8d0be; --ink:#20242c; --slate:#5b6472;
  --bg:#0e1b2d; --bg-panel:#14263e; --bg-raised:#192e49; --bg-input:#102037;
  --border:#718398; --border-soft:#34485f;
  --text:#e5edf5; --text-strong:#ffffff; --text-dim:#bac8d7; --text-faint:#bac8d7;
  --brand:#9eebd1; --brand-dim:#163c38; --brand-line:#3fb897;
  --green:#9eebd1; --red:#ffb19e; --amber:#e7b96b; --blue:#b3ccef;
  --power-bg:#9eebd1; --power-text:#0e1f3b; --heading:#e5edf5;
  --pw-on:#5fd37a; --pw-off:#ff7a70; --pw-unknown:#8795a6;
  --warn-text:#f2c35f; --warn-bg:#2a2414; --warn-line:#b88a2c;
  --crit-text:#ff8f85; --crit-bg:#2e1b1c; --crit-line:#d9574f;
  --clr-text:#bac8d7; --clr-bg:transparent; --clr-line:#5a6b80;
  --overlay:#00000099; --radius-control:6px; --radius-panel:4px;
  --font-body:"IBM Plex Sans",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  --font-mono:"IBM Plex Mono",Consolas,"SFMono-Regular",Menlo,"Liberation Mono",monospace;
  --fs-2xs:10px; --fs-xs:11px; --fs-s:12px; --fs-title:14px;
  --zone-bg:#ff7a701a;
  --nm-up:none; --nm-in:none;
}
html[data-theme="light"] {
  --bg:var(--cream); --bg-panel:var(--card); --bg-raised:var(--card); --bg-input:var(--mist);
  --border:#8c918e; --border-soft:var(--line);
  --text:var(--ink); --text-strong:var(--navy); --text-dim:var(--slate); --text-faint:var(--slate);
  --brand:#14705b; --brand-dim:#eaf7f1; --brand-line:var(--teal);
  --green:#14705b; --red:#a03c27; --amber:#795521; --blue:var(--navy-2);
  --power-bg:#14705b; --power-text:#ffffff; --heading:var(--navy);
  --pw-on:#1e7d32; --pw-off:#c62828; --pw-unknown:#7c828b;
  --warn-text:#735000; --warn-bg:#fff5d8; --warn-line:#c2922a;
  --crit-text:#a8241c; --crit-bg:#fdeceb; --crit-line:#d0473f;
  --clr-text:#58606b; --clr-bg:transparent; --clr-line:#a9b0b8;
  --overlay:#00000066;
  --zone-bg:#c6282814;
}
[hidden] { display:none !important; }
```

- İki tema zorunlu. Tercihi ilk stil öncesi `<head>` içindeki küçük yerel betikle uygula: kayıtlı tercih → sistem tercihi → açık tema. `localStorage` hatasını yakala. Tercih origin/tarayıcı kapsamındadır, sunucuya cihaz ayarı diye gönderilmez.
- İki temada lacivert başlık, beyaz başlık metni/teal yardımcı vurgu. Gövde açıkta krem/beyaz, koyuda laciverttir. Sıkı, ince çerçeveli, küçük yarıçaplı düz paneller; neumorfik kabartma, kart parıltısı ve ağır gölge yok.
- Açıklama/form/düğme IBM Plex Sans; başlık/menü/teknik etiket/kod/veri IBM Plex Mono. Normal ağırlık 400, vurgu seyrek; ölçüm `tabular-nums`. Font yüklenmezse sistem yığını çalışır.
- Fontlar cihazdan: Sans 400/600, Mono 400; Türkçe içeren resmî WOFF2 alt kümeleri, `font-display:swap`, doğru `unicode-range`, dağıtım lisansı. Referans ~105 KB bütçedir; hedefte ölç. Dar bellekte belgelenmiş sistem fontu profili mümkün; CDN'e geçme.
- İkon inline SVG/currentColor, emoji veya harici ikon kütüphanesi yok. Basit grafik SVG; seri rengi yanında çizgi/işaret/etiket. İki temada ayrışmayı ölç; marka ve durum anlamını karıştırma.
- Metin ≥4.5:1; anlam taşıyan simge/odak/kontrol sınırı uygun zemine karşı ≥3:1 ölçülür. Dekoratif ayraç tek kontrol sınırı yerine geçmez. Token kullanmak erişilebilirlik kanıtı değildir; pasif/hover/odak/hata durumlarını da ölç. Yarı saydam zeminlerde (`--zone-bg`) kontrast, alfa alttaki panel zeminiyle birleştirilerek ölçülür.

### Yerleşim ve erişilebilirlik

- Sayfalar: Kontrol, Programlar, Alarmlar, Denetim, Ayarlar, Oturum. Desteklenmeyen modülü gizle veya belirt; sahte veriyle tamamlanmış izlenimi verme.
- Ortak başlık/menü/bilgi şeridi: cihaz adı, IP, varsa mDNS, istemci IP, firmware sürümü/revizyonu. Alt satırda revizyon ve saat/saat bekleniyor. Başlıkta bilgi ve tema ikon düğmeleri; kimlik şeridi kutusuz düz metin. Menü her genişlikte ikonlu (16 px ikon + Mono 12 px), masaüstünde eşit sütun, etkin sayfa `--navy-2` dolgu + teal kenarlık. Durum çubuğu: `● Canlı · şimdi` + Wi-Fi/MQTT/Keşif haplari. Ayrıntı `arayuz-tasarimi.md` §5.
- Mobil ≤600 px: kompakt başlık + tema/bilgi ikonları; altı sayfa için sürekli görünür 3×2 menü. Ayarlarda yan sütun yerine üstte dört sütun bölüm kutuları, tam genişlik içerik. Çok dar ekranda kutular üç, kanal kartları bir sütun olabilir.
- Referans kontrol kartları masaüstünde dört, telefonda iki sütun. Kanal sayısını dörtle sınırlama; hedefe göre grid, okunurluk bozulursa daha az sütun. Form masaüstünde iki, telefonda tek sütun.
- Düğme/alan padding yaklaşık 5/8 ve 5/6 px; masaüstü kontrol 36 px, telefon hedefi ≥44×44 px; referans mobil güç düğmesi 64 px. Yazı ölçeği yalnız 10/11/12 px, en büyük başlık 14 px (grafik/ölçer sayısal değerleri hariç); alarm yazısı 10 px. Kritik bilgiyi sığsın diye 10 px altına indirme.
- Sistem durumu açılır/kapanır; kapalıyken uptime/boş RAM özeti, tercih tarayıcıda. Açıkken masaüstünde 4 sütun; etiket 10 px, değer 11 px. Bilgi şeridi düğmesi `aria-expanded/controls` taşır.
- Gerçek button/label/fieldset; ikon düğmede erişilebilir ad/ipucu. Busy durumunda SVG'yi silmeden adı değiştir. Riskli işlemler/onaylarda görünür eylem metni kalır.
- Görünür odak, “İçeriğe geç”, modal odak yönetimi, Escape/kapat ve tetikleyiciye dönüş. Sekmeler `tablist/tab/tabpanel`, ok/Home/End ve doğru seçili durum.
- `prefers-reduced-motion` altında animasyon/yanıp sönme yok; renk tek başına anlam taşımaz. Uzun Türkçe adlar ve %200 yakınlaştırmada taşmayı kontrol et.

### Kumanda ve bildirim

- Kartta ad, gerçek mod, durum, süre/ölçüm, program ve kilit nedeni. Varsayılan tekrar notları mobilde gizlenebilir; ret/kilit/bayat veri görünür kalır.
- Komut: hazır → gönderiliyor → kabul/onay bekleniyor → taze veriden onaylandı; ret/zaman aşımı ayrı. HTTP 2xx çıkış değişimi kanıtı değildir; fiziksel geri bildirim yoksa onay komutlanan durum içindir.
- Beklerken ikinci komutu engelle; `aria-busy/aria-disabled` yanında JS koruması. Polling DOM'u yeniden kurmaz; odak/seçim/form taslağı korunur.
- Veri yaşını göster; bayatlıkta kumanda kapanır, son bilinen durum yazılır. Örnek polling 1 sn/stale 4 sn/onay 5 sn; hedefe göre profillenir. İlk veri yoksa kapalı varsayma.
- Güç simgesi açık yeşil, kapalı kırmızı, bilinmeyen/bayat/bekleyen gri; metin rozeti de vardır. Manuel/kilitli ama taze durum gerçek rengini korur. Kapalı çıkışın kırmızısı tek başına alarm değildir.
- Tek eylemli kart tamamen gerçek düğmeye bağlı dokunma hedefi olabilir. İkinci etkileşim varsa kartı kaplayan görünmez katman kullanma. Odak kart çevresinde görünürdür.
- Alt bildirim kaydet çubuğunu örtmez: başarı `role=status`, yaklaşık 5 sn; hata `role=alert`, elle kapanır; en fazla üç bildirim.
- Uyarı/alarm: kesikli 1 px çerçeve, 6 px köşe, hafif önem zemini, üçgen SVG + metin. Uyarı amber, kritik kırmızı, giderildi gri/onay simgesi. Okunma fiziksel arızayı temizlemez; hareket tercihi animasyona uygulanır. Alarm satırı sıktır: 10 px yazı, 4 px satır arası.

## 7. Ayarlar: ortak özellik kataloğu

Yedi bölüm standarttır; donanım yeteneği yoksa ilgili özellik uygulanmaz. Sensör/kalibrasyon ekleri aynı sözleşmeye uyar. İsimler API göçü talimatı değildir; mevcut alan adlarını koru.

| Bölüm | Özellikler ve davranış |
|---|---|
| Ağ | Cihaz/mDNS adı; DHCP/statik, IP, maske, gateway, iki DNS, mevcut adres/durum. Statik alanlar yalnız statik seçimde etkin. Kablosuz değişimi referansta Bakım alanındadır. |
| MQTT | Broker/port, kullanıcı, yeni parola/kayıtlı parolayı kaldır; kök topic; boşta heartbeat/aktif yayın; HA keşfi ve kapatırken eski kayıt temizliği. Sabit client ID okunabilir tanıdır. |
| Kanallar | Kanal adı, desteklenen giriş tipi (LATCH/TACTILE vb.) ve gerçek buton eylemi. Adet/giriş-çıkış ilişkisi profilden. Polarite/kalibrasyon/limit yalnız donanım ve API destekliyorsa. |
| Güvenlik | Bakım; giriş/sensör arızası failsafe; iletişim kaybı ve sürekli çalışma süresi sınırı. Uygun donanımda hacim/dry-run/ortam koruması. `0=kapalı` yalnız desteklenen limitte açık yazılır. |
| LED | WS2812B durum şeridi: canlı şerit durumu, %0–100 parlaklık, durum başına renk. Sıra bütün cihazlarda sabit: LED1 Durum (alarm yoksa yeşil, uyarı/alarmda yanıp söner), LED2 Ağ (yok/Wi-Fi/AP), LED3 MQTT (kesik/bağlı/tanımsız), LED4 mDNS (yok/hazır/devre dışı), LED5+ cihaza özgü. Ayrıntı `durum-ledleri.md`. |
| Erişim | Web kullanıcı adı/misafir telemetri; mevcut/yeni/tekrar web parolası; isteğe bağlı kurtarma sorusu/cevabı, tanımlı durumu/kaldırma; bağımsız OTA parolası/kaldırma/etkinleşme durumu; audit toplayıcı. |
| Bakım | Ayrı risk alanı: Wi-Fi tara/değiştir, yalnız Wi-Fi silip AP başlat, reboot, tek/tüm sayaç reseti, fabrika ayarları. Yıkıcı işlemin hedefi/kapsamı onayda yazılır. Alan saydam kırmızı zeminli (`--zone-bg`), kesikli çerçevelidir. |

### Form sözleşmesi

- Masaüstü dikey sekme; seçilide sol teal şerit/hafif zemin. Mobil üst bölüm kutuları. Seçim URL hash'inde saklanır.
- **Her bölüm ayrı kaydedilir.** Her sekme kendi formu ve kaydet çubuğudur (“<Bölüm> ayarlarını kaydet”, Geri al). Kayıt yalnız o bölümün görünür alanlarını doğrular ve gönderir; başka bölümdeki geçersiz, eksik veya henüz desteklenmeyen alan (ör. broker adresi) ağ/statik IP kaydını **engelleyemez** ve taslağı silinmez. Tek büyük “Ayarları kaydet” düğmesi kullanılmaz: ilk kurulumda statik IP, MQTT reddi yüzünden kaydedilemeyip cihaz DHCP adresiyle açılmıştır.
- Alan baseline'ı ile değişiklik izi, sekmede adet; bölüm çubuğunda kendi adedi + “diğer bölümlerde N”. Değişiklik yokken Kaydet/Geri al pasif ve çubuk normal akışta; değişiklik/hata varken yapışkan. Sayfadan çıkış uyarısı bütün bölümleri kapsar.
- Geri al yalnız kendi bölümünü son başarılı yükleme/kayda döndürür. Hata taslağı korur. Kayıt sırasında düzenlemeyi kilitle veya snapshot/sürümle koru; geç gelen cevap yeni düzenlemeyi silemez.
- İstemci sınırları sunucuyu yansıtır: tür, uzunluk, UTF-8 bayt sınırı, IP/mDNS, aralık/topic ve alan ilişkileri. Bölen/kalibrasyonda sıfır/negatif reddedilir. Aday bütünüyle doğrulanmadan hiçbir alan RAM'e uygulanmaz.
- Gizli sekmede geçersiz alan varsa sekmeyi aç, etiketini bildir, odağı taşı. `novalidate` ile `checkValidity/reportValidity` açık yönetilebilir. `form=` ile dışarıdan bağlı alan olaylarını ortak kapsayıcıda dinle.
- Bağımlı alan anlamlıyken görünür/etkin olur, taslak değeri korunur. Devre dışı alanın payload anlamını API belirler; gizlenince otomatik sıfırlama yoktur.
- Parola GET'ten doldurulmaz. Genel ayarda **boş giriş mevcut parolayı korur**, açık kaldırma eylemi siler. Alanı çıkarmak ile boş string göndermek ayrıdır. Özel web parola kaldırma formu farklıysa etkisini açık yaz/doğrula.
- Ayrı parola/kurtarma formunun kendi düğmesi/bildirimi vardır. Bir işlem (bölüm kaydı dahil) diğer sekme taslağını değiştirmez. Başarıda gönderilmiş/kanonik değeri baseline yap, gizli alanları temizle.
- Görünen cihaz adı (`adN`) Ağ › Cihaz kimliği'nde düzenlenir ve kayıtta üst başlık + tarayıcı sekme başlığı anında güncellenir; salt okunur kimlikler (SLUG, client ID) “cihaz adı” gibi etiketlenmez, ipucunda düzenlenebilir adın yeri yazılır.
- Kullanıcı girdisi `textContent` ile yerleştirilir; SSID/cihaz/kanal/program adını HTML yorumlama. Dinamik SVG/grafik öznitelikleri doğrulanmış değerlerle sınırlanır.

### LED, bakım ve denetim ayrıntıları

- LED standardı, anahtarlar ve sürücü kuralları `durum-ledleri.md`'dedir. Ayarlar'da önce canlı şerit durumu (cihazın bildirdiği durum indeksleri, kayıtlı renklerle; yanıp sönen durum CSS animasyonuyla, `prefers-reduced-motion`'da kesikli çerçeve), sonra parlaklık, sonra renk kartları.
- LED grup kartı: durum etiketi + renkli daire tetikleyici. Palet kart genişliğine bağlıdır, dar hücreye sıkışmaz/kaydet çubuğunun altında kalmaz. Bir anda bir palet açık.
- Referans 16 temel renk; mevcut özel renk değiştirilmeden korunur. Native radio/klavye, seçili işaret/renk adı gerekir. Seçimde kapanır; Kapat/Escape/dışarı dokunma çalışır, seçimsiz kapanma ayarı değiştirmez.
- LED animasyonu non-blocking/hız sınırlıdır. Keşif yayımlandı HA çevrimiçi demek değildir. UI tema seçimi fiziksel LED rengini değiştirmez.
- Wi-Fi reseti kablosuz kimliği temizler/statik etkinliği kapatır; MQTT/program/sayaç/web erişimini silmez. Kayıt başarısızsa reboot yok.
- Sayaç reseti tek/tüm seçiminden sonra hedef onayı ister; çıkışlar etkilenmez, açık kanalda birikim sürer. Proses referansları §4'e göre kaydırılır.
- Collector adres/port, gerçek yol, gönderim açık/kapalı, RAM kapasitesi ve reboot kaybı gösterilir. Kapasite RAM'den gelir; 24 olay evrensel değildir. HMAC anahtarı yalnız yetkili ayrı eylemle açılır.
- Audit ekranı zaman/tür rozeti/aktör-kaynak/olay/teslim durumu; alarm ekranı önem/koşul/okundu-giderildi/kilit reseti. Okunmanın sayfa oturumu mu kalıcı mı olduğu yazılır. Audit listesi rozetli ve sıktır (`arayuz-tasarimi.md` §9).

## 8. Erişim, oturum ve OTA

- Telemetri, ayar okuma, kumanda, yönetim, audit anahtarı ve OTA ayrı yetki yüzeyidir. Misafir okuma yalnız belirlenen telemetriyi açar.
- Web parolası isteğe bağlı olabilir; yokluğu görünür. Mevcut erişim politikasını sessizce değiştirme; yeni cihazın ilk kurulum politikasını profilde seç.
- Web parolası tuzlu standart KDF/sabit zamanlı karşılaştırma kullanır. Yükü MCU'da ölç; referans PBKDF2/6000 sayısını evrensel güvenlik seviyesi yapma. Yeniden kullanılacak Wi-Fi/MQTT sırları web parola doğrulama özetinden farklıdır.
- Sınırlı oturum/ömür, güvenilir rastgele token, deneme sınırı, logout ve parola değişiminde oturum politikası. Cookie HttpOnly/SameSite, HTTPS varsa Secure; yazma POST + CSRF/origin/token koruması. Sırlar/tokenlar log/telemetri/GET'e sızmaz.
- Oturum: kullanıcı/parola, beni hatırla, giriş/çıkış ve opsiyonel kurtarma. Referans 8 saat/14 gün/dört oturum; hedef bütçesi belirler.
- Kurtarma cevabı ikinci parola gibi korunur; Türkçe normalizasyon kararlı. Denemeler sınırlı; bilet kısa ömürlü/tek kullanımlık, yalnız parola değiştirir, yönetici oturumu değildir. Fiziksel/seri alternatif profilde tanımlanır.
- OTA parolası web parolasından bağımsızdır. UI kayıtlı/aktif/bekleyen değişimi ayırır, reboot gerekirse söyler. Referans rölenin **varsayılan şifresiz OTA** tercihi bütün yeni cihazlara dayatılmaz; yeni politika profilden gelir.
- ESP8266 ArduinoOTA referansı: isteğe bağlı 8–64 karakter; protokolün MD5 özeti sır gibi korunur/GET'e çıkmaz. Kütüphane reboot gerektiriyorsa durum gösterilir; etkin parola varken upload `--auth` ister. MD5 web KDF yerine kullanılmaz; sırlar Git'e yazılmaz.
- OTA öncesi güvenli proses ve gerekli flush sağlanır, çelişen komut reddedilir. Progress/hata/WDT/toparlanma, bölüm boyutu, kesinti, rollback/imza desteği hedefte doğrulanır. OTA auth şifreleme veya firmware imzası değildir.
- HTTP/MQTT şifrelenmiyorsa gerçek sınırı belirt; var olmayan TLS koruması gösterme.

## 9. Audit, teşhis ve performans

- Sınırlı RAM halkasında kapasite/taşma/kayıp görünürdür. Ayar, komut/ret, oturum/kurtarma, OTA/reset olayları sırsız kaydedilir.
- Kalıcı sıra aralığını kullanmadan önce rezerve et; boot'ta yalnız halka kapasitesi eklemek numara tekrarını önlemez.
- HMAC zinciri, cihaz kimliği, sıra/replay kontrolü birlikte yürür. HMAC şifreleme değildir; RAM taşması/reset kaybını geri getirmez. Kayıp aralık görünürdür.
- Kilit altında sahipliği belirli olay snapshot'ı al, kilitsiz gönder. JSON kütüphanesiyle serileştir, UTF-8'i ortadan kesme. Snapshot yoksa gövde kurmayı da aynı kilitte tamamla; halka belleğine açık pointer taşıma.
- Collector protokolünün başarılı kabulü (referansta HTTP 2xx) teslimdir. ACK'te yuva sınırı/halka nesli/seq eşleşir; sarılan halkadaki yeni kayıt eski cevapla teslim sayılmaz.
- Tanı: uptime, free/min heap, en uzun loop/tick, aşım, kuyruk, ağ/FS hatası, boots/faultBoots/reset nedeni; RTOS'ta yığın payı. WDT beslemek gecikmeyi çözmez, ayrı çekirdek tek başına sert gerçek zaman garantisi değildir.

## 10. MCU uyarlamaları

### ESP8266 / tek çekirdek

Kısa proses tick'i, süreli ağ çağrıları ve kooperatif servis kullan; ESP32 mutex/çekirdek kodunu kopyalama. JSON/UI/oturum/audit tamponları heap bütçesine göre sınırlı; PROGMEM/sıkıştırılmış varlık uygundur. Flash/Wi-Fi altında proses gecikmesini cihazda ölç. Pin/UART/LED/interrupt/boot strap ilişkisi hedef profilindedir.

### ESP32 ailesi / FreeRTOS

- Tam model ve `CONFIG_FREERTOS_UNICORE` ayarını kontrol et. Her ESP32 ailesi üyesi çift çekirdek değildir: ESP32/ESP32-S3 çift çekirdek kullanımına örnek, ESP32-S2/ESP32-C3 tek çekirdektir.
- Çift çekirdekte proses/ağ görevlerini ayırabilirsin. `xTaskCreatePinnedToCore` yerleşimini ölçüme göre seç; ağ hep core 0/proses hep core 1 şartı koyma. Öncelik/yığın/idle payını doğrula.
- ISR paylaşımında desteklenen portMUX/spinlock ve kısa kritik bölüm kullan; yalnız yerel interrupt kapatma diğer çekirdeği durdurmaz. Kritik bölümde bloklama yok. IDF görev yığını boyutunun bayt olmasına dikkat et. [Espressif FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html).
- WDT API/config kullanılan IDF/Arduino sürümüne göre seçilir. Idle görevlerini uyarı kaybolsun diye topluca WDT'den çıkarma; aç bırakmayı/uzun kritik bölümü düzelt. İlerlemeyi izleyen kapsam kur, kayıt/çıkarma/timeout değişimini gerekçelendir. [Espressif watchdog](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/wdts.html).
- Hedefte varlığı doğrulanırsa pulse counter, DMA/RMT, zamanlayıcı, PSRAM ve OTA rollback değerlendirilebilir. Aile isminden yetenek çıkarma; pin/çevrebirimi çatışması, ISR/flash/bellek kısıtını hedefin resmî belgesinden doğrula. Hızlandırıcı taşınabilir proses API'sinin arkasında kalır.

## 11. Offline teslim, kaynak düzeni ve revizyon

- HTML/CSS/JS/SVG/font cihazdan gelir; CDN/analitik/uzak font/internet zorunluluğu yok. Küçük UI için büyük framework zorunlu değildir.
- Tek düzenlenebilir kaynak/ortak tema kullan. Örnek `tools/ui/{index.html,app.css,app.js,theme.js,fonts}` → üretici → firmware varlıkları. Dizinler uyarlanabilir; token'ı sayfa başına elle çoğaltma.
- Deterministik üretim: satır sonu normalize, gzip zamanı sabit, adres sürümü içerik özeti. WOFF2 yeniden gzip olmak zorunda değil. Uzun CSS açıklamalarını gömülü varlık yerine belgede tut.
- İçerik özetli CSS/JS/font için uzun ömürlü immutable; HTML/hassas-canlı API için no-store. Font değişince onu referanslayan CSS sürümü de değişir. İlk yükleme/geçiş istek ve bayt maliyetini ölç.
- Yerel harici betik/event listener ile üretim CSP'sini uygula; inline handler/eval ekleme. SVG/data URI'yi CSP ile sınama şarttır; gevşek test dosyası üretim kanıtı değildir.
- Revizyon kaynak değişince artar, aynı kaynak yeniden derlenince değişmez. UI/API/MQTT/keşif/açılış/seri aynı build kimliğini taşır. Yeni build'de UI bildirir; her build'de tarih yazarak gereksiz değişiklik üretme.
- Referans `tools/build_info.py`, `tools/build_revision.json`, `include/build_info.h`; ilgili üretilmiş revizyon dosyaları proje kuralıyla commit'e girer. Başka projede eşdeğer mekanizma kur.

## 12. Doğrulama ve tamamlanma

Yalnız belge değiştiyse skill yapısı/kapsam/çelişki/bağlantı/kaynak eşlemesini doğrula; firmware build'iyle ilgisiz üretilmiş dosyaları değiştirme. Uygulama değiştiyse ilgili katmanları çalıştır:

| Katman | Kanıt |
|---|---|
| Native | Gerçek kodla ayar reddi/rollback, kayıt kesinti/kurtarma, dirty nesil yarışı, komut öncelik/kuyruk/no-op, sayaç/takvim sınırı, günlük geçmiş payload/tarih dönüşümü, MQTT kimlik/yayın hatası, oturum/kurtarma, audit ACK kimliği |
| Tarayıcı | Her sayfa/ayar sekmesi × iki tema × 390/768/1280 px, ayrıca çok dar ekran/%200 zoom; taşma, kontrast (alfa birleştirmeli), yazı ölçeği (10/11/12/14 px), odak, ad, hedef boyutu, dış istek, konsol ve CSP |
| UI akışları | Kabul/ret/zaman aşımı/çift komut, bayatlık/toparlanma, taslak koruma, geri al/koşullu alan/geçersiz gizli sekme, parola koru/kaldır, Wi-Fi/AP, LED paleti, program/alarm, risk onayı, tema/font/cache/revizyon |
| MCU build | Gerçek hedef/SDK, RAM/flash/OTA payı, bağımlılık/uyarılar ve uygun native C++ testleri |
| Saha | Gerçek power-cut, broker kaybı, geçmiş topic'inin bağlanma/gün devri/saatlik yayını, statik/DHCP/AP, sensör/giriş kopması, elektriksel boot/polarite, OTA altında proses, gerçek telefon, uzun süre heap/gecikme |

- Gerçek UI'dan sahte API'li tek HTML test sayfası üret; cihazsız araçtır. Ayrı HTTP fixture üretim CSP/cache başlıklarını uygular. Gecikme, 503, 409, onaysız kabul ve kayıt hatası enjekte edilir.
- Computed CSS ölç; kuralın varlığı görüntü kanıtı değildir. Masaüstü/telefon ekran görüntülerini gözle incele. Tarayıcı enjeksiyonunu ürün kaynağıyla karıştırma.
- Referans depoda UI değişirse `python tools/ui/test/build_test_html.py`, `node tools/ui/verify_ui.cjs`, `python tools/ui/assemble.py`/pre-build; davranış değişirse `python tools/run_native_tests.py`, ilgili Python testleri ve PlatformIO ESP8266 build. Yeni projede eşdeğer araç kur; olmayan komutu çalıştı diye yazma.
- Rapor davranışı, ortak/proje özel kararı, yapılan doğrulamayı ve açık donanım sınırını ayırır. Fixture ve build gerçek cihaz testi değildir; canlı test yoksa açık bırak.

## 13. Kaynak belgeden aktarım kararları

- Korunanlar: proses otonomisi, tek sahiplik/kilit sırası, nesil kontrollü kalıcılık, ağ kurtarma, MQTT kimlik/availability/taşınma, sayaç/takvim tuzakları, audit sıra/ACK, offline UI ve katmanlı test.
- Görsel değişim: siyah/hardal + neumorfizm yerine Scale token'ları/düz kompakt kontroller/Sans-Mono ayrımı. Uzak font yok; lisanslı yerel font desteklenir.
- Düzeltilen genellemeler: bozuk kayıt otomatik fabrika sıfırlaması değildir; volatile mutex değildir; idle WDT'yi kaldırmak standart çözüm değildir; bütün ESP32'ler çift çekirdek değildir; kısa yanlış GPIO seviyesi güvenli çözüm değildir.
- Profile bırakılanlar: pin/polarite/adetler, manuel-bakım önceliği, bantlar, oturum/kayıt/packet kapasitesi, süre, IP ve ilk OTA politikası.
- Bu dosya tek SKILL.md olarak taşınır. Depodaki önceki references klasörü tarihsel uygulama notlarıdır; bu sürüm için zorunlu değildir ve çelişen eski görsel kurallar uygulanmaz.