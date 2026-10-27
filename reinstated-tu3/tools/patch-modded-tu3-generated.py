"""Apply bounded mod changes to copies of TU3-generated code, never TU0 code."""
from pathlib import Path
import json,re
R=Path(__file__).resolve().parent.parent
P=R/'compatibility-v16/ModdedTU3';source=R/'compatibility-v16/StockTU3/generated/default'
out=P/'generated-patched';out.mkdir(exist_ok=True)
counts={};caps=0
pattern=r'sub_824F5428\(ctx, base\);[^;]*;[^;]*;[^;]*REX_LOAD_U8\(ctx\.r3\.u32 \+ 0\);'
for path in source.glob('*recomp.*.cpp'):
 text=path.read_text();includes=set();edits=[]
 def reserve(m):return m[0].replace('REX_LOAD_U8(ctx.r3.u32 + 0)','pdz_weapons::ReserveLimit(base, ctx.r3.u32)')
 text,n=re.subn(pattern,reserve,text)
 if n:caps+=n;edits.append({'reserve_capacity_loads':n});includes.add('weapon_rules.h')
 def change(old,new,description):
  global text
  assert text.count(old)==1,(path.name,description,text.count(old))
  text=text.replace(old,new);edits.append(description)
 if 'DEFINE_REX_FUNC(sub_8221ED68)' in text:
  change('ctx.f31.f64 = double(temp.f32);\nloc_8221EE2C:', 'ctx.f31.f64 = pdz_cutscene::AheadFallback(double(temp.f32));\nloc_8221EE2C:', 'cutscene fallback')
  includes.add('cutscene_timing.h')
 if 'DEFINE_REX_FUNC(sub_821F41A8)' in text:
  change('// beq 0x821f4838\n\tif (ctx.cr0.eq) goto loc_821F4838;',
         '// Preserve TU3 package-only policy except installed mod characters.\n\tif (ctx.cr0.eq && !Tu3AllowsLooseCharacter(base,ctx.r24.u32)) goto loc_821F4838;',
         'TU3 installed character loose-resource discovery')
  # Declare after the generated includes, where fixed-width types are available.
  marker='DEFINE_REX_FUNC(sub_821F41A8)'
  text=text.replace(marker,'bool Tu3AllowsLooseCharacter(uint8_t*,uint32_t);\n'+marker,1)
 if 'DEFINE_REX_FUNC(sub_82262598)' in text:
  change('ctx.lr = 0x822626B8;\n\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);\n\t// mr r30,r3\n\tctx.r30.u64 = ctx.r3.u64;', 'ctx.lr = 0x822626B8;\n\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);\n\tctx.r30.u64 = pdz_weapons::SecondaryHudSlots(base,ctx.r3.u32);','secondary ammo gauge')
  includes.add('weapon_rules.h')
 if 'DEFINE_REX_FUNC(sub_82571190)' in text:
  change('ctx.lr = 0x825712B8;\n\tsub_825E6780(ctx, base);','if (!ctx.r3.u32) goto loc_82571354;\n\tctx.lr = 0x825712B8;\n\tsub_825E6780(ctx, base);','empty weapon callback guard')
 if 'DEFINE_REX_FUNC(sub_82240A90)' in text:
  change('// fadds f31,f0,f2\n\tctx.fpscr.disableFlushMode();\n\tctx.f31.f64 = double(float(ctx.f0.f64 + ctx.f2.f64));','// Local player aim cone, TU3 preserves player r31 here.\n\tctx.fpscr.disableFlushMode();\n\tctx.f31.f64 = pdz_weapons::Spread(base,ctx.r31.u32,ctx.r29.u32,ctx.f0.f64,ctx.f2.f64);','weapon spread')
  includes.add('weapon_rules.h')
 if edits:
  (out/path.name).write_text(''.join(f'#include "{name}"\n' for name in sorted(includes))+text)
  counts[path.name]=edits
assert caps==19,('TU3 reserve loads differ from audited TU0 sites',caps)
(P/'generated-patch-review.json').write_text(json.dumps(counts,indent=2))
print(f'Patched {len(counts)} TU3 copies with {caps} capacity loads and five bounded fixes')
