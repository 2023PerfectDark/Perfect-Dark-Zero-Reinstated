from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp')
s=p.read_text()
a=s.index('      if(handle==bot_coop_handle&&bot_coop_handle) {')
b=s.index('      } else if (choosing) {',a)
s=s[:a]+'      if (choosing) {'+s[b+len('      } else if (choosing) {'):]
a=s.index('\nREX_EXTERN(__imp__sub_8278CC40);',s.index('REX_HOOK_RAW(sub_8278CCC8)'))
s=s[:a]+'''
// The authored LAN button remains in the Co-operative scene, but uses an
// offline campaign session. Let its own navigation notification run normally.
namespace {
bool IsBotButton(PPCContext& ctx,uint8_t* base,uint32_t handle) {
  auto call=ctx;call.r1.u32-=1536;
  call.r3.u64=handle;sub_8276C0F8(call,base);
  return bot_coop_handle&&call.r3.u32==bot_coop_handle;
}
}
REX_EXTERN(__imp__sub_823F4F00);
REX_HOOK_RAW(sub_823F4F00) {
  if(Valid(ctx.r3.u32,12)&&IsBotButton(ctx,base,REX_LOAD_U32(ctx.r3.u32+4))) {
    auto call=ctx;call.r1.u32-=1536;
    call.r3.u64=REX_LOAD_U32(ctx.r3.u32+4);
    call.r4.u64=0x820B1418; // Enabled
    call.r5.u64=1;call.r6.u64=0;sub_823AEA88(call,base);
    ctx.r3.u64=0;return;
  }
  __imp__sub_823F4F00(ctx,base);
}
REX_EXTERN(__imp__sub_823F5018);
REX_HOOK_RAW(sub_823F5018) {
  if(Valid(ctx.r3.u32,12)&&IsBotButton(ctx,base,REX_LOAD_U32(ctx.r3.u32+4))) {
    auto call=ctx;call.r1.u32-=1536;
    pdz_coop::SelectBotCoop(true);REXCVAR_SET(pdz_coop_ally,true);
    call.r3.u64=78;call.r4.u64=0;sub_823B0388(call,base);
    call.r3.u64=79;call.r4.u64=0;sub_823B0388(call,base);
    REXLOG_INFO("PDZ Bot Co-op: local offline connection selected");
    ctx.r3.u64=0;return;
  }
  __imp__sub_823F5018(ctx,base);
}
'''
p.write_text(s)
