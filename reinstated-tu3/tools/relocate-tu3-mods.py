"""Generate a reviewable TU3 source tree. Does not build, install, or launch.

Function identity and structure compatibility are separate checks. This emits
an unresolved-address report and keeps an explicit pending layout audit.
"""
from pathlib import Path
import json
import re

ROOT=Path(__file__).resolve().parent.parent
PROJECT=ROOT/'compatibility-v16/ModdedTU3'

def run():
    review=json.loads((PROJECT/'function-map-review.json').read_text())
    inventory=json.loads((PROJECT/'port-inventory.json').read_text())
    functions={}
    decisions=[]
    for entry in review['functions']:
        if entry['status']=='instruction match':
            functions[entry['old']]=entry['new']
        else:
            assert entry.get('candidates'),entry['old']
            chosen=entry['candidates'][0]
            functions[entry['old']]=chosen['new']
            decisions.append({'old':entry['old'],'new':chosen['new'],
                              'status':'identity candidate; changed body requires layout/call-site review'})
    # TU3 splits voice localization into CC98 and text localization into CBD8.
    # Preserve that distinction instead of selecting a similar adjacent body.
    functions['823948D0']='8239CC98'
    # Same XUI wrapper body, but this target has both matching callees.
    functions['8277BBB8']='8278B918'
    assert len(set(functions.values()))==len(functions), 'Function identity collision'
    addresses={a:next(iter(candidates)) for a,candidates in review['global_candidates'].items() if len(candidates)==1}
    addresses.update({a:v['new'] for a,v in review.get('code_literal_candidates',{}).items()})
    addresses.update(functions)
    # Image metadata comes from TU3's generated pch; these are bounds, not hooks.
    addresses.update({'82140000':'82140000','82000000':'82000000','82CE62AC':'82CEE27C',
                      '820CED70':'820CD9E0'})
    bounds={'82100000','82D00000','82B00000','83000000'}
    for bound in bounds: addresses.pop(bound,None)
    extra=json.loads((PROJECT/'additional-address-evidence.json').read_text())
    for old,row in extra.items():
        if row['kind']=='return address' and row['candidates'] and row['candidates'][0]['score']>.95:
            addresses[old]=row['candidates'][0]['new']
    evidence_path=PROJECT/'extra-global-evidence.json'
    if evidence_path.exists():
        for old,rows in json.loads(evidence_path.read_text()).items():
            candidates={row['new'] for row in rows if row['score']>.9}
            if len(candidates)==1:addresses[old]=candidates.pop()
    # Six consecutive InventorySlotButton categories retain 44-byte vtables.
    # Constructor bodies are identical after normalization; verify the entire
    # table range rather than confusing the first constructor with the last.
    addresses.update({'820B6450':'820B4C18','820B652C':'820B4CF4'})
    original=(ROOT/'staging/audio-base-data.bin').read_bytes()
    updated=(ROOT/'staging/audio-tu3-data.bin').read_bytes()
    data_evidence=[]
    for literal in inventory['address_literals']:
        if literal in addresses or literal in bounds:continue
        address=int(literal,16)
        if not 0x82000000<=address<0x82140000:continue
        offset=address-0x82000000
        for length in (64,32,16,8):
            value=original[offset:offset+length]
            if len(value)!=length or len(set(value))<4:continue
            position=updated.find(value)
            if position>=0 and updated.find(value,position+1)<0:
                addresses[literal]=f'{position+0x82000000:08X}'
                data_evidence.append({'old':literal,'new':addresses[literal], 'identical_bytes':length})
                break
    unresolved={}
    changed=[]
    for path in sorted((PROJECT/'src-tu0').rglob('*')):
        if not path.is_file():continue
        relative=path.relative_to(PROJECT/'src-tu0')
        output=PROJECT/'src'/relative
        output.parent.mkdir(parents=True,exist_ok=True)
        if path.suffix not in {'.cpp','.h','.inl'} or path.name in {'blood_decal_pixels.inl','surface_intro_animation_data.h'}:
            output.write_bytes(path.read_bytes());continue
        text=path.read_text()
        text=text.replace('generated/default/perfectdarkzerorecomp_init.h','generated/default/pdzstocktu3_init.h')
        text=re.sub(r'sub_([0-9A-F]{8})',lambda m:'sub_'+functions.get(m[1],m[1]),text)
        def literal(match):
            key=match[1].upper()
            if key in addresses:return '0x'+addresses[key]
            if key not in bounds:unresolved.setdefault(key,set()).add(relative.as_posix())
            return match[0]
        text=re.sub(r'\b0x(8[2-3][0-9A-Fa-f]{6})(?![0-9A-Fa-f])',literal,text)
        if path.name=='game_constants.h':text=text.replace('0x013D0000','0x013E0000')
        if path.name=='mousehook.cpp':
            text=text.replace('09.11.05.0052','19.09.06.0082').replace('retail TU0 validation','TU3 validation')
        if path.name=='perfectdarkzerorecomp_app.h':
            text=text.replace('ctx, "perfectdarkzerorecomp",','ctx, "pdzmoddedtu3",')
        if path.name=='main.cpp':
            text=text.replace('REX_DEFINE_APP(perfectdarkzerorecomp,','REX_DEFINE_APP(pdzmoddedtu3,')
        if path.name=='dub_language.cpp':
            start=text.index('  std::string root;')
            end=text.index('  __imp__sub_8239CC98(ctx,base);',start)
            text=text[:start]+'''  // TU3 has a dedicated voice-root resolver; r4 is not a path argument.
  const bool previous=audioDirectory;
  audioDirectory=true;
'''+text[end:]
        # Deliberately explicit, typed relocation of shared fields. A blanket
        # integer replacement would corrupt unrelated weapon/model structures.
        before=text
        text=re.sub(r'\b(session\s*\+\s*)36\b',r'\g<1>40',text)
        text=re.sub(r'\b((?:local|owner|health|hp|l)\s*\+\s*)952\b',r'\g<1>964',text)
        text=re.sub(r'\b((?:Readable|Valid|Access)\((?:local|owner|l),\s*)956\b',r'\g<1>968',text)
        for old,new in ((968,980),(972,984),(976,988),(2004,2572),(2008,2576)):
            # Keep substitutions simultaneous: newly written 980 is not an
            # input in this table, and all source keys are distinct.
            text=re.sub(r'\b((?:health|hp)\s*\+\s*)'+str(old)+r'\b',lambda m:m[1]+str(new),text)
        for old,new in ((980,992),(976,988),(972,984),(2012,2580)):
            text=re.sub(r'\b((?:Readable|Valid|Access)\((?:health|hp),\s*)'+str(old)+r'\b',lambda m:m[1]+str(new),text)
        if relative.as_posix() in {'visible_holsters.cpp','surface_runtime.cpp'}:
            text=re.sub(r'\b(owner\s*\+\s*)1148\b',r'\g<1>1676',text)
            text=re.sub(r'\b(Readable\(owner,\s*)1152\b',r'\g<1>1680',text)
        if relative.as_posix()=='coop_ally.cpp':
            text=text.replace('table+340','table+344')
        if relative.as_posix()=='visible_holsters.cpp':
            text=text.replace('#include "nightclub_population.h"','#include "nightclub_population.h"\n#include "death_voice.h"').replace('  pdz_population::Tick(ctx,base);','  pdz_population::Tick(ctx,base);\n  pdz_death_voice::Tick(ctx,base);')
        if relative.as_posix()=='weapon_rules.cpp':
            text='#include "heel_audio.h"\n'+text
            text=text.replace('// Native positional cue constructor: r3 contains the cue name, r5 its actor.',(ROOT/'tools/tu3-footstep-owner.inl').read_text()+'// Native positional cue constructor: r3 contains the cue name, r5 its actor.')
            text=text.replace('(ctx.r5.u32==player||ctx.r4.u32==player+48)','(native_footstep_actor==player||ctx.r5.u32==player||ctx.r4.u32==player+48)')
            text=text.replace('      constexpr char prefix[]="fstep_";step=true;\n      for(unsigned i=0;i<6;++i){auto c=REX_LOAD_U8(cue+i);if(c>=\'A\'&&c<=\'Z\')c+=32;if(c!=prefix[i])step=false;}','      std::string name;\n      for(unsigned i=0;i<96&&Readable(cue+i,1);++i){char c=char(base[cue+i]);if(!c)break;if(c>=\'A\'&&c<=\'Z\')c+=32;name+=c;}\n      step=pdz_heels::CueName(name).starts_with("fstep_");')
            text=text.replace('  __imp__sub_821FF428(ctx,base);',(ROOT/'tools/tu3-heel-dispatch.inl').read_text()+'  __imp__sub_821FF428(ctx,base);')
        if relative.as_posix()=='mod/third_person.cpp':
            text=text.replace('#include "breathing_motion.h"','#include "breathing_motion.h"\n#include "landing_motion.h"')
            text=text.replace('    float d[3],u[3];float length2=0;',(ROOT/'tools/tu3-landing-camera.inl').read_text()+'    float d[3],u[3];float length2=0;')
        if relative.as_posix()=='nightclub_population.cpp':
            text=text.replace('table+828','table+832')
            text=text.replace('namespace pdz_population {','bool Tu3CrowdHasModelBudget(uint8_t* base);\nnamespace pdz_population {',1)
            marker='  if(actors.size()>=1024)return false; // Never create an untracked reinforcement.'
            assert text.count(marker)==1
            text=text.replace(marker,marker+'\n  if(!Tu3CrowdHasModelBudget(base))return false;')
            marker='  next_tick=now+std::chrono::milliseconds(50);'
            assert text.count(marker)==1
            text=text.replace(marker,marker+'\n  if(!Tu3CrowdHasModelBudget(base))return;')
        if relative.as_posix()=='mod/third_person.cpp':text=text.replace('kPlayerVtblCameraNear = 744','kPlayerVtblCameraNear = 748')
        if relative.as_posix()=='campaign_gore.cpp':text=text.replace('p+952','p+964').replace('Readable(p,956)','Readable(p,968)')
        if relative.as_posix()=='mp_characters.cpp':text=text.replace('o+952','o+964')
        if relative.as_posix() in {'mp_characters.cpp','weapon_challenge.cpp'}:
            text=text.replace('Valid(s,40)','Valid(s,44)').replace('s+36','s+40').replace('Valid(p,956)','Valid(p,968)')
        if relative.as_posix()=='native_campaign.cpp':
            text=text.replace('call.r3.u32+1152','call.r3.u32+1680').replace('Valid(call.r3.u32,1156)','Valid(call.r3.u32,1684)')
            # TU3 inserts setting action 50; subsequent native action IDs move.
            text=text.replace('call.r3.u64=79;call.r4.u64=0;sub_823B8AD8','call.r3.u64=80;call.r4.u64=0;sub_823B8AD8')
            text=text.replace('call.r3.u64=78;call.r4.u64=pdz_coop','call.r3.u64=79;call.r4.u64=pdz_coop')
        if relative.as_posix()=='surface_runtime.cpp':
            text=text.replace('Readable(ctx.r3.u32,40)||REX_LOAD_U32(ctx.r3.u32+36)', 'Readable(ctx.r3.u32,44)||REX_LOAD_U32(ctx.r3.u32+40)')
            start=text.index('void RegisterSurface(')
            end=text.index('void AssignNativeText(',start)
            text=text[:start]+(ROOT/'tools/tu3-surface-registry.inl').read_text()+'\n'+text[end:]
        text=re.sub(r'\b((?:Readable|Valid|Access)\(session,\s*)40\b',r'\g<1>44',text)
        if before!=text:changed.append(relative.as_posix())
        output.write_text(text)
    (PROJECT/'src/weather_audio.h').write_text((ROOT/'tools/tu3-weather-audio.h').read_text())
    (PROJECT/'src/tu3_offline_live.cpp').write_text((ROOT/'tools/tu3-offline-live.cpp').read_text())
    (PROJECT/'src/heel_audio.h').write_text((ROOT/'tools/tu3-heel-audio.h').read_text())
    (PROJECT/'src/model_render_guard.cpp').write_text((ROOT/'tools/tu3-model-render-guard.cpp').read_text())
    (PROJECT/'src/gore_cap_geometry.h').write_text((ROOT/'tools/tu3-gore-cap-geometry.h').read_text())
    for source,target in [('tu3-death-voice.cpp','death_voice.cpp'),('tu3-death-voice.h','death_voice.h'),('tu3-death-voice-policy.h','death_voice_policy.h'),('tu3-campaign-gore.cpp','campaign_gore.cpp'),('tu3-campaign-gore.h','campaign_gore.h'),('tu3-gore-policy.h','gore_policy.h'),('tu3-blood-decals.cpp','blood_decals.cpp'),('tu3-blood-decals.h','blood_decals.h')]:
        (PROJECT/'src'/target).write_text((ROOT/'tools'/source).read_text())
    (PROJECT/'src/mod/landing_motion.h').write_text((ROOT/'tools/tu3-landing-motion.h').read_text())
    (PROJECT/'src/tu3_character_heap.cpp').write_text((ROOT/'tools/tu3-character-heap.cpp').read_text())
    (PROJECT/'src/tu3_character_resources.cpp').write_text((ROOT/'tools/tu3-character-resources.cpp').read_text())
    status={'status':'relocated source; see validation.json for build and runtime status',
            'functions':functions,'address_map':addresses,'data_evidence':data_evidence,
            'unresolved_addresses':{k:sorted(v) for k,v in sorted(unresolved.items())},
            'changed_function_reviews':decisions,'layout_edits':changed,
            'pending':['Audit changed function signatures and internal register/call-site assumptions',
                       'Audit controller/session aliases and bounds not covered by typed substitutions',
                       'Port generated weapon capacity and cutscene timing edits',
                       'Compile/link and test in an isolated TU3 profile']}
    (PROJECT/'relocation-review.json').write_text(json.dumps(status,indent=2))
    print(f'Relocated {len(functions)} function identities; {len(unresolved)} unresolved address literals')
    print(json.dumps(status['unresolved_addresses'],indent=2))

if __name__=='__main__':run()
