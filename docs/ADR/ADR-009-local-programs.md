# ADR-009 — Yerel program modülü

Durum: DESIGN DECISION / kullanıcı isteği · 25.09.2026 · ADR-005'i genişletir, OI-S4'ü kapatır

## Problem
v1 tasarımında zamanlama yalnız MQTT Suite Programs'taydı; Suite yokken profil geçişi olmuyor ve Programs yalnız AÇ/KAPAT ürettiği için belirli sıcaklık zamanlanamıyordu (M7). Kullanıcı haftanın günleri, belirli tarihler, belirli saatler, belirli sıcaklık ve süre içeren yerel bir programlama modülü istedi.

## Seçenekler
1. Suite Programs'a bağlı kalmak (mevcut).
2. Yerel haftalık tablo (gün × saat dilimi → setpoint), tek tür.
3. Kayıt tabanlı program listesi: WEEKLY / DATE_RANGE / ONCE × SETPOINT / PROFILE / HEATING_OFF / VENTILATE, durumsuz değerlendirme, belirlilik önceliği.

## Karar
Seçenek 3 ([PROGRAMS.md](../PROGRAMS.md)). Öncelik zinciri: BOOST › açık profil seçimi › yerel program › `sched_away` › `sched_night` › DAY. Antifreeze ve Safety zincirin üstünde. Programlar yalnız geçerli duvar saatiyle çalışır.

## Gerekçe
- Suite olmadan zamanlama (MQTT LOST ≠ LOCAL CONTROL LOST ilkesiyle uyumlu).
- Durumsuz değerlendirme: boot, saat düzeltmesi ve kesintide kaçırılan kenar yok; test edilebilir saf fonksiyon.
- Belirlilik önceliği (ONCE › DATE_RANGE › WEEKLY) günlük kullanımı sezgisel kılar: tatil aralığı haftalık programı, tek seferlik etkinlik ikisini de geçersiz kılar.
- Kullanıcının açık seçimi ve BOOST, zamanlamadan üstün kalır (ADR-005 ilkesi korunur).

## Sonuçlar
- Saat kaynağı (checklist 16) programlar için zorunlu hâle gelir.
- Yeni değerler: `ProfileActive::PROGRAM`, `SetpointSource::PROGRAM`, olaylar `PROGRAM_START/END/HOLD`, `PROGRAMS_CHANGED`; entity'ler `programs_enabled`, `program_active`, `program_hold`, `program_until` (F5).
- Aşırı sıcaklık limiti program hedeflerinin en az 10 °C üstünde kalmalıdır (P8).
- UI'ye Programlar sayfası eklenir; menü 9 öğe (mobil 3×3).
