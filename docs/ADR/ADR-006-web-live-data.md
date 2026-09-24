# ADR-006 — Web canlı veri: HTTP polling

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Seçenekler
WebSocket push; Server-Sent Events; 1 s HTTP polling (`/api/data`).

## Karar
v1: 1 s polling, `STALE_MS=4000`, `CONFIRM_MS=5000` (scada-ui-design §7). Trend geçmişi açılışta `/api/trend` ile toplu alınır.

## Gerekçe
Skill standardı ve test kiti polling üzerine kurulu; yeniden bağlanma ve bayatlık mantığı basit; 1–2 istemcide ~1.5 KB/s yük önemsiz; httpd'de uzun ömürlü bağlantı ve soket bütçesi riski yok. MQTT web için gerekmez.

## Sonuçlar
En kötü 1 s gecikme; çok istemcide istek sayısı artar. FUTURE: WebSocket (aynı snapshot biçimi), polling yedek olarak kalır.
