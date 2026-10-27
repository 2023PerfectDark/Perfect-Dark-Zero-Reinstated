from pathlib import Path
import re,json,subprocess,wave,array,math,hashlib
root=Path('staging/rain-recordings');out=Path('staging/field-rain-audio');out.mkdir(exist_ok=True)
ff='exports/pd-ui-video-reference/deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
credits={'ground':('Rain on concrete sound','DBlover',412424),'metal':('Gentle rain on metal roof','wlabarron',509113),'wood':('Rain on wood','SamsterBirdies',584267),'vehicle':('Rain on Car Hood.wav','RoofDog',79872),'water':('180723 Rain, on water lake, modulating light to med, cottage ONTARIO.flac','TRP',572429),'gutter':('downspout-rainwater2.aif','alienistcog',124976)}
rows=[]
for kind,(title,author,sid) in credits.items():
 source=root/(kind+'.mp3')
 if not source.exists():raise RuntimeError('Download not ready: '+kind)
 html=(root/(kind+'.html')).read_text(encoding='utf-8')
 assert 'creativecommons.org/publicdomain/zero/1.0/' in html
 target=out/('rain-'+kind+'.wav')
 subprocess.run([ff,'-y','-v','error','-ss','0.5','-i',str(source),'-t','30','-ar','48000','-ac','1','-c:a','pcm_s16le',str(target)],check=True)
 with wave.open(str(target)) as w: samples=array.array('h',w.readframes(w.getnframes()))
 assert len(samples)>48000*3
 mean=sum(samples)/len(samples);values=[x-mean for x in samples]
 peak=max(abs(x) for x in values);rms=math.sqrt(sum(x*x for x in values)/len(values))
 gain=min(.65*32767/peak,.12*32767/rms)
 data=array.array('h',(round(x*gain) for x in values))
 with wave.open(str(target),'wb') as w:w.setparams((1,2,48000,0,'NONE','not compressed'));w.writeframes(data.tobytes())
 rows.append({'surface':kind,'title':title,'author':author,'url':f'https://freesound.org/people/{author}/sounds/{sid}/','license':'CC0 1.0','license_url':'https://creativecommons.org/publicdomain/zero/1.0/','media_url':re.search(r'data-static-file-url="([^"]+)"',html)[1],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'output':target.name,'output_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'seconds':len(data)/48000,'gain':gain,'peak':max(abs(x) for x in data)/32768})
 print(kind,'duration',round(len(data)/48000,2),'peak',round(rows[-1]['peak'],3))
(out/'sources.json').write_text(json.dumps(rows,indent=2))
(out/'RAIN_AUDIO_CREDITS.txt').write_text('Rain surface field recordings\nAll recordings: CC0 1.0, https://creativecommons.org/publicdomain/zero/1.0/\nDownloaded from public high-quality MP3 links supplied by the Freesound pages.\nEdits: trim to at most 30 seconds, mono conversion for positional playback, 48 kHz PCM, DC removal and level adjustment. Playback crossfades loop boundaries.\nWood recording is rain on a wooden table, used for wooden crates. Metal is corrugated metal, not specifically copper.\n\n'+'\n\n'.join(r['surface']+': '+r['title']+' by '+r['author']+'\n'+r['url'] for r in rows))
