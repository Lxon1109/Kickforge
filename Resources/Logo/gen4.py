import math, sys
exec(open("gen2.py").read().split("def c1():")[0])

BASE = FACE_Y + 2.0   # Mittellinie liegt hinter der Schlagfläche (verdeckt)

def peaks(x0, x1, amp, fStart, fEnd, ampDecay, pitchDecay, sharp=1.6, n=400):
    pts, phase = [], 0.0
    for i in range(n + 1):
        t = i / n
        f = fEnd + (fStart - fEnd) * math.exp(-t * pitchDecay)
        phase += f * (1.0 / n) * 2 * math.pi
        s = math.sin(phase)
        s = math.copysign(abs(s) ** sharp, s)
        y = BASE - amp * math.exp(-t * ampDecay) * s
        pts.append((x0 + (x1 - x0) * t, min(y, BASE)))   # negative Halbwellen auf Basis klemmen
    return pts

def c5():   # gefüllte Peaks (Flammen/Kick-Hüllkurve)
    pts = peaks(33, 90, 36, 4.6, 2.4, 2.4, 4.0, sharp=1.7)
    poly = pts + [(90, BASE), (33, BASE)]
    return f"""
  <path d="{P(poly)}" fill="url(#hot)"/>
  <path d="{P(anvil)}" fill="url(#steel)"/>
  <path d="{P(horn_hi)}" fill="url(#steelTop)"/>
  <path d="{P(face_bevel)}" fill="url(#steelTop)"/>"""

def c6():   # verfeinerte Linie, spitz, geklemmt
    pts = peaks(33, 90, 36, 4.6, 2.4, 2.4, 4.0, sharp=1.7)
    return f"""
  <path d="{taper_outline(pts, 4.2, 2.0)}" fill="url(#hot)"/>
  <path d="{P(anvil)}" fill="url(#steel)"/>
  <path d="{P(horn_hi)}" fill="url(#steelTop)"/>
  <path d="{P(face_bevel)}" fill="url(#steelTop)"/>"""

for name, fn in [("c5", c5), ("c6", c6)]:
    open(f"{OUT}/{name}.svg", "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">{DEFS}{fn()}\n</svg>\n')
print("ok")
