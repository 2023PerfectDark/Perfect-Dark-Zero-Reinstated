#include "pdzstocktu3_init.h"
#include <rex/logging.h>
// Restored retail music logic, relocated for TU3.
DECLARE_REX_FUNC(retail_music_821F83A0);
DECLARE_REX_FUNC(retail_music_821F8450);
DECLARE_REX_FUNC(retail_music_821F8570);
DECLARE_REX_FUNC(retail_music_821F8600);
DECLARE_REX_FUNC(retail_music_821F8660);
DECLARE_REX_FUNC(retail_music_821F86A0);
DECLARE_REX_FUNC(retail_music_821F86C0);
DECLARE_REX_FUNC(retail_music_821F8A78);
DECLARE_REX_FUNC(retail_music_821F8AC0);
DECLARE_REX_FUNC(retail_music_821F8B28);
DECLARE_REX_FUNC(retail_music_821F8CB0);
DECLARE_REX_FUNC(retail_music_821F8D10);
DECLARE_REX_FUNC(retail_music_82200128);
DECLARE_REX_FUNC(retail_music_82200198);
DECLARE_REX_FUNC(retail_music_822003F8);
DECLARE_REX_FUNC(retail_music_82200478);
DECLARE_REX_FUNC(retail_music_82200488);
DECLARE_REX_FUNC(retail_music_82200498);
DECLARE_REX_FUNC(retail_music_822004B8);
DECLARE_REX_FUNC(retail_music_82200550);
DECLARE_REX_FUNC(retail_music_822005C0);
DECLARE_REX_FUNC(retail_music_82200610);
DECLARE_REX_FUNC(retail_music_82200638);
DECLARE_REX_FUNC(retail_music_822006A0);
DECLARE_REX_FUNC(retail_music_82200718);
DECLARE_REX_FUNC(retail_music_82200740);
DECLARE_REX_FUNC(retail_music_822007E8);
DECLARE_REX_FUNC(retail_music_822007F0);
DECLARE_REX_FUNC(retail_music_82200890);
DECLARE_REX_FUNC(retail_music_822008D8);
DECLARE_REX_FUNC(retail_music_8220091C);
DECLARE_REX_FUNC(retail_music_82200920);
DECLARE_REX_FUNC(retail_music_822009B8);
DECLARE_REX_FUNC(retail_music_82200A08);
DECLARE_REX_FUNC(retail_music_82200A98);
DECLARE_REX_FUNC(retail_music_82200C10);
DECLARE_REX_FUNC(retail_music_82200DB8);
DECLARE_REX_FUNC(retail_music_82200E00);
DECLARE_REX_FUNC(retail_music_82200E38);
DECLARE_REX_FUNC(retail_music_82200E58);
DECLARE_REX_FUNC(retail_music_82200E68);
DECLARE_REX_FUNC(retail_music_82200E78);
DECLARE_REX_FUNC(retail_music_82200E88);
DECLARE_REX_FUNC(retail_music_82200E90);
DECLARE_REX_FUNC(retail_music_82200ED8);
DECLARE_REX_FUNC(retail_music_82200F20);
DECLARE_REX_FUNC(retail_music_82200F68);
DECLARE_REX_FUNC(retail_music_82200FA8);
DECLARE_REX_FUNC(retail_music_82200FF0);
DECLARE_REX_FUNC(retail_music_82201028);
DECLARE_REX_FUNC(retail_music_822010B0);

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

DEFINE_REX_FUNC(retail_music_821F83A0) {
	REX_FUNC_PROLOGUE();
    // TU3 also stripped the three channel indices from its constructor.
    { auto call=ctx; call.r1.u32-=256; call.r3.u64=0x82E682CC;
      retail_music_82200F20(call,base);
      call=ctx; call.r1.u32-=256; call.r3.u64=0x82E682CC;
      retail_music_82200920(call,base); }

	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82a9879c
	ctx.lr = 0x821F83A8;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// lis r29,-32041
	ctx.r29.s64 = -2099838976;
	// addi r31,r11,29952
	ctx.r31.s64 = ctx.r11.s64 + -32056;
	// li r11,0
	ctx.r11.s64 = 0;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// stw r11,0(r31)
	REX_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);
	// lwz r11,-19792(r29)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r29.u32 + -16424);
	// lwz r11,36(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 40);
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// beq cr6,0x821f8418
	if (ctx.cr6.eq) goto loc_821F8418;
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// addi r3,r31,4
	ctx.r3.s64 = ctx.r31.s64 + 4;
	// bl 0x82200a08
	ctx.lr = 0x821F83E0;
	retail_music_82200A08(ctx, base);
	// lwz r10,148(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 148);
	// lwz r11,8(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 8);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq cr6,0x821f8414
	if (ctx.cr6.eq) goto loc_821F8414;
	// cmpwi cr6,r10,2
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 2, ctx.xer);
	// bne cr6,0x821f8418
	if (!ctx.cr6.eq) goto loc_821F8418;
	// stw r11,172(r31)
	REX_STORE_U32(ctx.r31.u32 + 172, ctx.r11.u32);
	// lwz r10,168(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 168);
	// cmpw cr6,r10,r11
	ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.r11.s32, ctx.xer);
	// beq cr6,0x821f8418
	if (ctx.cr6.eq) goto loc_821F8418;
	// li r11,3
	ctx.r11.s64 = 3;
	// stw r11,152(r31)
	REX_STORE_U32(ctx.r31.u32 + 152, ctx.r11.u32);
	// b 0x821f8418
	goto loc_821F8418;
loc_821F8414:
	// stw r11,172(r31)
	REX_STORE_U32(ctx.r31.u32 + 172, ctx.r11.u32);
loc_821F8418:
	// lwz r11,12(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 12);
	// stw r11,176(r31)
	REX_STORE_U32(ctx.r31.u32 + 176, ctx.r11.u32);
	// lwz r11,12(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 12);
	// stw r11,180(r31)
	REX_STORE_U32(ctx.r31.u32 + 180, ctx.r11.u32);
	// lwz r11,-19792(r29)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r29.u32 + -16424);
	// lwz r11,36(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 40);
	// addi r11,r11,-2
	ctx.r11.s64 = ctx.r11.s64 + -2;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm r11,r11,27,31,31
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r11.u64 = ctx.r11.u64 ^ 1;
	// stb r11,184(r31)
	REX_STORE_U8(ctx.r31.u32 + 184, ctx.r11.u8);
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82a987ec
	__restgprlr_29(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_821F8450) {
	REX_FUNC_PROLOGUE();
    MusicTrace(ctx,base,"mission-init");
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r30,-24(r1)
	REX_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-1184(r1)
	ea = -1184 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32041
	ctx.r11.s64 = -2099838976;
	// addi r11,r11,-19792
	ctx.r11.s64 = ctx.r11.s64 + -16424;
	// lwz r11,32(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 32);
	// lwz r30,36(r11)
	ctx.r30.u64 = REX_LOAD_U32(ctx.r11.u32 + 36);
	// cmplwi r30,0
	ctx.cr0.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
	// beq 0x821f8558
	if (ctx.cr0.eq) goto loc_821F8558;
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x821f83a0
	ctx.lr = 0x821F8484;
	retail_music_821F83A0(ctx, base);
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r31,r11,29952
	ctx.r31.s64 = ctx.r11.s64 + -32056;
	// lwz r11,176(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 176);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// beq cr6,0x821f851c
	if (ctx.cr6.eq) goto loc_821F851C;
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// beq cr6,0x821f8508
	if (ctx.cr6.eq) goto loc_821F8508;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x821f84f4
	if (ctx.cr6.eq) goto loc_821F84F4;
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// beq cr6,0x821f84e0
	if (ctx.cr6.eq) goto loc_821F84E0;
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// bne cr6,0x821f8528
	if (!ctx.cr6.eq) goto loc_821F8528;
	// addi r3,r1,624
	ctx.r3.s64 = ctx.r1.s64 + 624;
	// lwz r4,84(r30)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r30.u32 + 84);
	// bl 0x821fac08
	ctx.lr = 0x821F84C4;
	sub_82202D20(ctx, base);
	// addi r3,r1,624
	ctx.r3.s64 = ctx.r1.s64 + 624;
loc_821F84C8:
	// li r4,0
	ctx.r4.s64 = 0;
	// li r5,0
	ctx.r5.s64 = 0;
	// li r6,0
	ctx.r6.s64 = 0;
	// bl 0x821f70d0
	ctx.lr = 0x821F84D8;
	sub_821FF428(ctx, base);
	// stw r3,0(r31)
	REX_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);
	// b 0x821f8524
	goto loc_821F8524;
loc_821F84E0:
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// lwz r4,80(r30)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r30.u32 + 80);
	// bl 0x821fac08
	ctx.lr = 0x821F84EC;
	sub_82202D20(ctx, base);
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// b 0x821f84c8
	goto loc_821F84C8;
loc_821F84F4:
	// addi r3,r1,352
	ctx.r3.s64 = ctx.r1.s64 + 352;
	// lwz r4,76(r30)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r30.u32 + 76);
	// bl 0x821fac08
	ctx.lr = 0x821F8500;
	sub_82202D20(ctx, base);
	// addi r3,r1,352
	ctx.r3.s64 = ctx.r1.s64 + 352;
	// b 0x821f84c8
	goto loc_821F84C8;
loc_821F8508:
	// addi r3,r1,896
	ctx.r3.s64 = ctx.r1.s64 + 896;
	// lwz r4,72(r30)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r30.u32 + 72);
	// bl 0x821fac08
	ctx.lr = 0x821F8514;
	sub_82202D20(ctx, base);
	// addi r3,r1,896
	ctx.r3.s64 = ctx.r1.s64 + 896;
	// b 0x821f84c8
	goto loc_821F84C8;
loc_821F851C:
	// addi r3,r31,4
	ctx.r3.s64 = ctx.r31.s64 + 4;
	// bl 0x82200db8
	ctx.lr = 0x821F8524;
	retail_music_82200DB8(ctx, base);
loc_821F8524:
	// lwz r11,176(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 176);
loc_821F8528:
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// beq cr6,0x821f8558
	if (ctx.cr6.eq) goto loc_821F8558;
	// lwz r11,24(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 24);
	// lbz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r11.u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne cr6,0x821f8550
	if (!ctx.cr6.eq) goto loc_821F8550;
	// lwz r11,48(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 48);
	// lbz r11,0(r11)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r11.u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x821f8558
	if (ctx.cr6.eq) goto loc_821F8558;
loc_821F8550:
	// addi r3,r31,4
	ctx.r3.s64 = ctx.r31.s64 + 4;
	// bl 0x82200db8
	ctx.lr = 0x821F8558;
	retail_music_82200DB8(ctx, base);
loc_821F8558:
	// addi r1,r1,1184
	ctx.r1.s64 = ctx.r1.s64 + 1184;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r30,-24(r1)
	ctx.r30.u64 = REX_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8570) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r11,r11,29952
	ctx.r11.s64 = ctx.r11.s64 + -32056;
	// lwz r10,176(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 176);
	// cmpwi cr6,r10,1
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 1, ctx.xer);
	// beq cr6,0x821f85a8
	if (ctx.cr6.eq) goto loc_821F85A8;
	// blelr cr6
	if (!ctx.cr6.gt) return;
	// cmpwi cr6,r10,5
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 5, ctx.xer);
	// bgtlr cr6
	if (ctx.cr6.gt) return;
	// lwz r3,0(r11)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f1.f64 = double(temp.f32);
	// b 0x821fe058
	sub_822061C8(ctx, base);
	return;
