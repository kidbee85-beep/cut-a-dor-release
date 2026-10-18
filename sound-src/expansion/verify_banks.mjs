// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {unpackBank,packBank,decodeIMA,encodeIMA} from '../cut-a-dor/dist/codec.mjs';
const base=new URL('./',import.meta.url),manifest=JSON.parse(readFileSync(new URL('manifest.json',base))),checks=JSON.parse(readFileSync(new URL('checks.json',base)));
let total=0,minSnr=Infinity,peak=0;
for(const [bank,b] of checks.banks.entries()){
 const data=readFileSync(new URL('banks/'+b.file,base)),sounds=unpackBank(data);
 assert.equal(sounds.length,16);assert.deepEqual(Buffer.from(packBank(sounds,b.name)),data);
 for(const [i,s] of sounds.entries()){
  const row=manifest.find(x=>x.bank===bank&&x.zone===i),wav=readFileSync(new URL(row.pcm_file,base));
  const pcm=Float64Array.from({length:row.frames},(_,n)=>wav.readInt16LE(44+2*n)/32767);
  assert.deepEqual(Buffer.from(encodeIMA(pcm)),Buffer.from(s.data));
  const decoded=decodeIMA(s.data,s.frames);let energy=0,error=0;
  for(let j=0;j<decoded.length;j++){
   const ref=wav.readInt16LE(44+2*j)/32768;assert(Number.isFinite(decoded[j]));energy+=ref*ref;error+=(ref-decoded[j])**2;peak=Math.max(peak,Math.abs(decoded[j]));
  }
  const snr=10*Math.log10(energy/error);minSnr=Math.min(minSnr,snr);assert(snr>10);assert.equal(s.category,Math.floor(i/2));total++;
 }
 const broken=Buffer.from(data);broken[broken.length-1]^=1;assert.throws(()=>unpackBank(broken));assert.throws(()=>unpackBank(data.subarray(0,data.length-1)));
}
const result={status:'PASS',sounds:total,minimum_snr_db:+minSnr.toFixed(2),peak:+peak.toFixed(6),bit_identical_with_editor_encoder:true,editor_pack_unpack_roundtrip:true,damaged_and_truncated_rejected:true};
writeFileSync(new URL('browser-codec-checks.json',base),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));
