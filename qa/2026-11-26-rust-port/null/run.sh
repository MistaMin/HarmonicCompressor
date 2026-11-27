#!/bin/sh
# Nulls the Rust port (CLAP build of plugin/) against the shipped 1.5.3 VST3 at 1x, for every model
# and feature, and writes report.txt and one JSON report per case here.
#   OPENPLUGIN_PORT=<path to openplugin-port> qa/2026-11-26-rust-port/null/run.sh
# Automation sits on block boundaries (multiples of 512): JUCE applies a VST3 change at the start of
# its block, the SDK at its sample. Values are 0..1 host values, as both plugins show them.
# 2x to 8x do not null: the port's anti-alias filters are other designs (see ../README.md).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/../../.."
ORIG="$ROOT/build-release/HarmonicCompressor_artefacts/Release/VST3/Harmonic Compressor.vst3"
PORT_TOOL="${OPENPLUGIN_PORT:-openplugin-port}"
(cd "$ROOT/plugin" && cargo build --release --lib -q)
PORT="$ROOT/plugin/target/release/libharmonic_compressor.dylib"
: > "$HERE/report.txt"
fails=0
run() {
  name=$1 input=$2 auto=$3 tol=${4:-1e-5}
  if out=$("$PORT_TOOL" null "$ORIG" "$PORT" --format vst3 --port-format clap --input "$input" \
      --automation "$auto" --tolerance "$tol" --json "$HERE/$name.json" 2>&1); then v=PASS; else v=FAIL; fails=$((fails + 1)); fi
  max=$(echo "$out" | grep -o 'max |diff| [^ ]*' | sed 's/max |diff| //' | tr '\n' ' ')
  rms=$(echo "$out" | grep -o 'RMS diff [^,]*' | head -1)
  echo "$v $name [$input] tol $tol max $max $rms | $auto" | tee -a "$HERE/report.txt"
}
# Since 2026-11-27 the port moves Threshold, Ratio, Knee, Makeup, the gains, the harmonics and the
# transient amounts in whole steps (display rules) where 1.5.3 moved in tenths: every value below
# sits on both grids, or (off-grid) between steps that both versions snap to the same value.
b="0:Oversample=0, 0:Threshold=0.5"
i=0
for n in 0 0.16666666666666666 0.3333333333333333 0.5 0.6666666666666666 0.8333333333333334 1; do
  for sig in noise:3 sweep:3 burst:3; do
    run "mode$i-${sig%%:*}" "$sig" "$b, 0:Algorithm=$n, 57344:Threshold=0.3, 95744:Ratio=0.602283"
  done
  run "mode$i-auto" noise:3 "$b, 0:Algorithm=$n, 0:Auto Release=1, 0:Auto Attack=1, 0:Attack=0.7, 0:Release=0.2"
  i=$((i + 1))
done
run harmonics sweep:3 "$b, 0:2nd Harmonic=0.6, 0:3rd Harmonic=0.4, 57344:4th Harmonic=0.8, 95744:5th Harmonic=1"
# These two differ by the C++ build's fused multiply-adds (RMS about -120 dB): an output near
# full scale (+24 dB makeup), and the f32 sidechain high-pass heard on its own.
run harmonics-hot noise:3 "$b, 0:Makeup Gain=1, 0:2nd Harmonic=1, 0:3rd Harmonic=1, 0:4th Harmonic=1, 0:5th Harmonic=1" 3e-5
run mix-knee noise:3 "$b, 0:Mix=0.4, 0:Knee=1, 57344:Mix=0.9"
run gains noise:3 "$b, 0:Input Gain=0.7083333, 0:Output Gain=0.2916667, 57344:Input Gain=0.3958333, 95744:Output Gain=0.7916667"
run sc-hpf sweep:3 "$b, 0:Sidechain Enable=1, 0:Sidechain HP Freq=1"
run sc-listen noise:3 "$b, 0:Sidechain Enable=1, 0:Sidechain Listen=1, 57344:Sidechain Listen=0" 3e-5
run transient burst:3 "$b, 0:Transient Enable=1, 0:Transient Attack=0.9, 0:Transient Sustain=0.2, 0:Transient Speed=0.3"
run transient-noise noise:3 "$b, 0:Transient Enable=1, 0:Transient Attack=0.2, 0:Transient Sustain=0.9"
run autogain noise:4 "$b, 0:Auto Gain=1, 0:Output Gain=0.6041667"
run delta noise:3 "$b, 0:Delta Monitor=1, 57344:Delta Monitor=0"
run delta-autogain noise:4 "$b, 0:Auto Gain=1, 0:Delta Monitor=1, 95744:Auto Gain=0"
run off-grid noise:3 "0:Oversample=0, 0:Threshold=0.43383, 0:Ratio=0.328845, 0:Attack=0.123, 0:Release=0.77, 0:Knee=0.415417, 0:Mix=0.555, 0:2nd Harmonic=0.3302, 0:Sidechain HP Freq=0.42, 0:Sidechain Enable=1, 0:Input Gain=0.604792"
run defaults-1x impulses:3 "0:Oversample=0"
echo "$fails failed" | tee -a "$HERE/report.txt"
[ "$fails" -eq 0 ]
