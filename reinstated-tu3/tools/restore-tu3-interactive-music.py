"""Restore the retail interactive music state machine stripped by TU3.

Uses local generated retail code, not redistributed game assets. Static audio
constants are checked against locally decoded images before relocation.
"""
from pathlib import Path
import re, struct
root=Path(__file__).resolve().parent.parent
project=root/'compatibility-v16/StockTU3'
old={}
for p in (root/'compatibility-v16/PerfectDarkZeroRecomp/generated/default').glob('*recomp.*.cpp'):
    for m in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{(.*?)(?=\nDEFINE_REX_FUNC|\Z)',p.read_text(),re.S):old[int(m[1],16)]=m[0].strip()
manager={0x821F83A0:0x822006F0,0x821F8450:0x82200738,0x821F8570:0x82200838,
0x821F8600:0x822008B8,0x821F8660:0x82200910,0x821F86A0:0x82200918,
0x821F86C0:0x82200920,0x821F8A78:0x82200C20,0x821F8AC0:0x82200C38,
0x821F8B28:0x82200C68,0x821F8CB0:0x82200DC0,0x821F8D10:0x82200DC8}
selected=set(manager)|{a for a in old if 0x82200128<=a<=0x822010B0}
external={0x821FAC08:0x82202D20,0x821F70D0:0x821FF428,0x821FD450:0x822055C0,
0x821FD580:0x822056F0,0x821FDA60:0x82205BD0,0x821FE058:0x822061C8,
0x821FE1D8:0x82206348,0x821FE378:0x822064E8,0x821F8D70:0x82200DD0,
0x82A92530:0x82AA2290}
base_data=(root/'staging/audio-base-data.bin').read_bytes()
tu_data=(root/'staging/audio-tu3-data.bin').read_bytes()
# 0.0, 1.0, epsilon, double 1.0 and 0.0, Ambivol, death cue.
constant_map={0x820E0F84:(0x820DFCA4,4),0x820E1014:(0x820DFD34,4),
0x82075D7C:(0x82073C20,4),0x820EC010:(0x820EAD28,8),
0x820EBFD0:(0x820EACE8,8),0x82096124:(0x8209431C,8),
0x82095FF8:(0x820941F0,35)}
for a,(b,n) in constant_map.items():
    assert base_data[a-0x82000000:a-0x82000000+n]==tu_data[b-0x82000000:b-0x82000000+n],hex(a)
def convert(a):
    s=old[a]
    # Resolve static loads using their preceding lis, retaining dynamic fields.
    blocks=re.split(r'(?=\t// )',s)
    regs={}
    for i,block in enumerate(blocks):
        lis=re.match(r'\t// lis r(\d+),(-?\d+)\n',block)
        if lis:regs[int(lis[1])]=(int(lis[2])<<16)&0xffffffff;continue
        ins=re.match(r'\t// (?:lfs|lfd|addi) [^,]+,(-?\d+)\(r(\d+)\)',block)
        add=re.match(r'\t// addi r\d+,r(\d+),(-?\d+)',block)
        if ins:reg,off=int(ins[2]),int(ins[1])
        elif add:reg,off=int(add[1]),int(add[2])
        else:continue
        address=(regs.get(reg,0)+off)&0xffffffff
        if address in constant_map:
            dest=constant_map[address][0]
            block=re.sub(r'ctx\.r'+str(reg)+r'\.(?:u32|s64) \+ '+str(off)+r'\b',f'0x{dest:08X}u',block)
            blocks[i]=block
    s=''.join(blocks)
    # Global audio manager, global subsystem table, and stack cookie.
    s=s.replace('= -2098855936;', '= -2098790400;')
    s=s.replace('+ 29952','+ -32056').replace('+ -19792','+ -16424')
    s=s.replace('ctx.r11.u32 + 240','ctx.r11.u32 + 656')
    if a==0x821F83A0:s=s.replace('ctx.r11.u32 + 36','ctx.r11.u32 + 40')
    def call(m):
        addr=int(m[1],16)
        if addr in selected:return f'retail_music_{addr:08X}'
        assert addr in external, f'unmapped dependency {addr:08X}'
        return f'sub_{external[addr]:08X}'
    s=re.sub(r'sub_([0-9A-F]{8})',call,s)
    if a==0x821F83A0:
        s=s.replace('REX_FUNC_PROLOGUE();','''REX_FUNC_PROLOGUE();
    // TU3 also stripped the three channel indices from its constructor.
    { auto call=ctx; call.r1.u32-=256; call.r3.u64=0x82E682CC;
      retail_music_82200F20(call,base);
      call=ctx; call.r1.u32-=256; call.r3.u64=0x82E682CC;
      retail_music_82200920(call,base); }
''',1)
    if a==0x821F8450:
        s=s.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n    MusicTrace(ctx,base,"mission-init");',1)
    if a==0x821F86C0:
        s=s.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n    MusicTrace(ctx,base,"tick");',1)
    return s
bodies=[convert(a) for a in sorted(selected)]
decl='\n'.join(f'DECLARE_REX_FUNC(retail_music_{a:08X});' for a in sorted(selected))
trace='''
static void MusicTrace(PPCContext& ctx,uint8_t* base,const char* phase) {
    constexpr uint32_t p=0x82E682C8;
    static uint32_t previous=~0u;
    const uint32_t state=REX_LOAD_U32(p+148),target=REX_LOAD_U32(p+172);
    const uint32_t mode=REX_LOAD_U32(p+176), a=REX_LOAD_U32(p+52),b=REX_LOAD_U32(p+100);
    const uint32_t hash=state^(target<<8)^(mode<<16)^a^b;
    if(previous==hash && phase[0]=='t')return;
    previous=hash;
    REXLOG_INFO("[Music restore] {} mode={} state={} target={} stealth={:08X} action={:08X}",phase,mode,state,target,a,b);
}
'''
output='#include "pdzstocktu3_init.h"\n#include <rex/logging.h>\n// Restored retail music logic, relocated for TU3.\n'+decl+'\n'+trace+'\n'+'\n\n'.join(bodies)+'\n'
(project/'src/retail_interactive_music.cpp').write_text(output)
gen=project/'generated/default'
backup=root/'staging/tu3-audio-original-generated';backup.mkdir(exist_ok=True)
for p in gen.glob('*recomp.*.cpp'):
    s=p.read_text();before=s
    for a,b in manager.items():
        pat=rf'DEFINE_REX_FUNC\(sub_{b:08X}\) \{{.*?(?=\nDEFINE_REX_FUNC|\Z)'
        replacement=f'DECLARE_REX_FUNC(retail_music_{a:08X});\nDEFINE_REX_FUNC(sub_{b:08X}) {{ retail_music_{a:08X}(ctx, base); }}\n\n'
        if f'DEFINE_REX_FUNC(sub_{b:08X})' in s and f'DECLARE_REX_FUNC(retail_music_{a:08X});' not in s:
            s=re.sub(pat,lambda m:replacement,s,flags=re.S)
    if s!=before:
        if not (backup/p.name).exists():(backup/p.name).write_text(before)
        p.write_text(s)
print('Restored',len(selected),'music functions; rewired',len(manager),'TU3 entry points')

def edit_function(address, transform, declarations=''):
    for p in gen.glob('*recomp.*.cpp'):
        s=p.read_text()
        pat=rf'DEFINE_REX_FUNC\(sub_{address:08X}\) \{{.*?(?=\nDEFINE_REX_FUNC|\Z)'
        m=re.search(pat,s,re.S)
        if not m:continue
        body=transform(m[0])
        if body==m[0]:return
        if not (backup/p.name).exists():(backup/p.name).write_text(s)
        p.write_text(s[:m.start()]+declarations+body+s[m.end():]);return
    raise RuntimeError(hex(address))

def tick(s):
    marker='\tsub_82201038(ctx, base);'
    if 'RESTORED_MUSIC_TICK' in s:return s
    assert s.count(marker)==1
    return s.replace(marker,'\t// RESTORED_MUSIC_TICK: same audio lock and pause gate as retail.\n\t{ auto call=ctx; retail_music_821F86C0(call, base); }\n'+marker)
edit_function(0x821F8A38,tick,'DECLARE_REX_FUNC(retail_music_821F86C0);\n')

def target(s):
    if 'RESTORED_MUSIC_TARGET' in s:return s
    return s.replace('loc_82200BD0:', 'loc_82200BD0:\n\t// RESTORED_MUSIC_TARGET\n\t{ auto call=ctx; call.r1.u32-=256; call.r3.u64=ctx.r29.u32; retail_music_821F8660(call,base); }')
edit_function(0x82200928,target,'DECLARE_REX_FUNC(retail_music_821F8660);\n')

def start(s):
    if 'RESTORED_MUSIC_START' in s:return s
    marker='\t// cmpwi cr6,r11,1\n\tctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);\n\t// ble cr6,'
    assert s.count(marker)==1
    return s.replace(marker,'\t// RESTORED_MUSIC_START: TU3 inline path skipped interactive mode.\n\tif (ctx.r11.u32==1) REX_STORE_U32(0x82E682C8u + 152, 2);\n'+marker)
for a in (0x821F8E40,0x821F9720):edit_function(a,start)

def event(s):
    if 'RESTORED_MUSIC_EVENT' in s:return s
    marker='\t// cmplwi r6,0'
    insert='''\t// RESTORED_MUSIC_EVENT: restore authored cue markers 5 and 6.
    if (ctx.r6.u32==5 || ctx.r6.u32==6) {
        auto call=ctx; call.r1.u32-=256; call.r3.u64=ctx.r10.u32;
        if(ctx.r6.u32==5) retail_music_821F8CB0(call,base);
        else retail_music_821F8D10(call,base);
    }
'''
    assert s.count(marker)==1
    s=s.replace(marker,insert+marker)
    marker='loc_82207208:'
    return s.replace(marker,marker+'\n\tif(REX_LOAD_U32(0x82E682C8u+176)==1) REX_STORE_U32(0x82E682C8u+152,2);')
edit_function(0x82207118,event,'DECLARE_REX_FUNC(retail_music_821F8CB0);\nDECLARE_REX_FUNC(retail_music_821F8D10);\n')
