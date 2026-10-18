// SPDX-License-Identifier: GPL-3.0-only
export const STEPS=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767];
const DELTA=[-1,-1,-1,-1,2,4,6,8], clamp=(v,l,h)=>Math.min(h,Math.max(l,v));
export function crc32(b){let c=0xffffffff;for(const v of b){c^=v;for(let j=0;j<8;j++)c=(c>>>1)^((0-(c&1))&0xedb88320);}return (~c)>>>0;}
export function encodeIMA(pcm){const out=new Uint8Array(Math.ceil(pcm.length/256)*132),view=new DataView(out.buffer);let idx=0;
 for(let at=0;at<pcm.length;at+=256){const o=at/256*132;let pred=clamp(Math.round(pcm[at]*32767),-32768,32767);view.setInt16(o,pred,true);out[o+2]=idx;
  for(let j=0;j<256;j++){const v=at+j<pcm.length?clamp(Math.round(pcm[at+j]*32767),-32768,32767):0;let d=v-pred,n=d<0?8:0;d=Math.abs(d);const s=STEPS[idx];let diff=s>>3;for(const [bit,val] of [[4,s],[2,s>>1],[1,s>>2]])if(d>=val){n|=bit;d-=val;diff+=val;}
   pred=clamp(pred+((n&8)?-diff:diff),-32768,32767);idx=clamp(idx+DELTA[n&7],0,88);out[o+4+(j>>1)]|=n<<((j&1)*4);
  }
 }return out;
}
export function decodeIMA(bytes,frames){if(!Number.isInteger(frames)||frames<1||frames>576000||bytes.length<Math.ceil(frames/256)*132)throw Error('Invalid sound data');const out=new Float32Array(frames),view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);
 for(let at=0;at<frames;at+=256){const o=at/256*132;let p=view.getInt16(o,true),idx=bytes[o+2];if(idx>88)throw Error('Invalid ADPCM block');for(let j=0;j<256&&at+j<frames;j++){const n=(bytes[o+4+(j>>1)]>>((j&1)*4))&15,s=STEPS[idx];let d=s>>3;if(n&1)d+=s>>2;if(n&2)d+=s>>1;if(n&4)d+=s;p=clamp(p+((n&8)?-d:d),-32768,32767);idx=clamp(idx+DELTA[n&7],0,88);out[at+j]=p/32768;}}return out;
}
export const bankBytes=sounds=>512+sounds.reduce((n,s)=>n+(s.data?.length??Math.ceil(s.pcm.length/256)*132),0);
const ascii=(v,n)=>new TextEncoder().encode(v.normalize('NFKD').replace(/[^\x20-\x7e]/g,'').slice(0,n-1));
export function packBank(sounds,name='DOJO BANK'){
 if(!sounds.length||sounds.length>16)throw Error('A bank needs 1 to 16 sounds.');const total=bankBytes(sounds);if(total>81920)throw Error('Bank too large: 80 KiB maximum. Trim or remove sounds.');
 const out=new Uint8Array(total),v=new DataView(out.buffer);v.setUint32(0,0x42444143,true);v.setUint16(4,1,true);v.setUint16(6,sounds.length,true);v.setUint32(8,total-512,true);out.set(ascii(name,12),20);let off=0;
 sounds.forEach((s,i)=>{const data=s.data||encodeIMA(s.pcm),frames=s.frames??s.pcm.length;if(frames<1||frames>576000||s.rate<8000||s.rate>24000||s.category<0||s.category>8)throw Error('Invalid sound.');const p=32+i*28;v.setUint32(p,off,true);v.setUint32(p+4,frames,true);v.setUint16(p+8,s.rate,true);out[p+10]=s.category;out.set(ascii(s.name,16),p+12);out.set(data,512+off);off+=data.length;});v.setUint32(12,crc32(out.subarray(512)),true);v.setUint32(16,crc32(out.subarray(0,512)),true);return out;
}
export function unpackBank(input){const b=new Uint8Array(input),v=new DataView(b.buffer,b.byteOffset,b.byteLength);if(b.length<512||b.length>81920||v.getUint32(0,true)!==0x42444143||v.getUint16(4,true)!==1)throw Error('Not a Cut-A-Dor bank.');const n=v.getUint16(6,true),len=v.getUint32(8,true),head=b.slice(0,512),hv=new DataView(head.buffer),crc=hv.getUint32(16,true);hv.setUint32(16,0,true);if(n<1||n>16||len!==b.length-512||crc32(head)!==crc||crc32(b.subarray(512))!==v.getUint32(12,true))throw Error('Bank is damaged or incomplete.');
 const sounds=[];for(let i=0;i<n;i++){const p=32+i*28,off=v.getUint32(p,true),frames=v.getUint32(p+4,true),rate=v.getUint16(p+8,true),category=b[p+10],size=Math.ceil(frames/256)*132;if(!frames||frames>576000||off+size>len||rate<8000||rate>24000||category>8||b[p+27]!==0)throw Error('Invalid bank entry.');const name=new TextDecoder().decode(b.subarray(p+12,p+28)).split('\0')[0],data=b.slice(512+off,512+off+size);decodeIMA(data,frames);sounds.push({name,frames,rate,category,data});}return sounds;
}
export function pack7(bytes){const a=[];for(let i=0;i<bytes.length;i+=7){const s=bytes.subarray(i,i+7);a.push(s.reduce((n,b,j)=>n|((b>>7)<<j),0));for(const b of s)a.push(b&127);}return a;}
export function unpack7(bytes){const a=[];for(let i=0;i<bytes.length;){const mask=bytes[i++];if(i===bytes.length)throw Error('Incomplete MIDI block');for(let j=0;j<7&&i<bytes.length;j++)a.push(bytes[i++]|((mask>>j&1)<<7));}return new Uint8Array(a);}
export const u21=n=>[n&127,(n>>7)&127,(n>>14)&127];
export function packProject(raw){const b=new Uint8Array(16+raw.length),v=new DataView(b.buffer);v.setUint32(0,0x4a504443,true);v.setUint32(4,1,true);v.setUint32(8,raw.length,true);v.setUint32(12,crc32(raw),true);b.set(raw,16);return b;}
export function unpackProject(b){const v=new DataView(b.buffer,b.byteOffset,b.byteLength);if(b.length<16||v.getUint32(0,true)!==0x4a504443||v.getUint32(4,true)!==1||v.getUint32(8,true)!==b.length-16||v.getUint32(12,true)!==crc32(b.subarray(16)))throw Error('Project is damaged or uses a different format.');return b.subarray(16);}