loc_821F85A8:
	// li r10,2
	ctx.r10.s64 = 2;
	// stw r10,152(r11)
	REX_STORE_U32(ctx.r11.u32 + 152, ctx.r10.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8600) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r30,-24(r1)
	REX_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r30,r11,29952
	ctx.r30.s64 = ctx.r11.s64 + -32056;
	// lwz r31,0(r30)
	ctx.r31.u64 = REX_LOAD_U32(ctx.r30.u32 + 0);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// beq cr6,0x821f8640
	if (ctx.cr6.eq) goto loc_821F8640;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x821fd450
	ctx.lr = 0x821F8630;
	sub_822055C0(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x821fda60
	ctx.lr = 0x821F8638;
	sub_82205BD0(ctx, base);
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,0(r30)
	REX_STORE_U32(ctx.r30.u32 + 0, ctx.r11.u32);
loc_821F8640:
	// addi r3,r30,4
	ctx.r3.s64 = ctx.r30.s64 + 4;
	// bl 0x82200f20
	ctx.lr = 0x821F8648;
	retail_music_82200F20(ctx, base);
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r30,-24(r1)
	ctx.r30.u64 = REX_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8660) {
	REX_FUNC_PROLOGUE();
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r11,r11,29952
	ctx.r11.s64 = ctx.r11.s64 + -32056;
	// lwz r10,148(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 148);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq cr6,0x821f8698
	if (ctx.cr6.eq) goto loc_821F8698;
	// cmpwi cr6,r10,2
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 2, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// stw r3,172(r11)
	REX_STORE_U32(ctx.r11.u32 + 172, ctx.r3.u32);
	// lwz r10,168(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 168);
	// cmpw cr6,r10,r3
	ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.r3.s32, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// li r10,3
	ctx.r10.s64 = 3;
	// stw r10,152(r11)
	REX_STORE_U32(ctx.r11.u32 + 152, ctx.r10.u32);
	// blr 
	return;
loc_821F8698:
	// stw r3,172(r11)
	REX_STORE_U32(ctx.r11.u32 + 172, ctx.r3.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F86A0) {
	REX_FUNC_PROLOGUE();
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// li r3,3
	ctx.r3.s64 = 3;
	// addi r11,r11,29952
	ctx.r11.s64 = ctx.r11.s64 + -32056;
	// lwz r10,148(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 148);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// lwz r3,168(r11)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 168);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F86C0) {
	REX_FUNC_PROLOGUE();
    MusicTrace(ctx,base,"tick");
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// bl 0x821f8d70
	ctx.lr = 0x821F86D0;
	sub_82200DD0(ctx, base);
	// clrlwi. r11,r3,24
	ctx.r11.u64 = ctx.r3.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x821f86f4
	if (ctx.cr0.eq) goto loc_821F86F4;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r11,r11,29952
	ctx.r11.s64 = ctx.r11.s64 + -32056;
	// lwz r10,176(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 176);
	// cmpwi cr6,r10,1
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 1, ctx.xer);
	// bne cr6,0x821f86f4
	if (!ctx.cr6.eq) goto loc_821F86F4;
	// addi r3,r11,4
	ctx.r3.s64 = ctx.r11.s64 + 4;
	// bl 0x82200a98
	ctx.lr = 0x821F86F4;
	retail_music_82200A98(ctx, base);
loc_821F86F4:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8A78) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r31,r11,29952
	ctx.r31.s64 = ctx.r11.s64 + -32056;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x821f8aa0
	if (ctx.cr6.eq) goto loc_821F8AA0;
	// bl 0x821fe1d8
	ctx.lr = 0x821F8AA0;
	sub_82206348(ctx, base);
loc_821F8AA0:
	// li r11,1
	ctx.r11.s64 = 1;
	// stw r11,152(r31)
	REX_STORE_U32(ctx.r31.u32 + 152, ctx.r11.u32);
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8AC0) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r31,r11,29952
	ctx.r31.s64 = ctx.r11.s64 + -32056;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x821f8afc
	if (ctx.cr6.eq) goto loc_821F8AFC;
	// lwz r11,176(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 176);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// beq cr6,0x821f8b08
	if (ctx.cr6.eq) goto loc_821F8B08;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f1.f64 = double(temp.f32);
	// bl 0x821fe058
	ctx.lr = 0x821F8AFC;
	sub_822061C8(ctx, base);
loc_821F8AFC:
	// lwz r11,176(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 176);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// bne cr6,0x821f8b10
	if (!ctx.cr6.eq) goto loc_821F8B10;
loc_821F8B08:
	// li r11,2
	ctx.r11.s64 = 2;
	// stw r11,152(r31)
	REX_STORE_U32(ctx.r31.u32 + 152, ctx.r11.u32);
loc_821F8B10:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8B28) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82a9879c
	ctx.lr = 0x821F8B30;
	__savegprlr_29(ctx, base);
	// stwu r1,-1472(r1)
	ea = -1472 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32041
	ctx.r11.s64 = -2099838976;
	// addi r11,r11,-19792
	ctx.r11.s64 = ctx.r11.s64 + -16424;
	// lwz r11,32(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 32);
	// lwz r29,36(r11)
	ctx.r29.u64 = REX_LOAD_U32(ctx.r11.u32 + 36);
	// cmplwi r29,0
	ctx.cr0.compare<uint32_t>(ctx.r29.u32, 0, ctx.xer);
	// beq 0x821f8c90
	if (ctx.cr0.eq) goto loc_821F8C90;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r31,r11,29952
	ctx.r31.s64 = ctx.r11.s64 + -32056;
	// lwz r11,176(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 176);
	// lwz r10,180(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 180);
	// cmpw cr6,r11,r10
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r10.s32, ctx.xer);
	// beq cr6,0x821f8c90
	if (ctx.cr6.eq) goto loc_821F8C90;
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// beq cr6,0x821f8b98
	if (ctx.cr6.eq) goto loc_821F8B98;
	// ble cr6,0x821f8ba0
	if (!ctx.cr6.gt) goto loc_821F8BA0;
	// cmpwi cr6,r11,6
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 6, ctx.xer);
	// bgt cr6,0x821f8ba0
	if (ctx.cr6.gt) goto loc_821F8BA0;
	// lwz r30,0(r31)
	ctx.r30.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
	// beq cr6,0x821f8ba0
	if (ctx.cr6.eq) goto loc_821F8BA0;
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x821fd450
	ctx.lr = 0x821F8B8C;
	sub_822055C0(ctx, base);
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x821fda60
	ctx.lr = 0x821F8B94;
	sub_82205BD0(ctx, base);
	// b 0x821f8ba0
	goto loc_821F8BA0;
loc_821F8B98:
	// addi r3,r31,4
	ctx.r3.s64 = ctx.r31.s64 + 4;
	// bl 0x82200f20
	ctx.lr = 0x821F8BA0;
	retail_music_82200F20(ctx, base);
loc_821F8BA0:
	// li r3,0
	ctx.r3.s64 = 0;
	// stw r3,0(r31)
	REX_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);
	// lwz r11,180(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 180);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// beq cr6,0x821f8c6c
	if (ctx.cr6.eq) goto loc_821F8C6C;
	// rotlwi r11,r11,0
	ctx.r11.u64 = __builtin_rotateleft32(ctx.r11.u32, 0);
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// beq cr6,0x821f8c44
	if (ctx.cr6.eq) goto loc_821F8C44;
	// lwz r11,180(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 180);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x821f8c30
	if (ctx.cr6.eq) goto loc_821F8C30;
	// rotlwi r11,r11,0
	ctx.r11.u64 = __builtin_rotateleft32(ctx.r11.u32, 0);
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// beq cr6,0x821f8c1c
	if (ctx.cr6.eq) goto loc_821F8C1C;
	// lwz r11,180(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 180);
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// beq cr6,0x821f8c08
	if (ctx.cr6.eq) goto loc_821F8C08;
	// rotlwi r11,r11,0
	ctx.r11.u64 = __builtin_rotateleft32(ctx.r11.u32, 0);
	// cmpwi cr6,r11,6
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 6, ctx.xer);
	// bne cr6,0x821f8c88
	if (!ctx.cr6.eq) goto loc_821F8C88;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// addi r3,r1,352
	ctx.r3.s64 = ctx.r1.s64 + 352;
	// addi r4,r11,24568
	ctx.r4.s64 = 0x820941F0u;
	// bl 0x821fac08
	ctx.lr = 0x821F8C00;
	sub_82202D20(ctx, base);
	// addi r3,r1,352
	ctx.r3.s64 = ctx.r1.s64 + 352;
	// b 0x821f8c54
	goto loc_821F8C54;
loc_821F8C08:
	// addi r3,r1,896
	ctx.r3.s64 = ctx.r1.s64 + 896;
	// lwz r4,84(r29)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r29.u32 + 84);
	// bl 0x821fac08
	ctx.lr = 0x821F8C14;
	sub_82202D20(ctx, base);
	// addi r3,r1,896
	ctx.r3.s64 = ctx.r1.s64 + 896;
	// b 0x821f8c54
	goto loc_821F8C54;
loc_821F8C1C:
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// lwz r4,80(r29)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r29.u32 + 80);
	// bl 0x821fac08
	ctx.lr = 0x821F8C28;
	sub_82202D20(ctx, base);
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// b 0x821f8c54
	goto loc_821F8C54;
loc_821F8C30:
	// addi r3,r1,624
	ctx.r3.s64 = ctx.r1.s64 + 624;
	// lwz r4,76(r29)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r29.u32 + 76);
	// bl 0x821fac08
	ctx.lr = 0x821F8C3C;
	sub_82202D20(ctx, base);
	// addi r3,r1,624
	ctx.r3.s64 = ctx.r1.s64 + 624;
	// b 0x821f8c54
	goto loc_821F8C54;
loc_821F8C44:
	// addi r3,r1,1168
	ctx.r3.s64 = ctx.r1.s64 + 1168;
	// lwz r4,72(r29)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r29.u32 + 72);
	// bl 0x821fac08
	ctx.lr = 0x821F8C50;
	sub_82202D20(ctx, base);
	// addi r3,r1,1168
	ctx.r3.s64 = ctx.r1.s64 + 1168;
loc_821F8C54:
	// li r6,0
	ctx.r6.s64 = 0;
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,0
	ctx.r4.s64 = 0;
	// bl 0x821f70d0
	ctx.lr = 0x821F8C64;
	sub_821FF428(ctx, base);
	// stw r3,0(r31)
	REX_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);
	// b 0x821f8c74
	goto loc_821F8C74;
loc_821F8C6C:
	// li r11,2
	ctx.r11.s64 = 2;
	// stw r11,152(r31)
	REX_STORE_U32(ctx.r31.u32 + 152, ctx.r11.u32);
loc_821F8C74:
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x821f8c88
	if (ctx.cr6.eq) goto loc_821F8C88;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f1.f64 = double(temp.f32);
	// bl 0x821fe058
	ctx.lr = 0x821F8C88;
	sub_822061C8(ctx, base);
loc_821F8C88:
	// lwz r11,180(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 180);
	// stw r11,176(r31)
	REX_STORE_U32(ctx.r31.u32 + 176, ctx.r11.u32);
