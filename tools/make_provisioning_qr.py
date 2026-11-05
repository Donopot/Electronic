"""Generer localement le QR Bluetooth Espressif depuis le diagnostic prive."""
import argparse
import json
from pathlib import Path
import qrcode

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
payloads = [line.split('[BLE] QR=', 1)[1] for line in
            args.log.read_text(encoding='utf-8').splitlines() if '[BLE] QR=' in line]
if not payloads:
    raise SystemExit('Aucun code Bluetooth dans le diagnostic.')
payload = json.loads(payloads[-1])
if payload.get('transport') != 'ble' or not payload.get('pop') or not payload.get('name'):
    raise SystemExit('Code de configuration Bluetooth incomplet.')
qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=8, border=4)
qr.add_data(json.dumps(payload, separators=(',', ':')))
qr.make(fit=True)
args.output.parent.mkdir(parents=True, exist_ok=True)
qr.make_image(fill_color='black', back_color='white').save(args.output)
print(f'QR Bluetooth local : {args.output.resolve()}')
