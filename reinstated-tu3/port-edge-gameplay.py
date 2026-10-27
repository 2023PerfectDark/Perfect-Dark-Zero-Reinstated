"""Generate the Edge gameplay adapter from the validated recomp gameplay sources.

Run from the pdz workspace. Original recomp sources remain untouched.
"""
from pathlib import Path
import re

root = Path('xenia-edge-mousehook/src/xenia/cpu/pdz')
root.mkdir(parents=True, exist_ok=True)
source = Path('compatibility-v13/PerfectDarkZeroRecomp/src')
units = ['weapon_rules', 'campaign_rules', 'weapon_challenge', 'campaign_gore', 'blood_decals', 'blood_effects', 'visible_holsters']
headers = ['weapon_rules', 'campaign_logic', 'campaign_damage', 'weapon_challenge', 'campaign_gore', 'blood_decals', 'holster_logic']
texts = []
for name in units:
    text = (source / (name + '.cpp')).read_text()
    if name == 'visible_holsters':
        # Port only inventory attachments, not Surface cinematics/body overrides.
        text = text[:text.index('void pdz_surface::RefreshIntroProp')] + text[text.index('REX_EXTERN(__imp__sub_8258A470);'):]
        start = text.index('void UpdateIntroPistol(')
        end = text.index('uint32_t Resource(', start)
        text = text[:start] + text[end:]
        text = re.sub(r'^.*intro_(?:pistol|hidden_weapon_models).*\n', '', text, flags=re.M)
        text = text.replace('thread_local uint32_t cosmetic_in_call = 0;', '')
        text = text.replace('cosmetic_in_call', '(pdz_edge::ThreadData().cosmetic_in_call)')
        text += '''
REX_HOOK_RAW(sub_8250D020) {
  pdz_edge::UpdateFov(base);
  __imp__sub_8250D020(ctx, base);
  Update(ctx, base); // Safe attachment changes after entity iteration.
}
'''
    text = re.sub(r'^#include [<"](?:rex/[^>"\n]+|generated/[^"\n]+|game_cvars.h|mousehook.h|surface_runtime.h)[>"]\n', '', text, flags=re.M)
    text = text.replace(' || pdz_surface::IntroCameraActive()', '')
    text = text.replace(' && !pdz_surface::IntroCameraActive()', '')
    text = text.replace('||pdz_surface::IntroCameraActive()||pdz_surface::EndingCameraActive()', '')
    if name == 'blood_effects':
        # This optional path should not reserve another full guest register
        # context on every native emitter call, even when extra blood is off.
        text = '#include <memory>\n' + text
        text = text.replace('auto burst = ctx;\n    __imp__sub_82220F78(burst, base);',
                            'auto burst = std::make_unique<PPCContext>(ctx);\n    __imp__sub_82220F78(*burst, base);')
    # Recomp guest threads use host TLS. Edge instead schedules guest fibers,
    # whose temporary callback state must follow the guest thread context.
    replacements = {}
    if name == 'weapon_rules':
        text = re.sub(r'^thread_local (?:uint32_t inspection_sampling|float inspection_sample_progress)=0;\n', '', text, flags=re.M)
        replacements = {'inspection_sampling':'inspection_sampling', 'inspection_sample_progress':'inspection_sample_progress'}
    elif name == 'campaign_rules':
        text = text.replace('namespace { thread_local uint32_t sniperVictim = 0; }', '')
        replacements = {'sniperVictim':'sniper_victim'}
    elif name == 'blood_decals':
        text = re.sub(r'^thread_local [^\n]+\n', '', text, flags=re.M)
        replacements = {'drawing':'decals_drawing','creating':'decals_creating','created':'decals_created','created_group':'decals_created_group'}
    for old, new in replacements.items():
        text = re.sub(r'\b'+old+r'\b', '(pdz_edge::ThreadData().'+new+')', text)
    texts.append(text)
    (root / (name + '.cc')).write_text('// Ported from the local PDZ recomp gameplay mods.\n#include "pdz_compat.h"\n#include "pdz_calls.h"\n' + text)
for name in headers:
    (root / (name + '.h')).write_text((source / (name + '.h')).read_text().replace('#include <rex/ppc/context.h>', '#include "pdz_context.h"'))
alltext = '\n'.join(texts)
hooks = sorted(set(re.findall(r'REX_HOOK_RAW\(sub_([0-9A-F]+)\)', alltext)) | {'824FB718'})
calls = sorted(set(re.findall(r'\b(?:__imp__)?sub_([0-9A-F]+)\b', alltext)) | {'824FB718'})
(root / 'pdz_calls.h').write_text('#pragma once\n' + ''.join(
    f'inline void {prefix}sub_{a}(PPCContext& ctx, uint8_t*) {{ pdz_edge::Call(ctx, 0x{a}, {str(bool(prefix)).lower()}); }}\n'
    for a in calls for prefix in ['', '__imp__']))
(root / 'pdz_hook_table.inc').write_text(''.join(f'PDZ_HOOK(0x{a}, hook_sub_{a})\n' for a in hooks))
cvars = sorted(set(re.findall(r'REXCVAR_GET\((\w+)\)', alltext)))
ints = {'pdz_campaign_rules','pdz_gore_decal_minutes'}
(root / 'pdz_cvars.h').write_text('#pragma once\n#include "xenia/base/cvar.h"\n' + ''.join(f'DECLARE_{"int32" if v in ints else "bool"}({v});\n' for v in cvars))
regs = '\n'.join(f'  GPR r{i};' for i in range(32)) + '\n' + '\n'.join(f'  FPR f{i};' for i in range(32))
imports = '\n'.join(f'    r{i}.u64 = native.r[{i}]; f{i}.f64 = native.f[{i}];' for i in range(32))
exports = '\n'.join(f'    out.r[{i}] = r{i}.u64; out.f[{i}] = f{i}.f64;' for i in range(32))
(root / 'pdz_context.h').write_text('''#pragma once
#include <cstring>
#include "xenia/cpu/ppc/ppc_context.h"
// Keep a full guest-register snapshot for nested native calls. Scheduler and
// thread metadata are deliberately never copied back into the live context.
struct PPCContext {
  union GPR { uint64_t u64; int64_t s64; uint32_t u32; int32_t s32; uint16_t u16; uint8_t u8; };
  union FPR { double f64; float f32; uint64_t u64; };
  xe::cpu::ppc::PPCContext native{};
  uint64_t lr;
''' + regs + '''
  explicit PPCContext(const xe::cpu::ppc::PPCContext& in) { Import(in); }
  void Import(const xe::cpu::ppc::PPCContext& in) {
    std::memcpy(&native, &in, offsetof(xe::cpu::ppc::PPCContext, thread_id));
    native.thread_state = in.thread_state; native.processor = in.processor;
    native.virtual_membase = in.virtual_membase;
    lr = in.lr;
''' + imports + '''
  }
  void Export(xe::cpu::ppc::PPCContext& out) const {
    std::memcpy(&out, &native, offsetof(xe::cpu::ppc::PPCContext, thread_id));
    out.lr = lr;
''' + exports + '''
  }
};
''')
print(f'Generated {len(units)} gameplay units, {len(hooks)} entry hooks and {len(calls)} guest call adapters.')
