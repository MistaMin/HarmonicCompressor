#!/bin/bash
cd /Volumes/Developer/OpenGrid/HarmonicCompressor
D=qa/2026-11-08-release-qa-macos
S="/Volumes/Developer/DSP Engineering Wiki/skills/plugin-daw-test/scripts/reaper_qa.py"
python3 "$S" suite --qa-dir "$D" --fx "CLAP: Harmonic Compressor" --exceptions qa/exceptions.txt > "$D/suite-clap.log" 2>&1; echo "exit $?" >> "$D/suite-clap.log"
mv "$D/report.md" "$D/report-clap.md"; rm -f "$D/report.json"
python3 "$S" suite --qa-dir "$D" --fx "LV2: Harmonic Compressor" --exceptions qa/exceptions.txt > "$D/suite-lv2.log" 2>&1; echo "exit $?" >> "$D/suite-lv2.log"
mv "$D/report.md" "$D/report-lv2.md"; rm -f "$D/report.json"
echo done
