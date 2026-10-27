#pragma once
#include <array>
#include <string_view>
#include <span>
#include <cmath>
namespace death_voice {
inline constexpr std::array<std::string_view,34> clips{
 "elite1_gen_s__0847","elite1_gen_s__0849","elite2_gen_s__0846",
 "elite3_gen_s__0837","elite3_gen_s__0839","elite3_gen_s__0841",
 "gangsta2_gen_s__0884","gangsta2_gen_s__0885","gangsta2_gen_s__0886",
 "gangsta3_gen_s__0883","gangsta3_gen_s__0884","gangsta3_gen_s__0885",
 "maihem_gen_s__0004","maihem_gen_s__0005","maihem_gen_s__0006",
 "jo_gen_s__0017","chandra_gen_s__0006",
 "trooper1_gen_s__1355","trooper1_gen_s__1356","trooper1_gen_s__1358","trooper1_gen_s__1359",
 "drcarrol_gen_s__0027","drcarrol_gen_s__0025","clay_gen_s__0005","clay_gen_s__0006",
 "jack_gen_s__0025","sci1_gen_s__0367","sci1_gen_s__0368","sci1_gen_s__0369",
 "virgil_gen_s__0004","virgil_gen_s__0005","virgil_gen_s__0006",
 "woman2_gen_s__0247","woman2_gen_s__0249"};
inline constexpr std::array<std::string_view,21> banks{
 "elite1","elite2","elite3","gangsta1","gangsta2","gangsta3","maihem","jo","chandra",
 "trooper1","trooper2","trooper3","drcarrol","clay","jack","sci1","virgil",
 "woman1","woman2","jon","sci2"};
inline std::string_view Bank(std::string_view cue){
 constexpr std::string_view prefix="xact_soundbank_";
 if(cue.starts_with(prefix))cue.remove_prefix(prefix.size());
 for(auto bank:banks)if(cue.starts_with(bank)&&cue.size()>bank.size()&&cue[bank.size()]=='_')return bank;
 return {};
}
inline bool Female(std::string_view bank,unsigned body){
 if(!bank.empty())return bank=="woman1"||bank=="woman2"||bank=="maihem"||bank=="jo"||bank=="chandra";
 // Native humanoid body IDs, including the campaign wardrobe variants.
 return body==6||body==13||body==54||body==73||body==80||body==82||body==85||body==90||body==94||body==109||body==110||body==111||body==115;
}
struct Pool {unsigned first,count;};
inline Pool Specific(std::string_view bank){
 if(bank=="elite1")return {0,2};if(bank=="elite2")return {2,1};if(bank=="elite3")return {3,3};
 if(bank=="gangsta2")return {6,3};if(bank=="gangsta3")return {9,3};
 if(bank=="maihem")return {12,3};if(bank=="jo")return {15,1};if(bank=="chandra")return {16,1};
 if(bank=="trooper1")return {17,4};return {0,0};
}
inline unsigned Choose(std::string_view bank,unsigned body,unsigned random){
 auto pool=Specific(bank);if(pool.count)return pool.first+random%pool.count;
 constexpr unsigned female[]{32,33,12,13,14};
 return Female(bank,body)?female[random%5]:21+random%11;
}
inline bool Fatal(unsigned region,float before,float after,bool headRemoved){
 return (region==1||region==2)&&std::isfinite(before)&&std::isfinite(after)&&before>0&&after<=0&&!headRemoved;
}
inline bool DeathCue(std::string_view cue){
 return cue.ends_with("_final_breath")||cue.ends_with("_short_scream")||cue.ends_with("_long_scream");
}
enum class Route { Native, Defer, Suppress };
inline Route RouteDeath(std::string_view cue,bool pendingHeadHit,bool fatalHeadshot,bool nativeDeathRoutine=false){
 // The native death routine can submit an already-resolved cue without a name.
 if(!nativeDeathRoutine&&!DeathCue(cue))return Route::Native;
 if(fatalHeadshot)return Route::Suppress;
 return pendingHeadHit?Route::Defer:Route::Native;
}
}
