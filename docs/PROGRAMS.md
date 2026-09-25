# Yerel Program Modülü

Durum: DESIGN DECISION ([ADR-009](ADR/ADR-009-local-programs.md)) · 25.09.2026 · çekirdek uygulandı (`lib/core/src/cc_schedule.*`), UI sayfası F4 önizlemesinde.

Cihaz; haftanın günlerine, tarih aralıklarına ve tek seferlik zamanlara bağlı programları **MQTT Suite olmadan** yürütür. Programlar bir hedef sıcaklık, bir profil, ısıtmayı durdurma ya da havalandırma isteği uygular. Suite Programs (`sched_night` / `sched_away`) yolu korunur; yerel programlar onun üstündedir.

## 1. Program kaydı

| Alan | Tür / aralık | Anlam |
|---|---|---|
| `name` | ≤ 23 bayt UTF-8, boş olamaz | Görünen ad |
| `enabled` | bool | Program tek tek duraklatılabilir |
| `kind` | `WEEKLY` / `DATE_RANGE` / `ONCE` | Tekrar türü |
| `days` | bit maskesi (bit0 = Pzt … bit6 = Paz) | WEEKLY'de zorunlu (≥ 1 gün); DATE_RANGE'de isteğe bağlı süzgeç (0 = aralığın her günü) |
| `date_from`, `date_to` | `YYYY-MM-DD` (2000–2199) | DATE_RANGE aralığı (dahil, ≤ 366 gün); ONCE günü `date_from` |
| `start` | `HH:MM` | Başlangıç saati (ALL_DAY'de yok sayılır) |
| `end` | `END_TIME` / `DURATION` / `ALL_DAY` | Bitiş türü |
| `end_time` | `HH:MM`, ≠ start | Başlangıçtan küçük/eşitse ertesi güne taşar (22:00 → 06:00) |
| `duration` | dk: 1–1440 (ONCE: 1–10080 = 7 gün) | Süre |
| `action` | `SETPOINT` / `PROFILE` / `HEATING_OFF` / `VENTILATE` | Eylem |
| `setpoint` | 5–30 °C, 0.5 adım; `cabin_overtemp_limit − 10` üstü reddedilir | SETPOINT hedefi |
| `profile` | `NIGHT` / `AWAY` / `FROST` | PROFILE eylemi |

Kapasite: 16 program. Liste bütünüyle doğrulanır; tek hata = hiçbir değişiklik uygulanmaz (baseline §4).

### Örnekler

| İhtiyaç | Kayıt |
|---|---|
| Hafta içi 06:30–08:30 22 °C | WEEKLY Pzt–Cum · 06:30 → END_TIME 08:30 · SETPOINT 22 |
| Her gece 23:00–06:00 gece profili | WEEKLY 7 gün · 23:00 → END_TIME 06:00 · PROFILE NIGHT |
| Cumartesi 14:00'ten 3 saat 24 °C | WEEKLY Cmt · 14:00 → DURATION 180 · SETPOINT 24 |
| 24–31 Aralık tatil: uzakta | DATE_RANGE 2026-12-24…2026-12-31 · ALL_DAY · PROFILE AWAY |
| Tatilde yalnız hafta sonları 20 °C | DATE_RANGE … · günler Cmt+Paz · 09:00 → 18:00 · SETPOINT 20 |
| 3 Ekim misafir: 3 gün 23 °C | ONCE 2026-10-03 14:00 · DURATION 4320 · SETPOINT 23 |
| Yaz boyunca ısıtma yok | DATE_RANGE 2027-06-01…2027-09-15 · ALL_DAY · HEATING_OFF |
| Her gün 12:00'de 20 dk havalandırma | WEEKLY 7 gün · 12:00 → DURATION 20 · VENTILATE |

## 2. Değerlendirme

**DESIGN DECISION — durumsuz değerlendirme:** Etkin program yalnız *(yerel zaman, program listesi, atlama kaydı)* fonksiyonudur. Kenar (start/stop) olayı saklanmaz; boot, saat düzeltmesi veya kesintiden sonra aynı sonuç yeniden hesaplanır, kaçırılan başlangıç/bitiş olmaz. Değerlendirme her kontrol periyodunda (2 s) yapılır.

- Yerel zaman = UTC + `tz_offset_min` (Türkiye UTC+3, yaz saati yok). **OPEN ISSUE:** Yaz saati uygulanan bölgede kullanım için DST kuralı FUTURE.
- Oluşum `[start, end)` aralığıdır; bitiş dakikası hariçtir. Gece yarısını geçen oluşum başladığı güne aittir (Cuma 22:00–06:00, Cumartesi sabahı da etkindir; Cumartesi seçili olmasa bile).
- İki bağımsız kanal: **iklim** (SETPOINT, PROFILE, HEATING_OFF) ve **havalandırma** (VENTILATE). Her kanalda en çok bir program etkindir.
- Çakışma: 1) sınıf `ONCE › DATE_RANGE › WEEKLY` (daha belirli olan kazanır) 2) daha geç başlayan oluşum 3) listede önce gelen.
- **Atla (hold):** Kullanıcı etkin iklim oluşumunu bitimine kadar atlayabilir. Kayıt `(program, oluşum başlangıcı)` çiftidir; aynı programın sonraki oluşumu normal çalışır. Liste değişince atlama silinir.
- **Sonraki değişim:** Önümüzdeki 8 gün içinde sonucu gerçekten değiştiren ilk sınır (UI “Sonraki: Pzt 06:30 · Sabah ısıtması”).

