"""Verifier deux relances reelles du service BLE via le port USB de diagnostic."""
import argparse
from pathlib import Path
import re
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='COM3')
parser.add_argument('--output', type=Path, default=Path('diagnostics/ble-session-recovery.log'))
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
port = serial.Serial(port=None, baudrate=115200, timeout=.3)
port.dtr = True
port.rts = False
port.port = args.port
with args.output.open('w', encoding='utf-8') as log:
    def record(line):
        if '[BLE] QR=' in line:
            return
        line = re.sub(r'(\[BLE\] Appareil=.* ; code=).*', r'\1[masque]', line)
        print(line, flush=True)
        log.write(line + '\n')
        log.flush()
    try:
        port.open()
        for cycle in range(2):
            port.reset_input_buffer()
            record(f'TEST : relance BLE {cycle + 1}/2 par wifi-setup.')
            port.write(b'wifi-setup\n')
            started = time.monotonic()
            closed = False
            while time.monotonic() - started < 15:
                line = port.readline().decode('utf-8', errors='replace').strip()
                if not line:
                    continue
                record(line)
                if 'Session fermee' in line:
                    closed = True
                if 'Configuration disponible dans ESP BLE Provisioning.' in line:
                    if cycle and not closed:
                        raise RuntimeError('La fermeture de la session precedente n’a pas ete confirmee.')
                    record(f'OK : nouvelle session BLE {cycle + 1}/2 demarree.')
                    break
            else:
                raise RuntimeError('La nouvelle session BLE n’a pas demarre dans les 15 secondes.')
            time.sleep(1)
        record('BILAN : deux relances BLE reussies sur la carte ; appairage Android a confirmer.')
    finally:
        port.close()
