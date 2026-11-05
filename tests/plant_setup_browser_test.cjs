// The actual embedded setup JavaScript, against explicitly simulated local APIs.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {chromium}=require('playwright');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'firmware/PlantStationVeml7700/src/web/PlantSetupPage.h'),'utf8');
const html=source.split('R"SETUPHTML(')[1].split(')SETUPHTML";')[0];
const output=path.join(root,'output/plant-dashboard-tests');fs.mkdirSync(output,{recursive:true});
let state='idle',message='SIMULATION DE TEST · Choisissez le Wi-Fi de la maison.',connects=0,cancels=0,scans=0,closed=false,hang=false,stayTesting=false,lastFields=null;
const special='Ma "box" <&> \\ jardin';
const server=http.createServer((req,res)=>{
 const url=new URL(req.url,'http://localhost');res.setHeader('Cache-Control','no-store');
 if(url.pathname==='/'){res.setHeader('Content-Type','text/html; charset=utf-8');return res.end(html);}
 res.setHeader('Content-Type','application/json');
 if(url.pathname==='/api/setup/status'){if(hang)return;return res.end(JSON.stringify({state,message,ssid:'PLANT-TEST12',ap_active:true,has_saved_network:true,lan_url:state==='connected'?'http://192.168.1.90/':'',close_in_s:300}));}
 if(url.pathname==='/api/setup/networks'){++scans;return res.end(JSON.stringify({scanning:scans%2===1,networks:scans%2===1?[]:[{ssid:special,secured:true,rssi:-42},{ssid:'<svg onload=alert(1)>',secured:false,rssi:-70}]}));}
 if(req.method==='POST'){
  let body='';req.on('data',chunk=>body+=chunk);req.on('end',()=>{
   assert.equal(req.headers['x-plant-request'],'1');assert.equal(req.headers['content-type'],'application/x-www-form-urlencoded');
   if(url.pathname==='/api/setup/connect'){
    lastFields=Object.fromEntries(new URLSearchParams(body));++connects;state='testing';message='SIMULATION · Connexion en cours…';
    if(!stayTesting)setTimeout(()=>{state=connects%2===1?'failed':'connected';message=state==='failed'?'SIMULATION · Connexion impossible. Vérifiez le mot de passe.':'SIMULATION · Wi-Fi enregistré, rejoignez votre réseau.';},200);
   }else if(url.pathname==='/api/setup/cancel'){++cancels;state='failed';message='SIMULATION · Essai annulé, ancien réseau conservé.';}
   else if(url.pathname==='/api/setup/close')closed=true;
   else{res.writeHead(404);return res.end('{}');}
   res.writeHead(202);res.end('{"ok":true}');
  });return;
 }
 res.writeHead(404);res.end('{}');
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));const base='http://127.0.0.1:'+server.address().port;
 const edge=process.env.PLANT_BROWSER_PATH||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
 const browser=await chromium.launch(fs.existsSync(edge)?{executablePath:edge,headless:true}:{headless:true});
 try{
  for(const viewport of [{width:390,height:844},{width:1280,height:900}]){
   state='idle';connects=0;scans=0;closed=false;stayTesting=false;const errors=[],requests=[];
   const page=await browser.newPage({viewport});page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>requests.push(r.url()));page.on('dialog',()=>assert.fail('SSID must never execute HTML'));
   await page.goto(base);await page.waitForFunction(()=>document.querySelectorAll('#networks option').length===3);
   assert.equal(await page.locator('#networks svg').count(),0);assert.equal(await page.locator('#networks option').nth(1).getAttribute('value'),special);
   await page.locator('#networks').selectOption(special);assert.equal(await page.locator('#ssid').inputValue(),special);
   await page.locator('#password').fill('FictionalWrongPass');await page.locator('#show-password').check();assert.equal(await page.locator('#password').getAttribute('type'),'text');await page.locator('#show-password').uncheck();
   await page.locator('#connect').click();await page.waitForFunction(()=>document.getElementById('status').textContent.includes('impossible'));
   assert.equal(await page.locator('#password').inputValue(),'');assert.equal(await page.locator('#connect').isEnabled(),true);assert.equal(lastFields.ssid,special);
   await page.screenshot({path:path.join(output,`setup-simulation-${viewport.width}.png`),fullPage:true});
   // Fixed-width SDK limits: valid 32-byte SSID and 64-hex PSK survive the form.
   await page.locator('#ssid').fill('S'.repeat(32));await page.locator('#password').fill('A'.repeat(64));await page.locator('#connect').click();
   await page.waitForFunction(()=>!document.getElementById('success').hidden);
   assert.equal(lastFields.ssid.length,32);assert.equal(lastFields.password.length,64);
   assert.equal(await page.locator('#dashboard').getAttribute('href'),'http://192.168.1.90/');
   assert.match(await page.locator('#success').textContent(),/Wi-Fi de la maison/);
   await page.locator('#close').click();await page.waitForFunction(()=>document.getElementById('close').disabled);assert.equal(closed,true);
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
   assert.deepEqual(errors,[]);assert(requests.every(url=>url.startsWith(base)),'No external resources');await page.close();
  }
  state='idle';stayTesting=true;const cancelPage=await browser.newPage();await cancelPage.goto(base);await cancelPage.locator('#ssid').fill('ManualHiddenNetwork');await cancelPage.locator('#password').fill('FictionalPassword');await cancelPage.locator('#connect').click();await cancelPage.locator('#cancel').waitFor({state:'visible'});await cancelPage.locator('#cancel').click();
  await cancelPage.waitForFunction(()=>document.getElementById('status').textContent.includes('annulé'));assert(cancels>0);assert.equal(await cancelPage.locator('#connect').isEnabled(),true);await cancelPage.close();
  state='idle';hang=true;const blocked=await browser.newPage();await blocked.goto(base);await blocked.waitForFunction(()=>document.getElementById('status').textContent.includes('ne répond pas'),{},{timeout:8000});hang=false;await blocked.evaluate(()=>poll());assert.equal(await blocked.locator('#connect').isEnabled(),true);await blocked.close();
  console.log('PASS setup browser desktop/mobile: async scan, escaped/special SSIDs, manual entry, wrong-password retry, password cleared, SSID32/PSK64, success LAN handoff, explicit close, cancellation, hung-fetch deadline/retry, keyboard-native fields, no JS errors/external resources (simulated APIs only)');
 }finally{await browser.close();server.close();}
})().catch(error=>{console.error(error);server.close();process.exitCode=1;});
