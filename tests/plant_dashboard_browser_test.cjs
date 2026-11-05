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
let unavailable = false, empty = false;
const fixture = {
  api_version: 1, source: 'live', mood: 'neutral', dry_mask: 0, warning_mask: 0,
  radio_ready: true, uptime_ms: 200000, utc_ms: now, lux: 530,
  wifi_connected: true, url: 'http://127.0.0.1/', hostname: 'plant-test.local',
  network_message: 'SIMULATION DE TEST · Wi-Fi local', history_state: 'ready', history_dropped: 0, history_corrupt: 0,
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
    if (unavailable) { res.writeHead(503); return res.end('{"error":"simulated_offline"}'); }
    return res.end(JSON.stringify(fixture));
  }
  if (url.pathname === '/api/history') {
    const node = Number(url.searchParams.get('node'));
    const records = empty ? [] : [
      { utc_ms: now - 8000000, boot: 1, uptime_ms: 0, microvolts: 1000000 + node * 200000, adc_status: 'ok' },
      { utc_ms: now - 6200000, boot: 1, uptime_ms: 1800000, microvolts: 1100000 + node * 200000, adc_status: 'ok' },
      { utc_ms: now - 5000000, boot: 1, uptime_ms: 3000000, microvolts: null, adc_status: 'adc_error' },
      { utc_ms: now - 4000000, boot: 1, uptime_ms: 4000000, microvolts: 1200000 + node * 200000, adc_status: 'ok' },
      { utc_ms: null, boot: 2, uptime_ms: 100, microvolts: 2000000, adc_status: 'ok' }
    ];
    return res.end(JSON.stringify({ node, days: Number(url.searchParams.get('days')), next: '123', more: false, records }));
  }
  if (url.pathname === '/api/time') return res.end('{"ok":true}');
  if (url.pathname === '/api/plants') {
    let text = ''; req.on('data', c => text += c); req.on('end', () => {
      const fields = new URLSearchParams(text); fixture.plants[Number(fields.get('id')) - 1].name = fields.get('name');
      res.end('{"ok":true}');
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
      assert.equal(await page.locator('#chart .curve').count(), 1); // Fault splits line, never interpolates across it.
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
    unavailable = false; fixture.utc_ms = null; await page.evaluate(async () => { await refresh(); await loadHistory(); });
    assert.match(await page.locator('#chart-note').textContent(), /Heure inconnue/);
    assert.equal(await page.locator('#chart .curve').count(), 0);
    await page.close();
    console.log('PASS browser desktop/mobile: real embedded JS, 4 states/cards, unknown battery, fault gaps, periods, empty/unknown clock, offline, long UTF-8/XSS-safe names, keyboard, zero external resources/JS errors (simulated APIs only)');
  } finally { await browser.close(); server.close(); }
})().catch(error => { console.error(error); server.close(); process.exitCode = 1; });
