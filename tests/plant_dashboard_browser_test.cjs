// Browser verification of the actual embedded page against explicitly simulated local APIs.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const { chromium } = require('playwright');
const root = path.resolve(__dirname, '..');
const source = fs.readFileSync(path.join(root, 'firmware/PlantStationVeml7700/src/web/PlantDashboardPage.h'), 'utf8');
const html = source.split('R"PLANTHTML(')[1].split(')PLANTHTML";')[0];
const out = path.join(root, 'output/plant-dashboard-tests');
fs.mkdirSync(out, { recursive: true });
const now = 1793700000000;
let unavailable = false, empty = false, hungStatus = false, slowHistory = false, missedCycle = false, namesFail = false, timeFail = false, failHistoryPage = false;
let timePosts = 0, plantPosts = 0;const historyCursors = [];
const fixture = {
  api_version: 1, source: 'live', mood: 'neutral', dry_mask: 0, warning_mask: 0,
  radio_ready: true, uptime_ms: 200000, utc_ms: now, lux: 530,
  wifi_connected: true, url: 'http://127.0.0.1/', hostname: 'plant-test.local',
  network_message: 'SIMULATION DE TEST · Wi-Fi local', history_state: 'ready', history_dropped: 0, history_corrupt: 0,
  names_persistence: 'saved', names_revision: 0, names_saved_revision: 0, clock_persistence: 'saved',
  plants: Array.from({ length: 4 }, (_, i) => ({ id: i + 1, name: 'Plante ' + (i + 1),
    state: 'uncalibrated', label: 'À étalonner', age_ms: 60000, microvolts: 1800000 + i * 100000,
    adc_status: 'ok', battery_percent: null, battery_status: 'not_reported' }))
};
const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  res.setHeader('Cache-Control', 'no-store');
  if (url.pathname === '/') { res.setHeader('Content-Type', 'text/html; charset=utf-8'); return res.end(html); }
  res.setHeader('Content-Type', 'application/json');
  if (url.pathname === '/api/status') {
    if (hungStatus) return;
    if (unavailable) { res.writeHead(503); return res.end('{"error":"simulated_offline"}'); }
    return res.end(JSON.stringify(fixture));
  }
  if (url.pathname === '/api/history') {
    const node = Number(url.searchParams.get('node'));
    const cursor=url.searchParams.get('after')||'0';historyCursors.push(cursor);
    if(failHistoryPage&&cursor==='123'){res.writeHead(503);return res.end('{"error":"simulated_page_failure"}');}
    let records = empty ? [] : [
      { utc_ms: now - 8000000, boot: 1, uptime_ms: 0, microvolts: 1000000 + node * 200000, adc_status: 'ok' },
      { utc_ms: now - 6200000, boot: 1, uptime_ms: 1800000, microvolts: 1100000 + node * 200000, adc_status: 'ok' },
      { utc_ms: now - 5000000, boot: 1, uptime_ms: 3000000, microvolts: null, adc_status: 'adc_error' },
      { utc_ms: now - 4000000, boot: 1, uptime_ms: 4000000, microvolts: 1200000 + node * 200000, adc_status: 'ok' },
      { utc_ms: null, boot: 2, uptime_ms: 100, microvolts: 2000000, adc_status: 'ok' },
      { utc_ms: now - 1800000, boot: 1, uptime_ms: 6200000, microvolts: 3030000, adc_status: 'ok' }
    ];
    if(missedCycle)records=[{utc_ms:now-4000000,boot:1,microvolts:1000000},{utc_ms:now-400000,boot:1,microvolts:2000000}];
    const index=cursor==='0'?0:cursor==='123'?1:cursor==='456'?2:-1;
    assert(index>=0,'Client requests the returned cursor');
    const more=!empty&&!missedCycle&&index<2;
    const page=empty||missedCycle?records:index===0?records.slice(0,2):index===1?records.slice(2,5):records.slice(5);
    const send=()=>res.end(JSON.stringify({node,days:Number(url.searchParams.get('days')),next:index===0?'123':index===1?'456':'789',more,records:page}));
    if(slowHistory)setTimeout(send,2200);else send();return;
  }
  if (url.pathname === '/api/time') {++timePosts;if(timeFail){res.writeHead(503);return res.end('{"error":"clock_failed"}');}fixture.utc_ms=now;fixture.clock_persistence='saved';return res.end('{"ok":true}');}
  if (url.pathname === '/api/plants') {
    ++plantPosts;
    let text = ''; req.on('data', c => text += c); req.on('end', () => {
      const fields = new URLSearchParams(text); fixture.plants[Number(fields.get('id')) - 1].name = fields.get('name');
      ++fixture.names_revision;fixture.names_persistence='pending';
      res.writeHead(202);res.end(JSON.stringify({ok:true,persistence:'pending',revision:fixture.names_revision}));
      setTimeout(()=>{fixture.names_persistence=namesFail?'failed':'saved';if(!namesFail)fixture.names_saved_revision=fixture.names_revision;},150);
    }); return;
  }
  res.writeHead(404); res.end('{}');
});
(async () => {
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = 'http://127.0.0.1:' + server.address().port;
  const edge = process.env.PLANT_BROWSER_PATH || 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
  const browser = await chromium.launch(fs.existsSync(edge) ? { executablePath: edge, headless: true } : { headless: true });
  try {
    for (const viewport of [{ width: 1280, height: 900 }, { width: 390, height: 844 }]) {
      const page = await browser.newPage({ viewport }); const errors = [], requests = [];
      page.on('pageerror', error => errors.push(error.message));
      page.on('request', request => requests.push(request.url()));
      await page.goto(base); await page.locator('.card').last().waitFor();
      await page.waitForFunction(() => document.getElementById('chart-note').textContent.includes('non placés'));
      assert.equal(await page.locator('.card').count(), 4);
      assert.equal(await page.getByText('Batterie : non communiqué', { exact: true }).count(), 4);
      assert.equal(await page.locator('.state.uncalibrated').count(), 4);
      assert.equal(await page.locator('#chart .curve').count(), 2); // Fault splits line; last-page-only point is connected separately.
      assert.equal(await page.evaluate(()=>history.some(r=>r.microvolts===3030000)),true);
      assert.match(await page.locator('#chart').innerHTML(),/29\.5/); // Final page's 3.030 V point is actually plotted.
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
      assert.equal(await page.locator('#chart text').first().evaluate(text =>
        parseFloat(getComputedStyle(text).fontSize) * text.getScreenCTM().a >= 11), true, 'Chart labels remain legible');
      if (viewport.width < 600) assert.equal(await page.locator('.sidebar').evaluate(el => getComputedStyle(el).display), 'block');
      await page.locator('[data-range="1"]').click();
      assert.equal(await page.locator('[data-range="1"]').getAttribute('aria-pressed'), 'true');
      await page.locator('#settings').click(); await page.locator('#name-1').fill('Été <b>balcon</b>');
      await page.locator('#save').click(); await page.waitForFunction(() => !document.getElementById('edit').open);
      await page.waitForFunction(() => document.querySelector('.card h3').textContent.includes('<b>'));
      assert.equal(await page.locator('.card h3 b').count(), 0);
      await page.locator('#settings').focus(); await page.keyboard.press('Enter');
      assert.equal(await page.locator('#edit').evaluate(dialog => dialog.open), true);
      await page.keyboard.press('Escape');
      await page.locator('#settings').click();await page.locator('#name-2').fill('🌿'+'a'.repeat(23));
      assert.equal(await page.locator('#name-2').inputValue(),'🌿'+'a'.repeat(23));
      await page.locator('#save').click();await page.waitForFunction(()=>!document.getElementById('edit').open);
      await page.screenshot({ path: path.join(out, `browser-simulation-${viewport.width}.png`), fullPage: true });
      assert.equal(errors.length, 0, errors.join('\n'));
      assert(requests.every(url => url.startsWith(base)), 'No external web resources');
      await page.close();
    }
    const page = await browser.newPage(); await page.goto(base); await page.locator('.card').last().waitFor();
    fixture.mood = 'sad'; fixture.dry_mask = 1; fixture.warning_mask = 8;
    fixture.plants[0].state = 'dry'; fixture.plants[3].state = 'adc_fault'; fixture.plants[3].microvolts = null;
    await page.evaluate(() => refresh());
    assert.match(await page.locator('#detail').textContent(), /SOIL-01/);
    assert.match(await page.locator('#warnings').textContent(), /SOIL-04.*Défaut ADC/);
    empty = true; await page.evaluate(() => loadHistory());
    assert.match(await page.locator('#chart-note').textContent(), /Aucune mesure datée/);
    unavailable = true; await page.evaluate(() => refresh());
    assert.equal(await page.locator('#offline').isVisible(), true);
    unavailable = false; fixture.utc_ms = null;timeFail=true; await page.evaluate(async () => { await refresh(); await loadHistory(); });
    assert.match(await page.locator('#chart-note').textContent(), /Heure inconnue/);
    assert.equal(await page.locator('#chart .curve').count(), 0);
    await page.close();
    empty=false;fixture.utc_ms=now;slowHistory=true;historyCursors.length=0;
    const slow=await browser.newPage();await slow.goto(base);
    await slow.waitForFunction(()=>!loading&&history.some(r=>r.microvolts===3030000));
    assert.deepEqual(historyCursors,['0','123','456']); // Automatic 5s status poll must not restart the 6.6s history download.
    assert.match(await slow.locator('#chart').innerHTML(),/29\.5/);
    const firstSlow=slow.waitForRequest(r=>r.url().includes('/api/history?node=1')&&r.url().endsWith('after=0'));
    await slow.evaluate(()=>{loadHistory();});await firstSlow;
    const changedNode=slow.waitForRequest(r=>r.url().includes('/api/history?node=2')&&r.url().endsWith('after=0'));
    await slow.locator('.card').nth(1).click();await changedNode;slowHistory=false;
    await slow.waitForFunction(()=>!loading&&selected===2&&history.some(r=>r.microvolts===3030000));
    assert.equal(await slow.evaluate(()=>history[0].microvolts),1400000,'Explicit node selection cancels and replaces the pending history');
    failHistoryPage=true;await slow.evaluate(()=>loadHistory());
    assert.equal(await slow.evaluate(()=>history.length),2,'A failed later page retains only an explicitly incomplete prefix');
    assert.match(await slow.locator('#chart-note').textContent(),/Historique incomplet.*simulated_page_failure/);
    await slow.setViewportSize({width:390,height:844});
    await slow.waitForFunction(()=>document.getElementById('chart').viewBox.baseVal.width<500);
    assert.match(await slow.locator('#chart-note').textContent(),/Historique incomplet.*simulated_page_failure/);
    assert.match(await slow.locator('#chart').getAttribute('aria-label'),/incomplet/);
    failHistoryPage=false;await slow.evaluate(()=>loadHistory());
    assert.doesNotMatch(await slow.locator('#chart-note').textContent(),/incomplet/);
    slowHistory=false;missedCycle=true;await slow.evaluate(()=>loadHistory());
    assert.equal(await slow.locator('#chart .curve').count(),0,'A missed 30min cycle leaves a 60min gap');
    assert.equal(await slow.locator('#chart circle').count(),2);missedCycle=false;
    const postsBeforeInvalid=plantPosts,firstName=fixture.plants[0].name;
    await slow.locator('#settings').click();await slow.locator('#name-1').fill('Ne doit pas être sauvé');await slow.locator('#name-4').fill('x'.repeat(25));await slow.locator('#save').click();
    await slow.waitForFunction(()=>document.getElementById('edit-error').textContent.includes('24 caractères'));
    assert.equal(plantPosts,postsBeforeInvalid,'All four names must validate before the first POST');assert.equal(fixture.plants[0].name,firstName);
    for(const control of ['\u0001','\u0085']){
      await slow.evaluate(()=>document.getElementById('edit-error').textContent='');await slow.locator('#name-4').fill('Nom'+control+'invalide');await slow.locator('#save').click();
      await slow.waitForFunction(()=>document.getElementById('edit-error').textContent.includes('24 caractères'));
      assert.equal(plantPosts,postsBeforeInvalid,'A control character in the final name prevents every POST');assert.equal(fixture.plants[0].name,firstName);
    }
    await slow.locator('#cancel').click();
    namesFail=true;await slow.locator('#settings').click();await slow.locator('#name-1').fill('Nom en attente');await slow.locator('#save').click();
    await slow.waitForFunction(()=>document.getElementById('edit-error').textContent.includes('refusée'));
    assert.equal(await slow.locator('#edit').evaluate(d=>d.open),true);namesFail=false;
    await slow.locator('#save').click();await slow.waitForFunction(()=>!document.getElementById('edit').open);
    await slow.close();
    hungStatus=true;const hung=await browser.newPage();await hung.goto(base);await hung.waitForFunction(()=>!refresh.busy&&!document.getElementById('offline').hidden,{},{timeout:8000});
    hungStatus=false;await hung.evaluate(()=>refresh());assert.equal(await hung.locator('#offline').isVisible(),false);await hung.close();
    fixture.utc_ms=null;timeFail=true;const before=timePosts;const clock=await browser.newPage();await clock.goto(base);
    await clock.waitForFunction(()=>document.getElementById('clock').textContent.includes('nouvel essai'));
    await clock.waitForFunction(()=>!clockBusy);await clock.evaluate(()=>refresh());assert.equal(timePosts,before+1,'Clock failure retries are rate limited');
    timeFail=false;await clock.evaluate(()=>{const real=Date.now;Date.now=()=>real()+31000;return refresh();});
    await clock.waitForFunction(()=>document.getElementById('clock').textContent==='Téléphone');
    fixture.utc_ms=null;fixture.uptime_ms=10;
    await clock.evaluate(()=>{const real=Date.now;Date.now=()=>real()+31000;return refresh();});
    await clock.waitForFunction(()=>document.getElementById('clock').textContent==='Téléphone');
    assert.equal(timePosts,before+3,'An open page resynchronizes a rebooted hub');await clock.close();
    console.log('PASS browser desktop/mobile: cursor-dependent pages/final plotted point, failed pagination remains incomplete across resize until successful reload, all names validated before any POST, slow history/status polls, hung fetch retry, clock retry/reboot, names durability/Unicode, gap/states/keyboard/axes/no external resources (simulated APIs only)');
  } finally { await browser.close(); server.close(); }
})().catch(error => { console.error(error); server.close(); process.exitCode = 1; });
