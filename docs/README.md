# Kulübe İklim Kontrolörü — ESP32 SCADA PID Climate Controller

Tarih: 24 Eylül 2026. Durum: **tasarım paketi; implementasyon değildir.**
Firmware, HTML/CSS/JS, PlatformIO projesi, build, test veya deployment bu teslimin kapsamı dışındadır. JSON/Mermaid blokları yalnız sözleşme ve diyagramdır.

Cihaz; iki AC rezistans (R1, R2), rezistanslara ait bir **Heater Fan**, kulübe için ayrı bir **Ventilation Fan** ve en az bir sıcaklık + nem sensörü ile kulübe iç sıcaklığını setpoint çevresinde tutar. Klasik termostat değil; **Climate Controller + PID + Power Manager + Safety Manager + SCADA cihazı** olarak tasarlanmıştır ve MQTT Suite'e (`ESP_MQTT_SOZLESMESI.md`) ek protokol icat etmeden katılır.

## Temel ilkeler

1. **PID çıkışı röle sürmez.** PID yalnız `heat_demand = 0…100 %` üretir; fiziksel çıkışlara çeviren Power Manager'dır ([ADR-001](ADR/ADR-001-demand-power-separation.md)).
2. **Safety Manager kontrolden bağımsız ve üstündür.** Yazılım tek güvenlik katmanı değildir; bağımsız termik kesici, sigorta ve donanım ARM hattı zorunludur ([SAFETY_DESIGN.md](SAFETY_DESIGN.md)).
3. **`R1 ∨ R2 ⇒ HeaterFan`** kuralı her komut kaynağından üstündür; kapanıştan sonra POST_COOL uygulanır ([OUTPUT_AND_INTERLOCKS.md](OUTPUT_AND_INTERLOCKS.md)).
4. **MQTT LOST ≠ LOCAL CONTROL LOST.** Wi-Fi, broker veya Suite olmadan yerel kontrol, güvenlik ve web arayüzü çalışır.
5. **Requested ≠ Effective.** Her kontrol edilebilir çıkış istenen, etkin durum ve geçersiz kılma nedeni ile raporlanır ([ADR-003](ADR/ADR-003-requested-effective-model.md)).
6. **Sensör ve sürücü bağımsızlığı.** Sensör ailesi ve SSR/röle türü soyutlanmıştır; donanım seçimi açık konudur.

## Doküman haritası

| Belge | İçerik |
|---|---|
| [REQUIREMENTS.md](REQUIREMENTS.md) | FR/NFR/SR gereksinimleri, kabul kriterleri |
| [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md) | Sistem bağlamı, mantıksal mimari, FreeRTOS görevleri, watchdog, boot, OTA |
| [CONTROL_ARCHITECTURE.md](CONTROL_ARCHITECTURE.md) | Modlar, profiller, kontrol akışı, ısıtma/havalandırma koordinasyonu, komut arbitrasyonu |
| [PID_DESIGN.md](PID_DESIGN.md) | P/PI/PID, anti-windup, deadband, ramp, bumpless transfer, zamanlama |
| [STATE_MACHINE.md](STATE_MACHINE.md) | Ana, ısıtma zinciri ve havalandırma durum makineleri |
| [OUTPUT_AND_INTERLOCKS.md](OUTPUT_AND_INTERLOCKS.md) | Power Manager, R1/R2 kademelendirme, sürücü soyutlama, interlock, post-cool |
| [SAFETY_DESIGN.md](SAFETY_DESIGN.md) | Safety Manager, öncelik matrisi, failsafe, donanım sınırı, Heating Performance Monitor |
| [SENSOR_ARCHITECTURE.md](SENSOR_ARCHITECTURE.md) | ClimateSensor arayüzü, kalite modeli, T2 hazırlığı |
| [MQTT_INTEGRATION.md](MQTT_INTEGRATION.md) | Topic, QoS/retain, LWT, discovery, komut/ACK, Programs, Suite uyumsuzlukları |
| [ENTITY_MODEL.md](ENTITY_MODEL.md) | Entity kataloğu, taksonomi ve Studio widget eşlemesi, SCADA kartı |
| [WEB_SCADA_UI.md](WEB_SCADA_UI.md) | Gömülü web HMI: sayfalar, wireframe, durum gösterimi, trend, erişilebilirlik |
| [PROGRAMS.md](PROGRAMS.md) | Yerel program modülü: haftalık / tarih aralığı / tek sefer, öncelik, REST, MQTT, kalıcılık ([ADR-009](ADR/ADR-009-local-programs.md)) |
| [ALARM_AND_EVENTS.md](ALARM_AND_EVENTS.md) | Alarm kataloğu, alarm durum makinesi, olay günlüğü |
| [CONFIGURATION_MODEL.md](CONFIGURATION_MODEL.md) | Ayar alanları, aralık/varsayılan, doğrulama kuralları, kalıcılık |
| [DIAGNOSTICS.md](DIAGNOSTICS.md) | Tanı değerleri, çalışma saatleri, anahtarlama sayaçları |
| [SECURITY.md](SECURITY.md) | Web/MQTT güvenliği, servis modu koruması, sırlar |
| [EXPANSION_ROADMAP.md](EXPANSION_ROADMAP.md) | Genişleme noktaları ve geliştirme sırası |
| [DECISIONS_AND_OPEN_ISSUES.md](DECISIONS_AND_OPEN_ISSUES.md) | Karar özeti, açık konular, **Implementation Readiness Checklist** |
| [ADR/](ADR/) | Sekiz mimari karar kaydı |

