#include "generated/default/pdzstocktu3_init.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <atomic>

// Native TU3 sampler with one missing-channel check at guest 0x8271F84C.
REX_HOOK_RAW(sub_8271F7B8) {
	REX_FUNC_PROLOGUE();
	PPCRegister temp{};
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82aa84f4
	ctx.lr = 0x8271F7C0;
	__savegprlr_27(ctx, base);
	// stfd f31,-56(r1)
	ctx.fpscr.disableFlushMode();
	REX_STORE_U64(ctx.r1.u32 + -56, ctx.f31.u64);
	// stwu r1,-192(r1)
	ea = -192 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// li r11,0
	ctx.r11.s64 = 0;
	// addi r30,r31,4
	ctx.r30.s64 = ctx.r31.s64 + 4;
	// addi r5,r1,100
	ctx.r5.s64 = ctx.r1.s64 + 100;
	// addi r4,r1,96
	ctx.r4.s64 = ctx.r1.s64 + 96;
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// lwz r28,116(r31)
	ctx.r28.u64 = REX_LOAD_U32(ctx.r31.u32 + 116);
	// stw r11,100(r1)
	REX_STORE_U32(ctx.r1.u32 + 100, ctx.r11.u32);
	// lhz r11,42(r30)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r30.u32 + 42);
	// lwz r10,112(r30)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r30.u32 + 112);
	// rotlwi r9,r11,1
	ctx.r9.u64 = __builtin_rotateleft32(ctx.r11.u32, 1);
	// add r9,r11,r9
	ctx.r9.u64 = ctx.r11.u64 + ctx.r9.u64;
	// rlwinm r11,r9,4,0,27
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r9.u32 | (ctx.r9.u64 << 32), 4) & 0xFFFFFFF0;
	// add r27,r11,r10
	ctx.r27.u64 = ctx.r11.u64 + ctx.r10.u64;
	// bl 0x82746a48
	ctx.lr = 0x8271F804;
	sub_82746A48(ctx, base);
	// li r6,0
	ctx.r6.s64 = 0;
	// lwz r29,164(r31)
	ctx.r29.u64 = REX_LOAD_U32(ctx.r31.u32 + 164);
	// addi r5,r1,112
	ctx.r5.s64 = ctx.r1.s64 + 112;
	// addi r4,r1,104
	ctx.r4.s64 = ctx.r1.s64 + 104;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82720d90
	ctx.lr = 0x8271F81C;
	sub_82720D90(ctx, base);
	// lwz r10,0(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// lbz r11,26(r10)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r10.u32 + 26);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1, ctx.xer);
	// blt cr6,0x8271f838
	if (ctx.cr6.lt) goto loc_8271F838;
	// beq cr6,0x8271f904
	if (ctx.cr6.eq) goto loc_8271F904;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3, ctx.xer);
	// blt cr6,0x8271f90c
	if (ctx.cr6.lt) goto loc_8271F90C;
loc_8271F838:
	// li r11,1
	ctx.r11.s64 = 1;
