// SPDX-License-Identifier: GPL-3.0-only
import {pack7,unpack7,u21,unpackBank} from './codec.mjs';
export class Device {
 constructor(){this.seq=0;this.pending=new Map();this.verified=false;this.busy=false;}
 disconnect(message='FM-1 disconnected.'){
  this.verified=false;if(this.input)this.input.onmidimessage=null;
  for(const p of this.pending.values()){clearTimeout(p.timer);p.reject(Error(message));}this.pending.clear();
  this.input=this.output=null;this.unlock?.();this.unlock=null;this.onstate?.(false,message);
 }
 async acquire(){
  if(!navigator.locks)throw Error('USB editing needs current desktop Chrome or Edge.');
  await new Promise((resolve,reject)=>{
   navigator.locks.request('fm1-firmware-update',{ifAvailable:true},lock=>{
    if(!lock){reject(Error('Another FM-1 editor or installer tab is connected. Disconnect it first.'));return;}
    return new Promise(release=>{this.unlock=release;resolve();});
   }).catch(reject);
  });
 }
 async connect(inputId,outputId){
  if(this.busy)throw Error('Wait for the current device transfer.');
  if(!navigator.requestMIDIAccess)throw Error('Web MIDI needs desktop Chrome or Edge.');
  this.access??=await navigator.requestMIDIAccess({sysex:true});
  if(!inputId)return {inputs:[...this.access.inputs.values()].filter(p=>p.state!=='disconnected'),outputs:[...this.access.outputs.values()].filter(p=>p.state!=='disconnected')};
  this.busy=true;this.disconnect();
  try{
   await this.acquire();this.input=this.access.inputs.get(inputId);this.output=this.access.outputs.get(outputId);
   if(!this.input||!this.output||this.input.state==='disconnected'||this.output.state==='disconnected')throw Error('MIDI port is missing.');
   this.access.onstatechange=()=>{if(this.input?.state==='disconnected'||this.output?.state==='disconnected')this.disconnect('USB connection lost. Reconnect before transferring.');};
   this.input.onmidimessage=({data:d})=>{
    if(d.length<9||d.length>1024||d[0]!==240||d[1]!==125||d[2]!==67||d[3]!==65||d[4]!==68||d.at(-1)!==247||d.slice(1,-1).some(x=>x>127))return;
    const p=this.pending.get(d[6]);if(!p||p.cmd!==d[5])return;
    clearTimeout(p.timer);this.pending.delete(d[6]);
    if(d[7])p.reject(Error(`FM-1: command rejected (${d[7]}). Stop the device and try again.`));else p.resolve(d.slice(8,-1));
   };
   const info=await this.request(1,[]),z=info.indexOf(0);
   if(z<10||z+1>=info.length||info[z+1]!==1)throw Error('Device is not running compatible Cut-A-Dor firmware.');
   this.name=new TextDecoder().decode(info.slice(0,z));if(!this.name.startsWith('CUT-A-DOR '))throw Error('Device is not running compatible Cut-A-Dor firmware.');
   this.verified=true;this.onstate?.(true,this.name);return this.name;
  }catch(e){this.disconnect(e.message);throw e;}finally{this.busy=false;}
 }
 request(cmd,payload,timeout=4000){
  if(!this.output||this.output.state==='disconnected')return Promise.reject(Error('FM-1 is not connected.'));
  if(this.pending.size>=16)return Promise.reject(Error('Too many pending MIDI requests.'));
  let seq=this.seq++%128;while(this.pending.has(seq))seq=this.seq++%128;
  return new Promise((resolve,reject)=>{const timer=setTimeout(()=>{this.pending.delete(seq);reject(Error('No response from FM-1. Check connection and firmware.'));},timeout);
   this.pending.set(seq,{cmd,resolve,reject,timer});try{this.output.send([240,125,67,65,68,cmd,seq,...payload,247]);}catch(e){clearTimeout(timer);this.pending.delete(seq);reject(e);}
  });
 }
 async operation(fn){if(!this.verified)throw Error('Connect to Cut-A-Dor first.');if(this.busy)throw Error('Wait for the current device transfer.');this.busy=true;try{return await fn();}finally{this.busy=false;}}
 async upload(bytes,slot,progress=()=>{}){
  if(!Number.isInteger(slot)||slot<0||slot>2)throw Error('Choose a valid bank slot.');unpackBank(bytes);
  return this.operation(async()=>{await this.request(2,[1,0,0,0]);try{await this.request(10,[slot,...u21(bytes.length)],15000);
   for(let off=0;off<bytes.length;off+=224){await this.request(11,[slot,...u21(off),...pack7(bytes.subarray(off,off+224))]);progress(Math.min(1,(off+224)/bytes.length));}
   await this.request(12,[slot]);
  }catch(e){try{await this.request(13,[]);}catch{}throw e;}});
 }
 async readProject(){return this.operation(async()=>{
  const size=await this.request(20,[]);if(size.length!==2)throw Error('Invalid project size.');const n=size[0]|size[1]<<7;if(n<1||n>3840)throw Error('Invalid project size.');
  const out=new Uint8Array(n);for(let off=0;off<n;off+=224){const raw=unpack7(await this.request(21,u21(off)));if(raw.length!==Math.min(224,n-off))throw Error('Incomplete project.');out.set(raw,off);}return out;
 });}
}
