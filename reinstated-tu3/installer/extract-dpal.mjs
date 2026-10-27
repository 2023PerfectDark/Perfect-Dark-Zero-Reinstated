// Decode the original Xenon BC2 frame textures, preserving their alpha.
import fs from 'node:fs';import zlib from 'node:zlib';
import {readCaff} from '../compatibility-v13/PerfectDarkZeroRecomp/tools/surface/caff.mjs';
const c=readCaff('compatibility-v16/ModdedTU3/assets/pdz/ASSETS/PackageData/1365415862.pkg',{relocations:true});
const out='installer/art/dpal';fs.mkdirSync(out,{recursive:true});
function crc(b){let n=~0;for(const v of b){n^=v;for(let j=0;j<8;j++)n=(n>>>1)^((n&1)?0xedb88320:0);}return (~n)>>>0;}
function png(file,w,h,p){const chunk=(n,b)=>{let t=Buffer.from(n),o=Buffer.alloc(12+b.length);o.writeUInt32BE(b.length);t.copy(o,4);b.copy(o,8);o.writeUInt32BE(crc(Buffer.concat([t,b])),8+b.length);return o;};let hd=Buffer.alloc(13);hd.writeUInt32BE(w);hd.writeUInt32BE(h,4);hd[8]=8;hd[9]=6;let raw=Buffer.alloc(h*(1+w*4));for(let y=0;y<h;y++)p.copy(raw,y*(w*4+1)+1,y*w*4,(y+1)*w*4);fs.writeFileSync(file,Buffer.concat([Buffer.from('89504e470d0a1a0a','hex'),chunk('IHDR',hd),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]));}
for(let i=0;i<c.resources.length;i++){
 const r=c.resources[i],b=r.data;if(!/dpal_bar_(left|right)/.test(r.name)||b.subarray(0,7).toString()!=='texture')continue;
 if(b.readUInt32BE(24)!==0x1a200153)throw Error('Unexpected frame texture format');
 const w=b.readUInt16BE(36),h=b.readUInt16BE(38),link=c.relocations.find(r=>r.source===i&&r.offset===40),d=c.resources[link.target].data,p=Buffer.alloc(w*h*4),pitch=Math.ceil(Math.ceil(w/4)/32)*32;
 for(let y=0;y<h/4;y++)for(let x=0;x<w/4;x++){
  const macro=((x>>5)+(y>>5)*(pitch>>5))<<11,micro=((x&7)+((y&6)<<2))<<4,o=macro+((micro&~15)<<1)+(micro&15)+((y&8)<<7)+((y&1)<<4);
  const a=((o&~511)<<3)+((o&448)<<2)+(o&63)+((y&16)<<7)+(((((y&8)>>2)+(x>>3))&3)<<6);
  const q=Buffer.from(d.subarray(a,a+16));q.swap16();const rgb=v=>[(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31],c0=rgb(q.readUInt16LE(8)),c1=rgb(q.readUInt16LE(10)),cs=[c0,c1,c0.map((v,k)=>(2*v+c1[k])/3),c0.map((v,k)=>(v+2*c1[k])/3)],ci=q.readUInt32LE(12);
  for(let j=0;j<16;j++){const at=((y*4+(j>>2))*w+x*4+(j&3))*4,co=cs[(ci>>>(j*2))&3];for(let k=0;k<3;k++)p[at+k]=co[k];p[at+3]=((q[j>>1]>>((j&1)*4))&15)*17;}
 }
 const side=r.name.includes('_left')?'left':'right';png(`${out}/${side}.png`,w,h,p);console.log(side,w,h);
}
fs.copyFileSync('C:/Users/Owner/AppData/Local/Temp/XboxLogo.png',out+'/xbox.png');
