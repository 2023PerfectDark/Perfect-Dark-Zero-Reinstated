from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp');s=p.read_text();s=s.replace('''    call.r3.u64=REX_LOAD_U32(ctx.r3.u32+4);
    call.r4.u64=0x820B1418; // Enabled
    call.r5.u64=1;call.r6.u64=0;sub_823AEA88(call,base);''','''    const auto handle=REX_LOAD_U32(ctx.r3.u32+4);
    call.r3.u64=handle;call.r4.u64=0x820B1418;call.r5.u64=0;
    sub_823AE6C8(call,base);
    if(!call.r3.u32) {
      call.r3.u64=handle;call.r4.u64=0x820B1418; // Enabled
      call.r5.u64=1;call.r6.u64=0;sub_823AEA88(call,base);
    }''');p.write_text(s)
