import fs from 'node:fs';
import path from 'node:path';
import {readCaff} from '../compatibility-v13/PerfectDarkZeroRecomp/tools/surface/caff.mjs';
const root='compatibility-v16/ModdedTU3/assets/pdz/ASSETS/PackageData';
fs.mkdirSync('installer/reference',{recursive:true});
const found=[];
for(const file of fs.readdirSync(root)) {
 if(!file.endsWith('.pkg'))continue;
 const c=readCaff(path.join(root,file));
 for(const r of c.resources)if(/darkxui|xuiscene|samsung|dpal/i.test(r.name)) {
  found.push({file,name:r.name,size:r.size,section:r.section});
  if(!/texture/i.test(r.name)&&r.size<4000000)fs.writeFileSync('installer/reference/'+r.name.replace(/[^a-z0-9_.-]/ig,'_')+'.bin',r.data);
 }
}
fs.writeFileSync('installer/reference/menu-assets.json',JSON.stringify(found,null,2));
console.log(found.filter(x=>!/texture/i.test(x.name)).slice(0,100));
