#!/usr/bin/env python3
"""Package a public Cut-A-Dor release. No device or network access."""
from pathlib import Path
import hashlib, json, zipfile, shutil
R=Path(__file__).resolve().parents[1];D=R/'dist';DL=D/'downloads'
FW='Cut-A-Dor-1.2.1-Sensei-FM1.fwsc'
assert hashlib.sha256((D/'install'/FW).read_bytes()).hexdigest()=='5d4be92061cc5bf1396d3ff599ac3a1ef9ec1261afe003b74e32f4298307f614'
assert hashlib.sha256((D/'install/FM-1-V15.fwsc').read_bytes()).hexdigest()=='db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a'
source_files=[]
for name in ['firmware','host','tools','tests','assets','sound-src','build/gen','dist']:
 for p in (R/name).rglob('*'):
  if not p.is_file() or '__pycache__' in p.parts or p.name=='.DS_Store':continue
  if p.suffix in ('.zip','.fwsc','.pyc') or p.name in ['release-manifest.json','SHA256SUMS.txt']:continue
  source_files.append(p)
source_files += [R/n for n in ['LICENSE','README.md','README.txt','UPSTREAM-LICENSING.md','build.sh','.gitignore']]
source_files += [R/'docs'/n for n in ['BUILDING.txt','MIDI.txt','PROTOCOL.txt','RELEASE.txt']]
def makezip(target,items,texts={}):
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p,n in items:z.write(p,n)
  for n,text in texts.items():z.writestr(n,text)
 with zipfile.ZipFile(target) as z:
  assert z.testzip() is None
  assert not any('..' in Path(n).parts or n.startswith('/') for n in z.namelist())
makezip(D/'source.zip',[(p,p.relative_to(R).as_posix()) for p in sorted(set(source_files))])
# Keep the installer at its normal path so its relative imports stay intact.
items=[(p,'install/'+p.name) for p in (D/'install').iterdir() if p.is_file()]
items += [(D/n,n) for n in ['launch.css','source.zip','site_start.py','OPEN-WEBSITE.command','OPEN-WEBSITE-WINDOWS.cmd']]
items += [(D/'media/dojo-mon.svg','media/dojo-mon.svg')]
items += [(p,'fonts/'+p.name) for p in (D/'fonts').iterdir() if p.is_file()]
items += [(DL/n,'downloads/'+n) for n in ['QUICKSTART.txt','TEST-REPORT.txt','RECOVERY.txt','CREDITS.txt','M-VAVE-THIRD-PARTY.txt']]
items += [(R/'tools/fm1_rescue.py','fm1_rescue.py'),(R/'tools/recovery-launch.command','RESCUE-MAC.command'),(D/'install/FM-1-V15.fwsc','Original-V15.fwsc'),(DL/'RECOVERY.txt','RECOVERY.txt'),(R/'LICENSE','LICENSE')]
index='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Cut-A-Dor Recovery</title><link rel="stylesheet" href="launch.css"><main style="max-width:800px;margin:12vh auto;padding:30px"><h1>THE WAY<br>BACK HOME.</h1><p>Cut-A-Dor 1.2.1 / M-VAVE Original V15. Read the recovery guide first. No automatic flashing.</p><p><a class="button" href="install/">OPEN INSTALLER / RECOVERY</a></p><p><a href="RECOVERY.txt">Recovery guide</a> · <a href="source.zip">Complete source</a> · <a href="downloads/TEST-REPORT.txt">Test report</a></p><p>RESCUE-MAC.command is a separate last-resort tool. Do not run it for an ordinary update.</p></main></html>'''
# Remove self-downloads from this standalone kit, retaining the real procedure.
installer=(D/'install/index.html').read_text().replace('../downloads/Cut-A-Dor-Recovery-Kit.zip','../RECOVERY.txt').replace('../dojo.html','../RECOVERY.txt')
items=[(p,n) for p,n in items if n!='install/index.html']
makezip(DL/'Cut-A-Dor-Recovery-Kit.zip',items,{'index.html':index,'install/index.html':installer,'START-HERE.txt':'Read RECOVERY.txt first. Start OPEN-WEBSITE.command (Mac) or OPEN-WEBSITE-WINDOWS.cmd (Windows); Python 3 required. No automatic firmware changes.\n'})
website_items=[(p,p.relative_to(D).as_posix()) for p in D.rglob('*') if p.is_file() and p.name not in ['Cut-A-Dor-Website.zip','Cut-A-Dor-Recovery-Kit.zip','source.zip','release-manifest.json','SHA256SUMS.txt']]
website_items=[(p,n) for p,n in website_items if n not in ['index.html','dojo.html','install/index.html']]
website_pages={n:(D/n).read_text().replace('href="source.zip"','href="https://cut-a-dor.sugarypixie10.chatgpt.site/source.zip"').replace('href="../source.zip"','href="https://cut-a-dor.sugarypixie10.chatgpt.site/source.zip"').replace('href="../downloads/Cut-A-Dor-Recovery-Kit.zip"','href="https://cut-a-dor.sugarypixie10.chatgpt.site/downloads/Cut-A-Dor-Recovery-Kit.zip"') for n in ['dojo.html','install/index.html']}
makezip(DL/'Cut-A-Dor-Website.zip',website_items,{**website_pages,'index.html':(D/'index.html').read_text().replace('href="downloads/Cut-A-Dor-Website.zip"','href="START-HERE.txt"').replace('href="downloads/Cut-A-Dor-Recovery-Kit.zip"','href="https://cut-a-dor.sugarypixie10.chatgpt.site/downloads/Cut-A-Dor-Recovery-Kit.zip"').replace('href="source.zip"','href="https://cut-a-dor.sugarypixie10.chatgpt.site/source.zip"')})
assets=[('Cut-A-Dor-1.2.1-Source.zip',D/'source.zip'),(FW,D/'install'/FW),('FM-1-Original-V15.fwsc',D/'install/FM-1-V15.fwsc')]+[(n,DL/n) for n in ['Cut-A-Dor-Recovery-Kit.zip','Cut-A-Dor-Soundbanks.zip','Cut-A-Dor-Website.zip','QUICKSTART.txt','TEST-REPORT.txt']]
manifest={'version':'1.2.1','identity':'FM-1_7010201','assets':[{'name':n,'path':p.relative_to(D).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for n,p in assets]}
(D/'release-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(DL/'SHA256SUMS.txt').write_text(''.join(f"{a['sha256']}  {a['name']}\n" for a in manifest['assets']))
print(json.dumps({'sourceFiles':len(source_files),'assets':manifest['assets']},indent=2))
