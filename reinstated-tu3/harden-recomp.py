from pathlib import Path
import shutil
root=Path('compatibility-v13/PerfectDarkZeroRecomp/src')
backup=Path('crash-investigation/preventive-backup-2026-10-07')
backup.mkdir(exist_ok=True)
for name in ['visible_holsters.cpp','campaign_gore.cpp']:
    target=root/name
    if not (backup/name).exists(): shutil.copy2(target,backup/name)

p=root/'visible_holsters.cpp'
s=p.read_text()
old='  auto call=ctx;call.r1.u32-=2048;'
new='''  auto call=ctx;
  if (call.r1.u32 < 2048 || !Readable(call.r1.u32 - 2048, 2048)) {
    intro_pistol_attempted=false;
    return;
  }
  call.r1.u32-=2048;'''
assert s.count(old)==1;s=s.replace(old,new)
old='  const auto prop_model=REX_LOAD_U32(object+72);'
new='''  if (!Valid(base,intro_pistol)) { intro_pistol={}; return; }
  const auto prop_model=REX_LOAD_U32(object+72);
  if (!Readable(prop_model,512)) { Remove(ctx,base,intro_pistol); return; }'''
assert s.count(old)==1;s=s.replace(old,new)
old='  const auto entry=0x82D72B00u+((model-0x82E735D0)/512)*8;'
new='''  // Validate the native scenegraph pool before deriving its visibility entry.
  if(model<0x82E735D0u || (model-0x82E735D0u)%512u ||
     (model-0x82E735D0u)/512u>=2500u || !Readable(model,512))return;
  const auto entry=0x82D72B00u+((model-0x82E735D0u)/512u)*8u;
  if(!Readable(entry,8))return;'''
assert s.count(old)==3;s=s.replace(old,new)
# Constructor failure must not leave a stale attachment or read its null model.
old='  attachments[index] = {object, player, weapon.object, weapon.model};'
new=old+'''
  if (!Valid(base,attachments[index])) { attachments[index]={}; return; }
  if (!Readable(REX_LOAD_U32(object+72),512)) {
    Remove(ctx,base,attachments[index]);
    return;
  }'''
assert s.count(old)==1;s=s.replace(old,new)
p.write_text(s)
p=root/'campaign_gore.cpp';s=p.read_text()
old='''        auto team=ctx;team.r1.u32-=1024;team.r3.u64=owner;team.r4.u64=shooter;sub_825104D8(team,base);
        same_team=team.r3.u8!=0;
        if(!team.r3.u8){head=REX_LOAD_U32(owner+(corpse?1008:4104));body=REX_LOAD_U32(owner+72);}'''
new='''        auto team=ctx;
        // Preserve the native hit even if there is no safe scratch frame for
        // the optional gore team query. Never wrap the guest stack pointer.
        same_team=true;
        if(team.r1.u32>=1024 && Readable(team.r1.u32-1024,1024)){
          team.r1.u32-=1024;team.r3.u64=owner;team.r4.u64=shooter;sub_825104D8(team,base);
          same_team=team.r3.u8!=0;
          if(!same_team){head=REX_LOAD_U32(owner+(corpse?1008:4104));body=REX_LOAD_U32(owner+72);}
        }'''
assert s.count(old)==1;s=s.replace(old,new);p.write_text(s)
print('Applied bounded cinematic attachment and gore scratch-frame checks.')
