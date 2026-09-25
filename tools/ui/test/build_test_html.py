#!/usr/bin/env python3
"""Önizleme/test sayfası üretir: gerçek index.html + app.css + app.js + theme.js + sahte cihaz (mock_device.js).
  test/ui_test.html      : file:// ile açılan tam belge (Google Fonts bağlantısıyla; cihazda fontlar yereldir)
  test/ui_preview.html   : Artifact gövdesi (doctype/html/head/body olmadan)
Gevşek test sayfası CSP kanıtı değildir; üretim başlıkları assemble.py + verify_ui ile sınanır."""
import pathlib, re
UI = pathlib.Path(__file__).resolve().parent.parent
read = lambda p: (UI / p).read_text(encoding='utf-8')
css, js, theme, mock = read('app.css'), read('app.js'), read('theme.js'), read('test/mock_device.js')
html = read('index.html')
body = html.split('<body>', 1)[1].rsplit('</body>', 1)[0]
body = body.replace('<script src="/app.js"></script>', '')
FONTS = ('<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>'
         '<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono&family=IBM+Plex+Sans:wght@400;600&display=swap">')
scripts = f'<script>{mock}</script>\n<script>{js}</script>'
full = ('<!doctype html>\n<html lang="tr">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">\n'
        f'<title>Kulübe İklim</title>\n{FONTS}\n<script>{theme}</script>\n<style>{css}</style>\n</head>\n<body>{body}{scripts}\n</body>\n</html>\n')
(UI / 'test/ui_test.html').write_text(full, encoding='utf-8')
art = (f'<title>Kulübe İklim HMI</title>\n{FONTS}\n<script>{theme}</script>\n<style>{css}</style>\n{body}{scripts}\n')
(UI / 'test/ui_preview.html').write_text(art, encoding='utf-8')
print('ui_test.html', len(full), 'ui_preview.html', len(art))
