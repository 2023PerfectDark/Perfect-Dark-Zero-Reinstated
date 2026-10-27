"""Convert the user-supplied EXR rain sequence into bounded GPU atlas pages."""
from pathlib import Path
import subprocess
import struct
import numpy as np
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / 'staging/rain-on-glass-exr/full/FX Elements - Rain on Glass - 004/RainOnGlass-004/RainOnGlass-004.%04d.exr'
ffmpeg = root / 'exports/pd-ui-video-reference/deps/imageio_ffmpeg/binaries/ffmpeg-win-x86_64-v7.1.exe'
dest = root / 'staging/rain-glass-assets'
dest.mkdir(exist_ok=True)
w, h, frames = 768, 432, 96
process = subprocess.Popen([str(ffmpeg), '-v', 'error', '-framerate', '24', '-start_number', '1001', '-i', str(source),
    '-vf', r'select=not(mod(n\,2)),scale=768:432', '-fps_mode', 'passthrough', '-frames:v', str(frames),
    '-pix_fmt', 'rgba64le', '-f', 'rawvideo', 'pipe:1'], stdout=subprocess.PIPE)
decoded=[]
for page in range(6):
    for tile in range(16):
        data = process.stdout.read(w*h*8)
        if len(data) != w*h*8:
            raise RuntimeError('Incomplete EXR frame')
        rgba = np.frombuffer(data, dtype='<u2').reshape(h,w,4).astype(np.float32)/65535
        alpha = rgba[:,:,3:4]
        # The supplied EXRs carry premultiplied linear RGB; the UI expects
        # straight-alpha display RGB. Unassociate before converting to sRGB.
        rgb = np.clip(rgba[:,:,:3]/np.maximum(alpha,1/65535),0,1)
        rgb = np.where(rgb<=.0031308,rgb*12.92,1.055*np.power(rgb,1/2.4)-.055)
        out = np.concatenate((rgb,alpha),axis=2)
        out[alpha[:,:,0]<.003] = 0
        pixels = np.uint8(np.clip(out*255+.5,0,255))
        decoded.append(pixels)
if process.wait()!=0:
    raise RuntimeError('EXR conversion failed')
# Keep the moving water, removing the stationary fine mist already supplied by
# the game's curved beads. This uses the actual sequence's temporal matte.
background=np.percentile(np.stack([f[:,:,3] for f in decoded]),10,axis=0)
for page in range(6):
    atlas = np.zeros((h*4,w*4,4),dtype=np.uint8)
    for tile in range(16):
        pixels=decoded[page*16+tile]
        pixels[:,:,3]=np.uint8(np.maximum(pixels[:,:,3].astype(float)-background,0))
        y,x=divmod(tile,4)
        atlas[y*h:(y+1)*h,x*w:(x+1)*w]=pixels
        if page==1 and tile==0:
            Image.fromarray(pixels).save(dest/'preview.png')
            bg=Image.new('RGBA',(w,h),(55,90,100,255))
            Image.alpha_composite(bg,Image.fromarray(pixels)).convert('RGB').save(dest/'preview-composite.jpg')
    with (dest/f'runoff-{page}.rgba').open('wb') as f:
        f.write(struct.pack('<II',w*4,h*4))
        f.write(atlas.tobytes())
    print(f'Atlas {page+1}/6 saved',flush=True)