loc_821F8C90:
	// addi r1,r1,1472
	ctx.r1.s64 = ctx.r1.s64 + 1472;
	// b 0x82a987ec
	__restgprlr_29(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_821F8CB0) {
	REX_FUNC_PROLOGUE();
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r11,r11,29952
	ctx.r11.s64 = ctx.r11.s64 + -32056;
	// lwz r10,52(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 52);
	// cmplw cr6,r10,r3
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r3.u32, ctx.xer);
	// bne cr6,0x821f8ce8
	if (!ctx.cr6.eq) goto loc_821F8CE8;
	// lbz r10,96(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 96);
	// clrlwi. r10,r10,31
	ctx.r10.u64 = ctx.r10.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,97(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 97);
	// ori r10,r10,1
	ctx.r10.u64 = ctx.r10.u64 | 1;
	// stb r10,97(r11)
	REX_STORE_U8(ctx.r11.u32 + 97, ctx.r10.u8);
	// blr 
	return;
loc_821F8CE8:
	// lwz r10,100(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 100);
	// cmplw cr6,r10,r3
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r3.u32, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// lbz r10,144(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 144);
	// clrlwi. r10,r10,31
	ctx.r10.u64 = ctx.r10.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,145(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 145);
	// ori r10,r10,1
	ctx.r10.u64 = ctx.r10.u64 | 1;
	// stb r10,145(r11)
	REX_STORE_U8(ctx.r11.u32 + 145, ctx.r10.u8);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_821F8D10) {
	REX_FUNC_PROLOGUE();
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// lis r11,-32026
	ctx.r11.s64 = -2098790400;
	// addi r11,r11,29952
	ctx.r11.s64 = ctx.r11.s64 + -32056;
	// lwz r10,52(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 52);
	// cmplw cr6,r10,r3
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r3.u32, ctx.xer);
	// bne cr6,0x821f8d48
	if (!ctx.cr6.eq) goto loc_821F8D48;
	// lbz r10,96(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 96);
	// rlwinm. r10,r10,0,30,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,97(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 97);
	// ori r10,r10,2
	ctx.r10.u64 = ctx.r10.u64 | 2;
	// stb r10,97(r11)
	REX_STORE_U8(ctx.r11.u32 + 97, ctx.r10.u8);
	// blr 
	return;
loc_821F8D48:
	// lwz r10,100(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 100);
	// cmplw cr6,r10,r3
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r3.u32, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// lbz r10,144(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 144);
	// rlwinm. r10,r10,0,30,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,145(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 145);
	// ori r10,r10,2
	ctx.r10.u64 = ctx.r10.u64 | 2;
	// stb r10,145(r11)
	REX_STORE_U8(ctx.r11.u32 + 145, ctx.r10.u8);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200128) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82a9879c
	ctx.lr = 0x82200130;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// mr r29,r4
	ctx.r29.u64 = ctx.r4.u64;
	// mr r30,r5
	ctx.r30.u64 = ctx.r5.u64;
	// bl 0x82200550
	ctx.lr = 0x82200144;
	retail_music_82200550(ctx, base);
	// li r11,0
	ctx.r11.s64 = 0;
	// lis r10,-32242
	ctx.r10.s64 = -2113011712;
	// stw r11,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r11.u32);
	// lfs f0,3972(r10)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// stw r11,28(r31)
	REX_STORE_U32(ctx.r31.u32 + 28, ctx.r11.u32);
	// stw r11,32(r31)
	REX_STORE_U32(ctx.r31.u32 + 32, ctx.r11.u32);
	// lwz r10,12(r30)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r30.u32 + 12);
	// stw r10,36(r31)
	REX_STORE_U32(ctx.r31.u32 + 36, ctx.r10.u32);
	// lfs f13,0(r30)
	temp.u32 = REX_LOAD_U32(ctx.r30.u32 + 0);
	ctx.f13.f64 = double(temp.f32);
	// stfs f13,4(r31)
	temp.f32 = float(ctx.f13.f64);
	REX_STORE_U32(ctx.r31.u32 + 4, temp.u32);
	// lfs f13,4(r30)
	temp.u32 = REX_LOAD_U32(ctx.r30.u32 + 4);
	ctx.f13.f64 = double(temp.f32);
	// stfs f13,8(r31)
	temp.f32 = float(ctx.f13.f64);
	REX_STORE_U32(ctx.r31.u32 + 8, temp.u32);
	// lfs f13,8(r30)
	temp.u32 = REX_LOAD_U32(ctx.r30.u32 + 8);
	ctx.f13.f64 = double(temp.f32);
	// stw r29,40(r31)
	REX_STORE_U32(ctx.r31.u32 + 40, ctx.r29.u32);
	// stfs f13,12(r31)
	temp.f32 = float(ctx.f13.f64);
	REX_STORE_U32(ctx.r31.u32 + 12, temp.u32);
	// stb r11,44(r31)
	REX_STORE_U8(ctx.r31.u32 + 44, ctx.r11.u8);
	// stfs f0,16(r31)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r31.u32 + 16, temp.u32);
	// stb r11,45(r31)
	REX_STORE_U8(ctx.r31.u32 + 45, ctx.r11.u8);
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82a987ec
	__restgprlr_29(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_82200198) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lwz r11,24(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 24);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x822003a0
	if (ctx.cr6.lt) goto loc_822003A0;
	// beq cr6,0x8220033c
	if (ctx.cr6.eq) goto loc_8220033C;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// blt cr6,0x822002ec
	if (ctx.cr6.lt) goto loc_822002EC;
	// beq cr6,0x822002c4
	if (ctx.cr6.eq) goto loc_822002C4;
	// cmplwi cr6,r11,5
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 5, ctx.xer);
	// blt cr6,0x82200294
	if (ctx.cr6.lt) goto loc_82200294;
	// beq cr6,0x8220021c
	if (ctx.cr6.eq) goto loc_8220021C;
	// cmplwi cr6,r11,7
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 7, ctx.xer);
	// bge cr6,0x8220026c
	if (!ctx.cr6.lt) goto loc_8220026C;
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x8220032c
	if (ctx.cr6.lt) goto loc_8220032C;
	// beq cr6,0x822001f8
	if (ctx.cr6.eq) goto loc_822001F8;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// bge cr6,0x8220026c
	if (!ctx.cr6.lt) goto loc_8220026C;
	// b 0x822003ec
	goto loc_822003EC;
loc_822001F8:
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq 0x82200208
	if (ctx.cr0.eq) goto loc_82200208;
	// bl 0x821fe1d8
	ctx.lr = 0x82200208;
	sub_82206348(ctx, base);
loc_82200208:
	// lwz r11,24(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 24);
	// li r10,1
	ctx.r10.s64 = 1;
	// stw r11,32(r31)
	REX_STORE_U32(ctx.r31.u32 + 32, ctx.r11.u32);
	// stw r10,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r10.u32);
	// b 0x8220026c
	goto loc_8220026C;
loc_8220021C:
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x8220032c
	if (ctx.cr6.lt) goto loc_8220032C;
	// beq cr6,0x82200280
	if (ctx.cr6.eq) goto loc_82200280;
	// cmplwi cr6,r11,5
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 5, ctx.xer);
	// bne cr6,0x8220026c
	if (!ctx.cr6.eq) goto loc_8220026C;
	// lfs f0,16(r31)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 16);
	ctx.f0.f64 = double(temp.f32);
	// fadds f0,f0,f1
	ctx.f0.f64 = double(float(ctx.f0.f64 + ctx.f1.f64));
	// lfs f13,12(r31)
	temp.u32 = REX_LOAD_U32(ctx.r31.u32 + 12);
	ctx.f13.f64 = double(temp.f32);
	// stfs f0,16(r31)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r31.u32 + 16, temp.u32);
	// fcmpu cr6,f0,f13
	ctx.cr6.compare(ctx.f0.f64, ctx.f13.f64);
	// blt cr6,0x82200264
	if (ctx.cr6.lt) goto loc_82200264;
	// bso cr6,0x82200264
	if (ctx.cr6.so) goto loc_82200264;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// li r10,0
	ctx.r10.s64 = 0;
	// lfs f0,3972(r11)
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// stfs f0,16(r31)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r31.u32 + 16, temp.u32);
	// stw r10,28(r31)
	REX_STORE_U32(ctx.r31.u32 + 28, ctx.r10.u32);
loc_82200264:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82200890
	ctx.lr = 0x8220026C;
	retail_music_82200890(ctx, base);
loc_8220026C:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
loc_82200280:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822006a0
	ctx.lr = 0x82200288;
	retail_music_822006A0(ctx, base);
	// clrlwi. r11,r3,24
	ctx.r11.u64 = ctx.r3.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82200208
	if (!ctx.cr0.eq) goto loc_82200208;
	// b 0x822001f8
	goto loc_822001F8;
loc_82200294:
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x8220032c
	if (ctx.cr6.lt) goto loc_8220032C;
	// beq cr6,0x822001f8
	if (ctx.cr6.eq) goto loc_822001F8;
	// cmplwi cr6,r11,4
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 4, ctx.xer);
	// beq cr6,0x822002b8
	if (ctx.cr6.eq) goto loc_822002B8;
	// cmplwi cr6,r11,5
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 5, ctx.xer);
	// bne cr6,0x8220026c
	if (!ctx.cr6.eq) goto loc_8220026C;
	// b 0x822003f0
	goto loc_822003F0;
loc_822002B8:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822007f0
	ctx.lr = 0x822002C0;
	retail_music_822007F0(ctx, base);
	// b 0x8220026c
	goto loc_8220026C;
loc_822002C4:
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x8220032c
	if (ctx.cr6.lt) goto loc_8220032C;
	// beq cr6,0x822001f8
	if (ctx.cr6.eq) goto loc_822001F8;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// blt cr6,0x822003ec
	if (ctx.cr6.lt) goto loc_822003EC;
	// bne cr6,0x8220026c
	if (!ctx.cr6.eq) goto loc_8220026C;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82200740
	ctx.lr = 0x822002E8;
	retail_music_82200740(ctx, base);
	// b 0x8220026c
	goto loc_8220026C;
loc_822002EC:
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x8220032c
	if (ctx.cr6.lt) goto loc_8220032C;
	// beq cr6,0x822001f8
	if (ctx.cr6.eq) goto loc_822001F8;
	// cmplwi cr6,r11,4
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 4, ctx.xer);
	// bne cr6,0x8220026c
	if (!ctx.cr6.eq) goto loc_8220026C;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// li r10,4
	ctx.r10.s64 = 4;
	// lfs f0,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// lbz r11,45(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 45);
	// stfs f0,16(r31)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r31.u32 + 16, temp.u32);
	// stw r10,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r10.u32);
	// stw r10,28(r31)
	REX_STORE_U32(ctx.r31.u32 + 28, ctx.r10.u32);
	// andi. r11,r11,253
	ctx.r11.u64 = ctx.r11.u64 & 253;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// stb r11,45(r31)
	REX_STORE_U8(ctx.r31.u32 + 45, ctx.r11.u8);
	// b 0x8220026c
	goto loc_8220026C;
loc_8220032C:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822008d8
	ctx.lr = 0x82200334;
	retail_music_822008D8(ctx, base);
	// li r11,0
	ctx.r11.s64 = 0;
	// b 0x822003f0
	goto loc_822003F0;