## Konum

Bu paket PlatformIO projesinin `docs/` klasöründedir (`C:\Users\t_yag\Documents\PlatformIO\Projects\Climate Controller PID\docs`). Kodlama oturumu için başlangıç talimatı: [IMPLEMENTATION_PROMPT.md](IMPLEMENTATION_PROMPT.md).

## Referans alınan kaynaklar

- MQTT Suite deposu `C:\Users\t_yag\Desktop\mqtt` — sözleşmeler: `ESP_MQTT_SOZLESMESI.md`, `ENTITY_TAXONOMY.md`, `ENTITY_WIDGET_MAPPING.md`, `OBJECT_TAXONOMY.md`, `MQTT_ENTITY_ARCHITECTURE.md`, `PROGRAMLAR.md`, `SCADA_PROJECTS.md`, `TICARI_CEKIRDEK.md`, `TUKETIM.md`; kod: `studio_tpl.html` (`haConfig`, `isOn`, `valueOf`), `discovery.py` (`parse`), `programlar.py` (`dogrula`), `tuketim.py`.
- Skill'ler: **scada-device-baseline**, **scada-ui-design**, **mqtt-studio-dugum**.
- Hedef kart varsayımı: `Climate Controller PID/platformio.ini` → `esp32-s3-devkitc-1` (çift çekirdek, FreeRTOS). Kesin kart/modül seçimi açık konudur.

## Etiketler

**DESIGN DECISION:** önerilen bağlayıcı karar. **ASSUMPTION:** ölçülmemiş varsayım. **OPEN ISSUE:** implementasyon öncesi netleşecek konu. **FUTURE ENHANCEMENT:** ilk sürüm dışında.

Belgelerdeki sayısal değerler (süreler, eşikler, PID katsayıları) **başlangıç tasarım değeridir**; ölçümle doğrulanmış nihai parametre değildir.

## Terimler

| Terim | Anlam |
|---|---|
| T1 | Kulübe iç ortam sıcaklığı (kontrol değişkeni) |
| T2 | Rezistans / hava çıkış sıcaklığı (ilk sürümde isteğe bağlı) |
| heat_demand | Power Manager'a giden ısı talebi, % |
| requested / effective | Kaynağın istediği durum / güvenlik ve interlock sonrası fiilen komutlanan durum |
| effective (çıkış) | Fiziksel geri bildirim yoksa **komutlanan** çıkış; kontak/akım kanıtı değildir |
| B | Cihaz MQTT taban adresi: `mqttsuite/climate/<slug>` |
