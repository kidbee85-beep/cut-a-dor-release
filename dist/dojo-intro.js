// Cut-A-Dor brand intro. Silent, skippable, once per tab session.
const splash=document.querySelector('#dojo-splash');
const skip=document.querySelector('#skip-intro');
const replay=document.querySelector('#replay-intro');
const reduced=window.matchMedia('(prefers-reduced-motion: reduce)');
let timer=0,returnFocus=null;
function finish(){
 clearTimeout(timer);
 if(splash.hidden)return;
 splash.hidden=true;
 document.documentElement.classList.remove('intro-open');
 if(returnFocus?.isConnected)returnFocus.focus({preventScroll:true});
 returnFocus=null;
}
function start(manual=false){
 if(reduced.matches)return;
 if(!manual){try{if(sessionStorage.getItem('cad-ember-intro-v1'))return;sessionStorage.setItem('cad-ember-intro-v1','seen');}catch{}}
 clearTimeout(timer);returnFocus=manual?replay:null;
 splash.hidden=false;document.documentElement.classList.add('intro-open');
 skip.focus({preventScroll:true});
 timer=setTimeout(finish,2600);
}
skip.addEventListener('click',finish);
replay.addEventListener('click',()=>start(true));
splash.addEventListener('keydown',e=>{if(e.key==='Escape'){e.preventDefault();finish();}if(e.key==='Tab'){e.preventDefault();skip.focus();}});
function motionSetting(){replay.disabled=reduced.matches;replay.title=reduced.matches?'Animation disabled by your reduced-motion preference.':'';if(reduced.matches)finish();}
reduced.addEventListener('change',motionSetting);
document.addEventListener('visibilitychange',()=>{if(document.hidden)finish();});
window.addEventListener('pagehide',finish);
motionSetting();start();
