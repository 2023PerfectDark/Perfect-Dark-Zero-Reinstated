from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp');s=p.read_text().replace('uint32_t solo_handle=0, bot_coop_handle=0;','uint32_t solo_handle=0, bot_coop_handle=0, local_coop_handle=0;').replace('  if(id==u"Solo")solo_handle=handle;','  if(id==u"Solo")solo_handle=handle;\n  if(id==u"buttonLocal")local_coop_handle=handle;')
s+='''
REX_HOOK_RAW(sub_8278CC40) {
  if(bot_coop_handle&&local_coop_handle&&IsBotButton(ctx,base,ctx.r3.u32)) {
    ctx.r3.u64=local_coop_handle;
    REXLOG_INFO("PDZ Bot Co-op: local scene notification");
  }
  __imp__sub_8278CC40(ctx,base);
}
'''
p.write_text(s)
