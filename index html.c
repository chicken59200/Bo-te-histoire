#pragma once
#include <Arduino.h>

// Page web servie par l'ESP32 (modifiable ici)
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html lang=fr><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,viewport-fit=cover"><title>Boîte à histoires</title>
<style>
:root{--bg:#f6f5f1;--s:#fff;--bd:rgba(0,0,0,.12);--bds:rgba(0,0,0,.3);--t1:#1c1c1a;--t2:#6b6b66;--ac:#2f6fed;--acb:#e3ecfd;--okb:#dff3e6;--okt:#1a6d3c;--wb:#fdf1cf;--wbd:#e6c96a;--wt:#7a5600;--dg:#b3261e}
@media(prefers-color-scheme:dark){:root{--bg:#161615;--s:#232321;--bd:rgba(255,255,255,.14);--bds:rgba(255,255,255,.3);--t1:#eeeeea;--t2:#a3a39c;--ac:#7aa5ff;--acb:#22304f;--okb:#1d3d2a;--okt:#7fd6a0;--wb:#3d3212;--wbd:#7a6420;--wt:#f0cf73;--dg:#ff8a80}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--t1);font-family:system-ui,-apple-system,"Segoe UI",sans-serif;font-size:16px}
.app{max-width:560px;margin:auto}
header{display:flex;align-items:center;gap:8px;padding:calc(14px + env(safe-area-inset-top,0px)) 14px 6px;font-size:18px;font-weight:500}
#v{padding:8px 14px 100px}
svg{fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;flex:none;vertical-align:-3px}
button{font:inherit;font-size:15px;min-height:44px;border-radius:10px;border:1px solid var(--bds);background:transparent;color:var(--t1);padding:0 14px;cursor:pointer}
button:active{transform:scale(.98)}button:disabled{opacity:.4;cursor:default}
input[type=text],select{font:inherit;font-size:16px;width:100%;min-height:44px;border-radius:10px;border:1px solid var(--bds);background:var(--s);color:var(--t1);padding:0 12px;margin-bottom:8px}
input[type=range]{flex:1;height:32px;accent-color:var(--ac)}
.tabs{position:fixed;left:0;right:0;bottom:0;display:flex;background:var(--s);border-top:1px solid var(--bd);padding-bottom:env(safe-area-inset-bottom,0px)}
.tab{flex:1;border:0;border-radius:0;background:transparent;min-height:58px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:2px;font-size:12px;color:var(--t2)}
.tab.on{color:var(--ac);font-weight:500}
.box,.item{background:var(--s);border:1px solid var(--bd);border-radius:14px;padding:12px;margin-bottom:10px}
.item{padding:0;overflow:hidden}.item.pair{background:var(--wb);border-color:var(--wbd)}
.hd{display:flex;align-items:center;gap:10px;width:100%;border:0;border-radius:0;text-align:left;padding:12px;min-height:60px}
.nm{flex:1;min-width:0;display:flex;flex-direction:column}
.nm b{font-weight:500;font-size:16px;word-break:break-word}.nm small{font-size:13px;color:var(--t2)}
.chip{font-size:12px;padding:3px 9px;border-radius:99px;background:var(--bg);color:var(--t2);white-space:nowrap}
.chip.ok{background:var(--acb);color:var(--ac)}.chip.play{background:var(--okb);color:var(--okt)}
.det{padding:0 12px 12px;border-top:1px solid var(--bd)}
.f{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:6px 0;border-bottom:1px solid var(--bd);font-size:15px;word-break:break-word}
.x{border:0;width:36px;min-height:36px;padding:0;color:var(--t2);display:flex;align-items:center;justify-content:center}
.up{display:flex;align-items:center;gap:6px;font-size:14px;color:var(--t2);padding:12px 0;cursor:pointer}
.row{display:flex;gap:8px;align-items:center}.wrap{flex-wrap:wrap}.between{justify-content:space-between;margin-bottom:10px}
.lbl{font-size:13px;color:var(--t2);margin-bottom:6px}
.seg{min-height:36px;padding:0 12px;border-radius:99px}.seg.on{background:var(--s);border-color:var(--t2);font-weight:500}
.pri{background:var(--t1);color:var(--s);border-color:var(--t1)}.sm{min-height:36px;padding:0 12px}.dng{color:var(--dg)}
.np{text-align:center;padding:8px 0 4px}
.art{width:132px;height:132px;border-radius:16px;background:var(--s);border:1px solid var(--bd);margin:6px auto 14px;display:flex;align-items:center;justify-content:center;color:var(--t2)}
.ttl{font-size:20px;font-weight:500}.sub{font-size:14px;color:var(--t2);margin:2px 0 16px;word-break:break-word}
.ctl{display:flex;justify-content:center;align-items:center;gap:20px;margin-bottom:18px}
.rb{width:56px;height:56px;border-radius:50%;padding:0;display:flex;align-items:center;justify-content:center}
.rb.big{width:68px;height:68px;background:var(--t1);color:var(--s);border-color:var(--t1)}
.vol{display:flex;align-items:center;gap:10px;color:var(--t2);margin-bottom:14px}
.wait{display:flex;align-items:center;gap:8px;color:var(--wt);font-size:15px;margin:10px 0}
.empty{text-align:center;color:var(--t2);padding:28px 0}.err{font-size:13px;color:var(--dg);margin-bottom:8px}
#pg{width:100%;height:8px;margin:0 0 6px}#stx{font-size:13px;color:var(--t2)}
.kv{display:flex;justify-content:space-between;gap:12px;padding:6px 0;border-bottom:1px solid var(--bd);font-size:14px}.kv:last-child{border:0}.kv span:last-child{text-align:right;word-break:break-word}
.e{color:var(--dg)}
.lg{font:12px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;background:var(--s);border:1px solid var(--bd);border-radius:12px;padding:10px;height:280px;overflow:auto;word-break:break-all}
.lg div{padding:1px 0}
</style></head><body>
<div class=app><header><span id=ttl style="display:flex;align-items:center;gap:8px"></span></header>
<div style="padding:0 14px"><progress id=pg hidden max=1></progress><div id=stx></div></div>
<main id=v></main></div>
<nav class=tabs id=t></nav>
<script>
const P={folder:'M5 4h4l3 3h7a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2',
x:'M18 6L6 18M6 6l12 12',upload:'M4 17v2a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2M7 9l5-5 5 5M12 4v12',
book:'M3 19a9 9 0 0 1 9 0a9 9 0 0 1 9 0M3 6a9 9 0 0 1 9 0a9 9 0 0 1 9 0M3 6v13M12 6v13M21 6v13',
radio:'M11 12a1 1 0 1 0 2 0a1 1 0 1 0-2 0M15.5 8.5a5 5 0 0 1 0 7M8.5 15.5a5 5 0 0 1 0-7M18.5 5.5a9 9 0 0 1 0 13M5.5 18.5a9 9 0 0 1 0-13',
nfc:'M8.5 8.5a5 5 0 0 1 0 7M5.5 5.5a9 9 0 0 1 0 13M11.5 12h.01M14.5 8.5a5 5 0 0 1 0 7',
headphones:'M4 15v-3a8 8 0 0 1 16 0v3M4 15a2 2 0 0 1 2-2h1v6H6a2 2 0 0 1-2-2zM20 15a2 2 0 0 0-2-2h-1v6h1a2 2 0 0 0 2-2z',
list:'M8 6h12M8 12h12M8 18h12M4 6h.01M4 12h.01M4 18h.01',
card:'M3 7a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2zM3 10h18M7 15h2',
play:'M7 4l13 8-13 8z',pause:'M8 5v14M16 5v14',back:'M19 5v14L8 12zM5 5v14',next:'M5 5v14l11-7zM19 5v14',
vol:'M4 9v6h4l5 4V5L8 9zM16 9a4 4 0 0 1 0 6',plus:'M12 5v14M5 12h14',down:'M6 9l6 6 6-6',up:'M6 15l6-6 6 6',
bug:'M9 9v-1a3 3 0 0 1 6 0v1M8 9h8a1 1 0 0 1 1 1v3a5 5 0 0 1-10 0v-3a1 1 0 0 1 1-1M3 13h4M17 13h4M12 20v-6M4 19l3-3M20 19l-3-3M4 7l3 3M20 7l-3 3'};
const S={dbg:{},log:[],next:0,tab:'play',f:'all',open:'',add:false,nt:'d',cf:'',err:'',items:[],pairing:'',playing:'',st:{volume:10,paused:false,track:'',idx:-1,count:0,src:'',last:''}};
const $=s=>document.querySelector(s);
const esc=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
const ic=(n,z)=>`<svg viewBox="0 0 24 24" width="${z||18}" height="${z||18}" aria-hidden="true"><path d="${P[n]}"/></svg>`;
const api=(p,q)=>fetch('/api/'+p+'?'+new URLSearchParams(q||{}),{method:'POST'});
const find=k=>S.items.find(x=>x.key===k);
const chip=it=>it.u?`<span class="chip ok">${esc(it.u)}</span>`:'<span class="chip">Sans carte</span>';
const conf=it=>`<div class="wait">Dissocier la carte ${esc(it.u)} ?</div><div class="row"><button class="dng" data-a="unpok" data-key="${esc(it.key)}">Oui, dissocier</button><button data-a="unpno">Annuler</button></div>`;
$('#ttl').innerHTML=ic('book',22)+'Boîte à histoires';
function vPlay(){
 const st=S.st,p=find(st.src)||find(S.playing),r=p&&p.t==='r',run=!!S.playing&&!st.paused;
 const sub=!p?'Passe une carte ou choisis un contenu':r?'Radio web':'Piste '+(st.idx+1)+' sur '+st.count+' : '+st.track;
 const L=st.last,k=L&&S.items.find(x=>x.u===L);
 return `<div class="np"><div class="art">${ic(p?(r?'radio':'folder'):'headphones',48)}</div>
 <div class="ttl">${p?esc(p.n):'Rien en lecture'}</div><div class="sub">${esc(sub)}</div>
 <div class="ctl"><button class="rb" data-a="prev" ${!p||r?'disabled':''} aria-label="Piste précédente">${ic('back',22)}</button>
 <button class="rb big" data-a="pp" ${!p?'disabled':''} aria-label="Lecture ou pause">${ic(run?'pause':'play',28)}</button>
 <button class="rb" data-a="next" ${!p||r?'disabled':''} aria-label="Piste suivante">${ic('next',22)}</button></div></div>
 <div class="vol">${ic('vol')}<input type="range" min="0" max="21" step="1" value="${st.volume}" id="vol" aria-label="Volume"><span id="vv" style="min-width:24px;text-align:right">${st.volume}</span></div>
 <div class="box"><div class="lbl">Dernière carte lue</div>${L?`<div class="row">${ic('card',20)}<span class="nm"><b>${esc(L)}</b><small>${k?esc(k.n):'Carte inconnue'}</small></span></div>`:'<div style="color:var(--t2)">Aucune carte lue</div>'}</div>`;
}
function row(it){
 const o=S.open===it.key,pl=S.playing===it.key,pr=S.pairing===it.key,K=esc(it.key);
 const meta=it.t==='d'?it.files.length+' fichier'+(it.files.length>1?'s':''):'Radio web';
 let d='';
 if(o){
  d=`<div class="det">${it.t==='d'?it.files.map(f=>`<div class="f"><span>${esc(f)}</span><button class="x" data-a="rmf" data-key="${K}" data-f="${esc(f)}" aria-label="Supprimer ${esc(f)}">${ic('x',16)}</button></div>`).join('')+`<label class="up">${ic('upload',16)}Ajouter des MP3<input type="file" accept=".mp3,audio/mpeg" multiple data-key="${K}" style="display:none"></label>`:`<div class="f" style="border:0;color:var(--t2);font-size:13px;word-break:break-all">${esc(it.url)}</div>`}
  ${pr?`<div class="wait">${ic('nfc',20)}Passe la carte sur le lecteur…</div>`
  :S.cf===it.key?conf(it)
  :`<div class="row wrap" style="margin-top:8px"><button class="pri" data-a="play" data-key="${K}">${ic('play',16)} Lire</button><button data-a="pair" data-key="${K}">${it.u?'Changer de carte':'Associer une carte'}</button>${it.u?`<button data-a="unp" data-key="${K}">Dissocier</button>`:''}<button class="dng" data-a="del" data-key="${K}">Supprimer</button></div>`}</div>`;
 }
 return `<div class="item ${pr?'pair':''}"><button class="hd" data-a="open" data-key="${K}" aria-expanded="${o}">${ic(it.t==='d'?'folder':'radio',22)}<span class="nm"><b>${esc(it.n)}</b><small>${meta}</small></span>${pl?'<span class="chip play">En lecture</span>':chip(it)}${ic(o?'up':'down',16)}</button>${d}</div>`;
}
function vLib(){
 const seg=[['all','Tout'],['d','Dossiers'],['r','Radios']].map(([k,l])=>`<button class="seg ${S.f===k?'on':''}" data-a="f" data-k="${k}">${l}</button>`).join('');
 const form=S.add?`<div class="box"><div class="row" style="margin-bottom:8px"><button class="seg ${S.nt==='d'?'on':''}" data-a="nt" data-k="d">Dossier</button><button class="seg ${S.nt==='r'?'on':''}" data-a="nt" data-k="r">Radio web</button></div>
 <input type="text" id="nn" placeholder="Nom">${S.nt==='r'?'<input type="text" id="nu" placeholder="https://… (URL du flux)">':''}
 ${S.err?`<div class="err">${S.err}</div>`:''}
 <div class="row"><button class="pri" data-a="create">Créer</button><button data-a="cancel">Annuler</button></div></div>`:'';
 const list=S.items.filter(it=>S.f==='all'||it.t===S.f).map(row).join('');
 return `<div class="row between"><div class="row" style="gap:4px">${seg}</div><button class="sm pri" data-a="add">${ic('plus',16)} Ajouter</button></div>${form}${list||'<div class="empty">Rien ici pour l’instant</div>'}`;
}
function vCards(){
 const a=S.items.filter(it=>it.u);
 const rows=a.map(it=>S.cf===it.key?`<div class="item pair" style="padding:12px"><div class="lbl">${esc(it.n)}</div>${conf(it)}</div>`:`<div class="item"><div class="hd" style="cursor:default">${ic('card',22)}<span class="nm"><b>${esc(it.u)}</b><small>${ic(it.t==='d'?'folder':'radio',13)} ${esc(it.n)}</small></span><button class="sm" data-a="play" data-key="${esc(it.key)}">Lire</button><button class="x" data-a="unp" data-key="${esc(it.key)}" aria-label="Dissocier">${ic('x',16)}</button></div></div>`).join('');
 const L=S.st.last,unk=L&&!S.items.find(x=>x.u===L)?`<div class="item pair" style="padding:12px"><div class="lbl" style="color:var(--wt)">Carte inconnue ${esc(L)}</div><select id="sel">${S.items.map(it=>`<option value="${esc(it.key)}">${esc(it.n)}</option>`).join('')}</select><button class="pri" data-a="assign" style="width:100%">Associer à ce contenu</button></div>`:'';
 return `<div class="lbl">${a.length} carte${a.length>1?'s':''} associée${a.length>1?'s':''}</div>${unk}${rows||'<div class="empty">Aucune carte associée.<br>Passe une carte sur le lecteur pour l’associer.</div>'}`;
}
function fmtUp(s){const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?h+' h '+m+' min':m?m+' min '+(s%60)+' s':s+' s'}
function dgHtml(){const d=S.dbg;if(d.up===undefined)return '<div class="lbl">Chargement…</div>';
 const bad=/PLANTAGE|BROWNOUT/.test(d.reset);
 return [['En marche depuis',fmtUp(d.up)],['Dernier démarrage','<span class="'+(bad?'e':'')+'">'+esc(d.reset)+'</span>'],
 ['Plantages enregistrés',d.crashes?'<span class="e">'+d.crashes+' (dernier : '+esc(d.lastcrash)+')</span>':'0'],
 ['Mémoire libre',Math.round(d.heap/1024)+' Ko (min '+Math.round(d.minheap/1024)+' Ko)'],
 ['Wi-Fi',d.rssi+' dBm · '+esc(d.ip)],['Stockage',d.sd?'OK · '+d.sdmb+' Mo':'<span class="e">ERREUR</span>'],['Marge de pile (octets)','loop '+d.stkL+' · nfc '+d.stkN+' · web '+d.stkW],['PSRAM',d.psram?Math.round(d.psfree/1024)+' Ko libres / '+Math.round(d.psram/1024)+' Ko':'désactivée']]
 .map(([k,v])=>`<div class="kv"><span>${k}</span><span>${v}</span></div>`).join('')}
function lgHtml(){return S.log.map(l=>`<div class="${/\[E\]|echec|erreur|error|PLANTAGE|BROWNOUT|flux coupe/i.test(l)?'e':''}">${esc(l)}</div>`).join('')||'<div class="lbl">Aucun message</div>'}
function prevHtml(){const p=S.dbg.prev;if(!p)return '';
 return '<div class="box"><div class="lbl">Juste avant le dernier redémarrage</div>'+[['Fonctionnait depuis',fmtUp(p.up)],['Mémoire libre',Math.round(p.heap/1024)+' Ko (min '+Math.round(p.minheap/1024)+' Ko)'],['Marge de pile (octets)','loop '+p.stkLoop+' · nfc '+p.stkNfc+' · web '+p.stkWeb],['Dernière action loop',esc(p.sLoop)],['Dernière action web',esc(p.sWeb)],['Dernière action nfc',esc(p.sNfc)]].map(([k,v])=>`<div class="kv"><span>${k}</span><span>${v}</span></div>`).join('')+'</div>'}
function vDbg(){return `<div class="box"><div class="lbl">État de la boîte</div><div id="dg">${dgHtml()}</div>${S.dbg.crashes?'<button class="sm" data-a="clrcrash" style="margin-top:10px">Remettre les plantages à zéro</button>':''}</div>
 ${prevHtml()}<div class="row between"><div class="lbl" style="margin:0">Journal</div><div class="row"><button class="sm" data-a="cplog">Copier</button><button class="sm" data-a="clrlog">Effacer</button></div></div>
 <div class="lg" id="lg">${lgHtml()}</div>`}
function render(){
 const keep=['nn','nu','sel'].map(i=>{const e=document.getElementById(i);return e?e.value:null});
 $('#v').innerHTML=S.tab==='play'?vPlay():S.tab==='lib'?vLib():S.tab==='cards'?vCards():vDbg();
 if(S.tab==='dbg'){const l=$('#lg');if(l)l.scrollTop=l.scrollHeight}
 ['nn','nu','sel'].forEach((i,j)=>{const e=document.getElementById(i);if(e&&keep[j]!==null)e.value=keep[j]});
 $('#t').innerHTML=[['play','headphones','Écoute'],['lib','list','Contenus'],['cards','card','Cartes'],['dbg','bug','Debug']].map(([k,i,l])=>`<button class="tab ${S.tab===k?'on':''}" data-a="tab" data-k="${k}">${ic(i,22)}${l}</button>`).join('');
}
async function load(){
 const s=await(await fetch('/api/state')).json();
 S.pairing=s.pairing;S.playing=s.playing;
 S.items=s.folders.map(f=>({t:'d',n:f.name,key:f.name,u:f.uid,files:f.files})).concat((s.radios||[]).map(r=>({t:'r',n:r.name,key:'radio:'+r.name,u:r.uid,url:r.url})));
 if(document.activeElement&&document.activeElement.id==='vol')return;
 render();
}
document.addEventListener('click',async e=>{
 const b=e.target.closest('[data-a]');if(!b)return;
 const a=b.dataset.a,k=b.dataset.key,it=k?find(k):null;
 if(a!=='unp')S.cf='';
 if(a==='tab'){S.tab=b.dataset.k;window.scrollTo(0,0)}
 else if(a==='f')S.f=b.dataset.k;
 else if(a==='open')S.open=S.open===k?'':k;
 else if(a==='add'){S.add=true;S.err=''}
 else if(a==='cancel')S.add=false;
 else if(a==='nt'){S.nt=b.dataset.k;S.err=''}
 else if(a==='unp')S.cf=k;
 else if(a==='unpno'){}
 else if(a==='clrlog')S.log=[];
 else if(a==='cplog'){const t=S.log.join('\n');try{navigator.clipboard.writeText(t)}catch(_){const x=document.createElement('textarea');x.value=t;document.body.appendChild(x);x.select();document.execCommand('copy');x.remove()}}
 else{
  if(a==='create'){
   const n=$('#nn').value.trim(),u=S.nt==='r'?$('#nu').value.trim():'';
   if(!n)S.err='Saisis un nom';
   else if(/[\/\\]/.test(n))S.err='Le nom ne peut pas contenir de barre oblique';
   else if(S.items.some(x=>x.n===n&&x.t===S.nt))S.err='Ce nom existe déjà';
   else if(S.nt==='r'&&!/^https?:\/\//i.test(u))S.err='L’URL doit commencer par http ou https';
   else{S.err='';
    if(S.nt==='d'){await api('mkdir',{folder:n});S.open=n}else{await api('radioadd',{name:n,url:u});S.open='radio:'+n}
    S.add=false;S.f='all';await load();return}
  }
  else if(a==='unpok'){await api('unpair',{folder:k});await load();return}
  else if(a==='pair'){await api('pair',{folder:k});await load();return}
  else if(a==='play'){await api('play',{folder:k});S.tab='play';render();setTimeout(poll,500);return}
  else if(a==='del'&&it){
   if(!confirm('Supprimer « '+it.n+' » ?'))return;
   await api(it.t==='d'?'rmdir':'radiodel',it.t==='d'?{folder:it.n}:{name:it.n});S.open='';await load();return}
  else if(a==='rmf'){
   if(!confirm('Supprimer ce fichier ?'))return;
   await api('rmfile',{folder:k,name:b.dataset.f});await load();return}
  else if(a==='prev'||a==='next'||a==='pp'){await api('ctl',{c:a});setTimeout(poll,400);return}
  else if(a==='assign'){await api('assign',{uid:S.st.last,folder:$('#sel').value});await load();return}
  else if(a==='clrcrash'){await api('clearcrash');S.dbg.crashes=0;render();return}
 }
 render();
 if(S.tab==='dbg')pollLog();
});
document.addEventListener('change',e=>{const t=e.target;if(t.type==='file'&&t.files.length)up(t.dataset.key,[...t.files])});
let vt;document.addEventListener('input',e=>{if(e.target.id==='vol'){const v=e.target.value;$('#vv').textContent=v;clearTimeout(vt);vt=setTimeout(()=>api('volume',{v}),150)}});
async function up(key,files){
 for(const file of files){await new Promise(r=>{const x=new XMLHttpRequest();
  x.open('POST','/api/upload?folder='+encodeURIComponent(key));
  x.upload.onprogress=e=>{$('#pg').hidden=false;$('#pg').value=e.loaded/e.total;$('#stx').textContent=file.name};
  x.onloadend=r;const fd=new FormData();fd.append('f',file,file.name);x.send(fd)})}
 $('#pg').hidden=true;$('#stx').textContent='';load()}
let fp='';
async function poll(){try{const s=await(await fetch('/api/status')).json();const f=JSON.stringify(s);if(f===fp)return;fp=f;S.st=s;await load()}catch(e){}}
async function pollLog(){
 if(S.tab!=='dbg')return;
 try{const s=await(await fetch('/api/log?since='+S.next)).json();
  if(S.dbg.up!==undefined&&s.up<S.dbg.up)S.log.push('— redémarrage détecté —');
  const first=S.dbg.up===undefined;S.dbg=s;S.next=s.next;S.log=S.log.concat(s.lines).slice(-300);
  if(first){render();return}
  const dg=$('#dg'),lg=$('#lg');
  if(dg)dg.innerHTML=dgHtml();
  if(lg){const bot=lg.scrollTop+lg.clientHeight>=lg.scrollHeight-24;lg.innerHTML=lgHtml();if(bot)lg.scrollTop=lg.scrollHeight}
 }catch(e){}}
load().then(poll);setInterval(poll,2000);setInterval(pollLog,1500);
</script></body></html>)HTML";
