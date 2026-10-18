// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {renderWav} from '../dist/bounce.mjs';
import {crc32} from '../dist/codec.mjs';
const wasm=readFileSync(new URL('../dist/engine.wasm',import.meta.url));
const e=(await WebAssembly.instantiate(wasm,{})).instance.exports;e.cad_init();
const cmd=(a,i,v)=>{assert.equal(e.cad_command(a,i,v),0);e.web_render(0);};
const project=()=>{e.web_export();return new Uint8Array(e.memory.buffer,e.web_project(),e.web_project_size()).slice();};
const shape=()=>new Uint8Array(e.memory.buffer,e.web_pattern(),256).slice().map(x=>+(x>0));
const hashes=new Set();let before=shape();for(let i=0;i<50;i++){cmd(3,0,0);const after=shape();assert.ok(after.some((x,j)=>x!==before[j]));hashes.add(crc32(after));before=after;}assert.ok(hashes.size>45);
cmd(23,0,0);const mutated=shape();const diff=mutated.reduce((n,x,i)=>n+(x!==before[i]),0);assert.ok(diff>=2&&diff<=5);cmd(7,0,0);assert.deepEqual(shape(),before);
let previous;for(const bars of [2,4,8]){const raw=project(),wav=await renderWav(wasm,raw,[],bars),v=new DataView(wav.buffer);assert.equal(new TextDecoder().decode(wav.slice(0,4)),'RIFF');assert.equal(wav.length,44+Math.round(44100*240*bars/92)*2);assert.equal(v.getUint32(24,true),44100);assert.equal(v.getInt16(44,true),0);assert.equal(v.getInt16(wav.length-2,true),0);assert.ok(wav.slice(44).some(x=>x));if(bars===2)previous=wav;}
const raw=project(),v=new DataView(raw.buffer);v.setUint16(1120,2,true);v.setUint16(1122,2,true); // count, tape bars (fixed project v2 layout)
v.setUint16(1124,0,true);raw[1126]=0;raw[1127]=7;v.setUint16(1128,1,true);v.setUint16(1130,300,true);raw[1132]=0;raw[1133]=7;v.setUint16(1134,0,true);
// Derive layout offsets from exported project size; fixed format is also checked in C.
assert.equal(raw.length,2676);
const wet=await renderWav(wasm,raw,[],2);assert.notEqual(crc32(wet),crc32(previous));
for(const [version,bytes] of [[2,2660],[3,2660],[4,2672]]){const old=project().slice(0,bytes);new DataView(old.buffer).setUint16(4,version,true);const audio=await renderWav(wasm,old,[],2);assert.ok(audio.slice(44).some(x=>x));}
await assert.rejects(()=>renderWav(wasm,new Uint8Array(12),[],2));await assert.rejects(()=>renderWav(wasm,project(),[],3));
writeFileSync(new URL('../build/pocket-bounce.wav',import.meta.url),wet);
console.log('PASS: WASM New/Mutate/Undo; 2/4/8-bar WAV lengths, format, sound, fades; FX tape audible; legacy v2/v3/v4 project bounce; invalid input rejected.');