loc_8220033C:
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x82200388
	if (ctx.cr6.lt) goto loc_82200388;
	// beq cr6,0x82200370
	if (ctx.cr6.eq) goto loc_82200370;
	// cmplwi cr6,r11,7
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 7, ctx.xer);
	// bge cr6,0x8220026c
	if (!ctx.cr6.lt) goto loc_8220026C;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822004b8
	ctx.lr = 0x8220035C;
	retail_music_822004B8(ctx, base);
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// li r10,0
	ctx.r10.s64 = 0;
	// stw r11,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r11.u32);
	// stw r10,32(r31)
	REX_STORE_U32(ctx.r31.u32 + 32, ctx.r10.u32);
	// b 0x8220026c
	goto loc_8220026C;
loc_82200370:
	// lwz r11,32(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 32);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82200264
	if (ctx.cr0.eq) goto loc_82200264;
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// bne cr6,0x8220026c
	if (!ctx.cr6.eq) goto loc_8220026C;
	// b 0x82200264
	goto loc_82200264;
loc_82200388:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822008d8
	ctx.lr = 0x82200390;
	retail_music_822008D8(ctx, base);
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r11.u32);
	// stw r11,32(r31)
	REX_STORE_U32(ctx.r31.u32 + 32, ctx.r11.u32);
	// b 0x8220026c
	goto loc_8220026C;
loc_822003A0:
	// lwz r11,28(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
	// cmplwi cr6,r11,2
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2, ctx.xer);
	// blt cr6,0x82200264
	if (ctx.cr6.lt) goto loc_82200264;
	// beq cr6,0x822003c4
	if (ctx.cr6.eq) goto loc_822003C4;
	// cmplwi cr6,r11,4
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 4, ctx.xer);
	// bge cr6,0x8220026c
	if (!ctx.cr6.lt) goto loc_8220026C;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82200638
	ctx.lr = 0x822003C0;
	retail_music_82200638(ctx, base);
	// b 0x8220026c
	goto loc_8220026C;
loc_822003C4:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822004b8
	ctx.lr = 0x822003CC;
	retail_music_822004B8(ctx, base);
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq 0x822003ec
	if (ctx.cr0.eq) goto loc_822003EC;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// addi r4,r11,24868
	ctx.r4.s64 = 0x8209431Cu;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,4116(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFD34u);
	ctx.f1.f64 = double(temp.f32);
	// bl 0x821fe378
	ctx.lr = 0x822003EC;
	sub_822064E8(ctx, base);
loc_822003EC:
	// li r11,2
	ctx.r11.s64 = 2;
loc_822003F0:
	// stw r11,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r11.u32);
	// b 0x8220026c
	goto loc_8220026C;
}

DEFINE_REX_FUNC(retail_music_822003F8) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-368(r1)
	ea = -368 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32047
	ctx.r11.s64 = -2100232192;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lwz r11,240(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 656);
	// lwz r10,0(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// stw r11,340(r1)
	REX_STORE_U32(ctx.r1.u32 + 340, ctx.r11.u32);
	// bne cr6,0x82200458
	if (!ctx.cr6.eq) goto loc_82200458;
	// lwz r11,20(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 20);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x82200458
	if (ctx.cr6.eq) goto loc_82200458;
	// lwz r11,40(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 40);
	// li r6,0
	ctx.r6.s64 = 0;
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,0
	ctx.r4.s64 = 0;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// stw r11,336(r1)
	REX_STORE_U32(ctx.r1.u32 + 336, ctx.r11.u32);
	// li r11,0
	ctx.r11.s64 = 0;
	// stb r11,80(r1)
	REX_STORE_U8(ctx.r1.u32 + 80, ctx.r11.u8);
	// bl 0x821f70d0
	ctx.lr = 0x82200454;
	sub_821FF428(ctx, base);
	// stw r3,0(r31)
	REX_STORE_U32(ctx.r31.u32 + 0, ctx.r3.u32);
loc_82200458:
	// lwz r3,340(r1)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r1.u32 + 340);
	// bl 0x82a92530
	ctx.lr = 0x82200460;
	sub_82AA2290(ctx, base);
	// addi r1,r1,368
	ctx.r1.s64 = ctx.r1.s64 + 368;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200478) {
	REX_FUNC_PROLOGUE();
	// li r11,2
	ctx.r11.s64 = 2;
	// stw r11,28(r3)
	REX_STORE_U32(ctx.r3.u32 + 28, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200488) {
	REX_FUNC_PROLOGUE();
	// li r11,1
	ctx.r11.s64 = 1;
	// stw r11,28(r3)
	REX_STORE_U32(ctx.r3.u32 + 28, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200498) {
	REX_FUNC_PROLOGUE();
	// lwz r11,24(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 24);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// lwz r10,32(r3)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32 + 32);
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,32(r3)
	REX_STORE_U32(ctx.r3.u32 + 32, ctx.r11.u32);
	// stw r10,28(r3)
	REX_STORE_U32(ctx.r3.u32 + 28, ctx.r10.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822004B8) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// bl 0x822003f8
	ctx.lr = 0x822004D0;
	retail_music_822003F8(ctx, base);
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq 0x8220053c
	if (ctx.cr0.eq) goto loc_8220053C;
	// lbz r11,44(r31)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r31.u32 + 44);
	// clrlwi. r11,r11,31
	ctx.r11.u64 = ctx.r11.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x8220052c
	if (ctx.cr0.eq) goto loc_8220052C;
	// lwz r11,340(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 340);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x82200508
	if (ctx.cr6.eq) goto loc_82200508;
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// beq cr6,0x82200508
	if (ctx.cr6.eq) goto loc_82200508;
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// li r11,0
	ctx.r11.s64 = 0;
	// bne cr6,0x8220050c
	if (!ctx.cr6.eq) goto loc_8220050C;
loc_82200508:
	// li r11,1
	ctx.r11.s64 = 1;
loc_8220050C:
	// clrlwi. r11,r11,24
	ctx.r11.u64 = ctx.r11.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x8220052c
	if (ctx.cr0.eq) goto loc_8220052C;
	// lwz r11,340(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 340);
	// addi r11,r11,-5
	ctx.r11.s64 = ctx.r11.s64 + -5;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm. r11,r11,27,31,31
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x8220052c
	if (!ctx.cr0.eq) goto loc_8220052C;
	// bl 0x821fd580
	ctx.lr = 0x8220052C;
	sub_822056F0(ctx, base);
loc_8220052C:
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// lfs f1,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f1.f64 = double(temp.f32);
	// bl 0x821fe058
	ctx.lr = 0x8220053C;
	sub_822061C8(ctx, base);
loc_8220053C:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200550) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r30,-24(r1)
	REX_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// lwz r11,0(r30)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r30.u32 + 0);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq 0x822005a4
	if (ctx.cr0.eq) goto loc_822005A4;
	// lwz r10,336(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 336);
	// ori r10,r10,1
	ctx.r10.u64 = ctx.r10.u64 | 1;
	// stw r10,336(r11)
	REX_STORE_U32(ctx.r11.u32 + 336, ctx.r10.u32);
	// lwz r31,0(r30)
	ctx.r31.u64 = REX_LOAD_U32(ctx.r30.u32 + 0);
	// cmplwi r31,0
	ctx.cr0.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// beq 0x8220059c
	if (ctx.cr0.eq) goto loc_8220059C;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x821fd450
	ctx.lr = 0x82200594;
	sub_822055C0(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x821fda60
	ctx.lr = 0x8220059C;
	sub_82205BD0(ctx, base);
loc_8220059C:
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,0(r30)
	REX_STORE_U32(ctx.r30.u32 + 0, ctx.r11.u32);
loc_822005A4:
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r30,-24(r1)
	ctx.r30.u64 = REX_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822005C0) {
	REX_FUNC_PROLOGUE();
	// mr r11,r3
	ctx.r11.u64 = ctx.r3.u64;
	// lwz r10,24(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// cmpwi cr6,r10,2
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 2, ctx.xer);
	// bne cr6,0x822005e4
	if (!ctx.cr6.eq) goto loc_822005E4;
	// lbz r9,44(r11)
	ctx.r9.u64 = REX_LOAD_U8(ctx.r11.u32 + 44);
	// rlwinm. r9,r9,0,30,30
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r9.s32, 0, ctx.xer);
	// bne 0x822005e4
	if (!ctx.cr0.eq) goto loc_822005E4;
loc_822005DC:
	// li r3,1
	ctx.r3.s64 = 1;
	// blr 
	return;
loc_822005E4:
	// cmpwi cr6,r10,6
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 6, ctx.xer);
	// bne cr6,0x822005f8
	if (!ctx.cr6.eq) goto loc_822005F8;
	// lbz r10,45(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 45);
	// rlwinm. r10,r10,0,30,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// bne 0x822005dc
	if (!ctx.cr0.eq) goto loc_822005DC;
loc_822005F8:
	// li r10,6
	ctx.r10.s64 = 6;
	// li r3,0
	ctx.r3.s64 = 0;
	// stw r10,24(r11)
	REX_STORE_U32(ctx.r11.u32 + 24, ctx.r10.u32);
	// stw r10,28(r11)
	REX_STORE_U32(ctx.r11.u32 + 28, ctx.r10.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200610) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f0,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// li r11,4
	ctx.r11.s64 = 4;
	// stfs f0,16(r3)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r3.u32 + 16, temp.u32);
	// stw r11,24(r3)
	REX_STORE_U32(ctx.r3.u32 + 24, ctx.r11.u32);
	// stw r11,28(r3)
	REX_STORE_U32(ctx.r3.u32 + 28, ctx.r11.u32);
	// lbz r11,45(r3)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r3.u32 + 45);
	// andi. r11,r11,253
	ctx.r11.u64 = ctx.r11.u64 & 253;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// stb r11,45(r3)
	REX_STORE_U8(ctx.r3.u32 + 45, ctx.r11.u8);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200638) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stfd f31,-24(r1)
	ctx.fpscr.disableFlushMode();
	REX_STORE_U64(ctx.r1.u32 + -24, ctx.f31.u64);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// bl 0x822004b8
	ctx.lr = 0x82200654;
	retail_music_822004B8(ctx, base);
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// lfs f31,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f31.f64 = double(temp.f32);
	// beq 0x82200678
	if (ctx.cr0.eq) goto loc_82200678;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// fmr f1,f31
	ctx.f1.f64 = ctx.f31.f64;
	// addi r4,r11,24868
	ctx.r4.s64 = 0x8209431Cu;
	// bl 0x821fe378
	ctx.lr = 0x82200678;
	sub_822064E8(ctx, base);