loc_8271F83C:
	// lis r9,-32028
	ctx.r9.s64 = -2098987008;
	// lfs f31,104(r1)
	ctx.fpscr.disableFlushMode();
	temp.u32 = REX_LOAD_U32(ctx.r1.u32 + 104);
	ctx.f31.f64 = double(temp.f32);
	// stw r11,9036(r9)
	REX_STORE_U32(ctx.r9.u32 + 9036, ctx.r11.u32);
	// lwz r11,36(r10)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r10.u32 + 36);
	// A missing primary channel is equivalent to its empty data pointer.
	// Crash 12100: descriptor+36 was null, and native code read guest 0x18.
	// Keep the secondary channel and buffer-unlock tail; do not return early
	// or alter the shared animation descriptor used by other model instances.
	if (!ctx.r11.u32) {
		static std::atomic<unsigned> reports{0};
		if (reports.fetch_add(1, std::memory_order_relaxed) < 16)
			REXLOG_WARN("Animation primary channel absent: state={:08X} definition={:08X}", ctx.r31.u32, ctx.r10.u32);
		goto loc_8271F8AC;
	}
	// lwz r3,24(r11)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r11.u32 + 24);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x8271f8ac
	if (ctx.cr6.eq) goto loc_8271F8AC;
	// lis r10,-32046
	ctx.r10.s64 = -2100166656;
	// lhz r11,22(r11)
	ctx.r11.u64 = REX_LOAD_U16(ctx.r11.u32 + 22);
	// addi r10,r10,-5872
	ctx.r10.s64 = ctx.r10.s64 + -5872;
	// addi r8,r10,24
	ctx.r8.s64 = ctx.r10.s64 + 24;
	// rotlwi r10,r11,2
	ctx.r10.u64 = __builtin_rotateleft32(ctx.r11.u32, 2);
	// add r7,r11,r10
	ctx.r7.u64 = ctx.r11.u64 + ctx.r10.u64;
	// rlwinm r6,r7,3,0,28
	ctx.r6.u64 = __builtin_rotateleft64(ctx.r7.u32 | (ctx.r7.u64 << 32), 3) & 0xFFFFFFF8;
	// lwzx r11,r6,r8
	ctx.r11.u64 = REX_LOAD_U32(ctx.r6.u32 + ctx.r8.u32);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x8271f8ac
	if (ctx.cr6.eq) goto loc_8271F8AC;
	// lwz r4,220(r31)
	ctx.r4.u64 = REX_LOAD_U32(ctx.r31.u32 + 220);
	// mr r8,r29
	ctx.r8.u64 = ctx.r29.u64;
	// lhz r10,216(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 216);
	// fmr f1,f31
	ctx.f1.f64 = ctx.f31.f64;
	// lhz r9,96(r1)
	ctx.r9.u64 = REX_LOAD_U16(ctx.r1.u32 + 96);
	// addi r5,r1,112
	ctx.r5.s64 = ctx.r1.s64 + 112;
	// lwz r6,100(r1)
	ctx.r6.u64 = REX_LOAD_U32(ctx.r1.u32 + 100);
	// stw r4,84(r1)
	REX_STORE_U32(ctx.r1.u32 + 84, ctx.r4.u32);
	// mr r4,r28
	ctx.r4.u64 = ctx.r28.u64;
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl 
	ctx.lr = 0x8271F8AC;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
loc_8271F8AC:
	// lwz r3,0(r31)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// lwz r3,68(r3)
	ctx.r3.u64 = REX_LOAD_U32(ctx.r3.u32 + 68);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x8271f8e4
	if (ctx.cr6.eq) goto loc_8271F8E4;
	// lwz r11,224(r31)
	ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 224);
	// li r9,1
	ctx.r9.s64 = 1;
	// mr r8,r29
	ctx.r8.u64 = ctx.r29.u64;
	// lhz r10,208(r31)
	ctx.r10.u64 = REX_LOAD_U16(ctx.r31.u32 + 208);
	// li r6,0
	ctx.r6.s64 = 0;
	// fmr f1,f31
	ctx.fpscr.disableFlushMode();
	ctx.f1.f64 = ctx.f31.f64;
	// addi r5,r1,112
	ctx.r5.s64 = ctx.r1.s64 + 112;
	// mr r4,r27
	ctx.r4.u64 = ctx.r27.u64;
	// stw r11,84(r1)
	REX_STORE_U32(ctx.r1.u32 + 84, ctx.r11.u32);
	// bl 0x8271fae0
	ctx.lr = 0x8271F8E4;
	sub_8271FAE0(ctx, base);
loc_8271F8E4:
	// lwz r10,172(r31)
	ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + 172);
	// cmpwi cr6,r10,1
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 1, ctx.xer);
	// bne cr6,0x8271f8f8
	if (!ctx.cr6.eq) goto loc_8271F8F8;
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x82746ac8
	ctx.lr = 0x8271F8F8;
	sub_82746AC8(ctx, base);
loc_8271F8F8:
	// addi r1,r1,192
	ctx.r1.s64 = ctx.r1.s64 + 192;
	// lfd f31,-56(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f31.u64 = REX_LOAD_U64(ctx.r1.u32 + -56);
	// b 0x82aa8544
	__restgprlr_27(ctx, base);
	return;
loc_8271F904:
	// li r11,0
	ctx.r11.s64 = 0;
	// b 0x8271f83c
	goto loc_8271F83C;
loc_8271F90C:
	// li r11,2
	ctx.r11.s64 = 2;
	// b 0x8271f83c
	goto loc_8271F83C;
}
