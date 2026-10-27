// Preserve footstep owner even when the native sound call uses a temporary
// position and a null actor (the local first-person path).
thread_local uint32_t native_footstep_actor=0;
REX_EXTERN(__imp__sub_821FF7C0);
REX_HOOK_RAW(sub_821FF7C0){
  const auto previous=native_footstep_actor;
  native_footstep_actor=ctx.r3.u32;
  __imp__sub_821FF7C0(ctx,base);
  native_footstep_actor=previous;
}