loc_82200678:
	// li r11,3
	ctx.r11.s64 = 3;
	// stfs f31,16(r31)
	ctx.fpscr.disableFlushMode();
	temp.f32 = float(ctx.f31.f64);
	REX_STORE_U32(ctx.r31.u32 + 16, temp.u32);
	// stw r11,24(r31)
	REX_STORE_U32(ctx.r31.u32 + 24, ctx.r11.u32);
	// stw r11,28(r31)
	REX_STORE_U32(ctx.r31.u32 + 28, ctx.r11.u32);
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// lfd f31,-24(r1)
	ctx.f31.u64 = REX_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822006A0) {
	REX_FUNC_PROLOGUE();
	// lbz r11,44(r3)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r3.u32 + 44);
	// clrlwi. r11,r11,31
	ctx.r11.u64 = ctx.r11.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82200710
	if (ctx.cr0.eq) goto loc_82200710;
	// lwz r11,24(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 24);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x822006c0
	if (ctx.cr0.eq) goto loc_822006C0;
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// bne cr6,0x82200710
	if (!ctx.cr6.eq) goto loc_82200710;
loc_822006C0:
	// lwz r10,0(r3)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// beq 0x82200710
	if (ctx.cr0.eq) goto loc_82200710;
	// lwz r11,340(r10)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + 340);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x822006ec
	if (ctx.cr6.eq) goto loc_822006EC;
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// beq cr6,0x822006ec
	if (ctx.cr6.eq) goto loc_822006EC;
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// li r11,0
	ctx.r11.s64 = 0;
	// bne cr6,0x822006f0
	if (!ctx.cr6.eq) goto loc_822006F0;
loc_822006EC:
	// li r11,1
	ctx.r11.s64 = 1;
loc_822006F0:
	// clrlwi. r11,r11,24
	ctx.r11.u64 = ctx.r11.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82200710
	if (ctx.cr0.eq) goto loc_82200710;
	// lwz r11,340(r10)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + 340);
	// li r3,1
	ctx.r3.s64 = 1;
	// addi r11,r11,-5
	ctx.r11.s64 = ctx.r11.s64 + -5;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm. r11,r11,27,31,31
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
loc_82200710:
	// li r3,0
	ctx.r3.s64 = 0;
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200718) {
	REX_FUNC_PROLOGUE();
	// li r11,1
	ctx.r11.s64 = 1;
	// lbz r10,44(r3)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r3.u32 + 44);
	// slw r11,r11,r4
	ctx.r11.u64 = ctx.r4.u8 & 0x20 ? 0 : (ctx.r11.u32 << (ctx.r4.u8 & 0x3F));
	// clrlwi r11,r11,24
	ctx.r11.u64 = ctx.r11.u32 & 0xFF;
	// and. r10,r10,r11
	ctx.r10.u64 = ctx.r10.u64 & ctx.r11.u64;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,45(r3)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r3.u32 + 45);
	// or r11,r10,r11
	ctx.r11.u64 = ctx.r10.u64 | ctx.r11.u64;
	// stb r11,45(r3)
	REX_STORE_U8(ctx.r3.u32 + 45, ctx.r11.u8);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200740) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mr r11,r3
	ctx.r11.u64 = ctx.r3.u64;
	// lfs f13,16(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r11.u32 + 16);
	ctx.f13.f64 = double(temp.f32);
	// fadds f13,f1,f13
	ctx.f13.f64 = double(float(ctx.f1.f64 + ctx.f13.f64));
	// lfs f0,4(r11)
	temp.u32 = REX_LOAD_U32(ctx.r11.u32 + 4);
	ctx.f0.f64 = double(temp.f32);
	// stfs f13,16(r11)
	temp.f32 = float(ctx.f13.f64);
	REX_STORE_U32(ctx.r11.u32 + 16, temp.u32);
	// fcmpu cr6,f13,f0
	ctx.cr6.compare(ctx.f13.f64, ctx.f0.f64);
	// blt cr6,0x82200794
	if (ctx.cr6.lt) goto loc_82200794;
	// bso cr6,0x82200794
	if (ctx.cr6.so) goto loc_82200794;
	// lis r10,-32242
	ctx.r10.s64 = -2113011712;
	// lwz r3,0(r11)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// li r9,2
	ctx.r9.s64 = 2;
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// lfs f0,3972(r10)
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// stfs f0,16(r11)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r11.u32 + 16, temp.u32);
	// stw r9,28(r11)
	REX_STORE_U32(ctx.r11.u32 + 28, ctx.r9.u32);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// addi r4,r11,24868
	ctx.r4.s64 = 0x8209431Cu;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,4116(r11)
	temp.u32 = REX_LOAD_U32(0x820DFD34u);
	ctx.f1.f64 = double(temp.f32);
	// b 0x821fe378
	sub_822064E8(ctx, base);
	return;
loc_82200794:
	// lis r10,-32242
	ctx.r10.s64 = -2113011712;
	// lfs f11,4116(r10)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFD34u);
	ctx.f11.f64 = double(temp.f32);
	// lis r10,-32249
	ctx.r10.s64 = -2113470464;
	// fmr f1,f11
	ctx.f1.f64 = ctx.f11.f64;
	// lfs f12,23932(r10)
	temp.u32 = REX_LOAD_U32(0x82073C20u);
	ctx.f12.f64 = double(temp.f32);
	// fcmpu cr6,f0,f12
	ctx.cr6.compare(ctx.f0.f64, ctx.f12.f64);
	// ble cr6,0x822007d0
	if (!ctx.cr6.gt) goto loc_822007D0;
	// fdivs f0,f13,f0
	ctx.f0.f64 = double(float(ctx.f13.f64 / ctx.f0.f64));
	// lis r10,-32241
	ctx.r10.s64 = -2112946176;
	// lfd f13,-16368(r10)
	ctx.f13.u64 = REX_LOAD_U64(0x820EAD28u);
	// lis r10,-32241
	ctx.r10.s64 = -2112946176;
	// fsubs f11,f11,f0
	ctx.f11.f64 = double(float(ctx.f11.f64 - ctx.f0.f64));
	// lfd f12,-16432(r10)
	ctx.f12.u64 = REX_LOAD_U64(0x820EACE8u);
	// fsel f13,f11,f0,f13
	ctx.f13.f64 = ctx.f11.f64 >= 0.0 ? ctx.f0.f64 : ctx.f13.f64;
	// fsel f1,f0,f13,f12
	ctx.f1.f64 = ctx.f0.f64 >= 0.0 ? ctx.f13.f64 : ctx.f12.f64;
loc_822007D0:
	// lwz r3,0(r11)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// addi r4,r11,24868
	ctx.r4.s64 = 0x8209431Cu;
	// b 0x821fe378
	sub_822064E8(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_822007E8) {
	REX_FUNC_PROLOGUE();
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822007F0) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	// lfs f13,16(r3)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r3.u32 + 16);
	ctx.f13.f64 = double(temp.f32);
	// fadds f13,f1,f13
	ctx.f13.f64 = double(float(ctx.f1.f64 + ctx.f13.f64));
	// lfs f0,8(r3)
	temp.u32 = REX_LOAD_U32(ctx.r3.u32 + 8);
	ctx.f0.f64 = double(temp.f32);
	// stfs f13,16(r3)
	temp.f32 = float(ctx.f13.f64);
	REX_STORE_U32(ctx.r3.u32 + 16, temp.u32);
	// fcmpu cr6,f13,f0
	ctx.cr6.compare(ctx.f13.f64, ctx.f0.f64);
	// blt cr6,0x82200830
	if (ctx.cr6.lt) goto loc_82200830;
	// bso cr6,0x82200830
	if (ctx.cr6.so) goto loc_82200830;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lwz r10,36(r3)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32 + 36);
	// lfs f0,3972(r11)
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// subfic r11,r10,0
	ctx.xer.ca = ctx.r10.u32 <= 0;
	ctx.r11.u64 = static_cast<uint64_t>(0) - ctx.r10.u64;
	// stfs f0,16(r3)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r3.u32 + 16, temp.u32);
	// subfe r11,r11,r11
	temp.u8 = (~ctx.r11.u32 + ctx.r11.u32 < ~ctx.r11.u32) | (~ctx.r11.u32 + ctx.r11.u32 + ctx.xer.ca < ctx.xer.ca);
	ctx.r11.u64 = ~ctx.r11.u64 + ctx.r11.u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	// andi. r11,r11,5
	ctx.r11.u64 = ctx.r11.u64 & 5;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// stw r11,28(r3)
	REX_STORE_U32(ctx.r3.u32 + 28, ctx.r11.u32);
	// blr 
	return;
loc_82200830:
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f1.f64 = double(temp.f32);
	// lis r11,-32249
	ctx.r11.s64 = -2113470464;
	// lfs f12,23932(r11)
	temp.u32 = REX_LOAD_U32(0x82073C20u);
	ctx.f12.f64 = double(temp.f32);
	// fcmpu cr6,f0,f12
	ctx.cr6.compare(ctx.f0.f64, ctx.f12.f64);
	// ble cr6,0x82200878
	if (!ctx.cr6.gt) goto loc_82200878;
	// fdivs f0,f13,f0
	ctx.f0.f64 = double(float(ctx.f13.f64 / ctx.f0.f64));
	// lis r11,-32241
	ctx.r11.s64 = -2112946176;
	// lfd f13,-16368(r11)
	ctx.f13.u64 = REX_LOAD_U64(0x820EAD28u);
	// lis r11,-32241
	ctx.r11.s64 = -2112946176;
	// fsub f0,f13,f0
	ctx.f0.f64 = ctx.f13.f64 - ctx.f0.f64;
	// lfd f12,-16432(r11)
	ctx.f12.u64 = REX_LOAD_U64(0x820EACE8u);
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f11,4116(r11)
	temp.u32 = REX_LOAD_U32(0x820DFD34u);
	ctx.f11.f64 = double(temp.f32);
	// frsp f0,f0
	ctx.f0.f64 = double(float(ctx.f0.f64));
	// fsubs f11,f11,f0
	ctx.f11.f64 = double(float(ctx.f11.f64 - ctx.f0.f64));
	// fsel f13,f11,f0,f13
	ctx.f13.f64 = ctx.f11.f64 >= 0.0 ? ctx.f0.f64 : ctx.f13.f64;
	// fsel f1,f0,f13,f12
	ctx.f1.f64 = ctx.f0.f64 >= 0.0 ? ctx.f13.f64 : ctx.f12.f64;
loc_82200878:
	// lwz r3,0(r3)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r3.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// addi r4,r11,24868
	ctx.r4.s64 = 0x8209431Cu;
	// b 0x821fe378
	sub_822064E8(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_82200890) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r9,r3
	ctx.r9.u64 = ctx.r3.u64;
	// bl 0x822006a0
	ctx.lr = 0x822008A4;
	retail_music_822006A0(ctx, base);
	// clrlwi. r11,r3,24
	ctx.r11.u64 = ctx.r3.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x822008c8
	if (ctx.cr0.eq) goto loc_822008C8;
	// lbz r11,45(r9)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r9.u32 + 45);
	// clrlwi. r10,r11,31
	ctx.r10.u64 = ctx.r11.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x822008c8
	if (ctx.cr0.eq) goto loc_822008C8;
	// rlwinm r11,r11,0,24,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0xFE;
	// lwz r3,0(r9)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r9.u32 + 0);
	// stb r11,45(r9)
	REX_STORE_U8(ctx.r9.u32 + 45, ctx.r11.u8);
	// bl 0x821fe1d8
	ctx.lr = 0x822008C8;
	sub_82206348(ctx, base);
loc_822008C8:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822008D8) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mr r11,r3
	ctx.r11.u64 = ctx.r3.u64;
	// lwz r3,0(r11)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,44(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 44);
	// clrlwi. r10,r10,31
	ctx.r10.u64 = ctx.r10.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x82200908
	if (ctx.cr0.eq) goto loc_82200908;
	// lis r11,-32247
	ctx.r11.s64 = -2113339392;
	// addi r4,r11,24868
	ctx.r4.s64 = 0x8209431Cu;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lfs f1,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f1.f64 = double(temp.f32);
	// b 0x821fe378
	sub_822064E8(ctx, base);
	return;
loc_82200908:
	// lwz r11,20(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 20);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// bne cr6,0x82200918
	if (!ctx.cr6.eq) goto loc_82200918;
	// b 0x821fe1d8
	sub_82206348(ctx, base);
	return;
