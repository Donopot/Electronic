// Verify remote freshness, missing data and connection failures using the real UI script.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const header = fs.readFileSync(path.join(__dirname, '../firmware/EnviroTerminalDiagnostic/WebDashboardPage.h'), 'utf8');
const script = header.split('<script>')[1].split('</script>')[0]
  .replace('refresh();setInterval(refresh,2000);', 'globalThis.ready=refresh();');
const elements = new Map();
let unavailable = false;
let fixture = {sensor_ok:true, temperature_c:24.1, pressure_hpa:1004.3,
  sample_age_ms:110000, sample_max_age_ms:180000, sample_count:1,
  measurement_source:'lora', radio_spi_ok:true, radio_rssi_dbm:-72,
  wifi_connected:true, ssid:'Test', hostname:'enviro-test.local',
  wifi_message:'Test', device_id:'test'};
const context = vm.createContext({
  document:{getElementById(id){
    if(!elements.has(id)) elements.set(id,{textContent:'',className:'',setAttribute(){},removeAttribute(){}});
    return elements.get(id);
  }},
  fetch:async()=>{if(unavailable)throw Error('offline');return {ok:true,json:async()=>fixture};},
  AbortSignal, setInterval(){}
});
vm.runInContext(script,context);
(async()=>{
  await context.ready;
  assert.equal(elements.get('temperature').textContent,'24,1');
  assert.equal(elements.get('pressure').textContent,'1004,30');
  assert.match(elements.get('sensor').textContent,/reçue par LoRa/);
  assert.match(elements.get('radio').textContent,/-72 dBm/);
  // Remote data stays visible for the two-minute transmission interval.
  fixture={...fixture,sample_age_ms:179999};await context.refresh();
  assert.equal(elements.get('temperature').textContent,'24,1');
  fixture={...fixture,sample_age_ms:180000};await context.refresh();
  assert.equal(elements.get('temperature').textContent,'—');
  assert.equal(elements.get('pressure').textContent,'—');
  assert.match(elements.get('connection').textContent,/attente de la sonde/);
  // Fresh local measurements retain the original behavior.
  fixture={...fixture,measurement_source:'local',sample_age_ms:200,sample_max_age_ms:7000,
    sample_count:2,radio_tx_enabled:true};await context.refresh();
  assert.match(elements.get('sensor').textContent,/I²C/);
  assert.match(elements.get('radio').textContent,/Émission météo/);
  unavailable=true;await context.refresh();
  assert.equal(elements.get('temperature').textContent,'—');
  assert.match(elements.get('connection').textContent,/inaccessible/);
  console.log('Dashboard: remote data, expiry boundary, local mode and network failure passed.');
})().catch(error=>{console.error(error);process.exitCode=1;});
