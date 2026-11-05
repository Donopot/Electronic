#pragma once
#include <Arduino.h>
// Tableau de bord local du concentrateur. Aucune ressource externe : tout le
// style et le script sont inline pour respecter la CSP du serveur (self only).
static const char PLANT_DASHBOARD_PAGE[] PROGMEM = R"PLANTHTML(<!doctype html><html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Plant Station · concentrateur</title><style>
:root{--bg:#f5f4ed;--ink:#24352c;--muted:#67736b;--line:#dce2d5;--green:#dbeab1;--dark:#314a37;--orange:#f4dfb9;--red:#a8432d;--white:#fffef9;--blue:#3f6f8f;--violet:#8a4f7d;--rust:#b0642f;--olive:#7a8f3f}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}
button,input,select{font:inherit}button,select{cursor:pointer}button{color:inherit}
button:focus-visible,a:focus-visible,select:focus-visible,input:focus-visible{outline:3px solid #5c782b;outline-offset:3px}
header{max-width:1320px;margin:auto;padding:20px 26px;display:flex;align-items:center;justify-content:space-between;gap:16px;flex-wrap:wrap}
.brand{display:flex;align-items:center;gap:12px;font-size:19px;font-weight:750;letter-spacing:-.7px}
.mark{background:var(--dark);color:var(--green);border-radius:12px;width:39px;height:39px;display:grid;place-items:center}
.pill{display:inline-flex;align-items:center;gap:8px;border:1px solid var(--line);border-radius:30px;padding:7px 13px;font-size:12px}
.dot{width:7px;height:7px;border-radius:50%;background:#678441}.dot.off{background:var(--red)}
nav{max-width:1320px;margin:0 auto;padding:0 26px;display:flex;gap:4px;overflow-x:auto;border-bottom:1px solid var(--line)}
nav button{border:0;background:transparent;padding:12px 14px;border-bottom:3px solid transparent;color:var(--muted);font-weight:600;white-space:nowrap}
nav button[aria-selected=true]{color:var(--ink);border-bottom-color:var(--dark)}
main{max-width:1320px;margin:auto;padding:24px 26px 40px}
section[hidden]{display:none}
h1{font-size:clamp(24px,3.4vw,34px);letter-spacing:-1.2px;margin:0 0 6px;font-weight:650}
h2{font-size:18px;margin:26px 0 12px;letter-spacing:-.4px}h3{font-size:15px;margin:0}
.tag{font-size:11px;text-transform:uppercase;letter-spacing:1.6px;color:var(--muted)}
.sub{color:var(--muted);margin:0 0 18px}
.grid{display:grid;gap:14px}.g2{grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}.g4{grid-template-columns:repeat(auto-fit,minmax(230px,1fr))}
.card{background:var(--white);border:1px solid var(--line);border-radius:16px;padding:16px 18px}
.card.top{background:var(--green);border-color:#c5d598}
.card h3{font-size:14px;color:var(--muted);font-weight:600;letter-spacing:.2px;margin-bottom:6px}
.big{font-size:26px;font-weight:650;letter-spacing:-1px;line-height:1.15}
.state{display:inline-block;border-radius:20px;padding:3px 9px;font-size:11px;font-weight:650;letter-spacing:.3px}
.state.waiting,.state.stale,.state.adc_fault{background:#f4dcd4;color:#95432e}
.state.uncalibrated{background:#eeeee7;color:#64665e}.state.healthy{background:#e5eddb;color:#3b592d}.state.dry{background:#f4dfb9;color:#6b4a1c}
.muted{color:var(--muted)}.small{font-size:12px}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{text-align:left;padding:8px 10px;border-bottom:1px solid var(--line);white-space:nowrap}
th{font-size:11px;text-transform:uppercase;letter-spacing:.8px;color:var(--muted);font-weight:600}
tbody tr:last-child td{border-bottom:0}
.scroll{overflow-x:auto}
.bar{height:8px;border-radius:6px;background:#e7e9e1;overflow:hidden}.bar span{display:block;height:100%;background:var(--dark)}
.bar.warn span{background:var(--rust)}
.kv{display:flex;justify-content:space-between;gap:14px;padding:7px 0;border-bottom:1px dashed var(--line);font-size:13px}
.kv:last-child{border-bottom:0}.kv b{font-weight:650}
.chart{width:100%;height:260px;display:block}
.legend{display:flex;gap:14px;flex-wrap:wrap;margin:10px 0 0;font-size:12px}
.legend i{display:inline-block;width:10px;height:10px;border-radius:3px;margin-right:6px}
.segmented{display:inline-flex;border:1px solid var(--line);border-radius:10px;overflow:hidden}
.segmented button{border:0;background:transparent;padding:7px 12px;font-size:12px}
.segmented button[aria-pressed=true]{background:var(--dark);color:var(--white)}
.outline{border:1px solid var(--line);background:transparent;border-radius:10px;padding:9px 14px}
.primary{background:var(--dark);color:var(--white);border:0;border-radius:10px;padding:9px 16px;font-weight:600}
.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.row.between{justify-content:space-between}
.card .name{font-size:15px;color:var(--ink);font-weight:650;margin:2px 0 4px;letter-spacing:-.2px}
.spark{width:100%;height:38px;display:block;margin-top:8px}
.banner{background:#f0e4b9;border:1px solid #dccb91;color:#615323;border-radius:12px;padding:11px 14px;font-size:13px;margin-bottom:16px}
.offline{background:#f4dcd4;color:#95432e;padding:11px 14px;text-align:center;font-size:13px}
.chip{display:inline-block;border:1px solid var(--line);border-radius:8px;padding:2px 7px;font-size:11px;font-variant-numeric:tabular-nums}
dialog{max-width:min(460px,95vw);border:1px solid var(--line);border-radius:16px;padding:20px}
dialog::backdrop{background:rgba(36,53,44,.35)}
input{max-width:100%;width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:9px;background:var(--white)}
label{display:block;margin-top:12px;font-size:12px;color:var(--muted)}
#edit-error{color:var(--red);font-size:13px}
footer{max-width:1320px;margin:auto;padding:16px 26px 30px;color:var(--muted);font-size:12px;display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}
@media(max-width:640px){header,main,nav{padding-inline:16px}th,td{padding:7px 8px}.g4{grid-template-columns:1fr 1fr}.big{font-size:22px}}
@media(max-width:420px){.g4{grid-template-columns:1fr}}
</style><body>
<header><div class="brand"><div class="mark" aria-hidden="true">↟</div>Plant Station · concentrateur</div>
<span class="pill" id="connection"><span class="dot off" id="dot"></span><span id="connection-text">Connexion au Heltec…</span></span></header>
<nav role="tablist" aria-label="Sections du tableau de bord"><button role="tab" data-tab="overview" aria-selected="true">Vue d’ensemble</button><button role="tab" data-tab="nodes" aria-selected="false">Sondes</button><button role="tab" data-tab="history" aria-selected="false">Historique</button><button role="tab" data-tab="radio" aria-selected="false">Radio</button><button role="tab" data-tab="system" aria-selected="false">Système</button><button role="tab" data-tab="settings" aria-selected="false">Réglages</button></nav>
<div id="offline" class="offline" role="alert" hidden>Heltec inaccessible. Les valeurs affichées sont les dernières reçues par cette page ; leur état peut avoir changé.</div>
<main>
<section id="tab-overview" role="tabpanel">
<h1>Comment vont les quatre sondes ?</h1><p class="sub" id="overview-sub">En attente des mesures.</p>
<div id="restore-banner" class="banner" hidden></div>
<div class="grid g4" id="cards"></div>
<h2>Derniers paquets reçus</h2><div class="card"><div class="scroll"><table><thead><tr><th>Nœud</th><th>État</th><th>Tension</th><th>Âge</th><th>RSSI</th><th>SNR</th><th>Il y a</th></tr></thead><tbody id="overview-rows"></tbody></table></div></div>
</section>
<section id="tab-nodes" role="tabpanel" hidden>
<h1>Sondes</h1><p class="sub">Toutes les valeurs transmises par PS v1, plus la mesure radio du concentrateur.</p>
<div class="card scroll"><table><thead><tr><th>Nœud</th><th>Nom</th><th>État</th><th>Tension</th><th>Brut</th><th>ADS</th><th>Session</th><th>Séquence</th><th>RSSI</th><th>SNR</th><th>Âge</th><th>Dernière mesure</th></tr></thead><tbody id="node-rows"></tbody></table></div>
<h2>Dernière mesure persistée</h2><p class="sub">Valeur conservée en NVS : elle survit au redémarrage du concentrateur et ne dépend pas du journal flash.</p>
<div class="card scroll"><table><thead><tr><th>Nœud</th><th>Présente</th><th>Tension</th><th>Statut ADC</th><th>Session</th><th>Séquence</th><th>Date</th></tr></thead><tbody id="known-rows"></tbody></table></div>
</section>
<section id="tab-history" role="tabpanel" hidden>
<h1>Historique</h1><p class="sub" id="history-sub">Journal persistant du Heltec, trente jours glissants.</p>
<div class="row" style="justify-content:space-between"><div class="segmented" aria-label="Période"><button data-range="1" aria-pressed="true">24 h</button><button data-range="7" aria-pressed="false">7 j</button><button data-range="30" aria-pressed="false">30 j</button></div><div class="row"><button class="outline" id="reload-history">Recharger</button><button class="outline" id="export-csv">Exporter CSV</button></div></div>
<div class="card" style="margin-top:14px"><svg class="chart" id="chart" viewBox="0 0 760 260" role="img" aria-label="Historique des quatre sondes"></svg><div class="legend" id="legend"></div><p class="small muted" id="chart-note" role="status">Aucune donnée chargée.</p></div>
</section>
<section id="tab-radio" role="tabpanel" hidden>
<h1>Radio</h1><p class="sub">Réception LoRa du concentrateur. PS v1 n’embarque ni RSSI, ni SNR, ni ACK : les valeurs ci-dessous sont mesurées ici.</p>
<div class="grid g2" id="radio-cards"></div>
<h2>Motifs de rejet</h2><div class="card" id="reject-card"><p class="small muted">Aucun paquet rejeté depuis le démarrage.</p></div>
</section>
<section id="tab-system" role="tabpanel" hidden>
<h1>Système</h1><p class="sub">État réel du Heltec, sans service externe.</p>
<div class="grid g2" id="system-cards"></div>
</section>
<section id="tab-settings" role="tabpanel" hidden>
<h1>Réglages</h1><p class="sub">Noms des quatre sondes, enregistrés en NVS sur le Heltec.</p>
<div class="card"><form id="edit-form"><div id="name-fields" class="grid g2"></div><p id="edit-error" role="alert" class="small"></p><div class="row" style="margin-top:14px"><button type="submit" class="primary" id="save">Enregistrer les noms</button><span class="small muted" id="names-state"></span></div></form></div>
<h2>Accès au tableau de bord</h2><div class="card small"><p>Depuis un téléphone ou un PC <b>connecté au même Wi-Fi que le Heltec</b> : ouvrir <code id="access-url">l’adresse du kit</code> et l’ajouter à l’écran d’accueil. Après un redémarrage, il rejoint seul votre Wi-Fi ; rien à refaire.</p><p>Changer de réseau ou déménager : <b>PRG 8 s</b> ouvre le Wi-Fi du kit <b id="ap-ssid">du kit</b> pendant 15 minutes, le temps de choisir un nouveau réseau.</p><p id="setup-link-row" hidden><a href="/setup">Configurer le Wi-Fi de la maison</a></p></div>
<h2>Aide</h2><div class="card small muted">PRG court : visage → sondes → QR d’accès. PRG 3 s puis relâcher : veille. PRG 8 s : rouvrir l’assistant Wi-Fi. RST : réveiller.<br>Une mesure arrive toutes les 30 minutes ; au-delà de 90 minutes sans paquet accepté, la valeur devient <b>PERIMEE</b>. Un redémarrage du concentrateur n’efface ni le journal, ni la dernière mesure persistée.</div>
</section>
</main>
<footer><span>Wi-Fi domestique · sans compte · sans service externe</span><span>SOIL-01 à SOIL-04 · PS v1 · NON AUTH</span></footer>
<script>
'use strict';
const $=id=>document.getElementById(id);
const TABS=['overview','nodes','history','radio','system','settings'];
const REQUEST_TIMEOUT_MS=6000,CLOCK_RETRY_MS=30000;
const COLORS={1:'#3f6f8f',2:'#7a8f3f',3:'#b0642f',4:'#8a4f7d'};
const labels={waiting:'En attente',uncalibrated:'À étalonner',healthy:'Satisfaisant',dry:'Sèche',adc_fault:'Défaut ADC',stale:'Périmée'};
const rejects={length:'longueur',header:'en-tête',node:'nœud inconnu',session:'session',status:'statut',address:'adresse ADS',raw:'valeur brute',duplicate:'doublon',old_sequence:'séquence ancienne'};
let state=null,history=[],range=1,histLoading=false,histError='',histAt=0,histAttempt=0,clockBusy=false,clockAt=-Infinity,namesBusy=false;
const nf=(v,d=0)=>v===null||v===undefined?'—':Number(v).toLocaleString('fr-FR',{minimumFractionDigits:d,maximumFractionDigits:d});
const idLabel=id=>'SOIL-'+String(id).padStart(2,'0');
const volts=uv=>uv===null||uv===undefined?'—':nf(uv/1e6,3)+' V';
function dur(ms){if(ms===null||ms===undefined)return '—';const s=Math.floor(ms/1000);if(s<60)return s+' s';const m=Math.floor(s/60);if(m<60)return m+' min';const h=Math.floor(m/60);if(h<48)return h+' h '+(m%60)+' min';return Math.floor(h/24)+' j '+(h%24)+' h';}
const when=utc=>utc?new Date(utc).toLocaleString('fr-FR',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'}):'date inconnue';
async function api(url,options={}){
 const controller=new AbortController(),external=options.signal;let timedOut=false,timer;
 const cancel=()=>controller.abort();if(external?.aborted)cancel();else external?.addEventListener('abort',cancel,{once:true});
 const deadline=new Promise((_,reject)=>{timer=setTimeout(()=>{timedOut=true;controller.abort();reject(new Error('Délai de réponse dépassé'));},REQUEST_TIMEOUT_MS);});
 try{return await Promise.race([(async()=>{const r=await fetch(url,{cache:'no-store',...options,signal:controller.signal});if(!r.ok){let reason='HTTP '+r.status;try{reason+=' · '+(await r.json()).error;}catch{}throw new Error(reason);}return r.json();})(),deadline]);}
 catch(e){if(timedOut)throw new Error('Délai de réponse dépassé');throw e;}
 finally{clearTimeout(timer);external?.removeEventListener('abort',cancel);}
}
const post=(path,fields)=>api(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Plant-Request':'1'},body:new URLSearchParams(fields).toString()});
function tab(name){
 if(!TABS.includes(name))name='overview';
 document.querySelectorAll('nav [role=tab]').forEach(b=>b.setAttribute('aria-selected',String(b.dataset.tab===name)));
 TABS.forEach(t=>$('tab-'+t).hidden=t!==name);
 if(location.hash.slice(1)!==name)history.replaceState(null,'','#'+name);
 if(name==='history'&&!history.length&&!histLoading)loadHistory();
 if(name==='history')drawChart();
}
document.querySelectorAll('nav [role=tab]').forEach(b=>b.onclick=()=>tab(b.dataset.tab));
addEventListener('hashchange',()=>tab(location.hash.slice(1)));
function spark(values){
 if(values.length<2)return '';
 const min=Math.min(...values),max=Math.max(...values),span=max-min||1;
 return values.map((v,i)=>`${i?'L':'M'}${(i/(values.length-1)*120).toFixed(1)} ${(34-((v-min)/span)*30).toFixed(1)}`).join(' ');
}
function seriesFor(node,days){
 const start=state?.utc_ms?state.utc_ms-days*86400000:null;
 const points=history.filter(r=>r.node===node&&r.microvolts!==null&&r.utc_ms!==null).sort((a,b)=>a.utc_ms-b.utc_ms);
 return start===null?points:points.filter(r=>r.utc_ms>=start);
}
function render(){
 if(!state)return;
 const plants=state.plants,missing=plants.filter(p=>['waiting','adc_fault','stale'].includes(p.state));
 $('overview-sub').textContent=missing.length?`${missing.length} sonde(s) sans mesure fraîche · péremption à 90 min`:'Les quatre sondes ont une mesure fraîche.';
 const restored=plants.filter(p=>p.age_ms===null&&p.last_known&&p.last_known.present);
 const banner=$('restore-banner');
 if(restored.length){banner.hidden=false;banner.textContent='Après redémarrage, aucune nouvelle trame depuis le boot. Dernière mesure persistée : '+restored.map(p=>`${idLabel(p.id)} ${volts(p.last_known.microvolts)} (${when(p.last_known.utc_ms)})`).join(' · ');}
 else banner.hidden=true;
 const cards=$('cards');cards.replaceChildren();
 for(const p of plants){
  const card=document.createElement('div');card.className='card'+(p.state==='healthy'?' top':'');
  const top=document.createElement('div');top.className='row between';
  const ident=document.createElement('span');ident.className='small muted';ident.textContent=idLabel(p.id);top.append(ident);
  const badge=document.createElement('span');badge.className='state '+p.state;badge.textContent=labels[p.state]||p.state;top.append(badge);
  const name=document.createElement('h3');name.className='name';name.textContent=p.name;
  const value=document.createElement('div');value.className='big';value.textContent=volts(p.microvolts);
  const meta=document.createElement('div');meta.className='small muted';
  meta.textContent=p.age_ms===null?'Aucun paquet depuis le démarrage':'Il y a '+dur(p.age_ms)+(p.state==='stale'?' · valeur périmée':'');
  const sig=document.createElement('div');sig.className='small muted';sig.style.marginTop='4px';
  sig.textContent=p.rssi_dbm===null?'RSSI —':'RSSI '+p.rssi_dbm+' dBm · SNR '+p.snr_db+' dB';
  const restored=document.createElement('div');restored.className='small muted';restored.style.marginTop='4px';
  if(p.age_ms===null&&p.last_known&&p.last_known.present)restored.textContent='Dernière connue : '+volts(p.last_known.microvolts)+(p.last_known.utc_ms?' ('+when(p.last_known.utc_ms)+')':'');
  const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('class','spark');svg.setAttribute('viewBox','0 0 120 38');svg.setAttribute('preserveAspectRatio','none');
  const points=seriesFor(p.id,1).map(r=>r.microvolts);
  svg.innerHTML=points.length>1?`<path d="${spark(points)}" fill="none" stroke="${COLORS[p.id]}" stroke-width="1.6" vector-effect="non-scaling-stroke"/>`:'';
  card.append(top,name,value,meta,sig,restored,svg);cards.append(card);
 }
 const rows=$('overview-rows');rows.replaceChildren();
 for(const p of plants){
  const tr=document.createElement('tr');
  for(const text of [idLabel(p.id),labels[p.state]||p.state,volts(p.microvolts),p.age_ms===null?'—':dur(p.age_ms),p.rssi_dbm===null?'—':p.rssi_dbm+' dBm',p.snr_db===null?'—':p.snr_db+' dB',p.last_known?.utc_ms?when(p.last_known.utc_ms):'—']){
   const td=document.createElement('td');td.textContent=text;tr.append(td);
  }
  rows.append(tr);
 }
 $('connection-text').textContent='Connecté au Heltec';$('dot').className='dot';
 renderNodes();renderRadio();renderSystem();
}
function renderNodes(){
 const rows=$('node-rows');if(!rows)return;rows.replaceChildren();
 for(const p of state.plants){
  const tr=document.createElement('tr');
  const cells=[idLabel(p.id),p.name,labels[p.state]||p.state,volts(p.microvolts),p.raw===null?'—':p.raw,p.ads_address===null?'—':'0x'+Number(p.ads_address).toString(16).toUpperCase(),p.session===null?'—':p.session,p.sequence===null?'—':p.sequence,p.rssi_dbm===null?'—':p.rssi_dbm+' dBm',p.snr_db===null?'—':p.snr_db+' dB',p.age_ms===null?'—':dur(p.age_ms),p.last_known?.utc_ms?when(p.last_known.utc_ms):'—'];
  for(const text of cells){const td=document.createElement('td');td.textContent=text;tr.append(td);}
  rows.append(tr);
 }
 const known=$('known-rows');known.replaceChildren();
 for(const p of state.plants){
  const k=p.last_known||{};
  const tr=document.createElement('tr');
  for(const text of [idLabel(p.id),k.present?'oui':'non',volts(k.microvolts),k.adc_status||'—',k.session??'—',k.sequence??'—',k.utc_ms?when(k.utc_ms):'date inconnue']){
   const td=document.createElement('td');td.textContent=text;tr.append(td);
  }
  known.append(tr);
 }
}
function renderRadio(){
 const box=$('radio-cards');if(!box)return;box.replaceChildren();
 const r=state.radio;
 if(!r){const c=document.createElement('div');c.className='card';c.textContent='Radio non compilée dans ce profil.';box.append(c);return;}
 const facts=[['État',r.ready?'à l’écoute':'indisponible'],['Fréquence',nf(r.frequency_mhz,1)+' MHz'],['Bande passante',nf(r.bandwidth_khz,0)+' kHz'],['Facteur d’étalement','SF'+r.spreading_factor],['Codage','CR '+r.coding_rate],['Puissance',r.power_dbm+' dBm'],['CRC','activé'],['Trames acceptées',nf(r.accepted)],['Trames rejetées',nf(r.rejected)]];
 const card=document.createElement('div');card.className='card';
 card.innerHTML='<h3>Récepteur SX1262</h3>'+facts.map(([k,v])=>`<div class="kv"><span>${k}</span><b>${v}</b></div>`).join('');
 box.append(card);
 const sun=document.createElement('div');sun.className='card';
 const total=r.accepted+r.rejected,okPct=total?Math.round(r.accepted/total*100):100;
 sun.innerHTML=`<h3>Qualité de réception</h3><div class="big">${okPct} %</div><p class="small muted">${nf(r.accepted)} trame(s) acceptée(s) sur ${nf(total)} reçue(s) depuis le démarrage.</p><div class="bar${okPct<90?' warn':''}"><span style="width:${okPct}%"></span></div><p class="small muted" style="margin-top:10px">Sans ACK ni répétition : un paquet perdu en l’air n’est compté nulle part.</p>`;
 box.append(sun);
 const reject=$('reject-card');const entries=Object.entries(r.reject_reasons||{}).filter(([,v])=>v>0).sort((a,b)=>b[1]-a[1]);
 reject.innerHTML=entries.length?entries.map(([k,v])=>`<div class="kv"><span>${rejects[k]||k}</span><b>${nf(v)}</b></div>`).join(''):'<p class="small muted">Aucun paquet rejeté depuis le démarrage.</p>';
}
function renderSystem(){
 const box=$('system-cards');if(!box)return;box.replaceChildren();
 const s=state.system||{},w=state.wifi||{};
 const heapPct=s.heap_free&&s.flash_bytes?Math.min(100,Math.round(s.heap_free/300000*100)):0;
 const clock=state.utc_ms===null?(state.clock_persistence==='failed'?'sauvegarde impossible':clockBusy?'synchronisation…':'inconnue'):'synchronisée par le téléphone';
 const histNames={ready:'prêt',unknown_data_preserved:'données inconnues préservées',io_error:'erreur E/S',partition_too_small:'partition trop petite'};
 const cards=[
  ['Mémoire',`<div class="big">${nf(s.heap_free)} o</div><p class="small muted">libre · minimum depuis le boot ${nf(s.heap_min)} o</p><div class="bar${heapPct<25?' warn':''}"><span style="width:${heapPct}%"></span></div><p class="small muted" style="margin-top:6px">part du tas libre</p>`],
  ['Journal flash',`<div class="kv"><span>État</span><b>${histNames[state.history_state]||state.history_state}</b></div><div class="kv"><span>Trames non enregistrées</span><b>${nf(state.history_dropped)}</b></div><div class="kv"><span>Écritures interrompues</span><b>${nf(state.history_corrupt)}</b></div><div class="kv"><span>Fenêtre</span><b>30 jours glissants</b></div>`],
  ['Réseau',`<div class="kv"><span>Wi-Fi</span><b>${w.connected?'connecté':'hors ligne'}</b></div><div class="kv"><span>Signal</span><b>${w.rssi_dbm===null||w.rssi_dbm===undefined?'—':w.rssi_dbm+' dBm'}</b></div><div class="kv"><span>Canal</span><b>${w.channel??'—'}</b></div><div class="kv"><span>Adresse</span><b>${w.ip||'—'}</b></div><div class="kv"><span>mDNS</span><b>${w.hostname||'—'}</b></div>`],
  ['Carte',`<div class="kv"><span>Puce</span><b>${s.chip||'—'} r${s.revision??'?'}</b></div><div class="kv"><span>Flash</span><b>${nf((s.flash_bytes||0)/1048576,0)} Mio</b></div><div class="kv"><span>Firmware</span><b>${s.version||'—'}</b></div><div class="kv"><span>Nœuds</span><b>${s.node_count??4}</b></div><div class="kv"><span>Uptime</span><b>${dur(state.uptime_ms)}</b></div>`],
  ['Horloge',`<div class="kv"><span>Date</span><b>${state.utc_ms===null?'inconnue':when(state.utc_ms)}</b></div><div class="kv"><span>Source</span><b>${clock}</b></div><div class="row" style="margin-top:10px"><button class="outline" id="sync-clock">Synchroniser</button></div><p class="small muted" style="margin-top:8px">Les dates viennent du téléphone ; sans elles, les mesures restent sans date.</p>`],
  ['Lumière',state.lux_available?`<div class="big">${state.lux===null?'—':nf(state.lux,0)+' lux'}</div><p class="small muted">VEML7700 présent.</p>`:'<p class="small muted">Aucun capteur de lumière monté sur ce concentrateur.</p>']
 ];
 for(const [title,html] of cards){const c=document.createElement('div');c.className='card';c.innerHTML=`<h3>${title}</h3>`+html;box.append(c);}
 const sync=$('sync-clock');if(sync)sync.onclick=syncClock;
 $('names-state').textContent=state.names_persistence==='saved'?'Noms enregistrés.':state.names_persistence==='pending'?'Enregistrement en cours…':'Sauvegarde refusée par le Heltec.';
}
function drawChart(){
 const c=$('chart');if(!c||!state)return;
 const days=range,now=state.utc_ms,start=now===null?null:now-days*86400000;
 const dated=history.filter(r=>r.microvolts!==null&&r.utc_ms!==null&&(start===null||r.utc_ms>=start)&&(!now||r.utc_ms<=now));
 const unknown=history.filter(r=>r.utc_ms===null).length;
 const width=Math.max(320,Math.round(c.getBoundingClientRect().width||760)),left=44,w=width-64,top=14,h=190;
 let body='';
 for(const node of [1,2,3,4]){
  const pts=dated.filter(r=>r.node===node).sort((a,b)=>a.utc_ms-b.utc_ms);
  let previous=null,path='';
  for(const r of pts){
   const x=start===null?left+((r.utc_ms-dated[0]?.utc_ms||0)/((dated.at(-1)?.utc_ms||1)-(dated[0]?.utc_ms||0)||1))*w:left+(r.utc_ms-start)/(days*86400000)*w;
   const y=top+h-((r.microvolts/1e6+0.01)/3.32)*h;
   const gap=!previous||r.boot!==previous.boot||r.utc_ms-previous.utc_ms>2400000;
   if(gap){path=`M${x.toFixed(1)} ${y.toFixed(1)}`;body+=`<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="1.8" fill="${COLORS[node]}"/>`;}
   else{body+=`<path d="${path} L${x.toFixed(1)} ${y.toFixed(1)}" fill="none" stroke="${COLORS[node]}" stroke-width="2"/>`;path=`M${x.toFixed(1)} ${y.toFixed(1)}`;}
   previous=r;
  }
 }
 const ticks=[0,1.65,3.32].map((v,i)=>`<line x1="${left}" x2="${left+w}" y1="${top+h*(1-v/3.32)}" y2="${top+h*(1-v/3.32)}" stroke="#dce2d5"/><text x="6" y="${top+h*(1-v/3.32)+4}" font-size="11" fill="#67736b">${v.toFixed(2).replace('.',',')}</text>`).join('');
 c.setAttribute('viewBox',`0 0 ${width} 240`);
 c.innerHTML=ticks+body+`<text x="${left}" y="232" font-size="11" fill="#67736b">${days===1?'il y a 24 h':'il y a '+days+' jours'}</text><text x="${left+w}" y="232" text-anchor="end" font-size="11" fill="#67736b">maintenant</text>`;
 $('legend').innerHTML=state.plants.map(p=>`<span><i style="background:${COLORS[p.id]}"></i>${idLabel(p.id)} ${p.name}</span>`).join('');
 const nb=dated.length;
 $('chart-note').textContent=histError||(start===null?'Heure inconnue : synchronisez le concentrateur avec ce téléphone pour placer les mesures sur l’axe. ':'')+`${nb} relevé(s) daté(s) sur la période.`+(unknown?` ${unknown} relevé(s) sans date, conservé(s) et non tracés.`:'')+' Un trou de plus de 40 min, un défaut ADC ou un redémarrage coupe la courbe.';
}
async function loadHistory(){
 if(!state||histLoading)return;
 histLoading=true;$('chart-note').textContent='Chargement de l’historique…';
 const collected=[];let after='0',guard=0;
 try{
  while(guard++<80){
   const page=await api(`/api/history?node=0&days=${range}&after=${after}`);
   collected.push(...page.records);
   if(!page.more)break;
   if(page.next===after)throw new Error('pagination interrompue');
   after=page.next;
  }
  history=collected;histError='';histAt=Date.now();
 }catch(e){histError='Historique incomplet : '+e.message+'. ';}
 finally{histLoading=false;}
 drawChart();render();
}
function exportCsv(){
 if(!history.length){$('chart-note').textContent='Rien à exporter : chargez l’historique.';return;}
 const rows=[['node','utc_ms','date_iso','microvolts','tension_v','adc_status']];
 for(const r of history)rows.push([idLabel(r.node),r.utc_ms??'',r.utc_ms?new Date(r.utc_ms).toISOString():'',r.microvolts??'',r.microvolts===null?'':(r.microvolts/1e6).toFixed(3),r.adc_status]);
 const csv=rows.map(r=>r.join(',')).join('\n');
 const url=URL.createObjectURL(new Blob([csv],{type:'text/csv;charset=utf-8'}));
 const a=document.createElement('a');a.href=url;a.download='sondes-'+new Date().toISOString().slice(0,10)+'.csv';document.body.append(a);a.click();a.remove();
 setTimeout(()=>URL.revokeObjectURL(url),2000);
}
document.querySelectorAll('[data-range]').forEach(b=>b.onclick=()=>{range=Number(b.dataset.range);document.querySelectorAll('[data-range]').forEach(o=>o.setAttribute('aria-pressed',String(o===b)));history=[];loadHistory();});
$('reload-history').onclick=()=>{history=[];loadHistory();};
$('export-csv').onclick=exportCsv;
async function syncClock(){
 if(clockBusy||Date.now()-clockAt<CLOCK_RETRY_MS)return;
 clockBusy=true;clockAt=Date.now();
 try{await post('/api/time',{utc_ms:String(Date.now())});await refresh();}
 catch(e){/* nouvel essai automatique */}
 finally{clockBusy=false;}
}
async function refresh(){
 if(refresh.busy)return;refresh.busy=true;
 try{
  const next=await api('/api/status');
  if(next.source!=='live'||!Array.isArray(next.plants)||next.plants.length!==4)throw new Error('réponse inattendue');
  state=next;$('offline').hidden=true;render();
  if(!$('name-fields').children.length)fillNames();
  // Charge une fois 24 h en arriere-plan : alimente les mini-courbes des cartes.
  if(!history.length&&!histLoading&&Date.now()-histAttempt>30000){histAttempt=Date.now();loadHistory();}
  if(state.utc_ms===null)syncClock();
  if(!histLoading&&Date.now()-histAt>60000&&document.querySelector('nav [data-tab=history]').getAttribute('aria-selected')==='true')loadHistory();
 }catch(e){$('offline').hidden=false;$('connection-text').textContent='Heltec inaccessible';$('dot').className='dot off';}
 finally{refresh.busy=false;}
}
$('edit-form').onsubmit=async e=>{
 e.preventDefault();if(namesBusy||!state)return;namesBusy=true;$('save').disabled=true;$('edit-error').textContent='';
 try{
  const expected=[];
  for(const input of $('name-fields').querySelectorAll('input')){
   const name=input.value.trim();
   if(!name||[...name].length>24||new TextEncoder().encode(name).length>48||/[\u0000-\u001f\u007f-\u009f]/.test(name))throw new Error('Nom : 24 caractères et 48 octets maximum.');
   expected.push({id:Number(input.name),name});
  }
  for(const p of expected)await post('/api/plants',p);
  $('edit-error').textContent='Sauvegarde en attente sur le Heltec…';
  const until=Date.now()+15000;
  while(true){
   const saved=await api('/api/status');
   if(saved.names_persistence==='failed')throw new Error('Sauvegarde refusée par le Heltec ; les noms restent en attente.');
   if(saved.names_persistence==='saved'&&expected.every(p=>saved.plants.some(q=>q.id===p.id&&q.name===p.name))){state=saved;render();fillNames();$('edit-error').textContent='Noms enregistrés.';break;}
   if(Date.now()>=until)throw new Error('Sauvegarde toujours en attente ; vérifiez la connexion avant de redémarrer.');
   await new Promise(r=>setTimeout(r,400));
  }
 }catch(error){$('edit-error').textContent='Enregistrement incomplet : '+error.message;}
 finally{namesBusy=false;$('save').disabled=false;}
};
function fillNames(){
 if(!state)return;const box=$('name-fields');box.replaceChildren();
 for(const p of state.plants){
  const wrap=document.createElement('div');
  const label=document.createElement('label');label.htmlFor='name-'+p.id;label.textContent=idLabel(p.id);
  const input=document.createElement('input');input.id=label.htmlFor;input.name=String(p.id);input.value=p.name;input.maxLength=48;input.required=true;
  wrap.append(label,input);box.append(wrap);
 }
}
function accessInfo(){
 $('access-url').textContent=location.origin+'/';
 if(location.hostname==='192.168.4.1')$('setup-link-row').hidden=false;
 const host=state&&state.wifi&&state.wifi.hostname?state.wifi.hostname:'';
 if(host)$('ap-ssid').textContent='PLANT-'+host.replace(/^plant-/i,'').replace(/\.local$/i,'').toUpperCase();
}
tab(location.hash.slice(1)||'overview');
refresh();
accessInfo();
setInterval(function(){refresh();accessInfo();},5000);
addEventListener('resize',()=>{if(!$('tab-history').hidden)drawChart();});
</script></body></html>)PLANTHTML";
