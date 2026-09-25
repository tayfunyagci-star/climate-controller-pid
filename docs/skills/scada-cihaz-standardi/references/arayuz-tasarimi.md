# Arayüz tasarımı (eski `scada-ui-design`)

Mikrodenetleyiciden (ESP8266/ESP32 vb.) sunulan, internet gerektirmeyen SCADA cihaz arayüzü standardı. Kaynak: `4chRelayModule` projesinin 2026-09-16 UI'si (tools/ui, 74 ekran + 35 akış testiyle doğrulanmış); 2026-09-24'te yazı ölçeği ve kompakt kabuk güncellemesi işlendi. Bu dosya tek başına yeterlidir.

**Kapsam ayrımı.** Bu belge görünümü, etkileşimi, ayarlar sayfasını, UI teslimini ve UI testini tanımlar. Proses güvenliği, kalıcılık, MQTT, oturum kriptografisi ve MCU mimarisi `cihaz-temeli.md`'nin alanıdır. UI kuralında çelişki olursa bu belge, cihaz davranışında cihaz temeli geçerlidir.

## 1. İş akışı

1. Projeyi oku: talimat/günlük, mevcut UI, API ve MQTT alan adları, donanım yetenekleri. Kullanıcının verilmiş kararlarını yeniden sorma.
2. **UI profili** çıkar: hangi sayfalar var, kanal/sensör adedi, hangi ayar bölümleri ve alanları uygulanıyor. §8.4 kataloğundaki her alanı `uygulanıyor / donanımda yok / kapsam dışı` diye işaretle. Olmayan donanım için alan, kart veya sahte veri gösterme.
3. Yeni projede §8.4 anahtarlarını aynen kullan. Mevcut projede API alan adlarını **değiştirme**; UI tarafında `anahtar eşleme tablosu` kur.
4. Yalnız düzenlenebilir kaynakları değiştir (§2). Üretilmiş header'ı elle düzenleme.
5. §11 testlerini çalıştır, ekran görüntülerini gözle incele, sonucu ve açık kalanları (gerçek telefon, cihaz üzerinde ölçüm) günlüğe yaz.

## 2. Kaynak düzeni ve teslim

```
tools/ui/
  index.html      tek sayfa iskeleti; bütün sayfalar <section id=...> olarak
  app.css         token + bileşen + mobil katmanı (tek dosya)
  app.js          davranış; inline handler yok
  theme.js        <head> içinde, ilk stilden önce tema uygular
  fonts/*.woff2   IBM Plex resmî alt kümeleri + OFL.txt
  assemble.py     -> include/ui_generated.h (gzip + ?v sürüm) ; --dump DIR
  verify_ui.cjs   Playwright doğrulaması
  test/mock_device.js, test/build_test_html.py -> test/ui_test.html
```

- Rota başına ayrı HTML yok: firmware `/`, `/settings`, `/schedule`, `/alarms`, `/audit`, `/login` için aynı HTML'i verir; JS `location.pathname` ile bölümü açar. Sayfa geçişinde yalnız ~5 KB HTML iner.
- `assemble.py`: satır sonunu normalize et; metin varlıklarını `gzip(level 9, mtime=0)` ile PROGMEM'e göm; CSS/JS/font adreslerine `?v=<sha256[:10]>` ekle. Font özeti CSS'e, CSS/JS özeti HTML'e yazılır. WOFF2'yi yeniden gzip'leme.
- Firmware başlıkları: sürümlü istek → `Cache-Control: public, max-age=31536000, immutable`; HTML ve `/api/*` → `no-store`. Gzip'li yanıtta `Content-Encoding: gzip` ve açık uzunluk.
- Güvenlik başlıkları: `Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`, `X-Content-Type-Options: nosniff`, `X-Frame-Options: DENY`, `Referrer-Policy: same-origin`. İkonlar DOM içi SVG olduğundan `data:` izni gerekmez. Inline `style=`/`<script>`/`on*=` yok; dinamik stil yalnız sınıf veya SVG özniteliğiyle.
- CDN, uzak font, analitik, framework yok. Hedef bütçe (referans): HTML+CSS+JS ~112 KB ham / ~33 KB gzip, font ~105 KB.
- Build kimliği: kaynak özetinden türeyen revizyon (`r<N>`), aynı kaynak yeniden derlenince değişmez. `fwVersion` + `fwBuild` her API kimlik yanıtında gelir; UI başlık bilgi şeridinde ve altbilgide gösterir, çalışırken değişirse “Yeni firmware çalışıyor: rN” bildirimi verir.

## 3. Tasarım token'ları

Bütün bileşen renkleri semantik token'dır. Koyu tema varsayılan tanımdır; açık tema `html[data-theme=light]` ile ezilir. Yeni projede bu bloğu olduğu gibi kopyala; tarihsel override katmanları (eski siyah/hardal, neumorfik gölge, `!important` yığını) taşınmaz.

```css
:root{
 --navy:#132a4c;--navy-2:#24406b;--navy-deep:#0e1f3b;--teal:#1d8f74;--teal-2:#3fb897;--teal-soft:#9eebd1;
 --cream:#fbf7f1;--card:#fff;--mist:#f4f1ea;--line:#e6e1d6;--ink:#20242c;--slate:#5b6472;
 --bg:#0e1b2d;--bg-panel:#14263e;--bg-raised:#192e49;--bg-input:#102037;--border:#718398;--border-soft:#34485f;
 --text:#e5edf5;--text-strong:#fff;--text-dim:#bac8d7;--heading:#e5edf5;
 --brand:#9eebd1;--brand-dim:#163c38;--brand-line:#3fb897;
 --green:#9eebd1;--red:#ffb19e;--amber:#e7b96b;--blue:#b3ccef;
 --pw-on:#5fd37a;--pw-off:#ff7a70;--pw-unknown:#8795a6;
 --warn-text:#f2c35f;--warn-bg:#2a2414;--warn-line:#b88a2c;
 --crit-text:#ff8f85;--crit-bg:#2e1b1c;--crit-line:#d9574f;
 --clr-text:#bac8d7;--clr-bg:transparent;--clr-line:#5a6b80;
 --overlay:#00000099;--r-control:6px;--r-panel:4px;
 --fs-2xs:10px;--fs-xs:11px;--fs-s:12px;--fs-title:14px;--zone-bg:#ff7a701a;
 --font-body:"IBM Plex Sans",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
 --font-mono:"IBM Plex Mono",Consolas,"SFMono-Regular",Menlo,"Liberation Mono",monospace;
}
html[data-theme=light]{
 --bg:var(--cream);--bg-panel:var(--card);--bg-raised:var(--card);--bg-input:var(--mist);
 --border:#8c918e;--border-soft:var(--line);--text:var(--ink);--text-strong:var(--navy);--text-dim:var(--slate);--heading:var(--navy);
 --brand:#14705b;--brand-dim:#eaf7f1;--brand-line:var(--teal);
 --green:#14705b;--red:#a03c27;--amber:#795521;--blue:var(--navy-2);
 --pw-on:#1e7d32;--pw-off:#c62828;--pw-unknown:#7c828b;
 --warn-text:#735000;--warn-bg:#fff5d8;--warn-line:#c2922a;
 --crit-text:#a8241c;--crit-bg:#fdeceb;--crit-line:#d0473f;
 --clr-text:#58606b;--clr-line:#a9b0b8;--overlay:#00000066;--zone-bg:#c6282814;
}
[hidden]{display:none!important}
```

