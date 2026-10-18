// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
const wasm=readFileSync(new URL('../dist/engine.wasm',import.meta.url));
const cmd=(e,a,i,v)=>{assert.equal(e.cad_command(a,i,v),0);e.web_render(0);};
async function engine(punch=0){const e=(await WebAssembly.instantiate(wasm,{})).instance.exports;e.cad_init();cmd(e,32,0,punch);cmd(e,2,22,0);cmd(e,2,2,0);return e;}
function render(e,n){const a=new Float32Array(n);for(let at=0;at<n;at+=256){const k=Math.min(256,n-at),p=e.web_render(k),v=new Int32Array(e.memory.buffer,p,2*k);for(let j=0;j<k;j++){a[at+j]=v[2*j]/8388607;assert.ok(Math.abs(a[at+j])<=.950001);}}return a;}
const metrics=a=>({rms:Math.sqrt(a.reduce((s,x)=>s+x*x,0)/a.length),peak:a.reduce((s,x)=>Math.max(s,Math.abs(x)),0)});
const reports=[];
for(let kit=0;kit<8;kit++){const row={kit};for(const punch of [0,50,100]){const e=await engine(punch);cmd(e,2,8,kit);render(e,4096);cmd(e,1,0,1);row[punch]=metrics(render(e,44100*6));}row.gainDb=20*Math.log10(row[50].rms/row[0].rms);assert.ok(row.gainDb>4&&row.gainDb<9,'middle PUNCH should add audible level, without excessive makeup');reports.push(row);}
// Lane gain follows the assigned pad, even if the sample category differs.
const e=await engine();cmd(e,6,0,0);cmd(e,21,1,0);cmd(e,31,1,0);render(e,8192);cmd(e,12,1,127);assert.ok(metrics(render(e,12000)).peak<.000001);
cmd(e,31,1,100);render(e,8192);cmd(e,12,1,127);assert.ok(metrics(render(e,12000)).peak>.01);
// All voices, full lane boosts, maximum punch, extreme FX and repeated changes.
const x=await engine(100);for(let t=0;t<8;t++)cmd(x,31,t,200);cmd(x,2,18,100);cmd(x,2,14,100);cmd(x,2,9,100);cmd(x,1,0,1);
let max=0;for(let n=0;n<80;n++){for(let p=0;p<16;p++)cmd(x,12,p,127);cmd(x,4,n%8,1);if(n>8)cmd(x,4,(n+1)%8,0);max=Math.max(max,metrics(render(x,2048)).peak);}
cmd(x,22,0,0);render(x,44100);assert.ok(metrics(render(x,4096)).peak<1e-6);
writeFileSync(new URL('../build/punch-tests.json',import.meta.url),JSON.stringify({result:'PASS',reports,stressPeak:max,checks:['eight factory kit level comparisons','soft peak bound','remapped sample follows lane','16 voices + boosted lanes + FX','panic settles to silence']},null,2));
console.log('PUNCH: eight kits gain dB',reports.map(x=>x.gainDb.toFixed(2)).join(', '),'stress peak',max.toFixed(4));
