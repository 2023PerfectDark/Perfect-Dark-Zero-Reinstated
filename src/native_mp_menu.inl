// Included beside the common native XUI helpers in native_campaign.cpp.
struct MpMenu {
 uint32_t scene=0,armor=0,character=0,assignment=0,returnFocus=0;
 std::array<uint32_t,4> team{};
 std::array<uint32_t,7> bot{};
 unsigned part=0;std::vector<unsigned> entries;
 std::array<uint32_t,15> list{};
 std::vector<std::pair<uint32_t,uint32_t>> hidden;
 unsigned page=0,target=0;
 int filter=-1;
 std::vector<pdz_mp::Slot> slots;
 bool open=false;
 int displayedArmor=-1,displayedChoice=-1;
 std::array<unsigned,2> displayedTeam{99,99};
 int64_t lastUpdate=0;
};
MpMenu mpMenu;
std::atomic<bool> mpBack{false},mpOpen{false};
std::atomic<int64_t> mpLastSeen{0};
std::atomic<bool> mpCanChoose{false},mpIsOpen{false};
int64_t MpNow(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool MpVisible(uint32_t e,uint8_t* base){for(unsigned n=0;e&&n<32;++n){if(!Valid(e,240)||!(REX_LOAD_U32(e+236)&1))return false;e=REX_LOAD_U32(e+40);}return !e;}
uint32_t MpButton(PPCContext& c,uint8_t* base,uint32_t parent,std::u16string_view id,std::u16string_view text,float x,float y,float w,float h,std::u16string_view visual=u"DarkButton"){
 if(auto existing=Child(c,parent,id,base))return existing;
 const auto out=c.r1.u32+80;REX_STORE_U32(out,0);c.r3.u64=0x820ACFFC;c.r4.u64=out;sub_82776858(c,base);
 if(c.r3.s32<0)return 0;auto b=REX_LOAD_U32(out);if(!b)return 0;
 SetText(c,b,id,base,0x820AB378);SetText(c,b,text,base);
 c.r3.u64=parent;c.r4.u64=b;sub_8277A658(c,base);
 if(c.r3.s32<0){c.r3.u64=b;sub_827768C8(c,base);return 0;}
 c.r3.u64=c.r1.u32+96;c.r4.u64=41;sub_82789C88(c,base);c.r3.u64=b;c.r4.u64=c.r1.u32+96;sub_82776920(c,base);
 WriteText(c.r1.u32+256,visual,base);c.r3.u64=b;c.r4.u64=REX_LOAD_U32(0x82D356F8);sub_8276C050(c,base);
 auto control=c.r3.u32;REX_STORE_U32(c.r1.u32+112,5);REX_STORE_U32(c.r1.u32+116,c.r1.u32+256);
 c.r3.u64=0;c.r4.u64=control;c.r5.u64=c.r1.u32+112;sub_8278D7C0(c,base);
 c.r3.u64=b;c.f1.f64=w;c.f2.f64=h;sub_8277E5F0(c,base);
 auto v=c.r1.u32+96;REX_STORE_U32(v,std::bit_cast<uint32_t>(x));REX_STORE_U32(v+4,std::bit_cast<uint32_t>(y));REX_STORE_U32(v+8,0);
 c.r3.u64=b;c.r4.u64=v;sub_8277AA88(c,base);return b;
}
void MpShow(PPCContext& c,uint8_t* base,uint32_t h,bool visible){if(!h)return;c.r3.u64=h;c.r4.u64=visible;sub_8278ABF8(c,base);c.r3.u64=h;c.r4.u64=visible;sub_8277A5B8(c,base);}
std::u16string MpNumber(unsigned n){auto s=std::to_string(n);return {s.begin(),s.end()};}
std::u16string MpRowId(unsigned n){return u"MpCharacterRow"+MpNumber(n);}
unsigned MpDisplayChoice(unsigned item){
 // Display Joanna's new outfit beside her other outfits without renumbering
 // the persistent character IDs used by player and bot selections.
 const auto& catalog=pdz_mp::Characters();
 unsigned dress=0;
 for(unsigned i=0;i<catalog.size();++i)if(std::u16string_view(catalog[i].name)==u"Joanna - Short Dress"){dress=i+1;break;}
 if(dress>5){if(item==5)return dress;if(item>5&&item<=dress)return item-1;}
 return item;
}
void MpListLabels(PPCContext& c,uint8_t* base){
 if(!mpMenu.open||mpMenu.slots.empty())return;
 const auto& target=mpMenu.slots[0];
 SetText(c,mpMenu.list[0],u"CUSTOMIZE: "+target.name,base);
 c.r3.u64=mpMenu.list[0];c.r4.u64=0;sub_8277A5B8(c,base);
 const bool heads=mpMenu.part==1;
 mpMenu.entries=mpMenu.part?pdz_mp::PartChoices(heads):std::vector<unsigned>{};
 if(mpMenu.part)mpMenu.entries.insert(mpMenu.entries.begin(),0);
 const unsigned total=mpMenu.part?unsigned(mpMenu.entries.size()):2;
 if(mpMenu.page*10>=total)mpMenu.page=0;
 const auto choice=heads?pdz_mp::HeadChoice(target.index):pdz_mp::Choice(target.index);
 const unsigned rows=std::min(10u,total-mpMenu.page*10);
 for(unsigned i=0;i<10;++i){MpShow(c,base,mpMenu.list[i+1],i<rows);if(i>=rows)continue;
  std::u16string label;
  if(!mpMenu.part)label=i?u"BODY CUSTOMIZE":u"HEAD CUSTOMIZE";
  else {const auto entry=mpMenu.entries[mpMenu.page*10+i];label=entry?pdz_mp::Characters()[entry-1].name:u"Native / default";if(entry==choice)label=u"> "+label;}
  SetText(c,mpMenu.list[i+1],label,base);
  UpdateNavigation(c,base,mpMenu.list[i+1],28,MpRowId(i?i:13));
  UpdateNavigation(c,base,mpMenu.list[i+1],32,MpRowId(i+1<rows?i+2:(mpMenu.part?11:13)));
 }
 MpShow(c,base,mpMenu.list[11],mpMenu.part!=0);MpShow(c,base,mpMenu.list[12],mpMenu.part!=0);
 UpdateNavigation(c,base,mpMenu.list[11],28,MpRowId(rows));
 UpdateNavigation(c,base,mpMenu.list[13],32,MpRowId(1));
 UpdateNavigation(c,base,mpMenu.list[13],28,MpRowId(mpMenu.part?12:rows));
 SetText(c,mpMenu.list[13],mpMenu.part?u"BACK":u"DONE",base);
 SetText(c,mpMenu.list[14],mpMenu.part?(heads?u"HEAD CUSTOMIZE":u"BODY CUSTOMIZE"):u"CUSTOMIZE CHARACTER",base);
}
void MpClose(PPCContext& c,uint8_t* base){
 for(auto h:mpMenu.list)MpShow(c,base,h,false);
 for(auto [h,flags]:mpMenu.hidden){auto e=Element(c,h,base);if(!Valid(e,240))continue;c.r3.u64=h;c.r4.u64=(flags&1)!=0;sub_8278ABF8(c,base);c.r3.u64=h;c.r4.u64=(flags&4)==0;sub_8277A5B8(c,base);}
 mpMenu.hidden.clear();mpMenu.open=false;mpIsOpen=false;
 c.r3.u64=mpMenu.returnFocus;c.r4.u64=255;sub_8277F5C8(c,base);
}
void MpOpenList(PPCContext& c,uint8_t* base,int filter=-1,unsigned slot=0){
 if(mpMenu.open)return;auto e=Element(c,mpMenu.scene,base);if(!Valid(e,240))return;
 mpMenu.slots={{slot,0,slot?u"Bot "+MpNumber(slot):u"Player 1"}};mpMenu.filter=-1;mpMenu.part=0;
 if(mpMenu.slots.empty())mpMenu.slots.push_back({0,unsigned(std::max(filter,0)),u"Player 1"});
 c.r3.u64=255;sub_82779D88(c,base);mpMenu.returnFocus=c.r3.u32;
 if(!mpMenu.list[0]){
  mpMenu.list[0]=MpButton(c,base,mpMenu.scene,MpRowId(0),u"PLAYER",64,190,565,30,u"DarkXuiBotKick");
  for(unsigned i=0;i<10;++i)mpMenu.list[i+1]=MpButton(c,base,mpMenu.scene,MpRowId(i+1),u"",64,235.f+i*30.f,565,30,u"DarkXuiBotKick");
  mpMenu.list[11]=MpButton(c,base,mpMenu.scene,MpRowId(11),u"PREVIOUS",64,550,256,36);
  mpMenu.list[12]=MpButton(c,base,mpMenu.scene,MpRowId(12),u"NEXT",354,550,256,36);
  mpMenu.list[13]=MpButton(c,base,mpMenu.scene,MpRowId(13),u"DONE",644,550,256,36);
  mpMenu.list[14]=MpButton(c,base,mpMenu.scene,MpRowId(14),u"CHARACTERS",64,150,565,30,u"DarkXuiBotKick");
  for(unsigned i=0;i<14;++i){UpdateNavigation(c,base,mpMenu.list[i],28,MpRowId((i+13)%14));UpdateNavigation(c,base,mpMenu.list[i],32,MpRowId((i+1)%14));}
 }
 // Hide the authored rows while the native list is open. Keep the scene itself
 // alive so XUI owns focus, mouse hit tests, and controller selection.
 for(auto p=REX_LOAD_U32(e+24),n=0u;p&&n<256&&Valid(p,240);p=REX_LOAD_U32(p+32),++n){
  auto h=REX_LOAD_U32(p);c.r3.u64=h;sub_8276C0F8(c,base);h=c.r3.u32;
  if(std::find(mpMenu.list.begin(),mpMenu.list.end(),h)!=mpMenu.list.end())continue;
  auto flags=REX_LOAD_U32(p+236);mpMenu.hidden.push_back({h,flags});MpShow(c,base,h,false);
 }
 mpMenu.target=0;mpMenu.page=0;mpMenu.open=true;mpIsOpen=true;
 for(auto h:mpMenu.list)MpShow(c,base,h,true);
 c.r3.u64=mpMenu.list[14];c.r4.u64=0;sub_8277A5B8(c,base);
 MpListLabels(c,base);c.r3.u64=mpMenu.list[1];c.r4.u64=255;sub_8277F5C8(c,base);
}
bool MpPress(PPCContext& c,uint8_t* base,uint32_t h,std::u16string_view id){
 if(!mpMenu.scene||MpNow()-mpLastSeen.load()>250||!MpVisible(Element(c,mpMenu.scene,base),base))return false;
 if(mpMenu.open){
  if(id==u"buttonBack"||id==u"buttonBack1"){if(mpMenu.part){mpMenu.part=0;mpMenu.page=0;MpListLabels(c,base);}else MpClose(c,base);return true;}
  for(unsigned i=0;i<mpMenu.list.size();++i)if(h==mpMenu.list[i]){
   if(i==0)return true;
   else if(i<=10){
    if(!mpMenu.part){if(i<=2){mpMenu.part=i;mpMenu.page=0;}}
    else {const auto row=mpMenu.page*10+i-1;if(row<mpMenu.entries.size())pdz_mp::ChoosePart(mpMenu.slots[0].index,mpMenu.entries[row],mpMenu.part==1);}
   }
   else if(i==11){const unsigned pages=unsigned((mpMenu.entries.size()+9)/10);mpMenu.page=mpMenu.page?mpMenu.page-1:pages-1;}
   else if(i==12)mpMenu.page=(mpMenu.page+1)%unsigned((mpMenu.entries.size()+9)/10);
   else if(i==13){if(mpMenu.part){mpMenu.part=0;mpMenu.page=0;}else {MpClose(c,base);return true;}}
   MpListLabels(c,base);return true;
  }
  return true; // Do not activate an obscured Create/Players shortcut.
 }
 for(unsigned i=0;i<mpMenu.bot.size();++i)if(h&&h==mpMenu.bot[i]){
  const auto selected=REX_LOAD_U32(0x82D27500);if(selected>=31)return true;
  if(i==6)MpOpenList(c,base,-1,selected+1);
  else pdz_mp::RandomPart(selected+1,i>=3,(i%3==2)?0:(i%3+1));
  return true;
 }
 if(id==u"MpChooseCharacter"){MpOpenList(c,base);return true;}
 if(id==u"MpVisibleArmor"){pdz_mp::SetArmorVisible(!pdz_mp::ArmorVisible());return true;}
 for(unsigned i=0;i<4;++i)if(h&&h==mpMenu.team[i]){
  unsigned team=i/2;pdz_mp::SetTeamMode(team,i%2?2:(pdz_mp::TeamMode(team)==1?0:1));
  if(i%2)MpOpenList(c,base,int(team));return true;
 }
 return false;
}
bool MpKey(uint16_t key,uint16_t flags){
 if(MpNow()-mpLastSeen.load()>250)return false;
 if(mpIsOpen&&key==0x5801){if(flags&1)mpBack=true;return true;}
 if(!mpIsOpen&&mpCanChoose&&key==0x5802){if(flags&1)mpOpen=true;return true;}
 return mpIsOpen&&(key==0x5802||key==0x5814||key==0x5806||key==0x5807);
}
void MpTick(PPCContext& ctx,uint8_t* base,uint32_t rendered){
 if(busy||!pdz_input::Supported())return;auto c=ctx;c.r1.u32-=2048;if(!Valid(c.r1.u32,2048))return;
 auto e=Element(c,rendered,base);if(!Valid(e,240)||!MpVisible(e,base))return;
 uint32_t root=e;std::u16string sceneId;bool matched=false;
 for(unsigned n=0;root&&n<32&&Valid(root,240);++n){sceneId=ReadText(REX_LOAD_U32(root+4),base);
  if(sceneId.starts_with(u"aid_darkxui_frontend_multi_creategame_")&&(sceneId.find(u"advanced")!=std::u16string::npos||sceneId.ends_with(u"teamchooser")||sceneId.ends_with(u"lobby_menu_main"))){matched=true;break;}
  root=REX_LOAD_U32(root+40);}
 if(!root||!matched)return;
 auto id=ReadText(REX_LOAD_U32(e+4),base);
 c.r3.u64=REX_LOAD_U32(root);sub_8276C0F8(c,base);auto parent=c.r3.u32;
 if(mpMenu.scene!=parent){mpMenu=MpMenu{};mpMenu.scene=parent;mpIsOpen=false;mpCanChoose=false;mpBack=false;mpOpen=false;}
 mpLastSeen=MpNow();busy=true;
 if(mpMenu.open)for(const auto& item:mpMenu.hidden)MpShow(c,base,item.first,false);
 if(mpMenu.lastUpdate+16>MpNow()){busy=false;return;}
 mpMenu.lastUpdate=MpNow();
 const bool teams=sceneId.ends_with(u"teamchooser");
 const bool bots=sceneId.ends_with(u"lobby_menu_main");
 if(bots){
  const auto kick=Child(c,parent,u"botKick",base);const auto ke=Element(c,kick,base);
  const bool available=Valid(ke,240)&&MpVisible(ke,base)&&REX_LOAD_U32(0x82D27500)<31;
  if(!mpMenu.open&&available&&!mpMenu.bot[0]){
   const float x=std::bit_cast<float>(REX_LOAD_U32(ke+44)),y=std::bit_cast<float>(REX_LOAD_U32(ke+48));
   constexpr std::u16string_view labels[]{u"RANDOM MALE BODY",u"RANDOM FEMALE BODY",u"RANDOM ALL BODIES",u"RANDOM MALE HEAD",u"RANDOM FEMALE HEAD",u"RANDOM ALL HEADS",u"CUSTOMIZE"};
   for(unsigned i=0;i<7;++i){mpMenu.bot[i]=MpButton(c,base,parent,u"MpBot"+MpNumber(i),labels[i],x,y+40.f+32.f*i,565,30,u"DarkXuiBotKick");
    UpdateNavigation(c,base,mpMenu.bot[i],28,i?u"MpBot"+MpNumber(i-1):u"botKick");
    UpdateNavigation(c,base,mpMenu.bot[i],32,i<6?u"MpBot"+MpNumber(i+1):u"botSkill");}
   UpdateNavigation(c,base,kick,32,u"MpBot0");
   const auto skill=Child(c,parent,u"botSkill",base);if(skill)UpdateNavigation(c,base,skill,28,u"MpBot6");
  }
  if(!mpMenu.open)for(auto h:mpMenu.bot)MpShow(c,base,h,available);
  mpCanChoose=false;
  if(mpMenu.open&&mpBack.exchange(false)){if(mpMenu.part){mpMenu.part=0;mpMenu.page=0;MpListLabels(c,base);}else MpClose(c,base);}
  busy=false;return;
 }
 if(teams&&!mpMenu.team[0]){
  constexpr std::u16string_view labels[]{u"TEAM 1: RANDOMS",u"TEAM 1: FREE CHOICE",u"TEAM 2: RANDOMS",u"TEAM 2: FREE CHOICE"};
  for(unsigned i=0;i<4;++i)mpMenu.team[i]=MpButton(c,base,parent,u"MpTeam"+MpNumber(i),labels[i],500,445.f+40.f*i,350,32);
  for(unsigned i=0;i<4;++i){UpdateNavigation(c,base,mpMenu.team[i],28,i?u"MpTeam"+MpNumber(i-1):u"buttonHenchmen");UpdateNavigation(c,base,mpMenu.team[i],32,i<3?u"MpTeam"+MpNumber(i+1):u"buttonDark");}
  auto last=Child(c,parent,u"buttonHenchmen",base);if(last)UpdateNavigation(c,base,last,32,u"MpTeam0");
 }
 if(!teams&&!mpMenu.armor){
  const auto friendly=Child(c,parent,u"optionFriendlyFire",base);auto f=Element(c,friendly,base);
  if(Valid(f,240)){
   const float x=std::bit_cast<float>(REX_LOAD_U32(f+44)),y=std::bit_cast<float>(REX_LOAD_U32(f+48));
   mpMenu.armor=MpButton(c,base,parent,u"MpVisibleArmor",u"VISIBLE ARMOR: ON",x,y+40.f,565,32);
   UpdateNavigation(c,base,friendly,32,u"MpVisibleArmor");UpdateNavigation(c,base,mpMenu.armor,28,u"optionFriendlyFire");UpdateNavigation(c,base,mpMenu.armor,32,u"optionBotDifficulty");
  }
 }
 if(!teams&&!mpMenu.character){
  mpMenu.assignment=Child(c,parent,u"optionLockedCharacters",base);
  mpMenu.character=MpButton(c,base,parent,u"MpChooseCharacter",u"CUSTOMIZE CHARACTER",123,598,500,34,u"X_Button");
  c.r3.u64=mpMenu.character;c.r4.u64=REX_LOAD_U32(0x82D356EC);sub_82779A50(c,base);if(Valid(c.r3.u32,8))REX_STORE_U32(c.r3.u32+4,0x5802);
 }
 if(mpMenu.open){if(mpBack.exchange(false)){if(mpMenu.part){mpMenu.part=0;mpMenu.page=0;MpListLabels(c,base);}else MpClose(c,base);}}
 else {
  if(teams)for(unsigned t=0;t<2;++t){auto mode=pdz_mp::TeamMode(t);if(mode!=mpMenu.displayedTeam[t]){
   SetText(c,mpMenu.team[t*2],u"TEAM "+MpNumber(t+1)+u": RANDOMS"+(mode==1?u" (ON)":u""),base);
   SetText(c,mpMenu.team[t*2+1],u"TEAM "+MpNumber(t+1)+u": FREE CHOICE"+(mode==2?u" (ON)":u""),base);
   mpMenu.displayedTeam[t]=mode;}}
  // Model Assignment is a session rule, not the XUI ButtonValue action ID.
  const bool free=!teams&&pdz_mp::FreeChoice(c,base);
  mpCanChoose=free;
  if(mpMenu.displayedChoice!=int(free)){MpShow(c,base,mpMenu.character,free);mpMenu.displayedChoice=int(free);}
  if(mpMenu.armor&&mpMenu.displayedArmor!=int(pdz_mp::ArmorVisible())){mpMenu.displayedArmor=int(pdz_mp::ArmorVisible());SetText(c,mpMenu.armor,mpMenu.displayedArmor?u"VISIBLE ARMOR: ON":u"VISIBLE ARMOR: OFF",base);}
  if(mpOpen.exchange(false)&&free)MpOpenList(c,base);
 }
 busy=false;
}
