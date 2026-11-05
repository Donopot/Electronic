"""Verifier PRG/RST, sommeil et reprise du firmware luminosite Plant Station."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM5')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=0.1)
    port.dtr = False
    port.rts = False
    port.port = args.port
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', encoding='utf-8') as log:
        def write(text):
            log.write(text)
            log.flush()
            print(text, end='', flush=True)

        def capture(seconds):
            result = ''
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                chunk = port.read(port.in_waiting or 1).decode('utf-8', errors='replace')
                if chunk:
                    result += chunk
                    write(chunk)
            return result

        def require(condition, message):
            if not condition:
                raise RuntimeError(message)

        def reset():
            port.dtr = False
            port.rts = True
            time.sleep(0.2)
            port.rts = False

        write(f'Plant Station sleep test {args.port}, UTC={datetime.now(timezone.utc).isoformat()}\n')
        try:
            port.open()
            port.reset_input_buffer()
            reset()
            boot = capture(5)
            require('v1.2.0' in boot and 'evenement=oled statut=ok' in boot and
                    'evenement=mesure statut=ok' in boot, 'Demarrage OLED et lux non confirmes')
            write('[TEST] Appui court PRG 0.3 s\n')
            port.dtr = True
            short = capture(0.3)
            port.dtr = False
            short += capture(2)
            require('veille_demande' not in short and 'evenement=mesure statut=ok' in short,
                    'Appui court : veille ou interruption inattendue')
            write('[TEST] Appui long PRG 3.6 s, puis relachement\n')
            port.dtr = True
            held = capture(3.6)
            require('Appui long valide' in held and 'Entree en veille profonde' not in held,
                    'Demande de relachement non confirmee')
            port.dtr = False
            released = capture(1)
            require('VEML7700 en sommeil confirme' in released and 'Entree en veille profonde' in released,
                    'Sommeil capteur/ESP32 non confirme')
            require(not capture(5).strip(), 'Sortie serie inattendue pendant la veille')
            write('[TEST] PRG seul ne doit pas reveiller\n')
            port.dtr = True
            alone = capture(0.3)
            port.dtr = False
            alone += capture(1)
            require(not alone.strip(), 'PRG seul a produit une sortie inattendue')
            write('[TEST] RST : reprise OLED et nouvelles mesures\n')
            reset()
            restarted = capture(5)
            require('evenement=oled statut=ok' in restarted and 'evenement=mesure statut=ok' in restarted,
                    'Reprise apres veille non confirmee')
            write('[PASS] Appui court, appui long puis relachement, sommeil capteur, silence, PRG seul et RST verifies. Carte laissee active.\n')
        finally:
            if port.is_open:
                # Laisser la carte active meme si un controle a echoue.
                reset()
                port.dtr = False
                port.rts = False
                port.close()


if __name__ == '__main__':
    main()
