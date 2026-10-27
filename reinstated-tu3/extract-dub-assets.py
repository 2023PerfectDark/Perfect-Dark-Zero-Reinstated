import json,pathlib,hashlib
out=pathlib.Path('PerfectDarkZeroRecomp/out/dub-assets'); results=[]
for index in pathlib.Path('.').glob('language-disc-index-Perfect*.json'):
 d=json.loads(index.read_text())
 with open(d['iso'],'rb') as f:
  for path,sector,size in d['files']:
   parts=path.strip('/').split('/')
   if len(parts)<5 or parts[2].lower()!='audio' or parts[3] not in ['german','italian','spanish','japanese']:continue
   dest=out.joinpath(*parts);dest.parent.mkdir(parents=True,exist_ok=True)
   f.seek(d['base']+sector*2048); data=f.read(size)
   if len(data)!=size:raise ValueError(path)
   dest.write_bytes(data);results.append({'path':'/'.join(parts),'size':size,'sha256':hashlib.sha256(data).hexdigest()})
pathlib.Path('dub-assets-manifest.json').write_text(json.dumps(results,indent=2));print('Extracted',len(results),'voice banks;',sum(x['size'] for x in results),'bytes')
