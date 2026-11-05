"""Capture simultanee des deux cartes, avec codes Bluetooth masques."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--seconds', type=float, default=55)
parser.add_argument('--tx', default='COM3')
parser.add_argument('--rx', default='COM4')
args = parser.parse_args()
if args.seconds <= 0:
    parser.error('--seconds doit etre positif')
root = Path(__file__).resolve().parents[1]
ports, logs, pending = {}, {}, {}
try:
    for name in dict.fromkeys((args.tx, args.rx)):
        port = serial.Serial(port=None, baudrate=115200, timeout=0)
        port.port, port.dtr, port.rts = name, True, False
        port.open()
        ports[name] = port
        logs[name] = (root / 'diagnostics' / f'lora-link-{name.lower()}.log').open('a', encoding='utf-8')
        logs[name].write('\nCapture UTC=' + datetime.now(timezone.utc).isoformat() + '\n')
        logs[name].flush()
        pending[name] = b''
    started = time.monotonic()
    while time.monotonic() - started < args.seconds:
        for name, port in ports.items():
            pending[name] += port.read(min(port.in_waiting, 4096))
            while b'\n' in pending[name]:
                raw, pending[name] = pending[name].split(b'\n', 1)
                line = raw.decode('utf-8', errors='replace').rstrip('\r')
                if '[BLE] QR=' in line or '[BLE] Appareil=' in line:
                    line = '[BLE] Configuration disponible (code masque).'
                logs[name].write(line + '\n')
                logs[name].flush()
                if name == args.rx or '[TX]' in line or '[ERREUR]' in line:
                    print(name, line, flush=True)
        time.sleep(0.03)
finally:
    for port in ports.values():
        port.close()
    for name, log in logs.items():
        if pending[name]:
            # Fragment incomplet : ne pas publier un eventuel code BLE tronque.
            log.write('[INFO] Fragment serie incomplet ignore.\n')
        log.close()
