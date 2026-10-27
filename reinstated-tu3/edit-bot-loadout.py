from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp');s=p.read_text();s+='''
// Bot loadout temporarily mirrors the primary profile. Never return a missing
// Player 2 profile to the native loadout editor (it dereferences +9444).
REX_EXTERN(__imp__sub_824FB718);
REX_HOOK_RAW(sub_824FB718) {
  if(pdz_coop::BotCoopSelected()) {
    constexpr uint32_t users=0x82D2CA58;
    const auto primary=REX_LOAD_U8(users+20);
    const auto secondary=REX_LOAD_U8(users+21);
    if(primary<4&&(ctx.r3.u32==secondary||ctx.r3.u32==255))ctx.r3.u64=primary;
  }
  __imp__sub_824FB718(ctx,base);
}
REX_EXTERN(__imp__sub_8239A530);
REX_HOOK_RAW(sub_8239A530) {
  __imp__sub_8239A530(ctx,base);
  if(pdz_coop::BotCoopSelected()&&ctx.r3.u32==255) {
    const auto primary=REX_LOAD_U8(0x82D2CA58+20);
    if(primary<4)ctx.r3.u64=primary;
  }
}
''';p.write_text(s)
p=Path('PerfectDarkZeroRecomp/src/coop_ally.cpp');s=p.read_text().replace('  combat_script=REX_LOAD_U8(source+172);','''  // The companion uses the primary player's selected weapon slots, including
  // category substitutions such as Laptop Gun instead of an assault rifle.
  auto profile_call=ctx;
  profile_call.r3.u64=REX_LOAD_U8(0x82D2CA58+20);
  sub_824FB718(profile_call,base);
  std::array<uint32_t,4> loadout{};
  if(Readable(profile_call.r3.u32,9460)) {
    for(unsigned i=0;i<loadout.size();++i) {
      const auto id=REX_LOAD_U32(profile_call.r3.u32+9444+i*4);
      if(id>0&&id<=31)loadout[i]=id;
    }
    for(unsigned i=0;i<2;++i) {
      REX_STORE_U32(owned_setup+184+i*12,loadout[i]);
      REX_STORE_U32(owned_setup+188+i*12,0);
      REX_STORE_U32(owned_setup+192+i*12,0);
    }
  }
  combat_script=REX_LOAD_U8(source+172);''').replace('  policy.Reset(Handle(player),Handle(ally));','''  for(unsigned i=2;i<loadout.size();++i)if(loadout[i]) {
    auto give=ctx;give.r3.u64=ally;give.r4.u64=loadout[i];
    give.r5.u64=0;give.r6.u64=0;give.r7.u64=0;give.r8.u64=0;
    sub_8253CCB8(give,base);
  }
  REXLOG_INFO("Coop ally: mirrored primary loadout [{}, {}, {}, {}]",
              loadout[0],loadout[1],loadout[2],loadout[3]);
  policy.Reset(Handle(player),Handle(ally));''');p.write_text(s)
