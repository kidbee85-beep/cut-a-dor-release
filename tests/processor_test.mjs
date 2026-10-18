// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';
const code=readFileSync(new URL('../dist/processor.js',import.meta.url),'utf8');
const bytes=readFileSync(new URL('../dist/engine.wasm',import.meta.url));
for(const sampleRate of [44100,48000,96000]){
 let Processor;const messages=[];
 class AudioWorkletProcessor{constructor(){this.port={postMessage:m=>messages.push(m)};}}
 vm.runInNewContext(code,{AudioWorkletProcessor,WebAssembly,Uint8Array,Int32Array,Array,Error,sampleRate,registerProcessor:(_name,c)=>Processor=c});
 const worklet=new Processor({processorOptions:{bytes}});worklet.port.onmessage({data:{type:'command',command:[1,0,1]}});
 const n=8192,output=new Float32Array(n);for(let at=0;at<n;at+=128){const l=new Float32Array(128),r=new Float32Array(128);assert.equal(worklet.process([],[[l,r]]),true);assert.deepEqual(l,r);output.set(l,at);}
 const e=(await WebAssembly.instantiate(bytes,{})).instance.exports;e.cad_init();e.cad_command(1,0,1);
 const count=Math.ceil(n*44100/sampleRate)+2,reference=new Float64Array(count);
 for(let at=0;at<count;at+=256){const k=Math.min(256,count-at),p=e.web_render(k),raw=new Int32Array(e.memory.buffer,p,2*k);for(let i=0;i<k;i++)reference[at+i]=raw[i*2]/8388607;}
 let maxError=0;for(let i=0;i<n;i++){const at=i*44100/sampleRate,j=Math.floor(at),expected=reference[j]+(reference[j+1]-reference[j])*(at-j);assert.ok(Number.isFinite(output[i]));maxError=Math.max(maxError,Math.abs(output[i]-expected));}
 assert.ok(maxError<1e-7,`${sampleRate} Hz: ${maxError}`);assert.ok(messages.some(x=>x.state));
}
console.log('PASS: actual AudioWorklet at 44.1/48/96 kHz, stereo match, cross-block interpolation, finite output and status delivery.');
