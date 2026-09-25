# ADR-008 — Rezistans ARM hattı ve bağımsız donanım korumaları

Durum: DESIGN DECISION / önerilen (donanım onayı bekliyor) · 24.09.2026

## Problem
Tek GPIO'nun takılı kalması, boot sırasında kısa yanlış seviye, yazılım hatası veya MCU donması rezistansı enerjilendirebilir. SSR'lerin tipik arıza modu kısa devredir (sürekli iletim).

## Karar
- Rezistans sürme = kanal GPIO ∧ `HEATER_ARM` (yalnız SafetyTask sürer) ∧ (önerilen) dinamik güvenlik sinyali; tüm girişlerde pull-down.
- Fan sürme yolu ARM'dan bağımsızdır (post-cool için).
- Bağımsız termik kesici (seri, tercihen elle resetli), sigorta/MCB, RCD, galvanik izolasyon, PE zorunlu; firmware bunların yerine geçmez.

## Sonuçlar
Ek bir GPIO ve lojik/transistör katı; OutputTask donmasında dinamik sinyal yoksa post-cool yazılımla sağlanamaz (termik kesici ve rezistans/fan fiziksel tasarımı kapsar). SSR kısa devre arızasını yalnız termik kesici sınırlar.

## Uygulama notu — F2 (25.09.2026)
Kullanıcı kararıyla v1 donanımında **ARM hattı yoktur** (sapma, [CHANGELOG F2](../CHANGELOG.md) #1). Telafi: OutputTask 1 s çalışmazsa veya SafetyTask çekirdek kilidini 1 s alamazsa acil yol R hatlarını doğrudan pasife çeker ve yeniden başlatır; SafetyTask donarsa TWDT ≤ 5 s içinde reset eder (pinler yüksek empedans → pull-down). Artık risk ≤ 5 s; elle resetli termik kesici zorunluluğu değişmez. GPIO17 ARM için ayrılmıştır.
