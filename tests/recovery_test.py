"""Exercise rescue boundaries with an in-memory flash, never real USB."""
from pathlib import Path
import sys,tempfile,hashlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import fm1_rescue as R
raw=Path('dist/install/FM-1-V15.fwsc').read_bytes()
assert hashlib.sha256(raw).hexdigest()==R.STOCK_SHA256
image=R.package_flash_image(raw)
assert len(image)==R.APP_END
class Device:
 def __init__(self,bad_chip=False,bad_head=False,bad_final=False):
  self.mem=bytearray(image+bytes([0xa5])*(R.FLASH_SIZE-len(image)))
  self.mem[R.WRITE_MIN+17]^=1;self.writes=[];self.restarted=False;self.bad_chip=bad_chip;self.bad_final=bad_final
  if bad_head:self.mem[7]^=1
 def inquiry(self):return ('WL82','UBOOT')
 def start_loader(self,loader):pass
 def chip(self):return (0 if self.bad_chip else 0x980f,3,0x856014)
 def read(self,start,size):
  b=bytes(self.mem[start:start+size])
  if self.writes and self.bad_final:b=bytes([b[0]^1])+b[1:]
  return b
 def write_sector(self,addr,data):
  assert R.WRITE_MIN<=addr<R.APP_END
  self.writes.append(addr);self.mem[addr:addr+len(data)]=data
 def run_app(self):self.restarted=True
with tempfile.TemporaryDirectory() as td:
 d=Device();before=bytes(d.mem);assert not R.rescue(d,image,b'',False,td,say=lambda *a,**k:None)
 assert not d.writes and not d.restarted
 backups=list(Path(td).glob('fm1-backup-*'));assert len(backups)==1 and backups[0].read_bytes()==before
 d=Device();tail=bytes(d.mem[R.APP_END:]);assert R.rescue(d,image,b'',True,td,say=lambda *a,**k:None)
 assert d.restarted and d.writes==[R.WRITE_MIN] and d.mem[R.APP_END:]==tail
 for option in ['bad_chip','bad_head','bad_final']:
  d=Device(**{option:True})
  try:R.rescue(d,image,b'',True,td,say=lambda *a,**k:None)
  except R.Fail:pass
  else:raise AssertionError('unsafe acceptance: '+option)
  assert not d.restarted
  if option!='bad_final':assert not d.writes
print('PASS rescue simulation: known V15 hash and container CRC; full backup before writes; check-only makes no writes; wrong chip and protected-head mismatch rejected; only application sector written; upper device data retained; final mismatch blocks reboot. No physical USB or FM-1 tested.')
