#!/usr/bin/env python3
"""Download only the pinned sources; verify Git's blob hash before accepting."""
import concurrent.futures,hashlib,json,urllib.request
from pathlib import Path
R=Path(__file__).resolve().parent
spec=json.loads((R/'sources.json').read_text())
def get(s):
 out=R/'originals'/s['collection']/s['path'];out.parent.mkdir(parents=True,exist_ok=True)
 data=out.read_bytes() if out.exists() else urllib.request.urlopen(s['raw_url'],timeout=60).read()
 assert len(data)==s['source_bytes']
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==s['git_blob_sha1'],s['source']
 out.write_bytes(data)
 return dict(s,original_file=str(out.relative_to(R)),source_sha256=hashlib.sha256(data).hexdigest())
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as ex: result=list(ex.map(get,spec))
(R/'sources.json').write_text(json.dumps(result,indent=2)+'\n')
seen=set();(R/'licenses').mkdir(exist_ok=True)
for s in spec:
 if s['collection'] in seen:continue
 seen.add(s['collection']);url=f"https://raw.githubusercontent.com/{s['repository']}/{s['revision']}/LICENSE"
 data=urllib.request.urlopen(url,timeout=30).read()
 assert b'CC0 1.0 Universal' in data
 (R/'licenses'/(s['collection']+'-CC0.txt')).write_bytes(data)
print('Verified',len(result),'distinct recorded source files;',sum(x['source_bytes'] for x in result),'bytes; 3 CC0 license files.')
