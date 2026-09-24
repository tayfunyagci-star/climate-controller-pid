# ADR-002 — Kademeli zaman-oransal güç yönetimi

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Problem
İki rezistansla 0–100 % talebi hem iyi çözünürlükle hem düşük anahtarlama/flicker ile karşılamak; sürücü (SSR/röle) seçilmedi.

## Seçenekler
Sabit kademe; tek duty ile iki rezistans birlikte; hızlı SSR PWM; kademeli zaman-oransal (R1 modüle, sonra R1 tam + R2 modüle); röle için kademe + uzun pencere.

## Karar
Varsayılan kademeli zaman-oransal; kademe geçişinde 55/45 % histerezis ve minimum kalış süresi; R1/R2 pencereleri yarım faz kaydırılmış; lider rotasyonu. Sürücü profili `RELAY` ise pencere ≥ 300 s, min ON/OFF ≥ 60 s zorunlu; doğrulama daha kısa değerleri reddeder.

## Gerekçe
Anlık güç ve flicker tek rezistans kadar; çözünürlük pencere/min darbe ile sınırlı (%5 @ SSR_ZC 20 s); röle ömrü profille korunur.

## Sonuçlar
Histerezis bandında ±5 % güç sapması (PID telafi eder). Farklı güçlü rezistanslar için oranlı eşleme gerekir. Faz açısı kontrolü kapsam dışı.
