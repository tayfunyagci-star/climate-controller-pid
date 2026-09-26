"""LED ve Bakım sayfası doğrulaması (Playwright, Python).
Önce: python tools/ui/test/build_test_html.py ; sonra: python tools/ui/test/verify_led_maint.py
Ekran görüntüleri test/ klasörüne shot_*.png olarak yazılır (git dışı)."""
import asyncio, json, sys, os
from playwright.async_api import async_playwright
HERE = os.path.dirname(os.path.abspath(__file__))
URL = 'file://' + HERE + '/ui_test.html#settings'
res = []
def ok(name, cond, info=''):
    res.append((name, bool(cond), info)); print(('PASS ' if cond else 'FAIL ') + name + ('' if cond else '  ' + str(info)))

async def open_tab(page, tab):
    await page.click('#tab-' + tab); await page.wait_for_timeout(300)

async def setup(b, w, theme):
    ctx = await b.new_context(viewport={'width': w, 'height': 900})
    page = await ctx.new_page()
    errs = []
    page.on('pageerror', lambda e: errs.append(str(e)))
    await page.add_init_script(f"try{{localStorage.setItem('scada-tema','{theme}')}}catch(e){{}}")
    await page.goto(URL); await page.wait_for_timeout(1500)
    await page.evaluate("""() => { window.__calls = []; const f = window.fetch; window.fetch = (u, o) => { if (o && o.method === 'POST') window.__calls.push(String(u)); return f(u, o); }; }""")
    return ctx, page, errs

async def dlg(page, accept):
    await page.wait_for_selector('#dlg[open]', timeout=3000)
    txt = await page.inner_text('#dlg-title')
    await page.click('#dlg-ok' if accept else '#dlg-cancel'); await page.wait_for_timeout(300)
    return txt

async def calls(page): return await page.evaluate('window.__calls.splice(0)')

