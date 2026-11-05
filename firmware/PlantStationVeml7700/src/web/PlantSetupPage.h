#pragma once
#include <Arduino.h>
static const char PLANT_SETUP_PAGE[] PROGMEM = R"SETUPHTML(<!doctype html>
<html lang="fr"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Configurer Plant Station</title>
<style>
:root{color-scheme:light;--ink:#24352c;--muted:#657367;--line:#d7dfce;--green:#dbeab1}*{box-sizing:border-box}body{margin:0;background:#f5f4ed;color:var(--ink);font:16px/1.5 system-ui,sans-serif}main{max-width:590px;margin:auto;padding:28px 20px 40px}.brand{font-weight:750;letter-spacing:-.5px}h1{font-size:32px;line-height:1.2;letter-spacing:-1px;margin:28px 0 12px}h2{font-size:21px;margin:0 0 10px}p{margin:10px 0}.muted,.small{color:var(--muted)}.small{font-size:13px}.steps{padding:16px 20px;background:var(--green);border-radius:16px;margin:24px 0}.steps ol{padding-left:20px;margin:0}.steps li{margin:7px 0}section{background:#fffef9;border:1px solid var(--line);border-radius:16px;padding:22px;margin:20px 0}label{display:block;font-weight:600;font-size:14px;margin:18px 0 5px}input,select,button{font:inherit;color:inherit;width:100%;min-height:46px;border:1px solid #afbea1;border-radius:9px;padding:10px;background:white}button{cursor:pointer;background:#314a37;color:#fffef9;font-weight:600;margin-top:16px}button.secondary{background:transparent;color:var(--ink)}button:disabled{opacity:.55;cursor:wait}input:focus-visible,select:focus-visible,button:focus-visible,a:focus-visible{outline:3px solid #6b852f;outline-offset:3px}.show{display:flex;align-items:center;gap:10px;font-weight:400}.show input{width:19px;min-height:19px;height:19px}.notice{padding:12px 14px;background:#f2e4c7;border-radius:9px;overflow-wrap:anywhere}.success{background:#e5eed9}.error{color:#95432e}a{color:#314a37;overflow-wrap:anywhere}#networks{max-width:100%}[hidden]{display:none!important}.actions{display:flex;gap:12px}.actions button{flex:1}.address{font-size:19px;font-weight:650}
</style>
<main><div class="brand">↟ Plant Station · Bienvenue chez vous</div><h1>Un Wi-Fi pour vos plantes.</h1><p class="muted">Cet assistant reste dans votre kit. Aucun compte ni application à installer.</p>
<p><a href="/dashboard">Voir le tableau de bord tout de suite (sans rien configurer)</a></p>
<div class="steps"><ol><li>Restez connecté au Wi-Fi du kit, même si le téléphone indique « pas d’Internet ».</li><li>Choisissez le Wi-Fi <strong>2,4 GHz</strong> de votre maison et saisissez son mot de passe.</li><li>Après confirmation, le kit ferme son réseau et vous redirige vers le tableau de bord.</li></ol></div>
<p class="small" id="ap-hint">Wi-Fi du kit : <b id="ap-ssid">PLANTES</b> — mot de passe : <b id="ap-password">plantes123</b></p>
<p id="status" class="notice" role="status">Connexion à l’assistant…</p>
<section id="configuration"><h2>Le Wi-Fi de la maison</h2><p class="small">Un ancien réseau reste enregistré tant que le nouveau n’a pas obtenu de connexion.</p>
<label for="networks">Réseaux à proximité</label><select id="networks"><option value="">Choisir un réseau ou saisir son nom ci-dessous</option></select><p id="scan-status" class="small" role="status">Recherche des réseaux…</p><button type="button" class="secondary" id="scan">Actualiser les réseaux</button>
<form id="wifi-form"><label for="ssid">Nom du Wi-Fi</label><input id="ssid" name="ssid" required maxlength="32" autocomplete="off" autocapitalize="none" spellcheck="false"><p class="small">Saisissez le nom exact du réseau. La saisie manuelle convient aussi aux réseaux masqués.</p>
<label for="password">Mot de passe du Wi-Fi de la maison</label><input id="password" name="password" type="password" maxlength="64" autocomplete="new-password" autocapitalize="none" spellcheck="false"><label class="show"><input type="checkbox" id="show-password">Afficher pendant la saisie</label><p class="small">Saisissez le mot de passe de votre box. Laissez vide uniquement pour un réseau sans mot de passe.</p>
<div class="actions"><button id="connect">Connecter le kit</button><button type="button" class="secondary" id="cancel" hidden>Annuler l’essai</button></div></form><p id="form-error" class="error" role="alert"></p></section>
<section id="success" class="success" hidden><h2>C’est fait.</h2><p>Le kit rejoint <strong id="joined-ssid">votre Wi-Fi</strong> et ferme son propre réseau.</p><p id="redirect-state" class="notice" role="status">Redirection vers le tableau de bord…</p><p><a id="dashboard" class="address"></a></p><p class="small">Si la page ne s’ouvre pas toute seule : rejoignez le Wi-Fi de la maison dans les réglages du téléphone, puis ouvrez l’adresse ci-dessus. Le kit l’affiche aussi sur son écran.</p><button id="close">Fermer le Wi-Fi du kit maintenant</button></section>
<section><h2>Pour une nouvelle maison</h2><p>Maintenez PRG sur le Heltec pendant <strong>8 secondes puis relâchez</strong>. Scannez son QR pour rejoindre de nouveau le Wi-Fi du kit.</p><p class="small">À 3 secondes : relâcher met le kit en veille ; continuer jusqu’à 8 secondes ouvre la configuration. Les noms, mesures enregistrées et sondes sont conservés.</p><p class="small">Si le portail ne s’ouvre pas : restez sur le Wi-Fi du kit, puis ouvrez <a href="http://192.168.4.1/">http://192.168.4.1/</a>. Un appui court PRG affiche aussi le QR de ce lien.</p></section></main>
<script>
'use strict';
const $=id=>document.getElementById(id);let busy=false,polling=false,scanning=false,submitting=false,lastState=null,redirecting=false;
async function api(path,options={}){
 const controller=new AbortController();let timer,timedOut=false;
 const deadline=new Promise((_,reject)=>{timer=setTimeout(()=>{timedOut=true;controller.abort();reject(new Error('Délai de réponse dépassé'));},6000);});
 try{return await Promise.race([(async()=>{const response=await fetch(path,{cache:'no-store',...options,signal:controller.signal});if(!response.ok)throw new Error('Le kit a refusé cette demande ('+response.status+').');return response.json();})(),deadline]);}
 catch(e){if(timedOut)throw new Error('Le kit ne répond pas. Restez sur son Wi-Fi puis réessayez.');throw e;}finally{clearTimeout(timer);}
}
function post(path,fields={}){return api(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Plant-Request':'1'},body:new URLSearchParams(fields).toString()});}
function lock(value){busy=value;for(const id of ['connect','ssid','password','networks','scan'])$(id).disabled=value;$('cancel').hidden=!value;}
function render(data){
 lastState=data.state;$('status').textContent=data.message;$('status').classList.toggle('error',data.state==='failed');
 if(data.ssid)$('ap-ssid').textContent=data.ssid;
 if(data.ap_password)$('ap-password').textContent=data.ap_password;
 const success=data.state==='connected';$('configuration').hidden=success;$('success').hidden=!success;
 lock(data.state==='testing'||submitting);
 if(success){const valid=/^http:\/\/\d{1,3}(?:\.\d{1,3}){3}\/$/.test(data.lan_url);$('dashboard').textContent=valid?data.lan_url:'Adresse en attente sur le Heltec';if(valid)$('dashboard').href=data.lan_url;else $('dashboard').removeAttribute('href');if(valid)startRedirect(data.lan_url);}
}
async function waitLan(url){
 // Le telephone revient sur le Wi-Fi de la maison des que le kit ferme le sien.
 for(let i=0;i<20;i++){
  try{const c=new AbortController();const t=setTimeout(()=>c.abort(),2500);
   await fetch(url+'api/status',{mode:'no-cors',cache:'no-store',signal:c.signal});clearTimeout(t);return true;}
  catch(e){}
  await new Promise(r=>setTimeout(r,1500));
 }
 return false;
}
async function startRedirect(url){
 if(redirecting)return;redirecting=true;
 const s=$('redirect-state');
 s.textContent='Le kit ferme son réseau (8 s)…';
 await new Promise(r=>setTimeout(r,6000));
 s.textContent='Recherche du tableau de bord sur votre Wi-Fi…';
 if(await waitLan(url)){s.textContent='Trouvé : ouverture du tableau de bord…';location.replace(url);return;}
 s.textContent='Tableau de bord pas encore joignable : rejoignez le Wi-Fi de la maison, puis ouvrez l’adresse ci-dessous.';
}
async function poll(){if(polling)return;polling=true;try{render(await api('/api/setup/status'));}catch(e){if(!redirecting)$('status').textContent=lastState==='connected'?'Le Wi-Fi du kit est fermé. Rejoignez le Wi-Fi de la maison pour ouvrir le tableau de bord.':e.message;}finally{polling=false;}}
async function scan(){
 if(scanning||busy)return;scanning=true;$('scan').disabled=true;$('scan-status').textContent='Recherche des réseaux…';
 try{const until=Date.now()+12000;let data;do{data=await api('/api/setup/networks');if(!data.scanning)break;if(Date.now()>=until)throw new Error('Recherche trop longue. Vous pouvez saisir le nom du Wi-Fi.');await new Promise(resolve=>setTimeout(resolve,1000));}while(true);
 $('networks').replaceChildren();const prompt=document.createElement('option');prompt.value='';prompt.textContent='Choisir un réseau ou saisir son nom';$('networks').append(prompt);
 for(const network of data.networks){if(!network.ssid)continue;const option=document.createElement('option');option.value=network.ssid;option.textContent=network.ssid+(network.secured?' · protégé':' · sans mot de passe');$('networks').append(option);}
 $('scan-status').textContent=data.networks.length?'Choisissez un réseau ou saisissez son nom.':'Aucun réseau trouvé. Vous pouvez saisir son nom manuellement.';
 }catch(e){$('scan-status').textContent=e.message;}finally{scanning=false;$('scan').disabled=busy;}
}
$('networks').onchange=()=>{if($('networks').value)$('ssid').value=$('networks').value;};$('scan').onclick=scan;
$('show-password').onchange=()=>$('password').type=$('show-password').checked?'text':'password';
$('wifi-form').onsubmit=async event=>{event.preventDefault();if(busy)return;$('form-error').textContent='';
 const ssid=$('ssid').value,password=$('password').value,bytes=new TextEncoder();
 if(!ssid||bytes.encode(ssid).length>32){$('form-error').textContent='Le nom du Wi-Fi est vide ou trop long.';return;}
 const length=bytes.encode(password).length;if(length&&!(length>=8&&length<=63)&&!/^[0-9a-fA-F]{64}$/.test(password)){$('form-error').textContent='Vérifiez la longueur du mot de passe.';return;}
 submitting=true;lock(true);try{await post('/api/setup/connect',{ssid,password});$('password').value='';await poll();}catch(e){lock(false);$('form-error').textContent=e.message;}finally{submitting=false;}
};
$('cancel').onclick=async()=>{try{await post('/api/setup/cancel');await poll();}catch(e){$('form-error').textContent=e.message;}};
$('close').onclick=async()=>{try{await post('/api/setup/close');$('close').disabled=true;$('status').textContent='Fermeture dans deux secondes. Rejoignez le Wi-Fi de la maison.';}catch(e){$('status').textContent=e.message;}};
poll();scan();setInterval(poll,1000);
</script></html>)SETUPHTML";
