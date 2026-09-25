s1 = open('sheet1.svg').read(); s2 = open('sheet2.svg').read()
bom = [
 ("U1", "Mean Well IRM-10-5", "AC/DC 5 V 2 A, 10 W, çift izolasyonlu", "1"),
 ("U2", "ESP32-S3-DevKitC-1 N8R8 / N8R2", "8 MB flash; kart üstü 3.3 V LDO, BOOT, WS2812", "1"),
 ("U3", "2 kanal 5 V röle modülü", "Optokuplörlü, JD-VCC jumper'lı (ör. SRD-05VDC-SL-C, 10 A 250 VAC kontak)", "1"),
 ("B1", "DHT22 / AM2302", "Sıcaklık + nem, tek hat", "1"),
 ("Q1, Q2", "BC337-40", "NPN TO-92, 45 V 800 mA", "2"),
 ("D1", "SS34", "Schottky 3 A 40 V", "1"),
 ("R1", "4.7 kΩ", "¼ W %1 — DHT22 pull-up", "1"),
 ("R2", "100 Ω", "¼ W — DATA seri koruma", "1"),
 ("R3, R5", "1 kΩ", "¼ W — taban direnci", "2"),
 ("R4, R6", "10 kΩ", "¼ W — taban pull-down (boot güvenliği)", "2"),
 ("R7, R8", "10 kΩ", "¼ W — IN pull-up (boot güvenliği)", "2"),
 ("R9, R10 · LED1, LED2", "1 kΩ · 3 mm LED", "Yalnız HIL-1 test yükü", "2+2"),
 ("C1", "470 µF 16 V", "Elektrolitik, 105 °C", "1"),
 ("C2, C4, C5", "100 nF 50 V", "Seramik X7R", "3"),
 ("C3", "10 µF 16 V", "Seramik X5R veya tantal", "1"),
 ("J1", "4 pin klemens / JST-XH", "DHT22 kablosu", "1"),
 ("J2, J3", "2 pin klemens", "SSR giriş (A1+/A2−)", "2"),
 ("SSR1, SSR2", "Sıfır geçişli SSR 25 A", "Giriş 3–32 VDC, çıkış 24–380 VAC (ör. SSR-25DA) + soğutucu Rth ≤ 3 K/W", "2"),
 ("RV1, RV2", "S14K275", "Varistör 275 VAC — SSR çıkışına paralel", "2"),
 ("R11, R12 · C6, C7", "100 Ω 1 W · 100 nF X2 275 VAC", "Fan RC snubber", "2+2"),
 ("QF1", "RCBO 2P C16", "30 mA, tip A", "1"),
 ("F1", "T 1 A 5×20 + yuva", "U1 beslemesi", "1"),
 ("F2", "MCB 1P C10", "Rezistans dalı", "1"),
 ("F3", "STB güvenlik termostatı", "Elle resetli, 16 A 250 V, ≈ 70 °C; yazılımdan bağımsız", "1"),
 ("F4", "T 2 A 5×20 + yuva", "Fan dalı", "1"),
 ("E1, E2", "Rezistans 1000 W 230 V", "Fan önünde, metal gövde PE'li", "2"),
 ("M1, M2", "Fan 230 VAC", "M1 ısıtıcı fanı, M2 havalandırma", "2"),
 ("X1", "Giriş klemensi L/N/PE", "2.5 mm²", "1"),
]
rows = "\n".join(f"<tr><td class='mono'>{r}</td><td>{v}</td><td>{d}</td><td class='num'>{q}</td></tr>" for r, v, d, q in bom)
pins = [
 ("GPIO4", "DHT22 DATA", "—", "R1 pull-up 3.3 V, R2 seri"),
 ("GPIO5", "R1 SSR (Q1)", "Aktif-HIGH", "R4 10 kΩ pull-down"),
 ("GPIO6", "R2 SSR (Q2)", "Aktif-HIGH", "R6 10 kΩ pull-down"),
 ("GPIO7", "HF röle IN1", "Aktif-LOW", "R7 10 kΩ pull-up 3.3 V"),
 ("GPIO15", "VF röle IN2", "Aktif-LOW", "R8 10 kΩ pull-up 3.3 V"),
 ("GPIO0", "BOOT butonu (kart)", "Aktif-LOW", "Yalnız boot sonrası giriş"),
 ("GPIO48", "WS2812 (kart)", "—", "v1.1 kartta GPIO38"),
 ("GPIO17", "Boş", "—", "İleride HEATER_ARM"),
]
prow = "\n".join(f"<tr><td class='mono'>{a}</td><td>{b}</td><td>{c}</td><td>{d}</td></tr>" for a, b, c, d in pins)
html = f'''<title>Kulübe İklim Devre Şeması</title>
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;600&display=swap">
<style>
:root{{
 --paper:#f5f6f2; --sheet:#ffffff; --ink:#1c2733; --dim:#586573; --rule:#c9cfd3; --boxfill:#fbfcfa;
 --L:#8a4a1f; --N:#2a5c9c; --PE:#2f7d34; --warn:#b3261e; --hil:#946400; --heat:#fbe7df; --ssr:#eef3f8; --head:#e9ede7;
 --font:"IBM Plex Sans",system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
 --mono:"IBM Plex Mono",Consolas,"SFMono-Regular",Menlo,monospace;
}}
@media (prefers-color-scheme:dark){{:root:not([data-theme="light"]){{color-scheme:dark;
 --paper:#10151b; --sheet:#151c24; --ink:#d8e0e7; --dim:#95a3b0; --rule:#34414d; --boxfill:#19222c;
 --L:#d8935f; --N:#86ade6; --PE:#7fc57d; --warn:#ff8a7e; --hil:#e0b35a; --heat:#3a2622; --ssr:#1d2a38; --head:#1b242d;}}}}
:root[data-theme="dark"]{{color-scheme:dark;
 --paper:#10151b; --sheet:#151c24; --ink:#d8e0e7; --dim:#95a3b0; --rule:#34414d; --boxfill:#19222c;
 --L:#d8935f; --N:#86ade6; --PE:#7fc57d; --warn:#ff8a7e; --hil:#e0b35a; --heat:#3a2622; --ssr:#1d2a38; --head:#1b242d;}}
body{{background:var(--paper);color:var(--ink);font:400 15px/1.55 var(--font);margin:0}}
.wrap{{max-width:1260px;margin:0 auto;padding-inline:16px;padding-block:24px 48px;display:grid;gap:28px}}
h1{{font:600 26px/1.2 var(--mono);letter-spacing:-.01em;margin:0;text-wrap:balance}}
h2{{font:600 15px/1.3 var(--mono);letter-spacing:.06em;text-transform:uppercase;margin:0 0 10px}}
.lead{{max-width:70ch;color:var(--dim);margin:6px 0 0}}
.tb{{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));border:1.5px solid var(--ink);background:var(--sheet)}}
.tb div{{padding:8px 12px;border-right:1px solid var(--rule);border-bottom:1px solid var(--rule)}}
.tb b{{display:block;font:500 10.5px var(--mono);letter-spacing:.08em;text-transform:uppercase;color:var(--dim)}}
.tb span{{font:500 14px var(--mono)}}
.alert{{border:1.5px dashed var(--warn);color:var(--ink);padding:10px 14px;background:var(--sheet);max-width:95ch}}
.alert strong{{color:var(--warn)}}
figure{{margin:0}}
.sheetwrap{{overflow-x:auto;border:1px solid var(--rule);background:var(--sheet)}}
.sheetwrap svg{{display:block;width:100%;min-width:980px;height:auto}}
figcaption{{font-size:13px;color:var(--dim);margin-top:8px;max-width:95ch}}
svg .w{{stroke:var(--ink);stroke-width:1.6;fill:none}}
svg .c{{stroke:var(--ink);stroke-width:1.8;fill:none;stroke-linecap:round}}
svg .fillc{{fill:var(--ink);stroke:none}}
svg .dot{{fill:var(--ink)}}
svg .pin{{fill:var(--sheet);stroke:var(--ink);stroke-width:1.4}}
svg .box{{fill:var(--boxfill);stroke:var(--ink);stroke-width:1.6}}
svg .ssr{{fill:var(--ssr);stroke:var(--ink);stroke-width:1.6}}
svg .heat{{fill:var(--heat);stroke:var(--ink);stroke-width:1.6}}
svg .frame{{fill:none;stroke:var(--rule);stroke-width:1.2}}
svg .hilbox{{fill:none;stroke:var(--hil);stroke-width:1.4;stroke-dasharray:6 4}}
svg .cable{{stroke:var(--ink);stroke-width:1.6;stroke-dasharray:7 5;fill:none}}
svg .mech{{stroke:var(--dim);stroke-width:1.4;stroke-dasharray:3 3;fill:none}}
svg .L{{stroke:var(--L);stroke-width:2.4;fill:none}}
svg .N{{stroke:var(--N);stroke-width:2.4;fill:none}}
svg .PE{{stroke:var(--PE);stroke-width:2.2;stroke-dasharray:10 4;fill:none}}
svg text{{fill:var(--ink);font-family:var(--mono)}}
svg .t{{font-size:12px}}
svg .sheet{{font-size:14px;font-weight:600;letter-spacing:.05em}}
svg .ref{{font-size:12.5px;font-weight:600}}
svg .lbl{{font-size:11.5px;fill:var(--dim)}}
svg .pinl{{font-size:11.5px}}
svg .net{{font-size:11.5px;font-weight:600}}
svg .sym{{font-size:22px;font-family:var(--font)}}
svg .note{{font-size:11.5px;fill:var(--dim);font-family:var(--font)}}
svg .warnt{{font-size:12px;fill:var(--warn);font-weight:600;font-family:var(--font)}}
.legend{{display:flex;flex-wrap:wrap;gap:6px 18px;font:500 12.5px var(--mono);color:var(--dim)}}
.legend i{{display:inline-block;width:28px;height:0;border-top:3px solid;vertical-align:middle;margin-right:6px}}
.tblwrap{{overflow-x:auto;border:1px solid var(--rule);background:var(--sheet)}}
table{{border-collapse:collapse;width:100%;font-size:13.5px}}
th,td{{text-align:left;padding:7px 12px;border-bottom:1px solid var(--rule);vertical-align:top}}
th{{font:600 11px var(--mono);letter-spacing:.07em;text-transform:uppercase;color:var(--dim);background:var(--head)}}
td.mono{{font-family:var(--mono);font-weight:500;white-space:nowrap}}
td.num{{font-family:var(--mono);text-align:right;font-variant-numeric:tabular-nums}}
.two{{display:grid;grid-template-columns:1fr 1fr;gap:28px}}
@media (max-width:900px){{.two{{grid-template-columns:1fr}}}}
ol.notes{{margin:0;padding-left:22px;display:grid;gap:8px;max-width:95ch}}
ol.notes li::marker{{font-family:var(--mono);color:var(--dim)}}
code{{font-family:var(--mono);font-size:.92em}}
</style>
<div class="wrap">
<header>
 <h1>Kulübe İklim Kontrolörü · Devre Şeması</h1>
 <p class="lead">ESP32-S3 tabanlı iki kademeli ısıtma ve havalandırma kontrolü. F2'de onaylanan donanım kararlarına ve <code>src/app/pins.h</code> pin haritasına göre çizildi.</p>
</header>
<div class="tb" role="group" aria-label="Çizim bilgileri">
 <div><b>Proje</b><span>Climate Controller PID</span></div>
 <div><b>Çizim</b><span>CC-SCH-01 · 2 sayfa</span></div>
 <div><b>Revizyon</b><span>A (F2)</span></div>
 <div><b>Tarih</b><span>25.09.2026</span></div>
 <div><b>Durum</b><span>Tasarım — HIL öncesi</span></div>
</div>
<div class="alert"><strong>Şebeke gerilimi.</strong> Sayfa 2 bir tasarım çizimidir; kurulum ve enerjilendirme yetkili elektrikçi tarafından, RCBO, STB ve PE bağlantıları ölçülerek yapılmalıdır. ARM hattı olmadığından SSR kısa devre arızasını yalnız F3 güvenlik termostatı sınırlar.</div>
<figure>
 <h2>Sayfa 1 — Güç ve mantık kartı</h2>
 <div class="sheetwrap">{s1}</div>
 <figcaption>ESP32-S3 kartı, 5 V besleme, DHT22 sensörü, iki SSR sürücü transistörü ve röle modülü. Tüm çıkış hatları reset anında dış dirençlerle pasif seviyede tutulur: SSR tabanları GND'ye, röle IN hatları 3.3 V'a çekilir.</figcaption>
</figure>
<figure>
 <h2>Sayfa 2 — Şebeke tarafı</h2>
 <div class="sheetwrap">{s2}</div>
 <figcaption>230 V dağıtımı: RCBO sonrası üç dal. U1 beslemesi (F1), rezistans dalı (F2 → F3 STB → SSR1/SSR2 → E1/E2) ve fan dalı (F4 → K1/K2 → M1/M2). SSR çıkışlarında varistör, fanlarda RC snubber var.</figcaption>
 <div class="legend" style="margin-top:10px"><span><i style="border-color:var(--L)"></i>L faz</span><span><i style="border-color:var(--N)"></i>N nötr</span><span><i style="border-top-style:dashed;border-color:var(--PE)"></i>PE koruma</span><span><i style="border-top-style:dashed;border-color:var(--ink)"></i>Sensör kablosu</span></div>
</figure>
<section class="two">
 <div>
  <h2>Pin ve polarite tablosu</h2>
  <div class="tblwrap"><table><thead><tr><th>Pin</th><th>İşlev</th><th>Seviye</th><th>Boot güvenliği</th></tr></thead><tbody>{prow}</tbody></table></div>
 </div>
 <div>
  <h2>Kurulum ve ölçüm notları</h2>
  <ol class="notes">
   <li><b>Röle modülü 3.3 V opto sürüşü:</b> IN LOW'da opto akımı yaklaşık (3.3 − 1.2 V) / 1 kΩ ≈ 2 mA olur (modüldeki seri direnç 1 kΩ varsayıldı). Çoğu modülde yeterlidir; HIL H1'de iki rölenin kesin çekip bıraktığı ölçülmeli. Yetersizse modüldeki 1 kΩ direnç 470 Ω ile değiştirilir.</li>
   <li><b>SSR ısısı:</b> 4.35 A'de SSR başına ≈ 5–6 W kayıp. Soğutucu Rth ≤ 3 K/W, termal macunla; SSR'ler pano içinde rezistans hava akışının dışında.</li>
   <li><b>DHT22:</b> 3.3 V beslemede kablo ≤ 5 m. Daha uzunsa VCC +5V'tan beslenir, R1 yine +3V3'e bağlı kalır (ESP32 girişi 3.3 V'u aşmaz). Sensör rezistans ve fan üflemesinden uzak.</li>
   <li><b>İzolasyon:</b> Şebeke ile SELV kartı arasında ≥ 6 mm açıklık; SSR giriş/çıkış ve röle kontak tarafı ayrı klemens bloklarında.</li>
   <li><b>Kablo kesitleri:</b> Giriş ve rezistans dalı ≥ 1.5 mm² (C10/C16 koruma ile uyumlu), fan dalı ≥ 0.75 mm², SELV 0.25–0.5 mm².</li>
   <li><b>USB ile çalışma:</b> Programlama sırasında şebeke bağlıysa D1 kart 5V hattından PSU'ya geri beslemeyi engeller. İlk testlerde (HIL-1) şebeke bağlı olmaz, kart USB'den beslenir.</li>
   <li><b>STB (F3):</b> Rezistansların üstünde, kulübe havasını değil ısıtıcı çıkış havasını ölçecek şekilde; eşik 70 °C çevresi, sahada ayarlanır. Açtığında elle resetlenmeden rezistans enerjilenmez.</li>
  </ol>
 </div>
</section>
<section>
 <h2>Malzeme listesi</h2>
 <div class="tblwrap"><table><thead><tr><th>Ref.</th><th>Değer / parça</th><th>Açıklama</th><th>Adet</th></tr></thead><tbody>{rows}</tbody></table></div>
</section>
</div>
'''
open('artifact_body.html', 'w').write(html)
full = '<!doctype html><html lang="tr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">' + html + '</html>'
open('../../docs/hardware/CC-SCH-01.html', 'w').write(full)
print("ok")
