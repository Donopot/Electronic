"""Observe le canal via /api/radio sans changer les reglages du recepteur."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url', default='http://192.168.1.197/api/radio')
parser.add_argument('--seconds', type=float, default=120)
parser.add_argument('--period', type=float, default=0.2)
parser.add_argument('--output', default='diagnostics/lora-radio-channel-433mhz.jsonl')
args = parser.parse_args()
if args.seconds <= 0 or args.period < 0.05:
    parser.error('Duree positive et periode >= 0,05 seconde requises')
path = Path(args.output)
path.parent.mkdir(parents=True, exist_ok=True)
started = time.monotonic()
samples, errors, minimum, maximum = 0, 0, None, None
print('Observation radio UTC=' + datetime.now(timezone.utc).isoformat(), flush=True)
with path.open('w', encoding='utf-8') as log:
    while time.monotonic() - started < args.seconds:
        tick = time.monotonic()
        row = {'time_utc': datetime.now(timezone.utc).isoformat()}
        try:
            with urllib.request.urlopen(args.url, timeout=2) as response:
                row['radio'] = json.load(response)
            level = row['radio']['current_rssi_dbm']
            minimum = level if minimum is None else min(minimum, level)
            maximum = level if maximum is None else max(maximum, level)
            samples += 1
        except (OSError, ValueError, KeyError) as error:
            row['error'] = str(error)
            errors += 1
        log.write(json.dumps(row, ensure_ascii=False) + '\n')
        log.flush()
        time.sleep(max(0, args.period - (time.monotonic() - tick)))
print(json.dumps({'samples': samples, 'errors': errors,
                  'min_rssi_dbm': minimum, 'max_rssi_dbm': maximum,
                  'output': str(path)}), flush=True)
