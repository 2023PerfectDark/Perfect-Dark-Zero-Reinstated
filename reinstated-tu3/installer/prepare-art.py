from pathlib import Path
import json,sys
R=Path(__file__).resolve().parent
from PIL import Image
D=Path('C:/Users/Owner/Downloads');T=Path('C:/Users/Owner/AppData/Local/Temp')
roles={
 'language':T/'codex-clipboard-b5f380bd-2267-41f1-a557-18e8f50ff60e.png',
 'welcome':D/'Tumblr_770573e77a26d2d7a5de79a81f0c6a2e_08f85fc6_1280.webp',
 'game':T/'codex-clipboard-6a592fb0-231a-4758-951c-a7ecb64a26d1.png',
 'dlc':T/"Mai_Hem,_heir_apparent_of_dataDyne's_Zhang_Li.webp",
 'display':T/'codex-clipboard-0d91dc5f-90c8-464e-811b-87f80c41ab7d.png',
 'dubs':D/'thddumbnail.jpeg',
 'bonus':D/'thddumbnail.jpeg',
 'space':T/'codex-clipboard-6a592fb0-231a-4758-951c-a7ecb64a26d1.png',
 'install':T/'codex-clipboard-b5f380bd-2267-41f1-a557-18e8f50ff60e.png',
 'complete':T/'codex-clipboard-d3b296bf-f952-4e79-9a68-a0e65387e290.png'}
comics=['Pg_15_Hong_Kong_Sunrise_.webp','046fd047104ce3e0818dc097ac2e2a6c.jpg','927963.jpg','JanusTears_3.webp','8-7be3a2af2d323c2aab8a4f2b080ae5d8.webp','10-2f6f2a5d17629e5acbd2336ac489ab91.webp','12-caa552871bc8b4f045135ce36591eb02.webp','21-c6792a971919c2a7ae86d79728a56bbe.webp','27-f3f2e01cbd0b4baa44cf098468496585.webp','28-4dac1c08a4d327f8368bdd3cd1aa1c28.webp','33-6292b41c5d49e75cf249cb2722d84f1f.webp','1-9c1d24642a047e111d88a8b42e5e5ea6.webp','9-b00f5c67541378d2fe8ddb2a223ad137.webp','25-839cc55a3a187d481637b5df85ca1fbc.webp','1166675.jpg','34-60f2dbe1824f58c51ccc0d19454c1492.webp']
(R/'art').mkdir(exist_ok=True)
manifest={'roles':{},'comics':[],'missing':[]}
def convert(src,dest):
 if not src.exists():
  for alt in (D/src.name,T/src.name):
   if alt.exists():src=alt;break
 if not src.exists():manifest['missing'].append(src.name);return False
 with Image.open(src) as im:im.save(R/dest)
 return True
for role,src in roles.items():
 dest='art/'+role+'.png'
 if convert(src,dest):manifest['roles'][role]=dest
for i,name in enumerate(comics):
 dest='art/comic-%02d.png'%i
 if convert(D/name,dest):manifest['comics'].append(dest)
(R/'art.json').write_text(json.dumps(manifest,indent=2))
print('Artwork:',len(manifest['roles']),'characters /',len(manifest['comics']),'comic pages; missing:',manifest['missing'])
