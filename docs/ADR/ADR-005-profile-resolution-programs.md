# ADR-005 — Profil çözümü ve Programs entegrasyonu

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Problem
Suite Programs yalnız AÇ/KAPAT talebi üretir, entity bazında "herhangi AÇ ise AÇ" birleştirir ve program bitişi doğrudan KAPAT değildir (PROGRAMLAR.md §3). Setpoint'i doğrudan zamanlamak bu semantiğe uymaz; aynı select'i farklı AÇ değerleriyle hedefleyen programlar belirsizlik yaratır.

## Seçenekler
1. Programs `temperature_setpoint` number'ını hedefler (AÇ=18, KAPAT=21).
2. Programs `profile` select'ini hedefler (AÇ=NIGHT, KAPAT=DAY).
3. Profil istek anahtarları `sched_night`, `sched_away` (+ `boost`); profil önceliği cihazda çözülür.

## Karar
Seçenek 3. Öncelik: BOOST › açık kullanıcı seçimi (`profile ≠ DAY`) › `sched_away` › `sched_night` › DAY. `sched_*` isteği `sched_timeout_h` sonra kendiliğinden düşer.

## Gerekçe
AÇ/KAPAT semantiği birebir; çakışan programlar entity bazında doğru birleşir; kullanıcının açık seçimi zamanlamadan üstündür; Suite kesilse bile takılı istek sınırlıdır.

## Sonuçlar
İki ek switch entity. Yerel haftalık program yok (FUTURE). Seçenek 2 dokümante edilir ama önerilmez.
