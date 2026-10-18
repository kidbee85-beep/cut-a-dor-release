// SPDX-License-Identifier: GPL-3.0-only
// A fresh copy of the firmware engine renders the selected part and saved FX tape.
export async function renderWav(wasm,project,banks,bars){
 if(![2,4,8].includes(bars))throw Error('Choose 2, 4 or 8 bars.');
 const {instance}=await WebAssembly.instantiate(wasm,{}),e=instance.exports;e.cad_init();
 for(let slot=0;slot<3;slot++)if(banks[slot]){const b=banks[slot];new Uint8Array(e.memory.buffer,e.web_bank(slot),b.length).set(b);if(e.web_bank_load(slot,b.length))throw Error('Invalid user bank.');}
 if(project.length!==e.web_project_size()&&project.length!==2656&&project.length!==2660&&project.length!==2672)throw Error('Invalid project size.');
 new Uint8Array(e.memory.buffer,e.web_project(),project.length).set(project);if(e.web_import(project.length))throw Error('Invalid project.');
 const cmd=(a,i,v)=>{e.cad_command(a,i,v);e.web_render(0);};cmd(2,2,0);cmd(2,19,0);cmd(1,0,1);
 let st=new Int32Array(e.memory.buffer,e.web_status(),66);if(st[7]&&!st[11])cmd(15,0,0);
 const frames=Math.round(44100*240*bars/st[16]),warmup=Math.round(44100*240*8/st[16]);
 const out=new Uint8Array(44+frames*2),v=new DataView(out.buffer);const tag=(n,s)=>{for(let i=0;i<s.length;i++)out[n+i]=s.charCodeAt(i);};tag(0,'RIFF');v.setUint32(4,36+frames*2,true);tag(8,'WAVE');tag(12,'fmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,44100,true);v.setUint32(28,88200,true);v.setUint16(32,2,true);v.setUint16(34,16,true);tag(36,'data');v.setUint32(40,frames*2,true);
 for(let at=0;at<warmup;at+=256)e.web_render(Math.min(256,warmup-at));
 for(let at=0;at<frames;at+=256){const n=Math.min(256,frames-at),p=e.web_render(n),pcm=new Int32Array(e.memory.buffer,p,n*2);for(let i=0;i<n;i++){const edge=Math.min(1,(at+i)/64,(frames-at-i-1)/64);v.setInt16(44+(at+i)*2,Math.round(Math.max(-1,Math.min(1,pcm[i*2]/8388607))*32767*edge),true);}}
 return out;
}
