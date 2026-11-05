"""Diagnostic de demarrage de la sonde avec reprise apres coupure USB."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import time
import serial
from serial.tools import list_ports

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--seconds', type=float, default=150)
parser.add_argument('--reset', action='store_true')
parser.add_argument('--save-ble', action='store_true', help='Conserver le QR BLE uniquement dans .tools/private')
args = parser.parse_args()
if args.seconds <= 0:
    parser.error('--seconds doit etre positif')
root = Path(__file__).resolve().parents[1]
identity = 'e83dc181ca70'
port, pending, reset_done, waiting = None, b'', False, False
started = time.monotonic()
with (root / 'diagnostics/transmitter-startup-recovery.log').open('a', encoding='utf-8') as log:
    def emit(line):
        if '[BLE] QR=' in line or '[BLE] Appareil=' in line:
            if args.save_ble:
                private = root / '.tools/private/transmitter-ble-setup.log'
                private.parent.mkdir(parents=True, exist_ok=True)
                with private.open('a', encoding='utf-8') as private_log:
                    private_log.write(line + '\n')
            line = '[BLE] Configuration disponible (code masque).'
        print(line, flush=True)
        log.write(line + '\n')
        log.flush()

    emit('\nCapture UTC=' + datetime.now(timezone.utc).isoformat())
    try:
        while time.monotonic() - started < args.seconds:
            if port is None:
                matches = [p for p in list_ports.comports()
                           if (p.serial_number or '').replace(':', '').replace('-', '').lower() == identity]
                if len(matches) != 1:
                    if not waiting:
                        emit('[USB] Attente de la sonde e83dc181ca70...')
                        waiting = True
                    time.sleep(1)
                    continue
                candidate = serial.Serial(port=None, baudrate=115200, timeout=0.2)
                candidate.port, candidate.dtr, candidate.rts = matches[0].device, True, False
                try:
                    candidate.open()
                    port, waiting = candidate, False
                    emit('[USB] Sonde ouverte sur ' + port.port)
                    if args.reset and not reset_done:
                        reset_done = True
                        emit('[USB] Un redemarrage demande pour observer le demarrage complet.')
                        port.dtr = False
                        port.rts = True
                        port.dtr = False
                        time.sleep(0.2)
                        port.rts = False
                        port.dtr = False
                        time.sleep(0.2)
                        port.dtr = True
                except (serial.SerialException, OSError) as error:
                    candidate.close()
                    port = None
                    emit('[USB] Ouverture interrompue : ' + str(error))
                    time.sleep(1)
                    continue
            try:
                pending += port.read(min(port.in_waiting or 1, 4096))
                while b'\n' in pending:
                    raw, pending = pending.split(b'\n', 1)
                    emit(raw.decode('utf-8', errors='replace').rstrip('\r'))
            except (serial.SerialException, OSError) as error:
                emit('[USB] Communication interrompue : ' + str(error))
                port.close()
                port, pending = None, b''
                time.sleep(0.5)
    finally:
        if port is not None:
            port.close()
    emit('[USB] Fin de capture : %.1f secondes.' % (time.monotonic() - started))
