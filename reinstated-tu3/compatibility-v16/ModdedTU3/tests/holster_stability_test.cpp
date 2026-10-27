#include "../src/holster_logic.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace pdz_holsters;
  std::array<Weapon, 7> inventory{{
      {1,101,201,false,true,1}, {2,102,202,false,false,2},
      {3,103,203,false,false,3}, {4,104,204,true,false,4},
      {5,105,205,true,false,5}, {6,106,206,true,false,6},
      {7,107,207,true,false,26}}};
  auto layout=SelectStable(inventory,{});
  assert(layout[2].object==2 && layout[3].object==3 && layout[4].object==7);
  // Reproduce the crash-time reshuffle: one pistol remains stored while
  // another is equipped. The remaining pistol must not migrate/reallocate.
  for (int i=0;i<1000;++i) {
    inventory[0].equipped=(i%2)==0;
    inventory[1].equipped=!inventory[0].equipped;
    auto next=SelectStable(inventory,layout);
    assert(next[3].object==3 && next[3].model==103);
    assert(next[2].object==(inventory[0].equipped?2u:1u));
    assert(next[4].object==7);
    assert(next[0].object==4 && next[1].object==5);
    layout=next;
  }
  // A dropped/invalid weapon retires; another eligible weapon fills its slot.
  inventory[3].resource=0;
  layout=SelectStable(inventory,layout);
  assert(layout[0].object==6 && layout[1].object==5);
  // Dual-wielded pistols never produce cosmetic copies.
  inventory[0].equipped=inventory[1].equipped=true;
  layout=SelectStable(inventory,layout);
  assert(!layout[2].object && layout[3].object==3);
  // Shield remains centre-back; a changed model uses the new descriptor.
  inventory[6].model=999;
  layout=SelectStable(inventory,layout);
  assert(layout[4].object==7 && layout[4].model==999);
  inventory[6].equipped=true;
  layout=SelectStable(inventory,layout);
  assert(!layout[4].object);
  for (const auto& slot:SelectStable({},layout)) assert(!slot.object);
  struct Resident { uint32_t object{},owner{},weapon{},model{}; };
  std::array<Resident,13> cache{};
  unsigned allocations=0;
  auto live=[](const Resident& r) { return r.object!=0; };
  // Repeated equips, pickups and drops reuse the same resident graphs rather
  // than freeing an object while the renderer can still hold its old frame.
  for(unsigned iteration=0;iteration<10000;++iteration) {
    const auto weapon=1u+iteration%13;
    Weapon desired{weapon,100+weapon,200+weapon,false,false,weapon};
    const auto slot=ResidentSlot(cache,77,desired,live);
    assert(slot<cache.size());
    if(!cache[slot].object)cache[slot]={++allocations,77,weapon,100+weapon};
    assert(cache[slot].weapon==weapon);
  }
  assert(allocations==13);
  const auto old=cache;
  Weapon extra{999,999,999,false,false,1};
  assert(ResidentSlot(cache,77,extra,live)==cache.size());
  // An owner/descriptor replacement cannot reuse another graph's identity.
  Weapon first{1,101,201,false,false,1};
  assert(ResidentSlot(cache,88,first,live)==cache.size());
  first.model=999;
  assert(ResidentSlot(cache,77,first,live)==cache.size());
  for(size_t i=0;i<cache.size();++i) assert(cache[i].object==old[i].object);
  // Only the native destruction notification makes a slot available again.
  cache[4]={};
  assert(ResidentSlot(cache,77,extra,live)==4);
  std::cout<<"Holster switching, dual-wield, drop and shield tests passed\n";
}
