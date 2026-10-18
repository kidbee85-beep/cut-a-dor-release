import assert from 'node:assert/strict';import fs from 'node:fs';
const source=fs.readFileSync('dist/install/fm1ota.js','utf8');
const {Updater,pack7,unpack7,parseIdentity,parseRequest,Link}=await import('data:text/javascript;base64,'+Buffer.from(source+'\nexport {parseIdentity,parseRequest,Link};').toString('base64'));
const realTimeout=globalThis.setTimeout;globalThis.setTimeout=(fn,ms,...args)=>realTimeout(fn,Math.min(ms,2),...args);
function request(addr,len=(addr===0xe0000000||addr===0xf0000000)?8:32){let b=[0,89,48,8,0,0,0,addr&255,(addr>>>8)&255,(addr>>>16)&255,(addr>>>24)&255,len&255,(len>>>8)&255,(len>>>16)&255];let sum=b.slice(6).reduce((a,b)=>a+b,0);b.push(~sum&255);return [240,...pack7(b),247];}
function link(packets,{lost=false,sendOK=true}={}){return {lost,closed:false,sent:[],async read(){return packets.shift()||null;},send(p){this.sent.push(p);return sendOK;},close(){this.closed=true;}};}
for(let n=0;n<1024;n++){let a=Array.from({length:n%513},(_,i)=>(i*131+n)&255);assert.deepEqual(unpack7(pack7(a)),a);}
assert.equal(parseIdentity([0,...Array(40).fill(0)]),null);
function identity(text){const b=[0,89,17,27,0,0,...Array.from({length:27},(_,i)=>text.charCodeAt(i)||0)];b.push(~b.slice(6).reduce((a,b)=>a+b,0)&255);return b;}
const good=identity('FM-1_015');assert.equal(parseIdentity([240,...pack7(good),247]).text,'FM-1_015');
for(const offset of [3,4,5,12,33]){const bad=[...good];bad[offset]^=1;assert.equal(parseIdentity([240,...pack7(bad),247]),null);}
const badHeader=unpack7(request(0).slice(1,-1));badHeader[3]=9;assert.equal(parseRequest([240,...pack7(badHeader),247]),null);
const inp={state:'connected'},out={state:'connected',send(){}};const l=new Link(inp,out);for(let i=0;i<10000;i++)inp.onmidimessage({data:[240,1,247]});assert.equal(l.q.length,128);l.close();assert.equal(inp.onmidimessage,null);
const u=new Updater({}),image=new Uint8Array(1024);
for(const [addr,len] of [[1020,16],[0,513],[0,0],[0xffffffff,32]])await assert.rejects(u.serve(link([request(addr,len)]),image,0xf0000000,1),e=>e.code==='badreq');
for(const [address,length] of [[0xe0000000,8],[0xf0000000,0],[0xf0000000,32],[0xf0000000,513]]){
 const invalid=link([request(address,length)]);await assert.rejects(u.serve(invalid,image,0xf0000000,1),e=>e.code==='badreq');assert.equal(invalid.sent.length,0);
}
const duplicate=link([request(0xf0000000),request(0xf0000000)]);
assert((await u.serve(duplicate,image,0xf0000000,1)).finished);assert.equal(duplicate.sent.length,2);
await assert.rejects(u.serve(link([request(0xe0000000),request(0xf0000000)]),image,0xe0000000,1),e=>e.code==='badreq');
let c=link([request(0,512),request(512,512),request(0xf0000000)]);assert.equal((await u.serve(c,image,0xf0000000,1)).finished,true);assert.equal(c.sent.length,3);
assert.equal((await u.serve(link([],{lost:true}),image,0xf0000000,1)).lost,true);
assert.equal((await u.serve(link([request(0xf0000000)],{sendOK:false}),image,0xf0000000,1)).finished,false);
for(const expected of ['FM-1_7010000','FM-1_015']){
 const first=link([request(0),request(0xe0000000)]),second=link([request(0),request(0xf0000000)]),back=link([]);let stages=[];
 const test=new Updater({});test.find=async()=>({id:{model:'FM-1',text:'FM-1_015'},link:first});let replies=[{id:{model:'ota-FM-1',text:'ota-FM-1_1'},link:second},{id:{model:'FM-1',text:expected},link:back}];test.waitFor=async()=>replies.shift();
 if(expected==='FM-1_7010000')assert.equal(await test.install(image,'FM-1_7010000',s=>stages.push(s)),expected);
 else await assert.rejects(test.install(image,'FM-1_7010000',s=>stages.push(s)),e=>e.code==='mismatch');
 assert(first.closed&&second.closed&&back.closed);assert.equal(stages.includes('done'),expected==='FM-1_7010000');
}
const bad=link([request(99999)]);const test=new Updater({});test.find=async()=>({id:{model:'FM-1',text:'FM-1_015'},link:bad});await assert.rejects(test.install(image,'FM-1_7010000'),e=>e.code==='badreq');assert(bad.closed);
const resumeLink=link([request(0xf0000000)]);test.find=async()=>({id:{model:'ota-FM-1'},link:resumeLink});assert(await test.resume(image));assert(resumeLink.closed);
globalThis.setTimeout=realTimeout;
console.log('PASS: 1024 binary roundtrips; bounded SysEx queue; malformed frames; zero/oversize/out-of-range reads; disconnect; failed completion ACK; install/identity mismatch; cleanup on error; loader resume; identity checksum/header; wrong-phase and invalid-length completion rejected; duplicate completion acknowledged; unexpected post-completion request rejected. Simulated protocol, not hardware.');
