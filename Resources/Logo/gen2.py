"""Runde 2: verfeinerter Amboss, Waveform mit Verjüngung (dick am Transient)."""
import math, sys
OUT = sys.argv[1]

def P(pts, close=True):
    d = "M " + " L ".join(f"{x:.2f} {y:.2f}" for x, y in pts)
    return d + (" Z" if close else "")

def kick_wave(x0, x1, cy, amp, n=260, fStart=9.0, fEnd=2.0, ampDecay=3.0, pitchDecay=7.0):
    pts, phase = [], 0.0
    for i in range(n + 1):
        t = i / n
        f = fEnd + (fStart - fEnd) * math.exp(-t * pitchDecay)
        phase += f * (1.0 / n) * 2 * math.pi
        env = math.exp(-t * ampDecay)
        pts.append((x0 + (x1 - x0) * t, cy - amp * env * math.sin(phase)))
    return pts

def taper_outline(pts, w0, w1):
    L, R = [], []
    n = len(pts)
    for i, (x, y) in enumerate(pts):
        a = pts[max(i-1, 0)]; b = pts[min(i+1, n-1)]
        dx, dy = b[0]-a[0], b[1]-a[1]
        l = math.hypot(dx, dy) or 1.0
        w = w0 + (w1 - w0) * (i / (n - 1))
        nx, ny = -dy/l*w/2, dx/l*w/2
        L.append((x+nx, y+ny)); R.append((x-nx, y-ny))
    return P(L + R[::-1])

DEFS = """
  <defs>
    <linearGradient id="orange" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#FFA040"/>
      <stop offset="0.5" stop-color="#FF6A1A"/>
      <stop offset="1" stop-color="#D2450E"/>
    </linearGradient>
    <linearGradient id="steelTop" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#8A847E"/>
      <stop offset="1" stop-color="#57524D"/>
    </linearGradient>
    <linearGradient id="steel" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#4A4541"/>
      <stop offset="0.6" stop-color="#2C2927"/>
      <stop offset="1" stop-color="#1B1918"/>
    </linearGradient>
    <linearGradient id="hot" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#FFB050"/>
      <stop offset="0.35" stop-color="#FF6A1A"/>
      <stop offset="1" stop-color="#B8380A"/>
    </linearGradient>
  </defs>"""

# Klassischer Amboss, kantig: Horn (spitz), Face, Heel, Taille, Fuß mit Kerbe
FACE_Y = 44
anvil = [
    (4, 47),            # Hornspitze
    (24, FACE_Y + 1),   # Horn oben
    (30, FACE_Y),       # Übergang Face
    (94, FACE_Y),       # Face rechts (Heel)
    (94, 58),           # Heel unten
    (80, 58),
    (72, 64),           # Taille rechts
    (72, 74),
    (86, 82), (86, 92), # Fuß rechts
    (62, 92), (58, 87), (50, 87), (46, 92),   # Kerbe im Fuß
    (22, 92), (22, 82),
    (36, 74), (36, 64), # Taille links
    (30, 58),
    (22, 56),           # Horn unten
]
face_bevel = [(30, FACE_Y), (94, FACE_Y), (94, FACE_Y + 4), (31, FACE_Y + 4)]
horn_hi    = [(4, 47), (24, FACE_Y + 1), (30, FACE_Y), (31, FACE_Y + 3), (24, FACE_Y + 3.6), (8, 47.2)]

def c1():
    # Waveform auf der Face, erster Ausschlag als "Schlag", verjüngt nach rechts
    wave = kick_wave(30, 92, FACE_Y - 15, 15, fStart=7.0, fEnd=1.6, ampDecay=3.4, pitchDecay=6.0)
    return f"""
  <path d="{P(anvil)}" fill="url(#steel)"/>
  <path d="{P(horn_hi)}" fill="url(#steelTop)"/>
  <path d="{P(face_bevel)}" fill="url(#steelTop)"/>
  <path d="{taper_outline(wave, 5.0, 1.6)}" fill="url(#hot)"/>"""

def c2():
    # Glühender Barren auf der Face, Waveform eingraviert in den Ambosskörper
    wave = kick_wave(40, 70, 70, 7.5, n=200, fStart=5.0, fEnd=1.4, ampDecay=2.6, pitchDecay=5.0)
    bar = [(34, FACE_Y - 6), (88, FACE_Y - 6), (92, FACE_Y), (30, FACE_Y)]
    return f"""
  <path d="{P(anvil)}" fill="url(#steel)"/>
  <path d="{P(horn_hi)}" fill="url(#steelTop)"/>
  <path d="{P(face_bevel)}" fill="url(#steelTop)"/>
  <path d="{P(bar)}" fill="url(#orange)"/>
  <path d="{taper_outline(wave, 3.0, 1.2)}" fill="url(#hot)"/>"""

for name, fn in [("c1", c1), ("c2", c2)]:
    open(f"{OUT}/{name}.svg", "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">{DEFS}{fn()}\n</svg>\n')
print("ok")