- Başlık bandı iki temada lacivert (`--navy-deep` → `#16375c` degrade), beyaz başlık, teal-soft üst etiket. Gövde açıkta krem/beyaz, koyuda lacivert.
- Düz yüzeyler: 1 px çerçeve, panel köşesi 4 px, kontrol köşesi 6 px; gölge, parıltı, neumorfizm yok.
- Ölçüler: kök yazı 12 px; masaüstü kontrol yüksekliği 36 px, iç boşluk düğme `5px 8px`, alan `5px 6px`; telefonda dokunma hedefi ≥44×44 px. Yazı boyutları yalnız §4'teki 10/11/12/14 px ölçeğindedir.
- Kırılımlar: `≤600px` telefon katmanı, `≤1000px` kart ızgarası 2 sütun, `≤340px` çok dar. Masaüstü azami genişlik ~1150 px.
- Tema: `theme.js` kayıtlı tercih (`localStorage 'scada-tema'`) → `prefers-color-scheme` → açık; `try/catch` ile. Tema tercihi cihaza gönderilmez.
- Kontrast: metin ≥4.5:1, anlam taşıyan simge/odak/kontrol sınırı ≥3:1; pasif, hover, hata ve disabled durumları dahil iki temada ölçülür. Pasif birincil düğmeye açık pasif renk ver (1:1 kontrast hatası yaygındır).

## 4. Tipografi ve ikonlar

- **IBM Plex Sans** 400/600: açıklama, form, düğme metni. **IBM Plex Mono** 400: başlıklar, menü, sekme, grup başlığı, etiket, rozet, ölçüm, kimlik şeridi, altbilgi, kod. Ölçümlerde `font-variant-numeric:tabular-nums`.
- **Yazı ölçeği (kullanıcı kararı, 2026-09-24).** Yalnız `10`, `11`, `12` px; en büyük başlık `14` px. Yazı boyutu `rem`/`em` ile yazılmaz; `--fs-2xs/xs/s/title` token'ları kullanılır.

| Boyut | Kullanım |
|---|---|
| 14 px | yalnız cihaz adı (`h1`) |
| 12 px | gövde, form alanı, düğme, menü, `h2/h3`, sistem durumu başlığı |
| 11 px | yardımcı metin/ipucu, kimlik şeridi, rozet, veri yaşı, metrik değeri, denetim satırı, altbilgi |
| 10 px | üst etiket, alan/metrik etiketi, kanal etiketi, **alarm satırı** |

  Grafik ve ölçerlerin **sayısal değerleri** (`.chart-value`, `.gauge-value`) bu ölçeğin dışında büyük olabilir; eksen, etiket, başlık ve diğer bütün yazılar ölçekte kalır. Alarm yazısı ölçeğin en küçüğü olan 10 px'tir. Telefonda alan yazısı da 12 px'tir; iOS Safari 16 px altındaki alanda odakta sayfayı yakınlaştırır. Bu bilinçli ödünleşimdir; gerekirse yalnız `@media(pointer:coarse)` içinde 16 px'e çıkar.
- IBM'in resmî Latin1 + Latin2 WOFF2 dosyaları **değiştirilmeden** gömülür (SIL OFL 1.1; “Plex” ayrılmış ad olduğundan kendi alt kümeni “IBM Plex” adıyla dağıtma). Her yüz için iki `@font-face` (`unicode-range` Latin1 / Latin2; Türkçe ğ, İ, ı, Ş Latin2'dedir), `font-display:swap`. Fontlar gelmezse sistem yığını çalışır. Bellek darsa belgelenmiş “sistem fontu profili” seçilebilir; CDN'e geçilmez.
- İkonlar 24×24 viewBox, `fill:none; stroke:currentColor; stroke-width:2; stroke-linecap:round; stroke-linejoin:round`, JS ile `createElementNS` üretilir; emoji/ikon fontu yok. Standart set:

```js
const ICONS={plus:'M12 5v14M5 12h14',x:'M6 6l12 12M18 6L6 18',save:'M5 3h11l3 3v15H5zM8 3v6h8V3M8 21v-7h8v7',
 undo:'M9 14L4 9l5-5M4 9h10a6 6 0 0 1 0 12h-3',trash:'M4 7h16M10 11v6M14 11v6M6 7l1 14h10l1-14M9 7V4h6v3',
 pencil:'M4 20h4L19 9l-4-4L4 16zM13.5 6.5l4 4',pause:'M8 5v14M16 5v14',play:'M7 4l13 8-13 8z',check:'M5 12.5l4.5 4.5L19 7',
 unlock:'M5 11h14v10H5zM8 11V7a4 4 0 0 1 7.5-2',key:'M14.5 9.5a4 4 0 1 0-3 3.9L13 15h2v2h2v2h3v-3l-5.4-5.4',
 refresh:'M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5',logout:'M10 5H5v14h5M15 8l4 4-4 4M19 12H9',
 wifi:'M2.5 9a14 14 0 0 1 19 0M5.5 12.5a9.5 9.5 0 0 1 13 0M9 16a4.5 4.5 0 0 1 6 0M12 19.5h.01',
 wifioff:'M3 3l18 18M5.5 12.5a9.5 9.5 0 0 1 5-2.4M9 16a4.5 4.5 0 0 1 6 0M12 19.5h.01M2.5 9a14 14 0 0 1 7-3.6M14 5.3a14 14 0 0 1 7.5 3.7',
 reboot:'M12 3v6M6.3 6.3a8 8 0 1 0 11.4 0',timer:'M12 8v5l3 2M9 2h6M12 21a8 8 0 1 0 0-16 8 8 0 0 0 0 16z',
 factory:'M3 21V10l6 4V10l6 4V4h6v17zM7 17h2M12 17h2M17 17h2',power:'M12 3v9M6.35 5.65a8 8 0 1 0 11.3 0',
 warn:'M12 3.5L2.5 20h19zM12 10v4.5M12 17.5v.01'};
```

