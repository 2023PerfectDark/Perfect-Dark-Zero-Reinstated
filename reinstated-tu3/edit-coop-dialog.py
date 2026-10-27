from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/coop_ally.h');s=p.read_text().replace('void SelectBotCoop(bool enabled);','void SelectBotCoop(bool enabled);\nbool BotCoopSelected();');p.write_text(s)
p=Path('PerfectDarkZeroRecomp/src/coop_ally.cpp');s=p.read_text().replace('void SelectBotCoop(bool enabled) { bot_coop_selected=enabled; }','void SelectBotCoop(bool enabled) { bot_coop_selected=enabled; }\nbool BotCoopSelected() { return bot_coop_selected; }');p.write_text(s)
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp');s=p.read_text();s+='''
// This authored Co-operative scene behavior starts timer 1101 and opens the
// second-controller identification dialog. A bot has no controller to identify.
REX_EXTERN(__imp__sub_823C1F10);
REX_HOOK_RAW(sub_823C1F10) {
  if(pdz_coop::BotCoopSelected()&&Valid(ctx.r3.u32,24)) {
    REX_STORE_U8(ctx.r3.u32+20,0);
    REXLOG_INFO("PDZ Bot Co-op: bypassed second-controller identification");
    ctx.r3.u64=0;return;
  }
  __imp__sub_823C1F10(ctx,base);
}
''';p.write_text(s)
