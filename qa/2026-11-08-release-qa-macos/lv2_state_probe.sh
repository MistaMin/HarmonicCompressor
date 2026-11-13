#!/bin/bash
# Full-parameter save/reopen probe for one LV2 (or any) FX in an isolated REAPER.
# usage: lv2_state_probe.sh "<FX name>" <qa dir>
cd /Volumes/Developer/OpenGrid/HarmonicCompressor
FX="$1"; D="$2"
S="/Volumes/Developer/DSP Engineering Wiki/skills/plugin-daw-test/scripts/reaper_qa.py"
mkdir -p "$D"
c(){ python3 "$S" call --qa-dir "$D" "$@"; }
python3 "$S" launch --qa-dir "$D" >/dev/null 2>&1
c add_fx name="$FX" > "$D/add.json" 2>&1
N=$(python3 -c "import json,sys;t=open('$D/add.json').read();print(json.loads(t[t.find('{'):]).get('numParams',0))")
for i in $(seq 0 $((N-1))); do v=$(python3 -c "print(round((0.13+0.37*$i)%1.0,3))"); c set_param index=$i value=$v >/dev/null 2>&1; done
c render seconds=0.01 name=flush >/dev/null 2>&1
c params > "$D/before.json" 2>&1
c save_project path="$PWD/$D/rt.rpp" >/dev/null 2>&1
c open_project path="$PWD/$D/rt.rpp" >/dev/null 2>&1
sleep 2
c render seconds=0.01 name=after >/dev/null 2>&1
c params > "$D/after.json" 2>&1
python3 - "$D" "$N" <<'PY'
import json,sys
d,n=sys.argv[1],sys.argv[2]
def L(f):
    t=open(f"{d}/{f}").read(); return {p['index']:p for p in json.loads(t[t.find('{'):])['params']}
b=L('before.json'); a=L('after.json')
diff=[(i,b[i]['name'],b[i]['text'],a.get(i,{}).get('text')) for i in sorted(b) if b[i]['text']!=a.get(i,{}).get('text')]
print('numParams',n,'params',len(b),'changed after reopen+render:',len(diff))
for x in diff[:40]: print(x)
PY
python3 "$S" quit --qa-dir "$D" >/dev/null 2>&1