async def main():
    async with async_playwright() as p:
        b = await p.chromium.launch()
        # ---- görsel matris
        for theme in ('light', 'dark'):
            for w in (1280, 768, 390):
                ctx, page, errs = await setup(b, w, theme)
                for tab in ('led', 'maint'):
                    await open_tab(page, tab)
                    await page.screenshot(path=f'{HERE}/shot_{tab}_{theme}_{w}.png', full_page=True)
                    ov = await page.evaluate('document.documentElement.scrollWidth - innerWidth')
                    ok(f'taşma yok {tab} {theme} {w}', ov <= 0, ov)
                    bad = await page.evaluate("""(t) => [...document.querySelectorAll('#panel-'+t+' *')].filter(e => e.offsetParent && e.childNodes.length && [...e.childNodes].some(n => n.nodeType===3 && n.textContent.trim())).map(e => getComputedStyle(e).fontSize).filter(s => !['10px','11px','12px','14px'].includes(s))""", tab)
                    ok(f'yazı ölçeği {tab} {theme} {w}', not bad, bad[:5])
                ok(f'konsol hatası yok {theme} {w}', not errs, errs)
                await ctx.close()
        # ---- işlev
        ctx, page, errs = await setup(b, 1280, 'light')
        await open_tab(page, 'led')
        combos = {
            'LED1 normal/uyarı/alarm': [([0,1,1,1,0,0], 0, 'Normal', False), ([1,1,1,1,0,0], 0, 'Uyarı', True), ([2,1,1,1,0,0], 0, 'Alarm', True)],
            'LED2 yok/Wi-Fi/AP': [([0,0,1,1,0,0], 1, 'Bağlantı yok', False), ([0,1,1,1,0,0], 1, 'Wi-Fi bağlı', False), ([0,2,1,1,0,0], 1, 'AP kurulum', False)],
            'LED3 kesik/bağlı/tanımsız': [([0,1,0,1,0,0], 2, 'Kesik', False), ([0,1,1,1,0,0], 2, 'Bağlı', False), ([0,1,2,1,0,0], 2, 'Tanımsız', False)],
            'LED4 yok/hazır/devre dışı': [([0,1,1,0,0,0], 3, 'Yok', False), ([0,1,1,1,0,0], 3, 'Hazır', False), ([0,1,1,2,0,0], 3, 'Devre dışı', False)],
            'LED5 ısıtma 0/1/2': [([0,1,1,1,0,0], 4, 'Kapalı', False), ([0,1,1,1,1,0], 4, '1 kademe', False), ([0,1,1,1,2,0], 4, '2 kademe', False)],
            'LED6 fan 0/1/2': [([0,1,1,1,0,0], 5, 'Kapalı', False), ([0,1,1,1,0,1], 5, 'Isıtıcı fanı', False), ([0,1,1,1,0,2], 5, 'Havalandırma', False)],
        }
        for name, cases in combos.items():
            for st, i, label, blink in cases:
                await page.evaluate(f'__mockFlags.ledStates = {json.dumps(st)}'); await page.wait_for_timeout(1400)
                r = await page.evaluate(f"""() => {{ const el = document.querySelectorAll('.led-live')[{i}]; const d = el.querySelector('.dot');
                  return [el.querySelector('.led-live-st').textContent, d.classList.contains('blink'), getComputedStyle(d).backgroundColor, el.getAttribute('aria-label')]; }}""")
                ok(f'{name}: {label}', r[0] == label and r[1] == blink and r[3] and label in r[3], r)
        yel = await page.evaluate("[...document.querySelectorAll('.led-live')][2].querySelector('.dot').style.background")
        await page.evaluate('__mockFlags.ledStates = [0,1,2,2,0,0]'); await page.wait_for_timeout(1400)
        yel = await page.evaluate("[2,3].map(i => getComputedStyle(document.querySelectorAll('.led-live')[i].querySelector('.dot')).backgroundColor)")
        ok('MQTT tanımsız ve mDNS devre dışı sarı', yel == ['rgb(255, 255, 0)'] * 2, yel)
        await page.evaluate('__mockFlags.ledStates = null')
        for v in ('0', '50', '100'):
            await page.evaluate(f"(() => {{ const r = document.querySelector('#f-ledB'); r.value = '{v}'; r.dispatchEvent(new Event('input', {{bubbles: true}})); }})()")
            out = await page.inner_text('.range-out')
            ok(f'parlaklık {v}', out.strip() == v + ' %', out)
        rw = await page.evaluate("document.querySelector('.range-row').getBoundingClientRect().width")
        ok('parlaklık kaydırıcısı kontrollü genişlik', rw <= 460, rw)
        dot = await page.evaluate("getComputedStyle(document.querySelector('.led-live .dot')).width")
        ok('canlı LED noktası 18 px', dot == '18px', dot)
        # Bakım
        await page.add_init_script("window.__MOCK_INIT = {pinSet: false}")
        await page.reload(); await page.wait_for_timeout(1500)
        await page.evaluate("""() => { window.__calls = []; const f = window.fetch; window.fetch = (u, o) => { if (o && o.method === 'POST') window.__calls.push(String(u)); return f(u, o); }; }""")
        await page.evaluate('__mockFlags.pinSet = false')
        await open_tab(page, 'maint')
        st = await page.inner_text('#svc-state')
        ok('servis PIN yok durumu', 'tanımlı değil' in st, st)
        await page.click('#svc-btn'); await dlg(page, True); await page.wait_for_timeout(500)
        st = await page.inner_text('#svc-state')
        ok('PIN yokken giriş 409 mesajı', 'tanımlı değil' in st, st)
        await open_tab(page, 'access'); await page.fill('#svc-pin', '1234'); await page.click('button:has-text("PIN’i kaydet")'); await page.wait_for_timeout(500)
        await open_tab(page, 'maint'); st = await page.inner_text('#svc-state')
        ok('PIN Erişim\'de kaydedilince Bakım durumu güncellenir', 'PIN tanımlı' in st, st)
        await calls(page)
        await page.fill('#svc-enter-pin', '9999'); await page.click('#svc-btn'); await dlg(page, True); await page.wait_for_timeout(500)
        st = await page.inner_text('#svc-state'); cls = await page.get_attribute('#svc-state', 'class')
        ok('servis modu giriş hatası (yanlış PIN)', 'yanlış' in st and 'crit' in cls, [st, cls])
        await calls(page)
        await page.evaluate("__mockCfg.operating_mode = 'OFF'"); await page.wait_for_timeout(6000)
        await page.fill('#svc-enter-pin', ''); await page.click('#svc-btn'); await page.wait_for_timeout(300)
        ok('boş PIN istemci doğrulaması, diyalog yok', not await page.evaluate("document.querySelector('#dlg').open"), '')
        await page.fill('#svc-enter-pin', '1234'); await page.click('#svc-btn'); await dlg(page, False)
        ok('servis girişi vazgeç → istek yok', not [c for c in await calls(page) if 'service' in c], '')
        await page.fill('#svc-enter-pin', '1234'); await page.click('#svc-btn'); await dlg(page, True); await page.wait_for_timeout(1500)
        st = await page.inner_text('#svc-state'); bt = await page.inner_text('#svc-btn')
        ok('servis modu etkin + kalan süre + çıkış düğmesi', 'etkin' in st and 'kalan' in st and bt.strip() == 'Servis modundan çık', [st, bt])
        await page.click('#svc-btn'); await page.wait_for_timeout(1500)
        bt = await page.inner_text('#svc-btn')
        ok('servis modundan çıkış', bt.strip() == 'Servis moduna gir', bt)
        await calls(page)
        kv = await page.inner_text('.red-zone .kv')
        ok('Wi-Fi kayıtlı: SSID + parola maskeli', 'Kulube-Ag' in kv and 'Kayıtlı' in kv, kv)
        await page.click('.red-zone button:has-text("Ağ tara")'); await page.wait_for_timeout(800)
        opened = await page.evaluate("[...document.querySelectorAll('dialog')].some(d => d.open)")
        ok('Wi-Fi değiştirme diyaloğu açılır', opened, '')
        await page.keyboard.press('Escape'); await page.wait_for_timeout(400)
        otype = await page.get_attribute('#ota-pw', 'type')
        ok('OTA parola alanı password', otype == 'password', otype)
        await page.click('.red-zone button:has-text("Firmware yükle")'); await page.wait_for_timeout(400)
        ok('firmware dosyası yok → istek yok', not await calls(page), '')
        tmp = HERE + '/fw.bin'; open(tmp, 'wb').write(b'\xe9' * 1024)
        await page.set_input_files('#ota-file', tmp)
        await page.click('.red-zone button:has-text("Firmware yükle")')
        try: t = await dlg(page, False)
        except Exception:
            await page.screenshot(path=HERE + '/dbg.png'); print(await page.evaluate("[document.querySelector('#ota-file').files.length, [...document.querySelectorAll('dialog')].map(d=>d.id+':'+d.open), document.querySelector('#toasts').innerText]")); raise
        ok('firmware seçilmiş → onay; vazgeç → istek yok', 'Firmware' in t and not await calls(page), t)
        await page.fill('#ota-pw', ''); await page.click('.red-zone button:has-text("Firmware yükle")'); await dlg(page, True); await page.wait_for_timeout(800)
        c = await calls(page); ok('OTA parolasız hazırlık isteği', any('ota/begin' in x for x in c), c)
        await page.fill('#ota-pw', 'otaparola1'); await page.click('.red-zone button:has-text("Firmware yükle")'); await dlg(page, True); await page.wait_for_timeout(800)
        c = await calls(page); ok('OTA parolalı hazırlık isteği', any('ota/begin' in x for x in c), c)
        await page.wait_for_timeout(2500)
        for label, path in (('Yeniden başlat', 'reboot'), ('Wi-Fi bilgilerini sil', 'reset-wifi')):
            await page.click(f'.red-zone button:has-text("{label}")'); await dlg(page, False)
            ok(f'{label}: vazgeç → istek yok', not [x for x in await calls(page) if path in x], '')
            await page.click(f'.red-zone button:has-text("{label}")'); await dlg(page, True); await page.wait_for_timeout(600)
            ok(f'{label}: onay → istek', any(path in x for x in await calls(page)), '')
            await page.evaluate('__mockFlags.ap = false; __mockFlags.ssid = "Kulube-Ag"')
        fb = '.red-zone button:has-text("Fabrika ayarlarına dön")'
        cls = await page.get_attribute(fb, 'class')
        ok('fabrika düğmesi danger critical', 'critical' in cls, cls)
        await page.click(fb); await dlg(page, False)
        ok('fabrika: 1. onay vazgeç → istek yok', not await calls(page), '')
        await page.click(fb); await dlg(page, True); t2 = await dlg(page, False)
        ok('fabrika: 2. onay vazgeç → istek yok', 'son onay' in t2 and not await calls(page), t2)
        await page.click(fb); await dlg(page, True); await dlg(page, True); await page.wait_for_timeout(600)
        ok('fabrika: iki onay → istek', any('factory-reset' in x for x in await calls(page)), '')
        await page.click('.red-zone button:has-text("Sayaçları sıfırla")'); await dlg(page, True); await page.wait_for_timeout(500)
        ok('sayaç sıfırlama onaylı istek', any('reset-counters' in x for x in await calls(page)), '')
        # stil ölçümleri
        m = await page.evaluate("""() => { const z = document.querySelector('.red-zone'), cs = getComputedStyle(z); const b = getComputedStyle(document.querySelector('#svc-btn'));
          return {bs: cs.borderStyle, bc: cs.borderColor, bg: cs.backgroundColor, r: cs.borderRadius, btnBg: b.backgroundColor, btnBorder: b.borderColor, sh: b.boxShadow, h3: getComputedStyle(z.querySelector('h3')).textTransform, emoji: z.querySelector('h3').textContent.includes('⚠')}; }""")
        ok('kırmızı alan: kesikli #c84a35, düşük radius, ikon (emoji yok)', m['bs'] == 'dashed' and m['bc'] == 'rgb(200, 74, 53)' and m['r'] in ('4px', '5px', '6px') and not m['emoji'], m)
        ok('normal düğme beyaz, açık çerçeve, gölgesiz', m['btnBg'] == 'rgb(255, 255, 255)' and m['sh'] == 'none', m)
        # erişilebilirlik
        a = await page.evaluate("""() => [...document.querySelectorAll('#panel-maint input, #panel-maint select')].filter(i => i.type !== 'hidden' && !(i.labels && i.labels.length) && !i.getAttribute('aria-label')).map(i => i.id)""")
        ok('bakım alanları etiketli', not a, a)
        nb = await page.evaluate("""() => [...document.querySelectorAll('#panel-maint button, #panel-led button')].filter(b => !(b.textContent.trim() || b.getAttribute('aria-label'))).length""")
        ok('adsız düğme yok', nb == 0, nb)
        ok('işlev testinde konsol hatası yok', not errs, errs)
        await ctx.close(); await b.close()
    f = [r for r in res if not r[1]]
    print(f'\n{len(res) - len(f)}/{len(res)} geçti')
asyncio.run(main())
