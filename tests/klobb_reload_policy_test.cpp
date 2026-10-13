#include "../src/klobb_reload_policy.h"
#include <cassert>
#include <iostream>
int main(){
  using namespace pdz_klobb;
  for(unsigned id=0;id<64;++id){
    if(id==39)continue;
    for(auto cue:{"cmp_clipin","cmp_clipout","cmp_cliphand","cmp_chamber",
                  "XACT_SOUNDBANK_GUNS_CMP_CLIPIN"})
      assert(KlobbReloadCue(id,6,cue)==ReloadCue::Native);
  }
  assert(KlobbReloadCue(39,6,"XACT_SOUNDBANK_GUNS_CMP_CLIPIN")==ReloadCue::Original);
  assert(KlobbReloadCue(39,6,"cmp_clipin")==ReloadCue::Original);
  for(auto cue:{"CMP_CLIPOUT","cmp_cliphand","XACT_SOUNDBANK_GUNS_CMP_CHAMBER"})
    assert(KlobbReloadCue(39,6,cue)==ReloadCue::Suppress);
  for(auto cue:{"cmp_gshot","cmp_holo_close","mp5_clipin","cmp_clipin_extra",
                "unrelated_cmp_clipin","","XACT_SOUNDBANK_VOICES_CMP_CLIPIN"})
    assert(KlobbReloadCue(39,6,cue)==ReloadCue::Native);
  assert(KlobbReloadCue(39,6,"XACT_SOUNDBANK_GUNS_CMP_RAISECHAMBER")==ReloadCue::Original);
  assert(KlobbReloadCue(39,16,"cmp_raisechamber")==ReloadCue::Original);
  for(unsigned animation=0;animation<28;++animation){
    if(animation==6||animation==16)continue;
    assert(KlobbReloadCue(39,animation,"cmp_raisechamber")==ReloadCue::Native);
    assert(KlobbReloadCue(39,animation,"cmp_clipin")==ReloadCue::Native);
  }
  std::cout<<"Klobb reload cue isolation checks passed\n";
}

