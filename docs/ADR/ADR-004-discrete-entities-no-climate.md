# ADR-004 — Ayrık entity'ler; Home Assistant `climate` bileşeni kullanılmaz

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Problem
HA'nın `climate` bileşeni termostat için doğal görünür; ancak Suite sunucusu `discovery.parse` yalnız sensor, binary_sensor, switch, light, siren, fan, number, select, button, text'i kabul eder ve diğerlerinde ValueError üretir; tarayıcı `haConfig` bilinmeyen bileşeni salt-okunur sensor'a düşürür.

## Karar
Yalnız Suite'in tanıdığı bileşenler; tek tablodan üretilen ayrık entity'ler; tüm `val_tpl` düz alan. `climate` yalnız FUTURE bayrağı (`ha_climate_entity`, varsayılan kapalı).

## Sonuçlar
HA kullanıcıları hazır termostat kartı yerine ayrık entity görür. Suite'te tam kontrol ve doğrulama korunur. Suite'e `climate` desteği eklenirse bu ADR yeniden değerlendirilir.
