// SPDX-License-Identifier: GPL-3.0-only
// Cut-A-Dor installer. Protocol by Leo Kuroshita / Hügelton Instruments.
import {Updater} from './fm1ota.js';
import {checkedPackage,KNOWN_HASHES} from './package-check.mjs';
const $=id=>document.getElementById(id),pendingKey='cut-a-dor-pending-update';
let manifest={},packs={},access,busy=false,connected=false,target='cad',pending=null,logLines=[];
try{const p=JSON.parse(localStorage.getItem(pendingKey)||'null');if(p&&Object.hasOwn(KNOWN_HASHES,p.target)&&/^[a-f0-9]{64}$/.test(p.hash)){pending=p;target=p.target;}}catch{}
document.querySelectorAll('[name=target]').forEach(e=>e.checked=e.value===target);
function log(s){logLines.push(new Date().toISOString()+' '+s);if(logLines.length>500)logLines.shift();$('log').textContent=logLines.join('\n');}
function refresh(){
 $('install').disabled=busy||!connected||!packs[target]||!$('ready').checked;
 $('connect').disabled=busy;$('ready').disabled=busy;
 document.querySelectorAll('[name=target]').forEach(e=>e.disabled=busy);
 $('selectedTarget').textContent=manifest[target]?.label||target;
 $('packageStatus').textContent=packs[target]?'Fingerprint verified · '+packs[target].product+' · '+packs[target].raw.length.toLocaleString('en')+' bytes':'Checking this firmware package…';
 $('install').textContent=target==='cad'?'INSTALL CUT-A-DOR':'RESTORE '+(manifest[target]?.label||target).toUpperCase();
}
async function midi(){
 if(!navigator.requestMIDIAccess||!window.isSecureContext)throw Error('Open this page in desktop Chrome or Edge using HTTPS or the local launcher.');
 if(!access)access=await navigator.requestMIDIAccess({sysex:true});return access;
}
async function exclusive(fn){
 if(!navigator.locks)throw Error('This browser cannot lock the installation session. Use current desktop Chrome or Edge.');
 return navigator.locks.request('fm1-firmware-update',{ifAvailable:true},async lock=>{if(!lock)throw Error('Another FM-1 editor or installer tab on this site is connected. Disconnect or close it first.');return fn();});
}
function oneDevice(a){
 const inputs=[...a.inputs.values()].filter(p=>p.state!=='disconnected'&&/fm-1|felucca|ota|composite|sinco|usb-midi/i.test(p.name||''));
 if(inputs.length!==1)throw Error('Connect exactly one FM-1. Candidate MIDI inputs found: '+inputs.length);
}
async function identify(){
 const a=await midi();oneDevice(a);const up=new Updater(a,log),d=await up.find(id=>/^(ota-)?FM-1$/i.test(id.model));
 if(!d)throw Error('No FM-1 identity response. Close other MIDI apps and check the USB data cable.');
 const text=d.id.text;d.link.close();return text;
}
$('connect').onclick=async()=>{
 if(busy)return;busy=true;refresh();
 try{await exclusive(async()=>{const id=await identify();connected=true;$('device').textContent='Detected: '+id;log('Read-only identity query: '+id);});}
 catch(e){connected=false;$('device').textContent=e.message;log(e.message);}
 finally{busy=false;refresh();}
};
$('ready').onchange=refresh;
document.querySelectorAll('[name=target]').forEach(e=>e.onchange=()=>{if(busy)return;target=e.value;$('ready').checked=false;refresh();});
const messages={start:'Device confirmed. Preparing the update.',verify:'Phase 1/2: staging the update loader.',loader:'Waiting for the update loader…',write:'Phase 2/2: writing firmware. Keep power and USB connected.',reboot:'Write complete. Waiting for the firmware identity…',done:'Installation and firmware identity confirmed. Test sound and controls on the device next.'};
function step(k,n){
 $('status').textContent=messages[k]||k;
 if(k==='verify')$('progress').value=Math.min(10,n/74*10);
 if(k==='write')$('progress').value=10+Math.min(84,n/Math.ceil(packs[target].image.length/512)*84);
 if(k==='reboot')$('progress').value=95;if(k==='done')$('progress').value=100;
 if(!['verify','write'].includes(k)||n%100===0)log(k+' '+(n??''));
}
const errors={notfound:'FM-1 not found.',model:'Device model does not match.',lost:'USB disconnected. Installation is not confirmed. Keep the device powered and read Escape Hatch.',stopped:'Transfer stopped. Installation is not confirmed.',noloader:'Update loader not found. Installation is not confirmed.',noreturn:'Restart not confirmed. Save the log.',mismatch:'The device reports a different firmware identity. Success is not confirmed.',badreq:'Invalid data request. Transfer stopped.'};
$('install').onclick=async()=>{
 if(busy||!connected||!packs[target]||!$('ready').checked)return;
 const selected=target,pkg=packs[selected];busy=true;refresh();let wake;
 try{await exclusive(async()=>{
   const a=await midi();oneDevice(a);await checkedPackage(pkg.raw,KNOWN_HASHES[selected],pkg.product);
   const up=new Updater(a,log),loader=await up.find(id=>/^ota-FM-1$/i.test(id.model));
   if(loader){loader.link.close();if(pending&&pending.hash!==pkg.hash)throw Error('An interrupted update is recorded. Resume the same target package before changing firmware.');}
   try{wake=await navigator.wakeLock?.request('screen');}catch{}
   pending={target:selected,hash:pkg.hash};try{localStorage.setItem(pendingKey,JSON.stringify(pending));}catch{}
   log('START '+selected+' '+pkg.product+' SHA256 '+pkg.hash);$('progress').value=0;
   if(loader){
     log('Loader detected: restarting the write phase with the selected verified package.');
     if(!await up.resume(pkg.image,step))throw Error('The loader did not complete the write.');
     step('reboot');const back=await up.waitFor(id=>!/^ota-/i.test(id.model),40000);
     if(!back)throw Error('No firmware identity after recovery.');
     const id=back.id.text;back.link.close();
     if(id!==pkg.product)throw Error('Identity after recovery differs: '+id+'. Success is not confirmed.');
     step('done',id);
   }else await up.install(pkg.image,pkg.product,step);
   $('device').textContent='Confirmed: '+pkg.product;pending=null;try{localStorage.removeItem(pendingKey);}catch{}
   log('DONE: identity confirmed. Real-device sound and controls still need checking.');
 });}catch(e){
   $('status').textContent=selected==='stock'&&e.code==='mismatch'?'The manufacturer package was transferred, but the reported identity differs: '+e.detail+'. Check the original start screen. Restore is not automatically confirmed.':(errors[e.code]||e.message)+(e.detail?' '+e.detail:'');
   log('ERROR '+(e.code||'')+' '+e.message);connected=false;
 }finally{try{await wake?.release();}catch{}busy=false;$('ready').checked=false;refresh();}
};
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
$('download').onclick=()=>{const url=URL.createObjectURL(new Blob([logLines.join('\n')],{type:'text/plain'})),a=document.createElement('a');a.href=url;a.download='Cut-A-Dor-Installation-Log.txt';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
(async()=>{
 try{
   const res=await fetch('manifest.json',{cache:'no-store'});if(!res.ok)throw Error('Firmware manifest could not be loaded.');manifest=await res.json();
   if(pending){$('status').textContent=(pending.hash===KNOWN_HASHES[pending.target]?'Previous update not confirmed: '+pending.target+'. Its target has been selected for recovery.':'An interrupted update used a different package. If the loader is active, reopen the saved recovery kit containing that exact package.');log($('status').textContent);}
   for(const name of Object.keys(KNOWN_HASHES)){
     try{const m=manifest[name];if(!m||m.sha256!==KNOWN_HASHES[name])throw Error('Unexpected firmware fingerprint in manifest.');
       const r=await fetch(m.file,{cache:'no-store'});if(!r.ok)throw Error('Firmware download failed.');
       packs[name]=await checkedPackage(new Uint8Array(await r.arrayBuffer()),KNOWN_HASHES[name],m.product);log(name+' package verified: '+packs[name].product);
     }catch(e){log(name+': '+e.message);if(name===target)$('status').textContent=e.message;}
     refresh();
   }
 }catch(e){$('status').textContent=e.message;log(e.message);}refresh();
})();
