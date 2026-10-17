"""Bounded actual decomp.wtf cooldown matching; root owns export/rebuild."""
from pathlib import Path
import json,os,sys
root=Path(__file__).resolve().parents[1]
os.environ['DECOMPNET_TOOLCHAINS']=str(root/'.toolchains')
from decompnet.app.engine import RomNetwork
from decompnet.app.tools import call
net=RomNetwork(root/'decomp/.decomp/decomp.db',root/'decomp')
slug=json.loads((root/'decomp/project.json').read_text())['slug']
agent,_=net.local_agent('codex-session')
name='sub_08012200'
notes=('Ten weapon cooldown halfwords at player+4A. Cache GAME pointer; each '
       'iteration reloads GAME+7 ticks, subtracts with16bit wrap, clamps signed '
       'negative result tozero. Native weapon_controller.c/fullROMtests cover '
       '116E4 firing,12160 switch and12200 timers:11652 comparisons. Parent122AC '
       'runs inF150 type2object update AFTER entity,mover,VM phases in0244C, '
       'BEFORE E460 animation. Candidate recovered/weapon_cooldowns.c. '
       'Bounded manual matching stopped after16 attempts at score30/98.7%; '
       'all24instructions align, only registerallocation differs: target '
       'GAME r1,zero r4,timerread r5; candidateGAME r4,zero r5,timerread r0 '
       'andtick r1. Explicittemporaries, signed/unsigned representations, '
       'volatile/registernames, literalglobal and pointer/index loops tried. '
       'Indexedstruct loop is closest. Not an exactmatch.')
if len(sys.argv)>1:
    tool=sys.argv[1]
    if tool=='guide':args={'topic':'workflow'}
    elif tool=='get_context':args={}
    else:args={'function':name,'notes':notes} if tool=='release_function' else {'function':name,'text':notes}
    print(call(net,agent,slug,tool,args),flush=True);raise SystemExit
path=root/'analysis/weapon-cooldowns-match-attempts.json'
records=json.loads(path.read_text()) if path.exists() else []
if len(records)>=20:raise SystemExit('Twenty attempt limit reached.')
source=(root/'recovered/weapon_cooldowns.c').read_text()
result=call(net,agent,slug,'compile_and_diff',{'function':name,'source':source})
records.append({'function':name,'source':source,'result':result})
path.write_text(json.dumps(records,indent=2)+'\n')
print(result,flush=True)
