#!/usr/bin/env python3
"""Deterministically rebuild all Cut-A-Dor expansion banks. Python standard library only.

Input: manifest.json and compact/*.wav. Output: banks/, audition/, checks.json.
The codec matches Cut-A-Dor's seekable IMA format (132B blocks, 256 frames),
which is intentionally not a RIFF IMA WAV stream.
"""
import argparse,array,hashlib,json,math,struct,wave,zlib
from pathlib import Path
R=Path(__file__).resolve().parent
STEP=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
DELTA=[-1,-1,-1,-1,2,4,6,8]
def encode(pcm):
 out=bytearray();idx=0
 for start in range(0,len(pcm),256):
  pred=int(pcm[start]);out+=struct.pack('<hBB',pred,idx,0);codes=[]
  for x in list(pcm[start:start+256])+[0]*max(0,start+256-len(pcm)):
   d=int(x)-pred;n=8 if d<0 else 0;d=abs(d);step=STEP[idx];diff=step>>3
   for bit,val in [(4,step),(2,step>>1),(1,step>>2)]:
    if d>=val:n|=bit;d-=val;diff+=val
   pred=max(-32768,min(32767,pred+(-diff if n&8 else diff)));idx=max(0,min(88,idx+DELTA[n&7]));codes.append(n)
  out+=bytes(codes[i]|codes[i+1]<<4 for i in range(0,256,2))
 return bytes(out)
def decode(data,frames):
 out=[]
 for off in range(0,len(data),132):
  pred,idx,res=struct.unpack_from('<hBB',data,off);assert idx<=88 and res==0
  for byte in data[off+4:off+132]:
   for n in [byte&15,byte>>4]:
    step=STEP[idx];diff=(step>>3)+(step>>2 if n&1 else 0)+(step>>1 if n&2 else 0)+(step if n&4 else 0)
    pred=max(-32768,min(32767,pred+(-diff if n&8 else diff)));idx=max(0,min(88,idx+DELTA[n&7]));out.append(pred)
 return out[:frames]
def write_wav(path,pcm,rate=22050):
 with wave.open(str(path),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(struct.pack('<'+'h'*len(pcm),*pcm))
def resample(p,rate,out_rate=22050):
 # Audition only: bank PCM uses the high-quality antialiased preparation above.
 if rate==out_rate:return p
 result=[]
 for j in range(round(len(p)*out_rate/rate)):
  t=j*rate/out_rate;k=min(int(t),len(p)-1);f=t-k
  result.append(round(p[k]*(1-f)+p[min(k+1,len(p)-1)]*f))
 return result
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=R);args=ap.parse_args();out=args.output
 for d in ['banks','audition']: (out/d).mkdir(parents=True,exist_ok=True)
 manifest=json.loads((R/'manifest.json').read_text());checks=[];bank_info=[];decoded={}
 for bank in range(3):
  samples=[s for s in manifest if s['bank']==bank];assert len(samples)==16
  assert all(sum(s['category']==c for s in samples)==2 for c in range(8))
  header=bytearray(512);payload=bytearray();audition=[];tracklist=[]
  for zone,s in enumerate(samples):
   f=R/s['pcm_file'];assert hashlib.sha256(f.read_bytes()).hexdigest()==s['pcm_sha256']
   with wave.open(str(f)) as w:
    assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())==(1,2,s['rate'],s['frames'])
    pcm=list(struct.unpack('<'+'h'*s['frames'],w.readframes(s['frames'])))
   data=encode(pcm);dec=decode(data,len(pcm));decoded[s['id']]=dec
   assert len(data)==((s['frames']+255)//256)*132 and pcm[0]==0 and pcm[-1]==0
   energy=sum(x*x for x in pcm);err=sum((a-b)**2 for a,b in zip(pcm,dec));snr=10*math.log10(energy/max(err,1))
   assert max(map(abs,dec))<32767 and snr>10,(s['name'],snr)
   row=dict(id=s['id'],bank=bank,zone=zone,name=s['name'],category=s['category'],rate=s['rate'],frames=s['frames'],bytes=len(data),snr_db=round(snr,2),decoded_peak=max(map(abs,dec)),first_frame=dec[0],last_frame=dec[-1],adpcm_sha256=hashlib.sha256(data).hexdigest())
   checks.append(row);struct.pack_into('<IIHBB16s',header,32+zone*28,len(payload),s['frames'],s['rate'],s['category'],0,s['name'].encode());payload+=data
   tracklist.append(f'{len(audition)/22050:06.2f}s  {zone+1:02}. {s["name"]} (category {s["category"]})')
   audition+=resample(dec,s['rate']);audition+=[0]*round(.4*22050)
  name=samples[0]['bank_name'];filename=name.lower().replace(' ','-')+'.cadb'
  struct.pack_into('<IHHIII12s',header,0,0x42444143,1,16,len(payload),zlib.crc32(payload),0,name.encode())
  struct.pack_into('<I',header,16,zlib.crc32(header));data=header+payload;assert len(data)<=81920,(name,len(data))
  (out/'banks'/filename).write_bytes(data);write_wav(out/'audition'/(name.lower().replace(' ','-')+'-hits.wav'),audition)
  (out/'audition'/(name.lower().replace(' ','-')+'-tracklist.txt')).write_text('\n'.join(tracklist)+'\n')
  bank_info.append(dict(name=name,file=filename,count=16,bytes=len(data),free_bytes=81920-len(data),sha256=hashlib.sha256(data).hexdigest(),categories={str(c):2 for c in range(8)}))
 # A reproducible dry 88 BPM groove audition, 4 bars per bank. Actual encoded sound.
 groove=[];sr=22050;bar_frames=round(sr*240/88)
 for bank in range(3):
  rows=[s for s in manifest if s['bank']==bank];mix=[0.]*(4*bar_frames)
  for bar in range(4):
   for cat,steps in [(0,[0,6,10] if bar%2==0 else [0,3,8,11]),(1,[4,12]),(2,list(range(0,16,2))),(3,[14]),(4,[12] if bar==3 else []),(5,[3,11]),(6,[7] if bar%2 else []),(7,[14,15] if bar==3 else [])]:
    smp=rows[cat*2+(bar%2)];p=resample(decoded[smp['id']],smp['rate'])
    for step in steps:
     at=bar*bar_frames+round((step+(.1 if step%2 else 0))*bar_frames/16);gain=[.60,.55,.19,.19,.22,.22,.22,.30][cat]
     for i,v in enumerate(p):
      if at+i<len(mix):mix[at+i]+=v*gain
  assert max(map(abs,mix))<32767
  groove.extend(round(x) for x in mix);groove.extend([0]*sr)
 write_wav(out/'audition'/'three-banks-groove-88bpm.wav',groove)
 result=dict(status='PASS',format='CADB v1 / 132-byte 256-frame seekable IMA blocks',banks=bank_info,sounds=checks)
 (out/'checks.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(dict(banks=bank_info,total_sounds=len(checks),minimum_snr_db=min(x['snr_db'] for x in checks),maximum_decoded_peak=max(x['decoded_peak'] for x in checks)),indent=2))
if __name__=='__main__':main()
