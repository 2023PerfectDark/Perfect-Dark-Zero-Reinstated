from pathlib import Path
import subprocess,shutil,json,concurrent.futures,os
R=Path(__file__).resolve().parent.parent;P=R/'compatibility-v16/ModdedTU3'
tests=['reload_timing','physics_timing','input_logic','breathing_motion','npc_placement','nightclub_suspicion','mp_progression_policy','mission_summary','debug_position','cheat_progress','campaign_logic','perfect_dark_zero_mode','coop_ally_logic','coop_sync_logic','surface_mission']
folder=P/'tests';folder.mkdir(exist_ok=True);out=P/'out/tests';out.mkdir(exist_ok=True)
def run(name):
 src=folder/(name+'_test.cpp');shutil.copy2(R/'compatibility-v16/PerfectDarkZeroRecomp/tests'/src.name,src)
 exe=out/(name+'.exe');obj=out/(name+'.obj');compiler=str(R/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe');cmd=[compiler,'-std=c++23','-O1','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-c',str(src),'-o',str(obj)]
 result=subprocess.run(cmd,capture_output=True,text=True)
 if result.returncode:raise RuntimeError(name+result.stdout+result.stderr)
 result=subprocess.run([compiler,str(obj),'-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt','-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(exe)],capture_output=True,text=True)
 if result.returncode:raise RuntimeError(name+result.stdout+result.stderr)
 temp=out/(name+'-temp');temp.mkdir(exist_ok=True)
 env=dict(os.environ,TEMP=str(temp),TMP=str(temp))
 runs=[[str(exe)]]
 if name=='debug_position':runs=[[str(exe),phase,str(temp)] for phase in ('save','load')]
 for command in runs:
  result=subprocess.run(command,cwd=out,capture_output=True,text=True,env=env)
  assert result.returncode==0,(name,result.stdout,result.stderr)
 print(name+': passed',flush=True);return name
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:passed=list(pool.map(run,tests))
(P/'policy-test-results.json').write_text(json.dumps({'passed':passed,'scope':'Host-side behavior tests; does not prove TU3 runtime field compatibility'},indent=2))
