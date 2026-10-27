import json,pathlib,subprocess,re,concurrent.futures,collections,wave
root=pathlib.Path('extracted/GoldenEye-Reloaded-music')
exe=str(pathlib.Path('tools/vgmstream/vgmstream-cli.exe').resolve())
entries=json.loads((root/'audio-manifest.json').read_text())
def inspect(e):
    p=subprocess.run([exe,'-m',e['path']],capture_output=True,text=True)
    e['metadata']=p.stdout+p.stderr;e['supported']=p.returncode==0
    for name,pattern in [('channels',r'channels: (\d+)'),('rate',r'sample rate: (\d+)'),('samples',r'stream total samples: (\d+)')]:
        m=re.search(pattern,e['metadata']);e[name]=int(m[1]) if m else 0
    e['seconds']=e['samples']/e['rate'] if e['rate'] else 0
    return e
cache=root/'audio-metadata.json'
if cache.exists(): entries=json.loads(cache.read_text())
else:
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: entries=list(pool.map(inspect,entries))
    cache.write_text(json.dumps(entries,indent=2))
print('Metadata:',collections.Counter((e['bank'],e['channels'],e['supported']) for e in entries),flush=True)
# Shared stereo streams include music, transitions and ambience; preserve all
# rather than silently discarding short musical cues based on length.
selected=[e for e in entries if e['bank']=='FILE_COM' and e['supported'] and e['channels']>=2]
def decode(e):
    output=root/'playable-stereo'/f"track_{e['index']:04}.wav";output.parent.mkdir(exist_ok=True)
    p=subprocess.run([exe,'-i','-o',str(output),e['path']],capture_output=True,text=True)
    if p.returncode: return dict(index=e['index'],error=p.stderr)
    with wave.open(str(output),'rb') as wav:
        assert wav.getnchannels()==e['channels'] and wav.getframerate()==e['rate']
        assert wav.getnframes()==e['samples']
    return dict(index=e['index'],path=str(output),seconds=e['seconds'],channels=e['channels'])
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: decoded=list(pool.map(decode,selected))
(root/'playable-manifest.json').write_text(json.dumps(decoded,indent=2))
print('Decoded and verified',len(decoded),'shared stereo streams; failures',sum('error' in e for e in decoded),flush=True)
