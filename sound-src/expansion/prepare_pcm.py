#!/usr/bin/env python3
"""Prepare compact acoustic PCM. Requires NumPy, SoundFile and macOS afconvert.

The committed compact/ PCM is the portable, hash-pinned input to build_banks.py.
No pitch shifting, replacement synthesis, added noise or artificial reverb.
"""
import hashlib,json,math,subprocess,wave
from pathlib import Path
import numpy as np
import soundfile as sf
R=Path(__file__).resolve().parent
spec=json.loads((R/'sources.json').read_text())
for d in ['compact','processing']: (R/d).mkdir(exist_ok=True)
manifest=[]
for s in spec:
 source=R/s['original_file'];assert hashlib.sha256(source.read_bytes()).hexdigest()==s['source_sha256']
 audio,rate=sf.read(source,always_2d=True);source_peak=float(np.max(np.abs(audio)))
 mono=np.mean(audio,axis=1)
 # Source peak and clipped-frame count remain in the manifest for a reviewable audit.
 pre=R/'processing'/(s['id']+'-mono.wav');resampled=R/'processing'/(s['id']+'-resampled.wav')
 sf.write(pre,mono,rate,subtype='PCM_24')
 subprocess.run(['/usr/bin/afconvert','-f','WAVE','-d','LEI16@'+str(s['rate']),'-c','1','--mix','-r','127',str(pre),str(resampled)],check=True,capture_output=True)
 with wave.open(str(resampled)) as w: pcm=np.frombuffer(w.readframes(w.getnframes()),'<i2').astype(float)
 peak=float(np.max(np.abs(pcm)));assert peak>16,s['name']
 first=int(np.flatnonzero(np.abs(pcm)>peak*.008)[0]);start=max(0,first-round(s['rate']*.001))
 pcm=pcm[start:start+round(s['duration_limit']*s['rate'])]
 assert len(pcm)>128,s['name']
 pcm-=np.mean(pcm)
 fade=min(round(.090*s['rate']),round(len(pcm)*.22))
 tail_before=20*math.log10(max(1e-10,float(np.sqrt(np.mean(pcm[-fade:]**2))/peak)))
 pcm[-fade:]*=.5+.5*np.cos(np.linspace(0,math.pi,fade));pcm[:8]*=np.linspace(0,1,8)
 pcm*=26000/np.max(np.abs(pcm));pcm=np.rint(pcm).astype('<i2')
 path=R/'compact'/(s['id']+'.wav')
 with wave.open(str(path),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(s['rate']);w.writeframes(pcm.tobytes())
 manifest.append(dict(s,pcm_file=str(path.relative_to(R)),pcm_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),frames=len(pcm),source_rate=rate,source_channels=audio.shape[1],source_peak=round(source_peak,6),source_clipped_samples=int(np.sum(np.abs(audio)>=.999969)),onset_trim_frames=start,tail_taper_frames=fade,tail_before_taper_db=round(tail_before,1),pcm_peak=int(np.max(np.abs(pcm.astype(int)))),))
(R/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Prepared',len(manifest),'mono PCM sounds; encoded estimate',sum(((x['frames']+255)//256)*132 for x in manifest)+3*512)
print('Worst retained tail before fade:',max((x['tail_before_taper_db'],x['name']) for x in manifest))
print('Source clipping:',[(x['name'],x['source_clipped_samples']) for x in manifest if x['source_clipped_samples']])
