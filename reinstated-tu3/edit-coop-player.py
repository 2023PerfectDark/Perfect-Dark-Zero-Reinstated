from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp');s=p.read_text().replace('    call.r3.u64=79;call.r4.u64=0;sub_823B0388(call,base);','''    call.r3.u64=79;call.r4.u64=0;sub_823B0388(call,base);
    // Action 78 updates the pending configuration, while this scene checks
    // the live session immediately for a second local controller.
    const auto session=REX_LOAD_U32(0x82D6B2B0);
    if(Valid(session,40))REX_STORE_U32(session+36,0);''');p.write_text(s)