loc_82200918:
	// b 0x821fd580
	sub_822056F0(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_8220091C) {
	REX_FUNC_PROLOGUE();
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200920) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	// lis r8,-32242
	ctx.r8.s64 = -2113011712;
	// li r9,2
	ctx.r9.s64 = 2;
	// addi r10,r3,8
	ctx.r10.s64 = ctx.r3.s64 + 8;
	// li r11,0
	ctx.r11.s64 = 0;
	// lfs f0,3972(r8)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
loc_82200934:
	// stfs f0,-4(r10)
	ctx.fpscr.disableFlushMode();
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r10.u32 + -4, temp.u32);
	// stw r11,-8(r10)
	REX_STORE_U32(ctx.r10.u32 + -8, ctx.r11.u32);
	// stfs f0,0(r10)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r10.u32 + 0, temp.u32);
	// stw r11,12(r10)
	REX_STORE_U32(ctx.r10.u32 + 12, ctx.r11.u32);
	// stfs f0,4(r10)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r10.u32 + 4, temp.u32);
	// stw r11,16(r10)
	REX_STORE_U32(ctx.r10.u32 + 16, ctx.r11.u32);
	// stfs f0,8(r10)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r10.u32 + 8, temp.u32);
	// stw r11,20(r10)
	REX_STORE_U32(ctx.r10.u32 + 20, ctx.r11.u32);
	// stw r11,24(r10)
	REX_STORE_U32(ctx.r10.u32 + 24, ctx.r11.u32);
	// addic. r9,r9,-1
	ctx.xer.ca = ctx.r9.u32 > 0;
	ctx.r9.s64 = ctx.r9.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r9.s32, 0, ctx.xer);
	// stw r11,28(r10)
	REX_STORE_U32(ctx.r10.u32 + 28, ctx.r11.u32);
	// stw r11,32(r10)
	REX_STORE_U32(ctx.r10.u32 + 32, ctx.r11.u32);
	// stb r11,36(r10)
	REX_STORE_U8(ctx.r10.u32 + 36, ctx.r11.u8);
	// stb r11,37(r10)
	REX_STORE_U8(ctx.r10.u32 + 37, ctx.r11.u8);
	// addi r10,r10,48
	ctx.r10.s64 = ctx.r10.s64 + 48;
	// bge 0x82200934
	if (!ctx.cr0.lt) goto loc_82200934;
	// li r9,3
	ctx.r9.s64 = 3;
	// stw r11,144(r3)
	REX_STORE_U32(ctx.r3.u32 + 144, ctx.r11.u32);
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// addi r10,r3,20
	ctx.r10.s64 = ctx.r3.s64 + 20;
	// stw r11,152(r3)
	REX_STORE_U32(ctx.r3.u32 + 152, ctx.r11.u32);
	// stw r11,156(r3)
	REX_STORE_U32(ctx.r3.u32 + 156, ctx.r11.u32);
	// stw r11,160(r3)
	REX_STORE_U32(ctx.r3.u32 + 160, ctx.r11.u32);
	// stw r9,164(r3)
	REX_STORE_U32(ctx.r3.u32 + 164, ctx.r9.u32);
	// stw r9,168(r3)
	REX_STORE_U32(ctx.r3.u32 + 168, ctx.r9.u32);
loc_82200998:
	// mr r9,r11
	ctx.r9.u64 = ctx.r11.u64;
	// addi r11,r11,1
	ctx.r11.s64 = ctx.r11.s64 + 1;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// stw r9,0(r10)
	REX_STORE_U32(ctx.r10.u32 + 0, ctx.r9.u32);
	// addi r10,r10,48
	ctx.r10.s64 = ctx.r10.s64 + 48;
	// blt cr6,0x82200998
	if (ctx.cr6.lt) goto loc_82200998;
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822009B8) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82a9879c
	ctx.lr = 0x822009C0;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r29,r3
	ctx.r29.u64 = ctx.r3.u64;
	// li r31,3
	ctx.r31.s64 = 3;
	// mr r30,r29
	ctx.r30.u64 = ctx.r29.u64;
loc_822009D0:
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x82200550
	ctx.lr = 0x822009D8;
	retail_music_82200550(ctx, base);
	// addic. r31,r31,-1
	ctx.xer.ca = ctx.r31.u32 > 0;
	ctx.r31.s64 = ctx.r31.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r31.s32, 0, ctx.xer);
	// addi r30,r30,48
	ctx.r30.s64 = ctx.r30.s64 + 48;
	// bne 0x822009d0
	if (!ctx.cr0.eq) goto loc_822009D0;
	// addi r31,r29,144
	ctx.r31.s64 = ctx.r29.s64 + 144;
	// li r30,2
	ctx.r30.s64 = 2;
loc_822009EC:
	// addi r31,r31,-48
	ctx.r31.s64 = ctx.r31.s64 + -48;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82200550
	ctx.lr = 0x822009F8;
	retail_music_82200550(ctx, base);
	// addic. r30,r30,-1
	ctx.xer.ca = ctx.r30.u32 > 0;
	ctx.r30.s64 = ctx.r30.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r30.s32, 0, ctx.xer);
	// bge 0x822009ec
	if (!ctx.cr0.lt) goto loc_822009EC;
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82a987ec
	__restgprlr_29(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_82200A08) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r30,-24(r1)
	REX_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-368(r1)
	ea = -368 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// lis r11,-32047
	ctx.r11.s64 = -2100232192;
	// li r10,-1
	ctx.r10.s64 = -1;
	// mr r31,r4
	ctx.r31.u64 = ctx.r4.u64;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// lwz r11,240(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 656);
	// stw r10,336(r1)
	REX_STORE_U32(ctx.r1.u32 + 336, ctx.r10.u32);
	// li r10,0
	ctx.r10.s64 = 0;
	// lwz r4,24(r31)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r31.u32 + 24);
	// stw r11,340(r1)
	REX_STORE_U32(ctx.r1.u32 + 340, ctx.r11.u32);
	// stb r10,80(r1)
	REX_STORE_U8(ctx.r1.u32 + 80, ctx.r10.u8);
	// bl 0x821fac08
	ctx.lr = 0x82200A4C;
	sub_82202D20(ctx, base);
	// addi r5,r31,28
	ctx.r5.s64 = ctx.r31.s64 + 28;
	// lwz r4,336(r1)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r1.u32 + 336);
	// addi r3,r30,48
	ctx.r3.s64 = ctx.r30.s64 + 48;
	// bl 0x82200128
	ctx.lr = 0x82200A5C;
	retail_music_82200128(ctx, base);
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// lwz r4,48(r31)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r31.u32 + 48);
	// bl 0x821fac08
	ctx.lr = 0x82200A68;
	sub_82202D20(ctx, base);
	// addi r5,r31,52
	ctx.r5.s64 = ctx.r31.s64 + 52;
	// addi r3,r30,96
	ctx.r3.s64 = ctx.r30.s64 + 96;
	// lwz r4,336(r1)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r1.u32 + 336);
	// bl 0x82200128
	ctx.lr = 0x82200A78;
	retail_music_82200128(ctx, base);
	// lwz r3,340(r1)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r1.u32 + 340);
	// bl 0x82a92530
	ctx.lr = 0x82200A80;
	sub_82AA2290(ctx, base);
	// addi r1,r1,368
	ctx.r1.s64 = ctx.r1.s64 + 368;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r30,-24(r1)
	ctx.r30.u64 = REX_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200A98) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82a98798
	ctx.lr = 0x82200AA0;
	__savegprlr_28(ctx, base);
	// stfd f31,-48(r1)
	ctx.fpscr.disableFlushMode();
	REX_STORE_U64(ctx.r1.u32 + -48, ctx.f31.u64);
	// stwu r1,-128(r1)
	ea = -128 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// fmr f31,f1
	ctx.f31.f64 = ctx.f1.f64;
	// li r28,3
	ctx.r28.s64 = 3;
	// mr r29,r31
	ctx.r29.u64 = ctx.r31.u64;
	// mr r30,r28
	ctx.r30.u64 = ctx.r28.u64;
loc_82200ABC:
	// mr r3,r29
	ctx.r3.u64 = ctx.r29.u64;
	// fmr f1,f31
	ctx.fpscr.disableFlushMode();
	ctx.f1.f64 = ctx.f31.f64;
	// bl 0x82200198
	ctx.lr = 0x82200AC8;
	retail_music_82200198(ctx, base);
	// addic. r30,r30,-1
	ctx.xer.ca = ctx.r30.u32 > 0;
	ctx.r30.s64 = ctx.r30.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r30.s32, 0, ctx.xer);
	// addi r29,r29,48
	ctx.r29.s64 = ctx.r29.s64 + 48;
	// bne 0x82200abc
	if (!ctx.cr0.eq) goto loc_82200ABC;
	// lwz r11,144(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 144);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x82200be4
	if (ctx.cr6.lt) goto loc_82200BE4;
	// beq cr6,0x82200bb4
	if (ctx.cr6.eq) goto loc_82200BB4;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// blt cr6,0x82200b40
	if (ctx.cr6.lt) goto loc_82200B40;
	// bne cr6,0x82200c00
	if (!ctx.cr6.eq) goto loc_82200C00;
	// lwz r11,148(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 148);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x82200bd8
	if (ctx.cr6.lt) goto loc_82200BD8;
	// beq cr6,0x82200b10
	if (ctx.cr6.eq) goto loc_82200B10;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// fmr f1,f31
	ctx.fpscr.disableFlushMode();
	ctx.f1.f64 = ctx.f31.f64;
	// bl 0x82200c10
	ctx.lr = 0x82200B0C;
	retail_music_82200C10(ctx, base);
	// b 0x82200c00
	goto loc_82200C00;
loc_82200B10:
	// addi r9,r31,28
	ctx.r9.s64 = ctx.r31.s64 + 28;
	// mr r10,r28
	ctx.r10.u64 = ctx.r28.u64;
	// li r11,1
	ctx.r11.s64 = 1;
loc_82200B1C:
	// stw r11,0(r9)
	REX_STORE_U32(ctx.r9.u32 + 0, ctx.r11.u32);
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r10.u32 > 0;
	ctx.r10.s64 = ctx.r10.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// addi r9,r9,48
	ctx.r9.s64 = ctx.r9.s64 + 48;
	// bne 0x82200b1c
	if (!ctx.cr0.eq) goto loc_82200B1C;
loc_82200B2C:
	// lwz r10,144(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 144);
	// stw r11,144(r31)
	REX_STORE_U32(ctx.r31.u32 + 144, ctx.r11.u32);
	// stw r11,148(r31)
	REX_STORE_U32(ctx.r31.u32 + 148, ctx.r11.u32);
	// stw r10,152(r31)
	REX_STORE_U32(ctx.r31.u32 + 152, ctx.r10.u32);
	// b 0x82200c00
	goto loc_82200C00;
loc_82200B40:
	// lwz r11,148(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 148);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x82200bd8
	if (ctx.cr6.lt) goto loc_82200BD8;
	// beq cr6,0x82200b94
	if (ctx.cr6.eq) goto loc_82200B94;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// bne cr6,0x82200c00
	if (!ctx.cr6.eq) goto loc_82200C00;
	// lwz r11,164(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// li r10,0
	ctx.r10.s64 = 0;
	// stw r28,144(r31)
	REX_STORE_U32(ctx.r31.u32 + 144, ctx.r28.u32);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// stw r28,148(r31)
	REX_STORE_U32(ctx.r31.u32 + 148, ctx.r28.u32);
	// stw r10,156(r31)
	REX_STORE_U32(ctx.r31.u32 + 156, ctx.r10.u32);
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// lwz r11,36(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 36);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x82200b8c
	if (!ctx.cr6.eq) goto loc_82200B8C;
	// li r11,2
	ctx.r11.s64 = 2;
	// stw r11,160(r31)
	REX_STORE_U32(ctx.r31.u32 + 160, ctx.r11.u32);
	// b 0x82200c00
	goto loc_82200C00;
