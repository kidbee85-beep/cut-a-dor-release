// SPDX-License-Identifier: GPL-3.0-only
class Dojo extends AudioWorkletProcessor {
 constructor(options){super();this.e=new WebAssembly.Instance(new WebAssembly.Module(options.processorOptions.bytes),{}).exports;this.e.cad_init();this.pos=256;this.fraction=0;this.primed=false;this.report=0;this.port.onmessage=({data:m})=>{try{const e=this.e;let result=true;
  if(m.type==='command'){if(e.cad_command(...m.command))throw Error('Command queue is full. Please retry.');}
  else if(m.type==='export'){e.web_export();result=new Uint8Array(e.memory.buffer,e.web_project(),e.web_project_size()).slice();}
  else if(m.type==='import'){if(m.bytes.length!==e.web_project_size()&&m.bytes.length!==2656&&m.bytes.length!==2660&&m.bytes.length!==2672)throw Error('Project format does not match.');new Uint8Array(e.memory.buffer,e.web_project(),m.bytes.length).set(m.bytes);if(e.web_import(m.bytes.length))throw Error('Project validation failed.');}
  else if(m.type==='bank'){if(m.slot<0||m.slot>2||m.bytes.length>81920)throw Error('Bank is too large.');new Uint8Array(e.memory.buffer,e.web_bank(m.slot),m.bytes.length).set(m.bytes);if(e.web_bank_load(m.slot,m.bytes.length))throw Error('Bank validation failed.');}
  if(m.request)this.port.postMessage({request:m.request,result});
 }catch(error){this.port.postMessage({request:m.request,error:error.message});}};this.port.postMessage({ready:true});
 }
 frame(){if(this.pos>=256){this.ptr=this.e.web_render(256);this.samples=new Int32Array(this.e.memory.buffer,this.ptr,512);this.pos=0;}return this.samples[this.pos++*2]/8388607;}
 process(_inputs,outputs){const e=this.e,out=outputs[0];if(!this.primed){this.a=this.frame();this.b=this.frame();this.primed=true;}
 for(let i=0;i<out[0].length;i++){const v=this.a+(this.b-this.a)*this.fraction;for(const channel of out)channel[i]=v;this.fraction+=44100/sampleRate;while(this.fraction>=1){this.a=this.b;this.b=this.frame();this.fraction-=1;}}
 this.report+=out[0].length;if(this.report>=2048){this.report=0;this.port.postMessage({state:Array.from(new Int32Array(e.memory.buffer,e.web_status(),77)),pattern:Array.from(new Uint8Array(e.memory.buffer,e.web_pattern(),512))});}return true;
 }
}
registerProcessor('cut-a-dor',Dojo);
