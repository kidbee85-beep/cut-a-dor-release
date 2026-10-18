// SPDX-License-Identifier: GPL-3.0-only
// Observable audio, persisted-project migration and automation regressions.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {packBank} from '../dist/codec.mjs';
const wasm=readFileSync(new URL('../dist/engine.wasm',import.meta.url));
async function engine(){const e=(await WebAssembly.instantiate(wasm,{})).instance.exports;e.cad_init();e.cad_command(32,0,0);e.web_render(0);return e;}
const cmd=(e,a,i,v)=>{assert.equal(e.cad_command(a,i,v),0);e.web_render(0);};
function render(e,n){const out=new Float64Array(n);for(let at=0;at<n;at+=256){const k=Math.min(256,n-at),p=e.web_render(k),v=new Int32Array(e.memory.buffer,p,k*2);for(let j=0;j<k;j++){assert.ok(Math.abs(v[j*2])<=8388607);out[at+j]=v[j*2]/8388607;}}return out;}
const rms=a=>Math.sqrt(a.reduce((s,x)=>s+x*x,0)/a.length);
function amplitude(a,hz){let re=0,im=0;for(let i=0;i<a.length;i++){const w=.5-.5*Math.cos(2*Math.PI*i/(a.length-1));re+=a[i]*w*Math.cos(2*Math.PI*hz*i/44100);im+=a[i]*w*Math.sin(2*Math.PI*hz*i/44100);}return Math.hypot(re,im)*4/a.length;}
async function tone(hz,{low=100,high=0,noise=0,dust=0,clean=false}={}){
 const e=await engine(),rate=16000,pcm=Float32Array.from({length:16000},(_,i)=>.55*Math.sin(2*Math.PI*hz*i/rate));
 const bank=packBank([{name:'CALIBRATION',category:0,rate,frames:pcm.length,pcm}]);new Uint8Array(e.memory.buffer,e.web_bank(0),bank.length).set(bank);assert.equal(e.web_bank_load(0,bank.length),0);
 for(const [p,v] of [[3,low],[23,high],[22,noise],[9,dust],[18,100]])cmd(e,2,p,v);
 render(e,4096);if(clean)cmd(e,6,0,0);cmd(e,21,0,128);cmd(e,12,0,127);return render(e,30000).subarray(4000);
}
const openLow=await tone(180),cutLow=await tone(180,{high:100}),openHigh=await tone(4000),cutHigh=await tone(4000,{low:25});
assert.ok(rms(cutLow)<rms(openLow)*.2,'high pass must remove low-frequency weight');
assert.ok(rms(cutHigh)<rms(openHigh)*.15,'low pass must remove high-frequency energy');
const dry=await tone(700),warm=await tone(700,{dust:100});assert.ok(rms(warm)>rms(dry));
// Linear interpolation must suppress the 16k-6k sampling image, not add a cheap
// sample-and-hold edge. The old nearest-neighbour path gives ~0.6 at this ratio.
const highTone=await tone(6000,{clean:true}),imageRatio=amplitude(highTone,10000)/amplitude(highTone,6000);
assert.ok(imageRatio<.45,`unexpected interpolation image ratio ${imageRatio}`);

const e=await engine();for(let t=0;t<8;t++)cmd(e,11,t,0);cmd(e,2,22,100);cmd(e,1,0,1);
const hiss=render(e,44100);assert.ok(rms(hiss)>.001&&rms(hiss)<.015,'noise level must be useful and bounded');
cmd(e,6,0,0);render(e,44100);assert.ok(rms(render(e,4096))<1e-6,'Clean must bypass noise');
cmd(e,2,22,100);render(e,4096);cmd(e,1,0,0);render(e,44100);assert.ok(rms(render(e,4096))<1e-6,'Stop must settle to silence');
const a=await engine(),b=await engine();cmd(a,2,22,100);cmd(b,2,22,0);cmd(a,1,0,1);cmd(b,1,0,1);render(a,44100);render(b,44100);cmd(a,3,0,0);cmd(b,3,0,0);
assert.deepEqual(new Uint8Array(a.memory.buffer,a.web_pattern(),256),new Uint8Array(b.memory.buffer,b.web_pattern(),256),'noise must not change the musical random sequence');

// A real v1-shaped project carries both parts, kit assignments and events.
const m=await engine();m.web_export();const current=new Uint8Array(m.memory.buffer,m.web_project(),m.web_project_size()).slice();assert.equal(current.length,2676);
const legacy=new Uint8Array(2656);legacy.set(current.subarray(0,56));legacy.set(current.subarray(60,2660),56);new DataView(legacy.buffer).setUint32(4,1,true);
new Uint8Array(m.memory.buffer,m.web_project(),legacy.length).set(legacy);assert.equal(m.web_import(legacy.length),0);m.web_export();const upgraded=new Uint8Array(m.memory.buffer,m.web_project(),2660).slice();
assert.equal(new DataView(upgraded.buffer).getUint32(4,true),5);assert.deepEqual(upgraded.subarray(60),current.subarray(60,2660));assert.deepEqual(upgraded.subarray(12,56),current.subarray(12,56));assert.ok(upgraded.subarray(56,60).every(x=>x===0));
assert.notEqual(m.web_import(2655),0);legacy[0]=0;new Uint8Array(m.memory.buffer,m.web_project(),legacy.length).set(legacy);assert.notEqual(m.web_import(legacy.length),0);

const rec=await engine();cmd(rec,2,12,1);cmd(rec,2,13,0);cmd(rec,5,0,0);render(rec,1);cmd(rec,2,22,62);cmd(rec,2,23,48);
for(let i=0;i<500&&new Int32Array(rec.memory.buffer,rec.web_status(),66)[1]<384;i++)render(rec,256);
rec.web_export();const take=new Uint8Array(rec.memory.buffer,rec.web_project(),2660).slice(),v=new DataView(take.buffer);assert.equal(v.getUint16(1120,true),2);assert.equal(take[1127],22);assert.equal(take[1133],23);assert.equal(rec.web_import(rec.web_project_size()),0);
const report={result:'PASS',projectFormat:5,oldProjectMigration:'v1 -> v5; patterns/maps/events preserved; new color controls off',highPassRmsRatio:rms(cutLow)/rms(openLow),lowPassRmsRatio:rms(cutHigh)/rms(openHigh),interpolationImageRatio:imageRatio,maximumNoiseRms:rms(hiss),checks:['full-rate low/high pass response','warmth response','interpolation image rejection','noise level, Clean and Stop silence','noise independent of beat seed','legacy and corrupt project handling','Noise and High Pass FX-tape record/import']};
writeFileSync(new URL('../build/tone-tests.json',import.meta.url),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(report,null,2));