loc_82200B8C:
	// stw r28,160(r31)
	REX_STORE_U32(ctx.r31.u32 + 160, ctx.r28.u32);
	// b 0x82200c00
	goto loc_82200C00;
loc_82200B94:
	// addi r9,r31,28
	ctx.r9.s64 = ctx.r31.s64 + 28;
	// mr r10,r28
	ctx.r10.u64 = ctx.r28.u64;
	// li r11,1
	ctx.r11.s64 = 1;
loc_82200BA0:
	// stw r11,0(r9)
	REX_STORE_U32(ctx.r9.u32 + 0, ctx.r11.u32);
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r10.u32 > 0;
	ctx.r10.s64 = ctx.r10.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// addi r9,r9,48
	ctx.r9.s64 = ctx.r9.s64 + 48;
	// bne 0x82200ba0
	if (!ctx.cr0.eq) goto loc_82200BA0;
	// b 0x82200b2c
	goto loc_82200B2C;
loc_82200BB4:
	// lwz r11,148(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 148);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82200bd8
	if (ctx.cr0.eq) goto loc_82200BD8;
	// addi r11,r11,-2
	ctx.r11.s64 = ctx.r11.s64 + -2;
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// bgt cr6,0x82200c00
	if (ctx.cr6.gt) goto loc_82200C00;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82201028
	ctx.lr = 0x82200BD4;
	retail_music_82201028(ctx, base);
	// b 0x82200c00
	goto loc_82200C00;
loc_82200BD8:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82200f20
	ctx.lr = 0x82200BE0;
	retail_music_82200F20(ctx, base);
	// b 0x82200c00
	goto loc_82200C00;
loc_82200BE4:
	// lwz r11,148(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 148);
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// bne cr6,0x82200c00
	if (!ctx.cr6.eq) goto loc_82200C00;
	// lwz r11,168(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 168);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// stw r11,164(r31)
	REX_STORE_U32(ctx.r31.u32 + 164, ctx.r11.u32);
	// bl 0x82200fa8
	ctx.lr = 0x82200C00;
	retail_music_82200FA8(ctx, base);
loc_82200C00:
	// addi r1,r1,128
	ctx.r1.s64 = ctx.r1.s64 + 128;
	// lfd f31,-48(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f31.u64 = REX_LOAD_U64(ctx.r1.u32 + -48);
	// b 0x82a987e8
	__restgprlr_28(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_82200C10) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lwz r11,156(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 156);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x82200d8c
	if (ctx.cr6.lt) goto loc_82200D8C;
	// beq cr6,0x82200d7c
	if (ctx.cr6.eq) goto loc_82200D7C;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// blt cr6,0x82200cf0
	if (ctx.cr6.lt) goto loc_82200CF0;
	// beq cr6,0x82200ca4
	if (ctx.cr6.eq) goto loc_82200CA4;
	// cmplwi cr6,r11,5
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 5, ctx.xer);
	// blt cr6,0x82200c60
	if (ctx.cr6.lt) goto loc_82200C60;
	// bne cr6,0x82200da4
	if (!ctx.cr6.eq) goto loc_82200DA4;
	// lwz r11,160(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);
	// cmpwi cr6,r11,5
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 5, ctx.xer);
	// bne cr6,0x82200da4
	if (!ctx.cr6.eq) goto loc_82200DA4;
	// li r9,0
	ctx.r9.s64 = 0;
	// b 0x82200d30
	goto loc_82200D30;
loc_82200C60:
	// lwz r11,160(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// bne cr6,0x82200da4
	if (!ctx.cr6.eq) goto loc_82200DA4;
	// lwz r11,164(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// lwz r11,24(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// addi r11,r11,-5
	ctx.r11.s64 = ctx.r11.s64 + -5;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm. r11,r11,27,31,31
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82200da4
	if (!ctx.cr0.eq) goto loc_82200DA4;
	// lwz r11,168(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 168);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r3,r11,r31
	ctx.r3.u64 = ctx.r11.u64 + ctx.r31.u64;
	// bl 0x82200638
	ctx.lr = 0x82200C9C;
	retail_music_82200638(ctx, base);
	// li r11,5
	ctx.r11.s64 = 5;
	// b 0x82200ce4
	goto loc_82200CE4;
loc_82200CA4:
	// lwz r11,160(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// bne cr6,0x82200da4
	if (!ctx.cr6.eq) goto loc_82200DA4;
	// lwz r11,164(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// lwz r11,24(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x82200cd4
	if (ctx.cr6.eq) goto loc_82200CD4;
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// li r11,0
	ctx.r11.s64 = 0;
	// bne cr6,0x82200cd8
	if (!ctx.cr6.eq) goto loc_82200CD8;
loc_82200CD4:
	// li r11,1
	ctx.r11.s64 = 1;
loc_82200CD8:
	// clrlwi. r11,r11,24
	ctx.r11.u64 = ctx.r11.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82200da4
	if (!ctx.cr0.eq) goto loc_82200DA4;
	// li r11,4
	ctx.r11.s64 = 4;
loc_82200CE4:
	// stw r11,156(r31)
	REX_STORE_U32(ctx.r31.u32 + 156, ctx.r11.u32);
	// stw r11,160(r31)
	REX_STORE_U32(ctx.r31.u32 + 160, ctx.r11.u32);
	// b 0x82200da4
	goto loc_82200DA4;
loc_82200CF0:
	// lwz r11,160(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// bne cr6,0x82200da4
	if (!ctx.cr6.eq) goto loc_82200DA4;
	// lwz r11,164(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// li r9,0
	ctx.r9.s64 = 0;
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// lwz r11,24(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x82200d24
	if (ctx.cr6.eq) goto loc_82200D24;
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// mr r11,r9
	ctx.r11.u64 = ctx.r9.u64;
	// bne cr6,0x82200d28
	if (!ctx.cr6.eq) goto loc_82200D28;
loc_82200D24:
	// li r11,1
	ctx.r11.s64 = 1;
loc_82200D28:
	// clrlwi. r11,r11,24
	ctx.r11.u64 = ctx.r11.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82200da4
	if (!ctx.cr0.eq) goto loc_82200DA4;
loc_82200D30:
	// lwz r10,168(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 168);
	// mulli r11,r10,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r10.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// lwz r11,24(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// beq cr6,0x82200d54
	if (ctx.cr6.eq) goto loc_82200D54;
	// cmpwi cr6,r11,4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 4, ctx.xer);
	// mr r11,r9
	ctx.r11.u64 = ctx.r9.u64;
	// bne cr6,0x82200d58
	if (!ctx.cr6.eq) goto loc_82200D58;
loc_82200D54:
	// li r11,1
	ctx.r11.s64 = 1;
loc_82200D58:
	// clrlwi. r11,r11,24
	ctx.r11.u64 = ctx.r11.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82200da4
	if (!ctx.cr0.eq) goto loc_82200DA4;
	// li r11,2
	ctx.r11.s64 = 2;
	// stw r9,156(r31)
	REX_STORE_U32(ctx.r31.u32 + 156, ctx.r9.u32);
	// stw r9,160(r31)
	REX_STORE_U32(ctx.r31.u32 + 160, ctx.r9.u32);
	// stw r10,164(r31)
	REX_STORE_U32(ctx.r31.u32 + 164, ctx.r10.u32);
	// stw r11,144(r31)
	REX_STORE_U32(ctx.r31.u32 + 144, ctx.r11.u32);
	// stw r11,148(r31)
	REX_STORE_U32(ctx.r31.u32 + 148, ctx.r11.u32);
	// b 0x82200da4
	goto loc_82200DA4;
loc_82200D7C:
	// lwz r11,160(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);
	// addi r11,r11,-1
	ctx.r11.s64 = ctx.r11.s64 + -1;
	// cmplwi cr6,r11,2
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2, ctx.xer);
	// b 0x82200d98
	goto loc_82200D98;
loc_82200D8C:
	// lwz r11,160(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);
	// addi r11,r11,-2
	ctx.r11.s64 = ctx.r11.s64 + -2;
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
loc_82200D98:
	// bgt cr6,0x82200da4
	if (ctx.cr6.gt) goto loc_82200DA4;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822010b0
	ctx.lr = 0x82200DA4;
	retail_music_822010B0(ctx, base);
loc_82200DA4:
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200DB8) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r30,-24(r1)
	REX_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// li r31,3
	ctx.r31.s64 = 3;
loc_82200DD4:
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x822003f8
	ctx.lr = 0x82200DDC;
	retail_music_822003F8(ctx, base);
	// addic. r31,r31,-1
	ctx.xer.ca = ctx.r31.u32 > 0;
	ctx.r31.s64 = ctx.r31.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r31.s32, 0, ctx.xer);
	// addi r30,r30,48
	ctx.r30.s64 = ctx.r30.s64 + 48;
	// bne 0x82200dd4
	if (!ctx.cr0.eq) goto loc_82200DD4;
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r30,-24(r1)
	ctx.r30.u64 = REX_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200E00) {
	REX_FUNC_PROLOGUE();
	// lwz r11,144(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 144);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82200e30
	if (ctx.cr0.eq) goto loc_82200E30;
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// lwz r11,164(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 164);
	// stw r4,168(r3)
	REX_STORE_U32(ctx.r3.u32 + 168, ctx.r4.u32);
	// cmpw cr6,r11,r4
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r4.s32, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// li r11,3
	ctx.r11.s64 = 3;
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// blr 
	return;
loc_82200E30:
	// stw r4,168(r3)
	REX_STORE_U32(ctx.r3.u32 + 168, ctx.r4.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200E38) {
	REX_FUNC_PROLOGUE();
	// lwz r11,144(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 144);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x82200e4c
	if (!ctx.cr6.eq) goto loc_82200E4C;
	// li r3,3
	ctx.r3.s64 = 3;
	// blr 
	return;
loc_82200E4C:
	// lwz r3,164(r3)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r3.u32 + 164);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200E58) {
	REX_FUNC_PROLOGUE();
	// li r11,2
	ctx.r11.s64 = 2;
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200E68) {
	REX_FUNC_PROLOGUE();
	// li r11,1
	ctx.r11.s64 = 1;
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200E78) {
	REX_FUNC_PROLOGUE();
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200E88) {
	REX_FUNC_PROLOGUE();
	// b 0x82200f20
	retail_music_82200F20(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_82200E90) {
	REX_FUNC_PROLOGUE();
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r4.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// addi r11,r3,48
	ctx.r11.s64 = ctx.r3.s64 + 48;
	// lwz r10,0(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplw cr6,r10,r4
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r4.u32, ctx.xer);
	// beq cr6,0x82200eb8
	if (ctx.cr6.eq) goto loc_82200EB8;
	// addi r11,r3,96
	ctx.r11.s64 = ctx.r3.s64 + 96;
	// lwz r10,0(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplw cr6,r10,r4
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r4.u32, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
loc_82200EB8:
	// lbz r10,44(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 44);
	// clrlwi. r10,r10,31
	ctx.r10.u64 = ctx.r10.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,45(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 45);
	// ori r10,r10,1
	ctx.r10.u64 = ctx.r10.u64 | 1;
	// stb r10,45(r11)
	REX_STORE_U8(ctx.r11.u32 + 45, ctx.r10.u8);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200ED8) {
	REX_FUNC_PROLOGUE();
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r4.u32, 0, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// addi r11,r3,48
	ctx.r11.s64 = ctx.r3.s64 + 48;
	// lwz r10,0(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplw cr6,r10,r4
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r4.u32, ctx.xer);
	// beq cr6,0x82200f00
	if (ctx.cr6.eq) goto loc_82200F00;
	// addi r11,r3,96
	ctx.r11.s64 = ctx.r3.s64 + 96;
	// lwz r10,0(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// cmplw cr6,r10,r4
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, ctx.r4.u32, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
loc_82200F00:
	// lbz r10,44(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 44);
	// rlwinm. r10,r10,0,30,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beqlr 
	if (ctx.cr0.eq) return;
	// lbz r10,45(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 45);
	// ori r10,r10,2
	ctx.r10.u64 = ctx.r10.u64 | 2;
	// stb r10,45(r11)
	REX_STORE_U8(ctx.r11.u32 + 45, ctx.r10.u8);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200F20) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82a9879c
	ctx.lr = 0x82200F28;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// li r30,3
	ctx.r30.s64 = 3;
	// mr r29,r31
	ctx.r29.u64 = ctx.r31.u64;
