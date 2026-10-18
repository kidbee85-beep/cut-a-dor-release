// Simulated DOM and device. Never opens MIDI or writes hardware.
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),{webcrypto}=require('node:crypto'),{parseHTML}=require('linkedom');
async function fixture(options={}){
 const {window}=parseHTML(fs.readFileSync('dist/install/index.html','utf8')),document=window.document;
 window.isSecureContext=true;const saved=options.pending?{'cut-a-dor-pending-update':JSON.stringify(options.pending)}:{};
 let writes=0,resumes=0,probes=0,id='FM-1_015',loader=!!options.loader;
 class Updater{
  async find(filter){probes++;const d={model:loader?'ota-FM-1':'FM-1',text:loader?'ota-FM-1_1':id};return !filter||filter(d)?{id:d,link:{close(){}}}:null;}
  async install(image,product,step){writes++;if(options.fail)throw Object.assign(Error('Simulated interruption'),{code:'lost'});id=product;step('done',id);}
  async resume(image,step){resumes++;return true;}
  async waitFor(){return {id:{model:'FM-1',text:options.wrongIdentity?'FM-1_999':'FM-1_7010201'},link:{close(){}}};}
 }
 const c={window,document,console,crypto:webcrypto,Uint8Array,Blob,URL,Date,JSON,Object,setTimeout,Updater,localStorage:{getItem:k=>saved[k]??null,setItem:(k,v)=>saved[k]=v,removeItem:k=>delete saved[k]},navigator:{requestMIDIAccess:async()=>({inputs:new Map(Array.from({length:options.devices??1},(_,i)=>[i,{state:'connected',name:'FM-1'}]))}),locks:{request:async(name,opts,fn)=>fn(options.locked?null:{name})}},fetch:async path=>{
  let b=fs.readFileSync('dist/install/'+path);if(options.corrupt&&path==='Cut-A-Dor-1.2.1-Sensei-FM1.fwsc'){b=Buffer.from(b);b[b.length-1]^=1;}
  return {ok:true,json:async()=>JSON.parse(b),arrayBuffer:async()=>b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength)};
 }};
 vm.createContext(c);
 for(const f of ['fm1pkg.js','package-check.mjs','installer.js'])vm.runInContext(fs.readFileSync('dist/install/'+f,'utf8').replace(/^import .*;$/gm,'').replaceAll('export ',''),c);
 const $=id=>document.getElementById(id);
 for(let i=0;i<100&&!$('log').textContent.includes('stock package verified');i++)await new Promise(r=>setTimeout(r,10));
 assert($('log').textContent.includes('stock package verified'),'stock loading completed');
 return {c,document,$,saved,stats:()=>({writes,resumes,probes,id}),async prepare(){await $('connect').onclick();$('ready').checked=true;$('ready').onchange();},select(value){document.querySelector(`[value=${value}]`).onchange();}};
}
(async()=>{
 let f=await fixture();assert.equal(f.stats().writes,0);assert.equal(f.stats().probes,0);assert(f.$('install').disabled);
 assert.deepEqual([...f.document.querySelectorAll('[name=target]')].map(x=>x.value),['cad','stock']);
 await f.prepare();assert(f.$('device').textContent.includes('FM-1_015'));assert.equal(f.stats().writes,0);assert.equal(f.$('install').disabled,false);
 await f.$('install').onclick();assert.equal(f.stats().writes,1);assert.equal(f.stats().id,'FM-1_7010201');assert.equal(f.$('progress').value,100);assert(!f.saved['cut-a-dor-pending-update']);
 f.select('stock');assert(!f.$('ready').checked);assert(f.$('install').disabled);await f.prepare();await f.$('install').onclick();assert.equal(f.stats().writes,2);assert.equal(f.stats().id,'FM-1_015');
 f=await fixture({corrupt:true});await f.prepare();assert(f.$('install').disabled);await f.$('install').onclick();assert.equal(f.stats().writes,0);
 f=await fixture({devices:2});await f.prepare();assert(f.$('install').disabled);assert(f.$('device').textContent.includes('exactly one'));assert.equal(f.stats().writes,0);
 f=await fixture({locked:true});await f.prepare();assert(f.$('install').disabled);assert(f.$('device').textContent.includes('Another FM-1'));assert.equal(f.stats().writes,0);
 f=await fixture({fail:true});await f.prepare();await f.$('install').onclick();assert(f.saved['cut-a-dor-pending-update']);assert.notEqual(f.$('progress').value,100);assert(f.$('status').textContent.includes('not confirmed'));assert(f.$('install').disabled);
 f=await fixture({loader:true});await f.prepare();await f.$('install').onclick();assert.equal(f.stats().resumes,1);assert.equal(f.$('progress').value,100);
 f=await fixture({loader:true,wrongIdentity:true});await f.prepare();await f.$('install').onclick();assert.equal(f.stats().resumes,1);assert.notEqual(f.$('progress').value,100);assert(f.$('status').textContent.includes('not confirmed'));
 const hash=JSON.parse(fs.readFileSync('dist/install/manifest.json')).cad.sha256;
 f=await fixture({loader:true,pending:{target:'cad',hash}});f.select('stock');await f.prepare();await f.$('install').onclick();assert.equal(f.stats().resumes,0);assert(f.$('status').textContent.includes('same target'));
 f=await fixture({loader:true,pending:{target:'cad',hash:'589595e0069d49fb774510b5bc41252d8c3882f32a30ba2a140d0587510e68dc'}});await f.prepare();await f.$('install').onclick();assert.equal(f.stats().resumes,0);assert(f.$('status').textContent.includes('same target'));
 console.log('PASS installer UI simulation: stock -> Cut-A-Dor -> stock; zero automatic device queries or writes; explicit acknowledgement; correct target list; corrupted file rejected; multiple-device guard; tab lock; interruption retained; loader resume; mismatched identity not marked successful; interrupted-target guard. Mock device only.');
})().catch(e=>{console.error(e);process.exit(1)});