## 3. Öncelik zinciri

```mermaid
flowchart LR
  B["BOOST (süreli)"] --> E["Açık profil seçimi<br/>profile ≠ DAY"] --> P["Yerel program<br/>(iklim kanalı)"] --> SA["Suite sched_away"] --> SN["Suite sched_night"] --> D["DAY (temperature_setpoint)"]
```

Üstteki etkinse alttakiler yok sayılır. Antifreeze bekçisi bu zincirin dışında ve üstündedir: HEATING_OFF programı ısıtmayı durdurur, ama T1 < `frost_guard_temperature` olunca donma koruması yine ısıtır. Safety/interlock kararları hiçbir programla aşılamaz.

| Eylem | `profile_active` | `setpoint_source` | Etki |
|---|---|---|---|
| SETPOINT | `PROGRAM` | `PROGRAM` | Hedef = program sıcaklığı (rampa yükselişte geçerli) |
| PROFILE | `NIGHT` / `AWAY` / `FROST` | `PROGRAM` | Hedef = o profilin setpoint'i |
| HEATING_OFF | `PROGRAM` | `PROGRAM` | AUTO'da ısıtma talebi 0 (yalnız antifreeze); MANUAL/VENT_ONLY/OFF değişmez |
| VENTILATE | — | — | Havalandırma kaynağı `SCHEDULED`; ısıtma koordinasyonu (§5.2: ısıtırken engellenir, changeover) ve antifreeze geçerli |

## 4. Saat gerekliliği

- Programlar yalnız geçerli duvar saatiyle çalışır (NTP veya RTC). Saat geçersizken hiçbir program uygulanmaz, UI “Saat bekleniyor · programlar çalışmıyor” gösterir, `TIME_INVALID` alarmı üretilir. Saat geldiği anda durumsuz değerlendirme doğru programı seçer.
- Bu karar checklist madde 16'yı (saat kaynağı) **F3 öncesinden F2 öncesine** çeker: program kullanılacaksa NTP sunucusu veya RTC modülü seçilmelidir.

## 5. Arayüzler

### 5.1 REST

| Uç | Yöntem | İçerik |
|---|---|---|
| `/api/programs` | GET | `{enabled, time_valid, now, active:{climate, vent, until, held}, next_change, list:[…]}` |
| `/api/programs` | POST | `{list:[…]}` tüm liste atomik; hata `400 {message, index, field}` |
| `/api/cmd` | POST | `programs_enabled` ON/OFF, `program_hold` PRESS |

### 5.2 MQTT (F5)

| Entity | Bileşen | Not |
|---|---|---|
| `programs_enabled` | switch | Uzaktan açılıp kapatılabilir (operasyonel) |
| `program_active` | sensor | Etkin iklim programının adı veya `—` |
| `program_hold` | button | Etkin oluşumu atla |
| `program_until` | sensor | Bitiş zamanı (ISO yerel) |

Program listesinin kendisi v1'de MQTT'den yazılamaz (yalnız yerel web).

### 5.3 Kalıcılık (F3)

`programs.json` ayrı dosya, şema sürümlü, primary/backup/temp nesil kontrollü atomik yazım. `programs_enabled` ve atlama kaydı operasyonel değer olarak ertelenmiş yazımla persist edilir. Boot'ta geçersiz program dosyası → son geçerli kopya; yoksa boş liste + `CONFIGURATION_ERROR` olmadan INFO olayı (programsız çalışma güvenli durumdur).

## 6. Doğrulama kuralları

| Kod | Kural |
|---|---|
| P1 | ≤ 16 program |
| P2 | Ad 1–23 bayt |
| P3 | WEEKLY ≥ 1 gün; gün maskesi yalnız 7 bit |
| P4 | Başlangıç 00:00–23:59; END_TIME ≠ başlangıç |
| P5 | Süre 1–1440 dk (ONCE 1–10080) |
| P6 | DATE_RANGE bitiş ≥ başlangıç, ≤ 366 gün |
| P7 | SETPOINT 5–30 °C, 0.5 adım |
| P8 | SETPOINT ≤ `cabin_overtemp_limit − 10` (V5 eşi). Limit düşürülürken bu kuralı bozacak konfigürasyon da reddedilir |
| P9 | PROFILE ∈ {NIGHT, AWAY, FROST} |

## 7. Testler (native)

`test/native/test_schedule`: takvim (2000–2199 gidiş-dönüş, artık yıl, 2100), haftalık pencere ve bitiş hariçliği, gece yarısı taşması, süre ve tüm gün, tarih aralığı + gün süzgeci, 3 günlük ONCE, öncelik (sınıf, geç başlayan, indeks), bağımsız kanallar, atlama yalnız o oluşum, sonraki değişim, P1–P9, ClimateCore entegrasyonu (öncelik zinciri, atla, modül kapalı, bitiş olayı, HEATING_OFF + antifreeze, VENTILATE, saat geçersiz, P8 konfigürasyon koruması).

## 8. Gelecek

Önceden ısıtma (optimum start: hedefe program başlangıcında ulaşmak için öğrenilen ısınma hızıyla erken başlatma), yaz saati kuralı, tatil takvimi içe aktarma, MQTT'den program listesi yazımı.