loc_82200F38:
	// mr r3,r29
	ctx.r3.u64 = ctx.r29.u64;
	// bl 0x82200550
	ctx.lr = 0x82200F40;
	retail_music_82200550(ctx, base);
	// addic. r30,r30,-1
	ctx.xer.ca = ctx.r30.u32 > 0;
	ctx.r30.s64 = ctx.r30.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r30.s32, 0, ctx.xer);
	// addi r29,r29,48
	ctx.r29.s64 = ctx.r29.s64 + 48;
	// bne 0x82200f38
	if (!ctx.cr0.eq) goto loc_82200F38;
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,144(r31)
	REX_STORE_U32(ctx.r31.u32 + 144, ctx.r11.u32);
	// stw r11,148(r31)
	REX_STORE_U32(ctx.r31.u32 + 148, ctx.r11.u32);
	// stw r11,156(r31)
	REX_STORE_U32(ctx.r31.u32 + 156, ctx.r11.u32);
	// stw r11,160(r31)
	REX_STORE_U32(ctx.r31.u32 + 160, ctx.r11.u32);
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82a987ec
	__restgprlr_29(ctx, base);
	return;
}

DEFINE_REX_FUNC(retail_music_82200F68) {
	REX_FUNC_PROLOGUE();
	// lwz r11,144(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 144);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 1, ctx.xer);
	// beqlr cr6
	if (ctx.cr6.eq) return;
	// addi r10,r3,28
	ctx.r10.s64 = ctx.r3.s64 + 28;
	// li r11,3
	ctx.r11.s64 = 3;
	// li r9,1
	ctx.r9.s64 = 1;
loc_82200F80:
	// stw r9,0(r10)
	REX_STORE_U32(ctx.r10.u32 + 0, ctx.r9.u32);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r11.u32 > 0;
	ctx.r11.s64 = ctx.r11.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// addi r10,r10,48
	ctx.r10.s64 = ctx.r10.s64 + 48;
	// bne 0x82200f80
	if (!ctx.cr0.eq) goto loc_82200F80;
	// lwz r11,144(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 144);
	// stw r9,144(r3)
	REX_STORE_U32(ctx.r3.u32 + 144, ctx.r9.u32);
	// stw r9,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r9.u32);
	// stw r11,152(r3)
	REX_STORE_U32(ctx.r3.u32 + 152, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200FA8) {
	REX_FUNC_PROLOGUE();
	// lwz r11,144(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 144);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x82200fcc
	if (ctx.cr6.lt) goto loc_82200FCC;
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// lwz r10,152(r3)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32 + 152);
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,152(r3)
	REX_STORE_U32(ctx.r3.u32 + 152, ctx.r11.u32);
	// stw r10,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r10.u32);
	// blr 
	return;
loc_82200FCC:
	// lwz r10,164(r3)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32 + 164);
	// li r11,2
	ctx.r11.s64 = 2;
	// mulli r10,r10,48
	ctx.r10.s64 = static_cast<int64_t>(ctx.r10.u64 * static_cast<uint64_t>(48));
	// add r10,r10,r3
	ctx.r10.u64 = ctx.r10.u64 + ctx.r3.u64;
	// stw r11,28(r10)
	REX_STORE_U32(ctx.r10.u32 + 28, ctx.r11.u32);
	// stw r11,144(r3)
	REX_STORE_U32(ctx.r3.u32 + 144, ctx.r11.u32);
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82200FF0) {
	REX_FUNC_PROLOGUE();
	// lwz r10,164(r3)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32 + 164);
	// li r11,3
	ctx.r11.s64 = 3;
	// li r9,0
	ctx.r9.s64 = 0;
	// mulli r10,r10,48
	ctx.r10.s64 = static_cast<int64_t>(ctx.r10.u64 * static_cast<uint64_t>(48));
	// stw r11,144(r3)
	REX_STORE_U32(ctx.r3.u32 + 144, ctx.r11.u32);
	// stw r11,148(r3)
	REX_STORE_U32(ctx.r3.u32 + 148, ctx.r11.u32);
	// stw r9,156(r3)
	REX_STORE_U32(ctx.r3.u32 + 156, ctx.r9.u32);
	// add r10,r10,r3
	ctx.r10.u64 = ctx.r10.u64 + ctx.r3.u64;
	// lwz r10,36(r10)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r10.u32 + 36);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// bne cr6,0x82201020
	if (!ctx.cr6.eq) goto loc_82201020;
	// li r11,2
	ctx.r11.s64 = 2;
loc_82201020:
	// stw r11,160(r3)
	REX_STORE_U32(ctx.r3.u32 + 160, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_82201028) {
	REX_FUNC_PROLOGUE();
	// lwz r11,152(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 152);
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// beq cr6,0x82201074
	if (ctx.cr6.eq) goto loc_82201074;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 3, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// addi r11,r3,32
	ctx.r11.s64 = ctx.r3.s64 + 32;
	// li r10,3
	ctx.r10.s64 = 3;
	// li r9,0
	ctx.r9.s64 = 0;
loc_82201048:
	// lwz r8,-8(r11)
	ctx.r8.u64 = REX_LOAD_U32(ctx.r11.u32 + -8);
	// cmpwi cr6,r8,1
	ctx.cr6.compare<int32_t>(ctx.r8.s32, 1, ctx.xer);
	// bne cr6,0x82201060
	if (!ctx.cr6.eq) goto loc_82201060;
	// lwz r8,0(r11)
	ctx.r8.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
	// stw r9,0(r11)
	REX_STORE_U32(ctx.r11.u32 + 0, ctx.r9.u32);
	// stw r8,-4(r11)
	REX_STORE_U32(ctx.r11.u32 + -4, ctx.r8.u32);
loc_82201060:
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r10.u32 > 0;
	ctx.r10.s64 = ctx.r10.s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// addi r11,r11,48
	ctx.r11.s64 = ctx.r11.s64 + 48;
	// bne 0x82201048
	if (!ctx.cr0.eq) goto loc_82201048;
	// li r11,3
	ctx.r11.s64 = 3;
	// b 0x822010a0
	goto loc_822010A0;
loc_82201074:
	// lwz r11,164(r3)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32 + 164);
	// li r9,0
	ctx.r9.s64 = 0;
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r3
	ctx.r11.u64 = ctx.r11.u64 + ctx.r3.u64;
	// lwz r10,24(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// cmpwi cr6,r10,1
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 1, ctx.xer);
	// bne cr6,0x8220109c
	if (!ctx.cr6.eq) goto loc_8220109C;
	// lwz r10,32(r11)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 32);
	// stw r9,32(r11)
	REX_STORE_U32(ctx.r11.u32 + 32, ctx.r9.u32);
	// stw r10,28(r11)
	REX_STORE_U32(ctx.r11.u32 + 28, ctx.r10.u32);
loc_8220109C:
	// li r11,2
	ctx.r11.s64 = 2;
loc_822010A0:
	// stw r9,152(r3)
	REX_STORE_U32(ctx.r3.u32 + 152, ctx.r9.u32);
	// stw r11,144(r3)
	REX_STORE_U32(ctx.r3.u32 + 144, ctx.r11.u32);
	// blr 
	return;
}

DEFINE_REX_FUNC(retail_music_822010B0) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r31,-16(r1)
	REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-96(r1)
	ea = -96 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lwz r11,164(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r3,r11,r31
	ctx.r3.u64 = ctx.r11.u64 + ctx.r31.u64;
	// bl 0x822005c0
	ctx.lr = 0x822010D4;
	retail_music_822005C0(ctx, base);
	// clrlwi. r11,r3,24
	ctx.r11.u64 = ctx.r3.u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82201148
	if (ctx.cr0.eq) goto loc_82201148;
	// lis r11,-32242
	ctx.r11.s64 = -2113011712;
	// lwz r9,164(r31)
	ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// li r10,4
	ctx.r10.s64 = 4;
	// lfs f0,3972(r11)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(0x820DFCA4u);
	ctx.f0.f64 = double(temp.f32);
	// mulli r11,r9,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r9.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// stw r10,24(r11)
	REX_STORE_U32(ctx.r11.u32 + 24, ctx.r10.u32);
	// stfs f0,16(r11)
	temp.f32 = float(ctx.f0.f64);
	REX_STORE_U32(ctx.r11.u32 + 16, temp.u32);
	// stw r10,28(r11)
	REX_STORE_U32(ctx.r11.u32 + 28, ctx.r10.u32);
	// lbz r10,45(r11)
	ctx.r10.u64 = REX_LOAD_U8(ctx.r11.u32 + 45);
	// andi. r10,r10,253
	ctx.r10.u64 = ctx.r10.u64 & 253;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// stb r10,45(r11)
	REX_STORE_U8(ctx.r11.u32 + 45, ctx.r10.u8);
	// lwz r11,164(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r11,r11,r31
	ctx.r11.u64 = ctx.r11.u64 + ctx.r31.u64;
	// lwz r11,36(r11)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 36);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x82201140
	if (!ctx.cr6.eq) goto loc_82201140;
	// lwz r11,168(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 168);
	// mulli r11,r11,48
	ctx.r11.s64 = static_cast<int64_t>(ctx.r11.u64 * static_cast<uint64_t>(48));
	// add r3,r11,r31
	ctx.r3.u64 = ctx.r11.u64 + ctx.r31.u64;
	// bl 0x82200638
	ctx.lr = 0x82201134;
	retail_music_82200638(ctx, base);
	// li r11,2
	ctx.r11.s64 = 2;
loc_82201138:
	// stw r11,160(r31)
	REX_STORE_U32(ctx.r31.u32 + 160, ctx.r11.u32);
	// b 0x8220114c
	goto loc_8220114C;
loc_82201140:
	// li r11,3
	ctx.r11.s64 = 3;
	// b 0x82201138
	goto loc_82201138;
loc_82201148:
	// li r11,1
	ctx.r11.s64 = 1;
loc_8220114C:
	// stw r11,156(r31)
	REX_STORE_U32(ctx.r31.u32 + 156, ctx.r11.u32);
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
	// blr 
	return;
}
