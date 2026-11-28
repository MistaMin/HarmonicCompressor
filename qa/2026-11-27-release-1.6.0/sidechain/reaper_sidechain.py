#!/usr/bin/env python3
"""External sidechain in a real host: does the signal on the plugin's sidechain input drive the compressor?

    reaper_sidechain.py <qa dir of a running reaper_qa.py instance> "<FX name>"...

A 4-channel file plays on a 4-channel track: a steady 1 kHz tone on channels 1/2 (the main input) and
200 Hz bursts on channels 3/4 (REAPER feeds a plugin's second input bus from there). With Threshold at
its minimum the output level must follow the bursts:
  A  before the first burst    the main input is detected (the key has not carried signal yet)
  B  during the first burst    compressed by the key
  C  between the bursts        the key is silent: no compression
  D  during the second burst   compressed again
Pass: B and D are at least 6 dB under C, and no 200 Hz from the key reaches the output.
Exit 0 when every plugin passed. PLUGIN_DAW_TEST_SCRIPTS must name the folder that holds reaper_qa.py.
"""
import math
import os
import sys
import wave

sys.path.insert(0, os.environ["PLUGIN_DAW_TEST_SCRIPTS"])  # the folder that holds reaper_qa.py
import numpy as np  # noqa: E402
import reaper_qa  # noqa: E402

RATE, SECONDS = 48000, 4.0
BURSTS = ((1.0, 1.5), (2.5, 3.0))
WINDOWS = {"A": (0.5, 0.9), "B": (1.2, 1.45), "C": (2.0, 2.4), "D": (2.7, 2.95)}


def write_source(path):
    t = np.arange(int(RATE * SECONDS)) / RATE
    main = 0.1 * np.sin(2 * math.pi * 1000 * t)  # -20 dBFS
    key = 0.5 * np.sin(2 * math.pi * 200 * t) * sum(((t >= a) & (t < b)) for a, b in BURSTS)  # -6 dBFS
    frames = np.stack([main, main, key, key], axis=1)
    with wave.open(path, "wb") as w:
        w.setnchannels(4)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((frames * 32767).astype("<i2").tobytes())


def level_db(x):
    return 20 * math.log10(max(float(np.sqrt(np.mean(x * x))), 1e-12))


def tone_db(x, hz):
    t = np.arange(len(x)) / RATE
    return 20 * math.log10(max(2 * abs(np.mean(x * np.exp(-2j * math.pi * hz * t))), 1e-12))


def test(bridge, qa_dir, fx_name, source):
    slug = "".join(c if c.isalnum() else "-" for c in fx_name.split(":")[0]).lower()
    bridge.call("action", id=40296)  # select all tracks
    bridge.call("action", id=40005)  # remove them: an empty project for this plugin
    info = bridge.call("add_fx", name=fx_name)
    threshold = next(p for p in bridge.call("params")["params"] if p["name"] == "Threshold")
    text = bridge.call("set_param", index=threshold["index"], value=0)["text"]
    bridge.call("add_audio", path=source)
    project = os.path.join(qa_dir, "sidechain-%s.rpp" % slug)
    bridge.call("save_project", path=project)
    # REAPER widens the track to the file's four channels, so channels 3/4 reach the plugin's second input bus
    assert "NCHAN 4" in open(project).read(), "the track did not take the file's four channels"
    out = bridge.call("render", timeout=600, name="sidechain-%s" % slug, seconds=SECONDS)["file"]
    _, data = reaper_qa.read_wav(out)
    left = data[0]
    lv = {k: level_db(left[int(a * RATE):int(b * RATE)]) for k, (a, b) in WINDOWS.items()}
    b0, b1 = (int(v * RATE) for v in WINDOWS["B"])
    leak = tone_db(left[b0:b1], 200)
    ok = lv["C"] - lv["B"] >= 6 and lv["C"] - lv["D"] >= 6 and leak < -60
    print("%s  %s (Threshold %s): A %.1f, B %.1f, C %.1f, D %.1f dBFS; key at the output %.1f dBFS" % (
        "PASS" if ok else "FAIL", info.get("name", fx_name), text, lv["A"], lv["B"], lv["C"], lv["D"], leak))
    return ok


def main():
    qa_dir = os.path.abspath(sys.argv[1])
    source = os.path.join(qa_dir, "sidechain-source.wav")
    write_source(source)
    bridge = reaper_qa.Bridge(qa_dir)
    results = [test(bridge, qa_dir, name, source) for name in sys.argv[2:]]
    return 0 if results and all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
