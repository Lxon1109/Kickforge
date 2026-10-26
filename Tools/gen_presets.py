"""Erzeugt Source/FactoryPresets.h aus gefitteten Preset-JSONs.
Aufruf: python3 Tools/gen_presets.py <engine-defaults.txt> <out.h> name=<json>:<key> [...]
  engine-defaults.txt = Ausgabe von `KickforgeRender --list`
Jedes Preset wird vollständig geschrieben (Engine-Defaults + Fit-Werte), auf die
Bereiche aus ParamTable.h begrenzt."""
import sys, re, json, os
here = os.path.dirname(os.path.abspath(__file__))
table = open(os.path.join(here, "..", "Source", "ParamTable.h")).read()
ranges = {m[0]: (float(m[1]), float(m[2])) for m in
          re.findall(r'\{\s*"(\w+)",\s*"[^"]*",\s*([-\d.]+)f,\s*([-\d.]+)f,', table)}
defaults = dict(l.split("=") for l in open(sys.argv[1]).read().split())
defaults = {k: float(v) for k, v in defaults.items()}
out = sys.argv[2]
presets = []
for spec in sys.argv[3:]:
    name, src = spec.split("=", 1)
    path, key = src.rsplit(":", 1)
    d = json.load(open(path))[key]
    p = d.get("params", d)
    vals = {}
    for pid, (lo, hi) in ranges.items():
        v = float(p.get(pid, defaults[pid]))
        if not (lo <= v <= hi):
            print(f"  ! {name}: {pid}={v} außerhalb [{lo}, {hi}] → begrenzt", file=sys.stderr)
            v = min(max(v, lo), hi)
        vals[pid] = v
    presets.append((name, vals))
lines = ["#pragma once", "", "// AUTOMATISCH ERZEUGT von Tools/gen_presets.py – nicht von Hand bearbeiten.",
         "// Quelle: Fits der IndustrialEngine an Referenz-Kicks (Analyse-Pipeline).", "",
         "#include <utility>", "#include <vector>", "", "namespace kf", "{",
         "    struct FactoryPreset", "    {", "        const char* name;",
         "        std::vector<std::pair<const char*, float>> values;", "    };", "",
         "    inline const std::vector<FactoryPreset>& factoryPresets()", "    {",
         "        static const std::vector<FactoryPreset> presets {"]
for name, vals in presets:
    def lit(v):
        t = f"{v:.6g}"
        return (t if any(c in t for c in ".e") else t + ".0") + "f"
    body = ", ".join(f'{{ "{k}", {lit(v)} }}' for k, v in vals.items())
    lines.append(f'            {{ "{name}", {{ {body} }} }},')
lines += ["        };", "        return presets;", "    }", "}", ""]
open(out, "w").write("\n".join(lines))
print(f"{len(presets)} Presets → {out}")
