#!/bin/sh
# Erzeugt Source/FactoryPresets.h neu (Pfade = Analyse-Ausgaben der Entwicklungsumgebung)
# Aufruf: Tools/make_presets.sh <KickforgeRender> <fit5.json> <harsh-presets.json>
R=$1; A=$2; H=$3
"$R" --list > /tmp/kf_engine_defaults.txt
python3 "$(dirname "$0")/gen_presets.py" /tmp/kf_engine_defaults.txt "$(dirname "$0")/../Source/FactoryPresets.h" \
 "Industrial Heavy=$A:Heavy" "Industrial Punchy=$A:Punchy" "Industrial Noisy=$A:Noisy" "Industrial Raw=$A:Raw" "Industrial Heavy Raw=$A:HeavyRaw" \
 "Harsh Ripped=$H:Kickforge_Harsh_01_BaseHarsh_B1_160BPM.wav" "Harsh Ripped 17=$H:Kickforge_Harsh_02_BaseHarsh17_C#2_160BPM.wav" \
 "Harsh Ripped 20=$H:Kickforge_Harsh_03_BaseHarsh20_F1_160BPM.wav" "Harsh Ripped A1=$H:Kickforge_Harsh_04_Tieferm2_A1_160BPM.wav" \
 "Harsh Ripped C#2=$H:Kickforge_Harsh_05_Hoeherp2_C#2_160BPM.wav" "Harsh Ripped G1=$H:Kickforge_Harsh_06_Tiefm4_G1_160BPM.wav" \
 "Harsh Screech Long=$H:Kickforge_Harsh_07_ScreechLang_B1_160BPM.wav" "Harsh Screech Short=$H:Kickforge_Harsh_08_ScreechKurz_B1_160BPM.wav" \
 "Harsh Extra=$H:Kickforge_Harsh_09_ExtraHarsh_B1_160BPM.wav" "Harsh Dark=$H:Kickforge_Harsh_10_Dunkel_B1_160BPM.wav" \
 "Harsh Rumble=$H:Kickforge_Harsh_11_MehrRumble_B1_160BPM.wav" "Harsh Wide=$H:Kickforge_Harsh_12_Breit_B1_160BPM.wav" \
 "Harsh Variation 2=$H:Kickforge_Harsh_13_Seed2_B1_160BPM.wav" "Harsh Variation 3=$H:Kickforge_Harsh_14_Seed3_B1_160BPM.wav" \
 "Harsh Variation 4=$H:Kickforge_Harsh_15_Seed4Harsh_B1_160BPM.wav"
