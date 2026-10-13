#pragma once
#include <cstdint>
#include <string>
#include <vector>
struct PPCContext;
namespace pdz_mp {
struct Character { const char16_t* name; unsigned body, head, hands; };
const std::vector<Character>& Characters();
unsigned Choice(unsigned slot,int team=-1);
void Choose(unsigned slot,unsigned choice,int team=-1);
unsigned HeadChoice(unsigned slot);
bool IndependentParts(unsigned slot);
void ChoosePart(unsigned slot,unsigned choice,bool head);
void RandomPart(unsigned slot,bool head,unsigned gender);
std::vector<unsigned> PartChoices(bool head,unsigned gender=0);
unsigned TeamMode(unsigned team);
void SetTeamMode(unsigned team,unsigned mode);
bool ArmorVisible();
bool FreeChoice(PPCContext& ctx,uint8_t* base);
void SetArmorVisible(bool visible);
void HideArmor(uint8_t* base,uint32_t object,uint32_t model);
void RecoverRingleaderHeadShader(PPCContext& ctx,uint8_t* base,uint32_t graph);
struct Slot { unsigned index, team; std::u16string name; };
std::vector<Slot> Slots(PPCContext& ctx,uint8_t* base);
}
