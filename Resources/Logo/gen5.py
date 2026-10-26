import math, sys
exec(open("gen4.py").read().split("def c5():")[0])

def emblem_body(fStart, fEnd, amp, ampDecay, pitchDecay, sharp):
    pts = peaks(33, 91, amp, fStart, fEnd, ampDecay, pitchDecay, sharp=sharp)
    poly = pts + [(91, BASE), (33, BASE)]
    return f"""
  <path d="{P(poly)}" fill="url(#hot)"/>
  <path d="{P(anvil)}" fill="url(#steel)"/>
  <path d="{P(horn_hi)}" fill="url(#steelTop)"/>
  <path d="{P(face_bevel)}" fill="url(#steelTop)"/>"""

variants = {
  "e1": (3.6, 2.6, 38, 2.0, 3.0, 1.45),
  "e2": (4.0, 3.0, 38, 1.9, 2.5, 1.35),
}
for name, args in variants.items():
    open(f"{OUT}/{name}.svg", "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">{DEFS}{emblem_body(*args)}\n</svg>\n')

# Lockup-Vorschau (Emblem + Wordmark) — nur Vorschau, im Plugin zeichnet JUCE die Schrift
for name in variants:
    body = open(f"{OUT}/{name}.svg").read().split("</defs>")[1].replace("</svg>", "")
    lock = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 420 100" width="420" height="100">{DEFS}
  <g transform="translate(0,4) scale(0.92)">{body}</g>
  <text x="100" y="58" font-family="Chakra Petch" font-weight="700" font-size="46" letter-spacing="3" fill="#ECE7E2">KICK<tspan fill="#FF6A1A">FORGE</tspan></text>
  <text x="102" y="80" font-family="Chakra Petch" font-weight="600" font-size="11.5" letter-spacing="4.2" fill="#8A807A">HARDTECHNO · INDUSTRIAL · SCHRANZ</text>
</svg>'''
    open(f"{OUT}/{name}_lockup.svg", "w").write(lock)
print("ok")
