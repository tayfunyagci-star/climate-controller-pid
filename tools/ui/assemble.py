#!/usr/bin/env python3
"""tools/ui → include/ui_generated.h (scada-ui-design §2).
Satır sonu normalize, gzip(level 9, mtime=0), CSS/JS adreslerine ?v=<sha256[:10]>. Aynı kaynak → aynı çıktı.
Kullanım: python tools/ui/assemble.py [--dump DIR]"""
import gzip, hashlib, io, pathlib, sys

UI = pathlib.Path(__file__).resolve().parent
ROOT = UI.parent.parent
OUT = ROOT / 'include' / 'ui_generated.h'

def rd(name):
    return (UI / name).read_bytes().replace(b'\r\n', b'\n')

def gz(data):
    b = io.BytesIO()
    with gzip.GzipFile(fileobj=b, mode='wb', compresslevel=9, mtime=0) as f:
        f.write(data)
    return b.getvalue()

def ver(data):
    return hashlib.sha256(data).hexdigest()[:10]

def main():
    css, js, theme = rd('app.css'), rd('app.js'), rd('theme.js')
    html = rd('index.html')
    for name, data in (('app.css', css), ('app.js', js), ('theme.js', theme)):
        html = html.replace(f'"/{name}"'.encode(), f'"/{name}?v={ver(data)}"'.encode())
    assets = [('/', 'text/html; charset=utf-8', html, False),
              ('/app.css', 'text/css; charset=utf-8', css, True),
              ('/app.js', 'application/javascript; charset=utf-8', js, True),
              ('/theme.js', 'application/javascript; charset=utf-8', theme, True)]
    build = 'r' + hashlib.sha256(b''.join(a[2] for a in assets)).hexdigest()[:8]
    lines = ['// OTOMATİK ÜRETİLDİ — tools/ui/assemble.py. Elle düzenlemeyin; kaynak tools/ui/.',
             '#pragma once', '#include <cstddef>', '#include <cstdint>', '', 'namespace ui {', '',
             'struct Asset {', '  const char* path;', '  const char* type;', '  const uint8_t* data;',
             '  size_t len;', '  bool immutable;  // ?v= sürümlü: max-age=31536000, immutable', '};', '']
    raw_total = gz_total = 0
    for i, (path, typ, data, imm) in enumerate(assets):
        z = gz(data)
        raw_total += len(data); gz_total += len(z)
        body = ','.join(str(b) for b in z)
        lines.append(f'static const uint8_t kA{i}[] = {{{body}}};')
    lines.append('')
    lines.append('static const Asset kAssets[] = {')
    for i, (path, typ, data, imm) in enumerate(assets):
        lines.append(f'  {{"{path}", "{typ}", kA{i}, sizeof(kA{i}), {"true" if imm else "false"}}},')
    lines += ['};', f'static const size_t kAssetCount = {len(assets)};', f'static const char kUiBuild[] = "{build}";',
              f'// ham {raw_total} B, gzip {gz_total} B', '', '}  // namespace ui', '']
    text = '\n'.join(lines)
    if not OUT.exists() or OUT.read_text(encoding='utf-8') != text:
        OUT.write_text(text, encoding='utf-8', newline='\n')
    print(f'ui_generated.h: {build} ham {raw_total} B → gzip {gz_total} B')
    if '--dump' in sys.argv:
        d = pathlib.Path(sys.argv[sys.argv.index('--dump') + 1]); d.mkdir(parents=True, exist_ok=True)
        for path, _, data, _ in assets:
            (d / ('index.html' if path == '/' else path[1:])).write_bytes(data)

if __name__ == '__main__':
    main()