- Düğme sözleşmesi: HTML'de `<button data-icon="save">Metin</button>`. `iconize()` metni `.sr-only` span'a taşır, `title` ekler, sınıf `icon-only` (36×36; telefonda 44×44). Metnin görünmesi gerekiyorsa `data-text` → `icon-text` (ikon 16 px + metin). **Riskli ve onay düğmeleri daima görünür metinlidir** (Kaydet, Onayla, Yeniden başlat, Fabrika ayarları, Wi-Fi sil). Meşgul durumda SVG silinmez, erişilebilir ad değişir, `aria-busy=true`.

## 5. Kabuk ve yerleşim

- **Başlık**: kutu `--navy-deep` degrade, 6 px köşe, iç boşluk `14px 16px`. Üst etiket (kurum/ürün ailesi, Mono 10 px, teal-soft, harf aralıklı), altında cihaz adı (`h1`, API'den, Mono 14 px), sağda iki ikon düğme: bilgi (`aria-expanded/aria-controls="identity"`) ve tema; düğmeler masaüstünde 36 px, telefonda 44 px. Kimlik şeridi başlığın altından ince düz çizgiyle (`1px solid #547087`) ayrılır: `IP · mDNS(.local) · İstemci IP · Firmware · Revizyon`, Mono 11 px; etiket soluk, değer beyaz, **kutusuz düz metin**. Bilgi düğmesi şeridi her genişlikte açıp kapatır: masaüstünde varsayılan açık, telefonda kapalı (kapalıyken yalnız IP görünür).
- **Menü**: Kontrol `/`, Programlar `/schedule`, Alarmlar `/alarms`, Denetim `/audit`, Ayarlar `/settings`, Oturum `/login`. Her öğe **ikon (16 px) + Mono 12 px etiket**; ikon her genişlikte görünür. Menü başlığın içinde, kimlik şeridinin altındadır, ayrı çizgi yok. Masaüstünde altı eşit sütun (`flex:1 1 0`, öğe 36 px yüksek, 6 px köşe, kenarlık şeffaf, hover `#ffffff14`); etkin sayfa `aria-current=page` ile `--navy-2` dolgu + `--teal-2` 1 px kenarlık. Telefonda aynı öğeler ≥44 px yükseklikte sürekli görünür 3×2 ızgara, aynı etkin biçim. Desteklenmeyen modülün sayfası menüden çıkarılır. İkonlar: power, takvim, çan, belge, ayar, kullanıcı (24×24 viewBox, stroke 2).
- `document.title = "<cihaz adı> · <sayfa>"`. “İçeriğe geç” bağlantısı ilk odaktır.
- **Altbilgi**: `FW <sürüm> · <revizyon> · <saat | Saat bekleniyor>`.
- **Genel uyarılar** (`main` başında): bayat veri (`role=alert`, kritik), parola tanımsız (uyarı), kurulum/AP modu (uyarı, kurulum adımlarıyla).
- **Kontrol kartları**: masaüstü 4, ≤1000 px 2, ≤340 px 1 sütun; kanal adedi profilden. Kartta numara (Mono `01`), ad, mod rozeti (AUTO/MANUEL/KİLİT), durum metni, süre/ölçüm, program özeti, kart içi mesaj. Referans güç düğmesi dairesel, telefonda 64 px.
- **Durum çubuğu** (bölüm başlığının sağında): yeşil nokta + `Canlı · şimdi` (veri yaşı; bayatta kırmızı) ve üç hap `Wi-Fi · Hazır`, `MQTT · Hazır`, `Keşif · Hazır`. Mono 11 px, 1 px kenarlık, tam yuvarlak; hazır yeşil, yok kırmızı, durum metinle de yazılır. Cihazda olmayan bağlantı (Ethernet'te Wi-Fi) gösterilmez.
- **Sistem durumu**: `<details class="diag-panel">`; başlık `h2` 12 px, kapalı özet “çalışma süresi · boş RAM” (11 px) + chevron; açık/kapalı tercihi `localStorage 'scada-diag'`. Panel iç boşluğu `6px 12px`. Açıkken metrikler `<dl>`: masaüstünde 4, telefonda 2 sütun; hücre = etiket (Mono 10 px, soluk) + değer (Mono 11 px), hücre altında 1 px ince çizgi, hücre dikey boşluğu 5 px. Metrikler: uptime, free/min heap, en uzun döngü, aşım, boots/faultBoots, reset nedeni, ağ modu, RSSI, MQTT, FS hatası.

## 6. Bileşen kataloğu

| Bileşen | Kural |
|---|---|
| Panel | `section.panel`, `--bg-panel`, 1 px `--border-soft`, 4 px köşe, iç boşluk 12–16 px (telefonda 12). Başlık `h3` Mono. Grup başlığı `h4.group-heading` Mono, küçük, `--text-dim`. |
| Form ızgarası | `.form-grid` masaüstü 2 sütun, telefonda 1; `.full` tam satır; kanal ayarları `.three`. Etiket alanın üstünde, yardım metni `small.field-hint` + `aria-describedby`. |
| Onay kutusu | `.field.toggle.full`, etiket metni sağda, satır tamamı tıklanır. |
| Seçim | 2–4 seçenekli `<select>` görünümde radyo düğme grubuna (`role=radiogroup`, `.radio-choice`) çevrilir; gizli select değer kaynağı olarak kalır, `change` kabarcıklanır. Uzun listelerde native select. |
| Aralık | `input[type=range]` + `<output>` (`%` birimi). |
| Düğmeler | Varsayılan ikincil; `.primary` teal dolgu (tek birincil eylem); `.danger` kırmızı çerçeve/metin. 6 px köşe; güç düğmesi daire. |
| Uyarı kutusu | `.notice` + `warn`/`critical`/`cleared`: 1 px **kesikli** çerçeve, 6 px köşe, hafif önem zemini, 16 px üçgen (giderildi: onay) SVG + metin. Uyarı sarı/amber, kritik kırmızı, giderildi gri. Renk tek başına anlam taşımaz; metin önemi söyler. **Alarm satırı** (Alarmlar sayfası) bu kutunun sık biçimidir: yazı 10 px, dikey iç boşluk 2 px, satır arası 4 px, 14 px ikon; “Okundu” düğmesi masaüstünde 32 px, telefonda 44 px. |
| Kırmızı alan | `section.panel.red-zone`: 1 px kesikli `--red` çerçeve, **saydam kırmızı zemin** (`--zone-bg`; koyuda `#ff7a701a`, açıkta `#c6282814`). Opak zemin kullanılmaz; alttaki tema zemini görünür. Başlık 12 px Mono, büyük harf. |
| Bildirim (toast) | Sağ alt, kaydet çubuğunun üstünde; başarı `role=status` 5 sn, hata `role=alert` elle kapanır; en fazla 3; kapat ikon düğmesi 32 px. |
| Kaydet çubuğu | §8.6. |
| Diyalog | Native `<dialog>` + `showModal()`; başlık satırında `h2` ve `x` kapat; Escape kapatır, odak tetikleyiciye döner; alt satırda Vazgeç + birincil eylem. Onay diyaloğu metni işlemin hedefini ve kapsamını yazar. |
| Rozet/sayı | Mono, küçük; sekme değişiklik sayısı amber çerçeveli yuvarlak rozet, `aria-label="N değişiklik"`. |
| Odak | `:focus-visible` 2 px `--brand` çerçeve; kart kumandasında kartın çevresine kesikli odak. |
| Hareket | Yalnız kısa geçiş/darbe; `prefers-reduced-motion: reduce` altında animasyon yok. |

## 7. Kontrol ve komut akışı

- Veri: `GET /api/data` 1 sn aralıkla; `STALE_MS=4000` sonrası bayat. Bayatlıkta kumanda kapanır (`aria-disabled`), son bilinen durum ve veri yaşı (“3 sn önce”) yazılır. İlk veri gelmeden durum “bilinmiyor”, kapalı varsayılmaz.
- Güncelleme yerinde yapılır: kartlar bir kez kurulur, polling yalnız metin/sınıf değiştirir; odak, açık diyalog ve form taslağı korunur.
- Komut fazları: `hazır → gönderiliyor → onay bekleniyor → onaylandı` | `reddedildi` | `zaman aşımı`. POST 2xx/202 yalnız kabuldür; onay, taze `/api/data` istenen durumu gösterince verilir (`CONFIRM_MS=5000`). Bekleyen komutta ikinci tıklama JS ile engellenir. Ret mesajı (409 kilit/fiziksel kumanda, 503 kuyruk/kesik) kart içinde `critical` notice olarak kalır.
- Güç simgesi rengi: açık `--pw-on` yeşil, kapalı `--pw-off` kırmızı, bilinmiyor/bayat/bekliyor `--pw-unknown` gri; ayrıca metin rozeti. Kapalının kırmızısı alarm değildir.
- Kartın tamamı tıklanır: gerçek `<button>` korunur, `button::after{position:absolute;inset:0}` kartı kaplar. Kartta ikinci etkileşim varsa bu katman kullanılmaz.
- Fiziksel geri bildirim yoksa sayfada “Gösterilen durum komutlanan çıkıştır” notu bulunur.

## 8. Ayarlar sayfası standardı

### 8.1 İskelet

```html
<section id="settings" hidden>
 <div class="section-head"><h2>Cihaz ayarları</h2></div>
 <div class="tabs" id="settings-tabs" role="tablist" aria-orientation="vertical" aria-label="Ayar bölümleri">
  <button type="button" role="tab" id="tab-net" aria-controls="panel-net"><svg class="m-ico">…</svg><span class="tab-label">Ağ</span></button>
  … mqtt, io, safety, led, access, maint …
 </div>
 <form id="sf-net" novalidate data-sec="net"></form> <form id="sf-mqtt" novalidate data-sec="mqtt"></form> …  <!-- bölüm başına form -->
 <div role="tabpanel" id="panel-net" aria-labelledby="tab-net">
  <section class="panel"><h3>…</h3><div class="form-grid"><!-- alanlar form="sf-net" --></div></section>
  <div class="savebar" id="savebar-net"><p id="dirty-text-net" class="dirty-text">Kaydedilmemiş değişiklik yok</p>
   <div class="savebar-actions"><button type="button" id="revert-net" data-icon="undo" disabled>Geri al</button>
   <button type="submit" form="sf-net" id="save-net" class="primary" data-icon="save" data-text disabled>Ağ ayarlarını kaydet</button></div></div>
 </div>
 …
</section>
```

- Masaüstü: solda dikey sekme sütunu (~180 px) + içerik. Seçili sekme: **sol 3 px `--brand` şerit**, `--brand-dim` zemin; alt çizgi yok. Hover `--bg-input`.
- Telefon (≤600 px): sekme sütunu yerine içeriğin üstünde **4 sütunlu bölüm kutuları** (ikon 20 px + etiket, ≥58 px yükseklik), ≤340 px'de 3 sütun; içerik tam genişlik.
- Klavye: ←/→/↑/↓ döngüsel, Home/End; `aria-selected`, roving `tabindex`. Seçim `#<bölüm>` hash'inde (`history.replaceState`), sayfa yenilenince aynı bölüm açılır.

### 8.2 Standart bölümler

Sıra ve kimlikler sabittir; donanımda karşılığı olmayan bölüm tamamen çıkarılır, sıra korunur.

| # | id | Sekme | İkon | İçerik paneli |
|---|---|---|---|---|
| 1 | `net` | Ağ | wifi | “Cihaz kimliği” + “IP yapılandırması” grupları |
| 2 | `mqtt` | MQTT | yayın/anten | Broker, kimlik bilgisi, topic, yayın aralıkları, keşif |
| 3 | `io` | Kanallar (veya Sensörler/Girişler) | ızgara | Kanal başına `fieldset.panel` (legend “Kanal 01”) |
| 4 | `safety` | Güvenlik | kalkan | Bakım modu, arıza/iletişim/süre korumaları |
| 5 | `led` | LED | ampul | Canlı şerit durumu + parlaklık + durum renkleri (WS2812B standart donanım; yalnız şeridi olmayan eski kartta çıkarılır) |
| 6 | `access` | Erişim | anahtar | “Web erişimi ve parola” paneli + “Denetim kaydı ve uzak toplayıcı” paneli |
| 7 | `maint` | Bakım | uyarı | “⚠ Kırmızı alan”: kablosuz değişimi ve yıkıcı işlemler |

Ek modül (kalibrasyon, sayaç hedefleri, Ethernet) gerekiyorsa `io` ile `safety` arasına kendi sekmesi olarak eklenir; aynı alan sözleşmesine uyar.

### 8.3 Bildirimsel alan tanımı

Alanlar HTML'de elle yazılmaz; `definitions` sözlüğünden üretilir. Biçim: `[ad, etiket, tür, seçenekler]`.

| Seçenek | Anlam |
|---|---|
| `ml` / `minl` | `maxLength` / `minLength` (sunucu sınırıyla aynı; UTF-8 bayt sınırı varsa gönderimden önce `TextEncoder` ile ayrıca denetle) |
| `min`, `max` | sayı/aralık sınırı |
| `req` | zorunlu |
| `pat` | `pattern` (tarayıcı `v` bayrağı kullanır: sınıf içindeki `-` kaçırılır, ör. `[A-Za-z0-9\-]`) |
| `hint` | alan altı yardım metni |
| `dep` | bağımlı olduğu onay kutusu; kapalıyken alan gizli **ve** disabled, taslak değer korunur |
| `off` | işaretlenince bu alanı devre dışı bırakıp boşaltan “kaldır” onay kutusu (sır alanları) |

```js
function field([name,label,type,o={}],value){ /* .field sarmalayıcı, label>input, input.form='settings-form',
 id='f-'+name; password alanı daima boş başlar ve autocomplete='new-password'; checkbox -> .toggle.full;
 range -> output; hint -> small.field-hint + aria-describedby; dep/off -> data-dep / data-off */ }
```

IPv4 deseni: `((25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.){3}(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)`, `inputmode=decimal`.

### 8.4 Standart alan kataloğu

Yeni projede anahtarlar aynen kullanılır. Etiketler Türkçe standart metindir.

**Ağ (`net`)**

| Grup | Anahtar | Etiket | Tür / kural |
|---|---|---|---|
| Cihaz kimliği | `adN` | Cihaz adı | text, zorunlu, ≤64 |
| Cihaz kimliği | `mdns` | mDNS adı | text, zorunlu, ≤63, `[A-Za-z0-9]([A-Za-z0-9\-]*[A-Za-z0-9])?`, ipucu “Harf, rakam ve tire; “.local” eki eklenir.” |
| IP yapılandırması | `staticEnabled` | Statik IP kullan | checkbox; ipucu “Kapalıyken adres DHCP ile alınır. Statik bağlantı kurulamazsa cihaz DHCP’ye döner.” |
| IP yapılandırması | `staticIP`, `gateway`, `subnet` | IP adresi, Ağ geçidi, Alt ağ maskesi | IPv4, zorunlu, `dep:staticEnabled` |
| IP yapılandırması | `dns1`, `dns2` | Birincil DNS, İkincil DNS | IPv4, isteğe bağlı, `dep:staticEnabled`; dns1 ipucu “Boşsa ağ geçidi kullanılır.” |

Kablosuz kimlik (`ssid` ≤32, `pass` ≤64 password `off:clearWifiPassword`, `clearWifiPassword` “Kayıtlı Wi-Fi parolasını sil (açık ağ)”) aynı formun parçasıdır ama **Bakım › Kırmızı alan** içinde gösterilir. Ethernet cihazında Wi-Fi alanları ve tarama yoktur.

**MQTT (`mqtt`)**

| Anahtar | Etiket | Tür / kural |
|---|---|---|
| `mqS` | Broker adresi | text ≤63 (host veya IPv4; boş = MQTT kapalı) |
| `mqP` | Broker portu | number 1–65535, zorunlu |
| `mqU` | Kullanıcı adı | text ≤64 |
| `mqPw` | Yeni MQTT parolası | password ≤128, `off:clearMqttPassword`, ipucu “Boş bırakılırsa kayıtlı parola korunur.” |
| `clearMqttPassword` | Kayıtlı MQTT parolasını sil | checkbox (yalnız UI; gönderimde `mqPw:""` olur) |
| `mqT` | Kök topic | text ≤96, zorunlu, `+ # boşluk` yok |
| `heartbeat` | Boşta kalp atışı (saniye) | number 10–90; ipucu “Tüketicinin bayat veri süresinden kısa olmalı.” |
| `activeReport` | Çıkış açıkken yayın (saniye) | number 1–10 |
| `adE` | Otomatik keşif (Home Assistant) | checkbox; ipucu “Kapatılınca yayımlanmış keşif kayıtları silinir.” |

Salt okunur tanı: sabit client ID ve bağlantı durumu (düzenlenmez).

**Kanallar (`io`)** — kanal başına `fieldset`, `.form-grid.three`

| Anahtar (dizi) | Etiket | Tür / kural |
|---|---|---|
| `chN[i]` | Kanal adı | text zorunlu, ≤23 bayt UTF-8 |
| `it[i]` | Giriş tipi | seçim: `0` “LATCH · Kalıcı anahtar (ON/OFF)”, `1` “TACTILE · Bas-çek buton” |
| `ta[i]` | Buton işlemi | seçim; yalnız `it[i]==1` iken görünür |

Formda düz alanlar `ch<i>/it<i>/ta<i>` olarak üretilir, gönderimde dizilere toplanır. Polarite, kalibrasyon, limit yalnız donanım ve API destekliyorsa aynı fieldset'e eklenir. Bölüm ipucu: “Buton işlemi yalnız giriş tipi “Buton” olan kanallarda kullanılır.”

**Güvenlik (`safety`)**

| Anahtar | Etiket | Tür / kural |
|---|---|---|
| `maintenance` | Bakım modu | checkbox; ipucu “Uzak kumanda kapatılır; AUTO çıkışlar kapanır.” |
| `inputFailsafe` | Giriş modülü arızasında AUTO çıkışları kapat | checkbox |
| `offlineSeconds` | İletişim kaybı sınırı (sn) | number 0–86400, ipucu “0: kapalı” |
| `maxOnSeconds` | AUTO sürekli açık süre sınırı (sn) | number 0–86400, ipucu “0: kapalı” |

Proses eki (hacim, dry-run, sıcaklık) aynı panelde, `0=kapalı` yalnız desteklenen limitte.

**LED (`led`)** — ayrıntı ve firmware tarafı `durum-ledleri.md`

- Panel “LED durumu (canlı)”: şeritteki her LED için daire + “LED n <grup>” + durum adı; `/api/data.led_states` (durum indeksleri) ve kayıtlı renklerle. Sürücü hatası (`led_ok:false`) not satırında yazılır.
- Panel “LED parlaklığı”: `ledB` Parlaklık (%) range 0–100, değer `output` ile yanında.
- Panel “LED renkleri”: grup kartları (başlık “LED n · <grup>”); her grup = durum başına satır (etiket + renkli daire + renk adı). Ortak gruplar ve durumlar `<grup><0..2>`, **sıra sabit**: `cls` LED 1 · Durum (Normal, Uyarı, Alarm), `clw` LED 2 · Ağ (Bağlantı yok, Wi-Fi bağlı, AP kurulum), `clq` LED 3 · MQTT (Kesik, Bağlı, Tanımsız), `clm` LED 4 · mDNS (Yok, Hazır, Devre dışı). LED 5+ cihaza özgü grup (ör. röle kartında `clr` Röle/Çıkış: Kapalı, AUTO açık, MANUEL açık; iklim kontrolöründe `clr` Isıtma, `clf` Fan). Değer `#rrggbb`. Eski `clh` Keşif grubu fiziksel LED değildir; keşif durumu MQTT/tanı ekranında gösterilir.
- Palet: 16 temel renk (Siyah (sönük), Beyaz, Kırmızı, Yeşil, Mavi, Sarı, Turkuaz, Mor, Turuncu, Pembe, Lime, Teal, Lacivert, Eflatun, Gri, Bordo). Listede olmayan mevcut renk “Mevcut renk (korunur)” olarak başa eklenir. `<details>` ile açılır, **grup kartı genişliğine** yerleşir, aynı anda tek palet açık; radyo + renk dairesi + ad; fare/dokunuş seçimi ve Enter/Boşluk kapatır, ok tuşları yalnız seçimi değiştirir; Kapat/Escape/dışarı tıklama seçimsiz kapatır. Palet açılmadan doldurulur (boş kutu görünmez). Daire düğmesinin adı: “<grup> · <durum> · <renk> rengini değiştir”. Renk CSSOM (`el.style`) ile verilir; `style=` özniteliği CSP'de engellidir.

**Erişim (`access`)** — panel “Web erişimi ve parola”, dört grup, bu sırayla:

1. *Erişim* (ana form): `user` Web kullanıcı adı (≤32, zorunlu), `guestRead` Misafirler durum okuyabilir.
2. *OTA parolası* (**ayrı form**, `POST /api/ota/password {password}`, `""` = kaldır): parolasızken panelin başında `notice warn` “OTA parolasız açık: aynı ağdaki herkes bu cihaza firmware yükleyebilir.” ve üstte kalıcı genel uyarı (`/api/data.ota_password_set:false`). Durum satırı `Durum: <b id="ota-state">` → “Tanımlı değil · OTA parolasız açık” (amber) / “Tanımlı · yüklemede parola (--auth) gerekir” / reboot gerektiren kütüphanede “Kaydedildi · yeniden başlatma bekliyor”. Alanlar: Yeni OTA parolası + tekrar (8–64); düğmeler “OTA parolasını kaydet” ve “Parolayı kaldır” (`.danger`, onaylı, parola yokken disabled). Başarıda alanlar temizlenir, durum ve genel uyarı beklemeden güncellenir. İpucu: yalnız özet saklanır, yükleme aracında `--auth` gerekir.
3. *Web parolası* (**ayrı form**, kendi düğmesi “Parolayı kaydet”): Mevcut parola, Yeni parola, Yeni parola tekrar (≤128). Boş yeni parola korumayı kaldırır → önce onay diyaloğu. Tanımlanacak parola ≥8. Eşleşmezse gönderilmez. Başarıda form temizlenir.
4. *Kurtarma sorusu* (**ayrı form**): `Durum: Tanımlı / Tanımlı değil`; Soru (≤160), Cevap (password ≤128); düğmeler “Kurtarma sorusunu iptal et” (`.danger`, tanımlı değilken disabled, onaylı) ve “Kurtarma sorusunu kaydet”. Soru veya cevap boşsa kaydetme; kaldırma yalnız iptal düğmesiyle. Fiziksel/seri alternatif ipucunda yazılır.
Bilgi notu: web parolası isteğe bağlıdır; HTTP/MQTT şifrelenmiyorsa bu açıkça yazılır; OTA ve web parolaları bağımsızdır.

Panel “Denetim kaydı ve uzak toplayıcı”: toplayıcı kapalıysa `notice warn` (“Kayıtlar yalnız RAM'de tutulur…”); `collector` Denetim toplayıcı IPv4 (boş = kapalı), `collectorPort` 1–65535; salt okunur HTTP yolu, **API'den gelen** RAM kapasitesi, bütünlük (HMAC) notu ve Denetim sayfası bağlantısı.

**Bakım (`maint`)** — tek panel `section.panel.red-zone` “⚠ Kırmızı alan”, kırmızı kesikli çerçeve ve saydam kırmızı zemin (`--zone-bg`):
- *Kablosuz bağlantıyı değiştir*: mevcut SSID + `pass`/`clearWifiPassword` alanları ve “Ağ tara ve değiştir” (`wifi`, metinli) → Wi-Fi diyaloğu.
- Eylem düğmeleri, hepsi metinli ve onaylı: “Yeniden başlat” (`reboot`), “Wi-Fi bilgilerini sil ve AP başlat” (`wifioff`, danger), “Çalışma sürelerini sıfırla” (`timer`, danger; tek/tüm kanal seçimli diyalog), “Fabrika ayarlarına dön” (`factory`, danger). Kapsam ipucu: yeniden başlatmada çıkışların hali; fabrika sıfırlamasının sildikleri.

### 8.5 API sözleşmesi

| İstek | Kural |
|---|---|
| `GET /api/settings` | Bütün düz alan değerleri + kimlik (`devName, ip, mdns, clientIp, fwVersion, fwBuild`) + durum bayrakları. **Sır dönmez**: parola/özet yerine `otaPasswordSet`, `otaPasswordActive`, `otaRestartPending`; önerilen `mqPwSet`, `passSet`. `question` metni döner, cevap dönmez. |
| `POST /api/settings` | JSON, **kaydedilen bölümün** tüm görünür alanları (yalnız değişenler değil, başka bölümün alanları asla); boş password alanı gönderilmez (= koru). Kaldırma: `mqPw:""`, `otaPw:""`, `pass:""`. Disabled (dep kapalı) alanlar gönderilmez. Gövdede olmayan alan sunucuda korunur. Sunucu o bölümün adayını bütünüyle doğrular; tek hata = o istekteki hiçbir alan uygulanmaz. Yalnız ilgili modül yazılır (ör. LED kaydı ağ ayarını NVS'e yeniden yazmaz, ağ yeniden bağlanmaz). |
| Yanıt | 200 `{message}` (ör. “Kaydedildi”, “Kaydedildi; OTA parolası yeniden başlatmadan sonra geçerli olur”); 400/409/507 `{message, field?}`. UI mesajı olduğu gibi gösterir, `field` varsa o sekmeyi açıp alana odaklanır. |
| Ayrı uç noktalar | `/api/password {oldPassword,password}`, `/api/question {question,answer}` (boş = kaldır), `GET /scan` (`pending` ise ~700 ms aralıkla en çok 20 deneme), `/api/reset-wifi`, `/api/reboot`, `/api/factory-reset`, `/api/reset-runtime {ch|all}`. |
| CSRF | Yazma istekleri özel başlık (ör. `X-SCADA: 1`) + `SameSite=Strict` oturum çerezi; `fetch(..., {cache:'no-store'})`. |

### 8.6 Davranış kuralları

1. **Baseline ve değişiklik izi.** `renderSettings(d)` alanları üretir, `baseline[name]=valueOf(el)` alır. Her `input/change` olayında (ortak `#settings` kapsayıcısında dinle; `form=` ile bağlı alanlar formun içinde değildir) değişen alanın `.field`'ı `changed` sınıfı alır; password alanı dolu olması değişiklik sayılır.
2. **Sayaçlar.** Her sekmede değişiklik rozeti; bölüm çubuğunda “N alanda kaydedilmemiş değişiklik” / “Kaydedilmemiş değişiklik yok”, başka bölümde taslak varsa “ · diğer bölümlerde N”.
3. **Kaydet çubuğu (bölüm başına).** Her sekmenin panelinin altında; düğme “<Bölüm> ayarlarını kaydet”. Düzenlenebilir alanı olmayan bölümde (Bakım) çubuk yoktur. Ayrı formlar (parola, PIN) çubuğun altında kalır. Değişiklik yokken normal akışta, Kaydet ve Geri al disabled. Değişiklik/hata varken ekran altına yapışkan (`is-dirty`, `is-error`), bildirimler üstünde kalır; telefonda 12 px kenar boşluğuyla tam genişlik.
4. **Geri al** yalnız kendi bölümünü son başarılı yükleme/kayda döndürür ve “Değişiklikler geri alındı” bildirir. Değişiklik varken sayfadan çıkışta `beforeunload` uyarısı.
5. **Doğrulama.** Form `novalidate`; kayıtta **o bölümün** disabled olmayan ilk geçersiz alanı bulunur → `reportValidity()` → odak → hata bildirimi “Kaydedilmedi: “<etiket>” alanını düzeltin”. Alan ilişkileri (ör. statik IP alt ağı) sunucuda doğrulanır ve `field` ile döner.
6. **Kayıt sırasında** Kaydet meşgul (“Kaydediliyor…”), alanlar `fieldset disabled` ya da baseline snapshot'ıyla korunur: yanıt geldiğinde yalnız **gönderilen** değerler baseline olur; kayıt sırasında yapılan yeni düzenleme silinmez ve değişiklik olarak kalır.
7. **Hata** taslağı korur, çubuk `is-error` + “Kaydedilemedi · değişiklikler formda duruyor”.
8. **Başarı**: gönderilen/kanonik değerler baseline olur, password alanları ve “kaldır” kutuları temizlenir, durum satırları (OTA) güncellenir, sunucu mesajı bildirilir.
9. **Bağımlı alanlar** (`dep`) gizlenir ve disabled olur, değer silinmez. “Kaldır” kutusu (`off`) işaretliyken sır alanı disabled ve boş. Koşullu seçim alanı (ör. Buton işlemi) koşul sağlanmadıkça gizli.
10. **Ayrı formlar** (web parolası, kurtarma sorusu, Wi-Fi diyaloğu) kendi düğmesi/bildirimiyle çalışır; bölüm formlarının taslağını ve değişiklik sayısını değiştirmez. Wi-Fi diyaloğu yalnız `ssid/pass` gönderir, başarıda ana formdaki `ssid` baseline'ını günceller.
11. **Riskli işlemler** onay diyaloğu ister; metin hedefi ve kapsamı yazar (ör. “Yalnız Wi-Fi adı ve parolası silinecek, statik IP kapatılacak. Cihaz AP kurulum modunda yeniden başlayacak. Devam edilsin mi?”).
12. Kullanıcı verisi (SSID, cihaz/kanal/program adı) yalnız `textContent` ile yazılır.
13. **Cihaz adı** (`adN`) kaydı başarılıysa üst başlıktaki ad ve `document.title` beklemeden güncellenir; sonraki `/api/data.device_name` aynı değeri getirir.
14. Üst çubuk ile genel uyarı çerçeveleri (`#global-notices`) arasında boşluk vardır (12 px, yalnız uyarı varken).

**Wi-Fi diyaloğu**: başlık + Kapat; bant bilgisi (yalnız donanımın desteklediği; SSID'den bant tahmini yok); “Yeniden tara”; `role=status` durum satırı (taranıyor / N ağ / ağ bulunamadı / hata); ağ listesi `role=list`, satırda SSID, sinyal çubuğu + dBm, kilit simgesi; seçimde form açılır: “Wi-Fi parolası” (zorunlu, ≤64) ve “Açık ağ (parolasız)”; Vazgeç + “Ağı kaydet”. İptal/kapanışta parola temizlenir, hatada diyalog açık kalır.

### 8.7 Standart metinler

| Durum | Metin |
|---|---|
| Kayıt başarı | Sunucu `message`, yoksa “Kaydedildi” |
| Geri alma | “Değişiklikler geri alındı” |
| Parola ipucu | “Boş bırakılırsa kayıtlı parola korunur.” |
| Sayısal kapalı | “0: kapalı” |
| Web koruması kaldırma onayı | “Web parola koruması kaldırılsın mı?” |
| Kurtarma iptal onayı | “Kurtarma sorusu kaldırılsın mı? Parolayı unutursanız yalnız <fiziksel yöntem> kalır.” |
| Yeniden başlatma onayı | “Çıkışlar kapatılarak cihaz yeniden başlatılsın mı?” (proses profiline göre) |
| Fabrika onayı | “Ağ, parola, program ve sayaçlar silinecek. Fabrika ayarlarına dönülsün mü?” |

## 9. Diğer sayfalar

- **Programlar**: bölüm başında “Program ekle” (birincil). Kart: ad (+ “devre dışı”), saat, günler, kanallar, bitiş; ikon düğmeler düzenle/duraklat-başlat/sil (sil onaylı). Diyalog: ad (≤23 bayt), Etkin, kanal ve gün onay grupları (`fieldset`), başlangıç saati/işlemi, bitiş türü (Süre/Saat) ile koşullu alan. Zaman dilimi ve “saat eşitlenene kadar programlar çalışmaz” ipucu. Boş liste: “Henüz program yok.”
- **Alarmlar**: satır = `notice` (önem rengi) + metin + durum (Okunmadı/Okundu/Giderildi) + “Okundu” ikon düğmesi; okunmamış kalın. Önem eşlemesi sabit tabloda (ör. giriş/depolama/kanal arızası kritik; parola/DHCP'ye düşme uyarı). “Okundu”nun kapsamı (sayfa oturumu / kalıcı) sayfada yazılır. Kilit sıfırlama ayrı ve onaylı. Satırlar sıktır (§6 alarm satırı: 10 px yazı, 4 px aralık); geniş dikey boşluk kullanma.
- **Denetim**: satır = sıra no, zaman (UTC yoksa uptime), tür rozeti, olay, teslim durumu; kayıp/gönderilen sayısı; “Toplayıcı anahtarını göster” ayrı yetkili eylem. Kapasite API'den. Liste rozetli ve sıktır: sütunlar `rozet 76 px · sıra+zaman 210 px · olay`; satır iç boşluğu 3 px, tek/çift satır zebra; rozet 10 px, 1 px kenarlık, 4 px köşe; `k-<tür>` sınıfıyla tür rengi (`command` yeşil, `relay` amber, diğerleri marka), tür adı rozette yazılır. Listenin üstünde “Son N olay RAM'de tutulur…” ve “Gönderim öncesi kayıp: N · Son teslim edilen sıra: N” satırları bulunur. Telefonda iki sütun (rozet + zaman), olay metni alt satırda.
- **Oturum**: kullanıcı adı, parola, “Beni hatırla (<gün> gün)”, Giriş yap (birincil), Çıkış yap; `<details>` “Parolamı unuttum” → soru → cevap → kısa ömürlü bilet → yeni parola + tekrar.

## 10. Erişilebilirlik ve kabul ölçütleri

- Gerçek `button/label/fieldset/legend`; her ikon düğmenin erişilebilir adı var; adsız düğme 0.
- İki tema × 390/768/1280 px (+ 320 px ve %200 yakınlaştırma) her sayfa ve her ayar sekmesinde: yatay taşma yok; metin kontrastı ≥4.5:1; telefonda dokunma hedefi ≥44 px; bütün yazılar 10/11/12/14 px (grafik değerleri hariç), en küçük yazı 10 px; saydam zeminde kontrast, üstteki opak zeminle alfa birleştirilerek ölçülür; inline stil/betik yok; dış istek yok; konsol hatası yok.
- Sekme/diyalog klavye ile tam kullanılabilir; `:focus-visible` görünür (test için Shift+Tab ile odak ver, programatik focus `:focus-visible` tetiklemez).

## 11. Test kiti

- `test/mock_device.js`: gerçek API şekliyle bellek içi cihaz; bayraklar `offline` (503), `control: apply|delay|reject` (202 / gecikmeli / 409), `rejectSave` (507), engelli kanal, OTA parola uzunluk reddi, tarama, oturum, alarmlar. POST'ları kaydeder.
- `test/build_test_html.py` → `ui_test.html`: gerçek index/css/js + mock, fontlar data URI, `file://` ile açılan tek dosya; sayfa ve bayrak paneli içerir. Gevşek test sayfası CSP kanıtı değildir.
- `verify_ui.cjs` (Playwright): `assemble.py --dump` çıktısını firmware başlıklarıyla (CSP, önbellek başlıkları) sunan yerel sunucu + mock. Matris: tema × genişlik × rota ve her ayar sekmesi; §10 ölçütleri `getComputedStyle` ile ölçülür (yazı ölçeği denetimi: görünür her metin/alan öğesinin `font-size` değeri {10,11,12,14} içinde olmalı, `.chart-value/.gauge-value` hariç), ekran görüntüleri kaydedilir ve gözle incelenir.
- Zorunlu akışlar: komut kabul/onay, ret, zaman aşımı, çift tıklama; bayatlık ve toparlanma; kartın her yerine tıklama (`mouse.click`, overlay locator'ı engeller); tema kalıcılığı; sistem durumu aç/kapa kalıcılığı; sekme hash'i ve klavye; değişiklik sayacı; geri al; gizli sekmede geçersiz alan; bağımlı alan; parola koru/kaldır; OTA durum metinleri; kurtarma kaydet/iptal; Wi-Fi tarama/seçim/iptal; LED paleti (tek açık, Escape, mevcut renk korunur, seçimde kapanır); **bölüm yalıtımı** (Ağ kaydı yalnız Ağ anahtarlarını gönderir, başka sekmedeki geçersiz/taslak alan ağ kaydını engellemez ve taslak korunur); cihaz adı kaydında başlık/sekme adı; canlı LED şeridi; kayıt hatasında taslak; riskli işlem onayı; font aileleri ve Latin2 inişi; ikinci sayfa geçişinde CSS/JS/font yeniden inmez; yeni build bildirimi.
- Firmware değişikliği varsa ayrıca native testler ve hedef MCU build'i (bkz. `cihaz-temeli.md`). Fixture ve tarayıcı testi gerçek cihaz/telefon testi değildir; yapılmadıysa açık bırak.

## 12. Tamamlanma kontrol listesi

- [ ] UI profili ve alan kataloğu işaretlendi; olmayan donanım gösterilmiyor.
- [ ] Token bloğu tek yerde; tarihsel override/`!important` yığını yok.
- [ ] Yedi bölüm sırası, kimlikleri, masaüstü sol şerit / mobil kutu yerleşimi.
- [ ] Sır alanları GET'ten dolmuyor; boş = koru; kaldır kutusu; ayrı formlar bağımsız.
- [ ] Bölüm başına form + kaydet çubuğu, sayaçlar (“diğer bölümlerde N”), bölüm geri alma, kayıt sırası koruması; tek genel “Ayarları kaydet” yok.
- [ ] LED sekmesi: canlı şerit, parlaklık, ilk dört LED ortak sırada, cihaza özgü LED'ler sonra.
- [ ] Revizyon başlık/altbilgide; yeni build bildirimi.
- [ ] gzip + `?v` + immutable; HTML no-store; CSP sıkı; fontlar lisansıyla.
- [ ] Yazı ölçeği 10/11/12 px, en büyük başlık 14 px; `rem`/`em` yazı boyutu yok; alarm satırı 10 px.
- [ ] Üst çubuk (etiket, ad, bilgi + tema düğmesi, kutusuz kimlik şeridi), ikonlu eşit sütunlu menü, durum çubuğu, 4 sütunlu sistem durumu, sık alarm/denetim listesi ve saydam kırmızı alan §5–§9'daki gibi.
- [ ] §10 matrisi ve §11 akışları geçti; ekran görüntüleri incelendi; açık kalanlar günlükte.