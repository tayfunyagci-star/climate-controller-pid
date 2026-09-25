# Devre şeması üreticisi: IEC tarzı semboller, satır içi SVG
import html
E = html.escape

class S:
    def __init__(s): s.o = []
    def add(s, x): s.o.append(x)
    def line(s, x1, y1, x2, y2, cls="w"):
        s.add(f'<line class="{cls}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}"/>')
    def poly(s, pts, cls="w"):
        s.add(f'<polyline class="{cls}" points="{" ".join(f"{x},{y}" for x, y in pts)}"/>')
    def text(s, x, y, t, cls="t", anchor="start"):
        s.add(f'<text class="{cls}" x="{x}" y="{y}" text-anchor="{anchor}">{E(t)}</text>')
    def dot(s, x, y):
        s.add(f'<circle class="dot" cx="{x}" cy="{y}" r="3.2"/>')
    def rect(s, x, y, w, h, cls="c"):
        s.add(f'<rect class="{cls}" x="{x}" y="{y}" width="{w}" height="{h}"/>')

    # --- pasif elemanlar ---
    def res_h(s, x, y, ref, val, above=True, cls="c"):   # x: sol uç, 60 uzun
        s.line(x, y, x + 15, y); s.rect(x + 15, y - 6, 30, 12, cls); s.line(x + 45, y, x + 60, y)
        yy = y - 12 if above else y + 22
        s.text(x + 30, yy, f"{ref} {val}", "lbl", "middle")
    def res_v(s, x, y, ref, val, side="r", cls="c"):     # y: üst uç, 60 uzun
        s.line(x, y, x, y + 15); s.rect(x - 6, y + 15, 12, 30, cls); s.line(x, y + 45, x, y + 60)
        if side == "r": s.text(x + 11, y + 28, ref, "ref"); s.text(x + 11, y + 42, val, "lbl")
        else: s.text(x - 11, y + 28, ref, "ref", "end"); s.text(x - 11, y + 42, val, "lbl", "end")
    def cap_v(s, x, y, ref, val, pol=False, side="r"):   # y üst, 50 uzun
        s.line(x, y, x, y + 21); s.line(x - 11, y + 21, x + 11, y + 21, "c")
        if pol:
            s.add(f'<path class="c" d="M{x-11},{y+31} Q{x},{y+25} {x+11},{y+31}"/>')
            s.text(x + (16 if side == "l" else -16), y + 18, "+", "lbl", "middle")
        else:
            s.line(x - 11, y + 29, x + 11, y + 29, "c")
        s.line(x, y + 29 if not pol else y + 28, x, y + 50)
        a = "start" if side == "r" else "end"; dx = 15 if side == "r" else -15
        s.text(x + dx, y + 22, ref, "ref", a); s.text(x + dx, y + 36, val, "lbl", a)
    def gnd(s, x, y):
        s.line(x, y, x, y + 8); s.line(x - 10, y + 8, x + 10, y + 8, "c"); s.line(x - 6, y + 12, x + 6, y + 12, "c"); s.line(x - 2, y + 16, x + 2, y + 16, "c")
    def flag(s, x, y, name, up=True):
        d = -1 if up else 1
        s.line(x, y, x, y + d * 10); s.line(x - 9, y + d * 10, x + 9, y + d * 10, "c")
        s.text(x, y + d * (16 if up else 24), name, "net", "middle")
    def npn(s, cx, cy, ref, val):   # baz solda (cx-22), C üst (cx+12, cy-30), E alt (cx+12, cy+30)
        s.add(f'<circle class="c" cx="{cx}" cy="{cy}" r="22"/>')
        s.line(cx - 22, cy, cx - 6, cy); s.line(cx - 6, cy - 12, cx - 6, cy + 12, "c")
        s.line(cx - 6, cy - 5, cx + 12, cy - 16, "c"); s.line(cx + 12, cy - 16, cx + 12, cy - 30)
        s.line(cx - 6, cy + 5, cx + 12, cy + 16, "c"); s.line(cx + 12, cy + 16, cx + 12, cy + 30)
        s.add(f'<polygon class="fillc" points="{cx+12},{cy+16} {cx+2},{cy+15} {cx+7},{cy+8}"/>')
        s.text(cx + 30, cy - 2, ref, "ref"); s.text(cx + 30, cy + 12, val, "lbl")
    def diode_h(s, x, y, ref, val, schottky=True):   # anot solda, 60 uzun
        s.line(x, y, x + 20, y); s.add(f'<polygon class="fillc" points="{x+20},{y-9} {x+20},{y+9} {x+36},{y}"/>')
        s.line(x + 36, y - 9, x + 36, y + 9, "c")
        if schottky: s.poly([(x + 32, y - 6), (x + 32, y - 9), (x + 36, y - 9)], "c"); s.poly([(x + 36, y + 9), (x + 40, y + 9), (x + 40, y + 6)], "c")
        s.line(x + 36, y, x + 60, y)
        s.text(x + 30, y - 16, f"{ref} {val}", "lbl", "middle")
    def fuse_v(s, x, y, ref, val, side="r"):   # 50 uzun
        s.line(x, y, x, y + 12); s.rect(x - 7, y + 12, 14, 26); s.line(x, y + 8, x, y + 42, "c"); s.line(x, y + 38, x, y + 50)
        a = "start" if side == "r" else "end"; dx = 14 if side == "r" else -14
        s.text(x + dx, y + 24, ref, "ref", a); s.text(x + dx, y + 38, val, "lbl", a)
    def switch_v(s, x, y, cls="c"):   # açık kontak, 50 uzun
        s.line(x, y, x, y + 15); s.line(x, y + 35, x, y + 50); s.line(x, y + 35, x - 13, y + 17, cls)
    def conn(s, x, y, pins, title, left=True, w=74):  # pinler 20 aralıklı, pin noktası kutunun solunda/sağında
        h = 20 * len(pins) + 10
        s.rect(x, y, w, h, "box")
        s.text(x + w / 2, y - 8, title, "ref", "middle")
        pts = []
        for i, p in enumerate(pins):
            py = y + 15 + 20 * i
            if left:
                s.add(f'<circle class="pin" cx="{x}" cy="{py}" r="3"/>'); s.text(x + 8, py + 4, p, "pinl")
                pts.append((x, py))
            else:
                s.add(f'<circle class="pin" cx="{x+w}" cy="{py}" r="3"/>'); s.text(x + w - 8, py + 4, p, "pinl", "end")
                pts.append((x + w, py))
        return pts
    def svg(s, w, h, label, idname):
        return (f'<svg id="{idname}" viewBox="0 0 {w} {h}" role="img" aria-label="{E(label)}" '
                f'xmlns="http://www.w3.org/2000/svg">' + "".join(s.o) + "</svg>")

