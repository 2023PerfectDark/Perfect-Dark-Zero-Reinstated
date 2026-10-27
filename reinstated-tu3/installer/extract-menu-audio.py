"""Read the retail XACT cue table, then decode the actual menu waves."""
from pathlib import Path
import struct, subprocess, json, wave, array, sys

# Attenuate freshly decoded originals so repeated preparation is idempotent.
MENU_GAIN = 0.30
root=Path(__file__).resolve().parent.parent
audio=root/'compatibility-v16/ModdedTU3/assets/pdz/ASSETS/Audio'
b=(audio/'stock.xsb').read_bytes()
u16=lambda o:struct.unpack_from('>H',b,o)[0]
u32=lambda o:struct.unpack_from('>I',b,o)[0]
z=lambda o:b[o:b.index(0,o)].decode('ascii')
banks=[z(u32(0x32)+i*64) for i in range(b[0x11])]
found={}
for i in range(u16(9)):
    name=z(u32(u32(0x3a)+i*6))
    if not name.startswith('menu_'):continue
    off=u32(u32(0x1a)+i*5+1)
    flags=b[off];p=off+9
    waves=[]
    if not flags&1: waves.append((u16(p),b[p+2]))
    else:
        clips=b[p];p+=1
        if flags&0x0e:p+=u16(p)
        if flags&0x10:p+=u16(p)
        for j in range(clips):
            q=u32(p+j*9+1); count=b[q];q+=1
            for k in range(count):
                event=u32(q)&31;q+=6
                if event==1:waves.append((u16(q+1),b[q+3]));q+=5
                elif event==4:waves.append((u16(q+2),b[q+4]));q+=0x22
                else:raise ValueError((name,event,q))
    found[name]=[{'stream':w+1,'bank':banks[k]} for w,k in waves]
print(json.dumps(found,indent=2))
out=root/'installer/art/audio';out.mkdir(exist_ok=True)
for role,cue in {'select':'menu_forward','back':'menu_back','cursor':'menu_cursor_small','error':'menu_negative'}.items():
    w=found[cue][0]
    subprocess.run([str(root/'tools/vgmstream/vgmstream-cli.exe'),'-i','-s',str(w['stream']),'-o',str(out/(role+'.wav')),str(audio/(w['bank']+'.xwb'))],check=True,stdout=subprocess.DEVNULL)
    path=out/(role+'.wav')
    with wave.open(str(path),'rb') as wav:
        params=wav.getparams()
        assert params.sampwidth==2 and params.comptype=='NONE'
        samples=array.array('h',wav.readframes(params.nframes))
    if sys.byteorder!='little':samples.byteswap()
    samples=array.array('h',(round(sample*MENU_GAIN) for sample in samples))
    if sys.byteorder!='little':samples.byteswap()
    with wave.open(str(path),'wb') as wav:
        wav.setparams(params)
        wav.writeframes(samples.tobytes())
(out/'sources.json').write_text(json.dumps({k:{'cue':v,**found[v][0],'gain':MENU_GAIN} for k,v in {'select':'menu_forward','back':'menu_back','cursor':'menu_cursor_small','error':'menu_negative'}.items()},indent=2))
