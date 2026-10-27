"""Check the port's linkage and exclusions without running the game."""
from pathlib import Path
import json,re,hashlib
R=Path(__file__).resolve().parent.parent;P=R/'compatibility-v16/ModdedTU3';S=R/'compatibility-v16/StockTU3'
inventory=json.loads((P/'port-inventory.json').read_text())
relocation=json.loads((P/'relocation-review.json').read_text())
assert not relocation['unresolved_addresses']
text='\n'.join(x.read_text() for x in (P/'src').rglob('*') if x.suffix in {'.cpp','.h','.inl'})
hooks=re.findall(r'REX_HOOK\w*\(sub_([0-9A-F]{8})',text)
assert len(hooks)==len(set(hooks)), 'Duplicate replacement hooks'
native=(S/'generated/default/pdzstocktu3_register.cpp').read_text()
native_functions=set(re.findall(r'SetFunction\(0x([0-9A-F]{8})',native))
assert set(hooks)<=native_functions,sorted(set(hooks)-native_functions)
for address in inventory['removed_darkops_hooks']:
 assert address not in inventory['hooks']
assert 'darkOpsBots' not in text
assert 'displayedBotCount' not in text
assert 'Retail disc code destroys the bot manager' not in text
link=(P/'out/link.rsp').read_text()
assert 'retail_interactive_music.cpp.obj' in link
assert not re.search(r'PerfectDarkZeroRecomp[^\n]*generated[^\n]*\.obj',link)
assert 'async_shader_compilation = false' in (P/'settings/hardware.toml').read_text()
result={'hooks_present_in_TU3':len(hooks),'removed_custom_darkops_hooks':inventory['removed_darkops_hooks'],
 'custom_darkops_buttons_removed':True,'stock_TU3_generated_code_used':True,
 'music_restoration_linked':True,'async_shader_compilation':False,
 'host_policy_tests':json.loads((P/'policy-test-results.json').read_text()),
 'runtime_status':'First build launched and validated TU3; final character-heap correction awaiting restart',
 'runtime_pending':['Main/mod menu interaction','Mission loading and mod behavior','Native DarkOps bot match','Expanded character pool initialization'],
 'binary_sha256':hashlib.sha256((P/'out/pdzmoddedtu3-next.exe').read_bytes()).hexdigest()}
(P/'validation.json').write_text(json.dumps(result,indent=2))
print(f"Audit passed: {len(hooks)} TU3 hook targets, custom DarkOps controls removed, music restoration retained")
