#include "generated/default/pdzstocktu3_init.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <string>
#include "utils.h"
#include "mousehook.h"
#include "klobb_weapon.h"
#include "surface_runtime.h"
namespace {
std::string ResourceName(uint8_t* base,uint32_t address) {
  std::string result;
  auto* runtime=rex::Runtime::instance();
  for(unsigned i=0;address&&i<260;++i) {
    auto* heap=runtime->memory()->LookupHeap(address+i);uint32_t flags=0;
    if(!heap||!heap->QueryProtect(address+i,&flags)||!(flags&rex::memory::kMemoryProtectRead))break;
    char c=char(base[address+i]);if(!c)break;result+=c;
  }
  return result;
}
}
// TU3 disables loose-file discovery for packaged multiplayer resources. Allow
// only the character assets already installed for the retained appearance mod;
// leave the native cache lookup, loader, reference counting and teardown intact.
bool Tu3AllowsLooseCharacter(uint8_t* base,uint32_t name) {
  if(!pdz_input::Supported())return false;
  const auto asset=ResourceName(base,name);
  // Surface's Klobb uses CMP150's native model/animation package, which this
  // mission doesn't normally request. Keep the exception restricted to that
  // exact package and Surface; the native loader still validates the file.
  if(asset=="aid_darkpackage_gun_GunType_CMP" && pdz_surface::Selected(base)) {
    REXLOG_INFO("TU3 Surface CMP150 donor package allowed");
    return true;
  }
  // Surface's self-contained barrel bundle isn't in the retail TU3 registry.
  // Permit only this installed package while Surface is selected; native
  // loading, resource ownership and teardown still handle its dependencies.
  if(asset=="aid_darkpackage_surface_barrels" && pdz_surface::Selected(base)) {
    std::error_code error;
    const auto package=utils::RepoRoot()/"assets/pdz/ASSETS/PackageData/832751055.pkg";
    if(std::filesystem::is_regular_file(package,error) && !error &&
       std::filesystem::file_size(package,error)==702942 && !error) {
      REXLOG_INFO("TU3 installed Surface barrel package allowed");
      return true;
    }
    return false;
  }
  if(!asset.starts_with("aid_darkcharacter_")||asset.size()>=192||
     asset.find("..")!=std::string::npos||asset.back()=='.')return false;
  for(const unsigned char c:asset)
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'))return false;
  const auto session=REX_LOAD_U32(0x82D6BFD8);
  if(!session)return false;
  const auto mode=REX_LOAD_U32(session+40);
  const bool allowed=mode==2||mode==3||(mode<=1&&REX_LOAD_U8(session+25)==1)||
    (pdz_klobb::LoadingModel()&&asset==pdz_klobb::kModelName);
  if(!allowed)return false;
  std::error_code error;
  const auto path=utils::RepoRoot()/"assets"/"pdz"/asset/"default.dbm";
  if(!std::filesystem::is_regular_file(path,error)||error)return false;
  const auto size=std::filesystem::file_size(path,error);
  if(error||size<=384)return false;
  REXLOG_INFO("TU3 installed character resource allowed: {}",asset);
  return true;
}
REX_EXTERN(__imp__sub_82414070);
REX_HOOK_RAW(sub_82414070) {
  const auto definition=ctx.r3.u32;
  REXLOG_INFO("TU3 character resource initialization: definition={:08X} id={} body={} ragdoll={} hands={}",
    definition,REX_LOAD_U32(definition),ResourceName(base,REX_LOAD_U32(definition+4)),
    ResourceName(base,REX_LOAD_U32(definition+32)),ResourceName(base,REX_LOAD_U32(definition+16)));
  __imp__sub_82414070(ctx,base);
}
REX_EXTERN(__imp__sub_82413FD8);
REX_HOOK_RAW(sub_82413FD8) {
  const auto requested=ResourceName(base,ctx.r3.u32),fallback=ResourceName(base,ctx.r4.u32);
  __imp__sub_82413FD8(ctx,base);
  if(!ctx.r3.u32&&requested!="_"&&requested!="_noname_"&&!requested.empty())
    REXLOG_WARN("TU3 character resource missing: requested={} fallback={}",requested,fallback);
}
