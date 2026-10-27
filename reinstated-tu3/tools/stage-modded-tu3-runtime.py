"""Prepare a private TU3 test copy; leave both existing installs/saves intact."""
from pathlib import Path
import shutil,json,re
R=Path(__file__).resolve().parent.parent;P=R/'compatibility-v16/ModdedTU3'
S=R/'compatibility-v16/StockTU3';M=R/'releases/PDZ-Recomp-v1.6-Modded-Update-2026-10-18/PDZ-Recomp'
assert (P/'out/pdzmoddedtu3.exe').exists()
marker=P/'runtime-staged.json'
if marker.exists():raise SystemExit('Runtime already staged; refusing to overwrite its test saves/settings')
shutil.copytree(S/'assets',P/'assets',dirs_exist_ok=True)
shutil.copytree(S/'update',P/'update',dirs_exist_ok=True)
shutil.copytree(M/'assets',P/'assets',dirs_exist_ok=True)
shutil.copytree(M/'mod_assets',P/'mod_assets',dirs_exist_ok=True)
shutil.copytree(S/'user',P/'user',dirs_exist_ok=True,ignore=shutil.ignore_patterns('cache'))
prior=Path('C:/Users/Owner/OneDrive/Documents/perfectdarkzerorecomp')
for name in ('mod-progress','local-profiles'):
 if (prior/name).is_dir():shutil.copytree(prior/name,P/'user'/name,dirs_exist_ok=True)
installed=Path('H:/Perfect Dark Zero Xbox360/PDZrecomp-win64 (3)/PDZ-Recomp')
shutil.copytree(installed/'settings',P/'settings',dirs_exist_ok=True)
config=P/'settings/hardware.toml';text=config.read_text()
text=re.sub(r'(?m)^async_shader_compilation\s*=.*$','async_shader_compilation = false',text)
config.write_text(text)
for name in ('gamecontrollerdb.txt','mp-characters.cfg'):
 if (installed/name).exists():shutil.copy2(installed/name,P/name)
(P/'Launch Modded TU3.cmd').write_text('''@echo off
cd /d "%~dp0"
start "PDZ Modded Title Update 3" out\\pdzmoddedtu3.exe --gpu_plugin=xenos --game_data_root="%~dp0assets" --update_data_root="%~dp0update" --user_data_root="%~dp0user" --cache_root="%~dp0user\\cache" --async_shader_compilation=false --log_level=info --log_file="%~dp0modded-tu3.log" --unregistered_function_log="%~dp0missing-functions.toml"
''')
marker.write_text(json.dumps({'stock_source':str(S),'mod_assets_source':str(M),'saves':'Private copy; original saves unchanged','renderer':'User setting retained; async shaders disabled'},indent=2))
print('Isolated runtime staged')
