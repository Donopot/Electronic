"""Test PRG/RESET via the Heltec CP2102 DTR/RTS circuit, without radio TX."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--leave-asleep", action="store_true")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    port = serial.Serial(port=None, baudrate=115200, timeout=0.05)
    port.dtr = False
    port.rts = False
    port.port = args.port

    with args.output.open("w", encoding="utf-8") as log:
        def write(message):
            log.write(message)
            log.flush()
            print(message, end="", flush=True)

        def mark(message):
            write(f"\n[TEST] {message}\n")

        def capture(seconds):
            chunks = []
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                chunk = port.read(port.in_waiting or 1).decode("utf-8", "replace")
                if chunk:
                    chunks.append(chunk)
                    write(chunk)
            return "".join(chunks)

        def require(condition, message):
            if not condition:
                raise RuntimeError(message)

        def reset():
            port.dtr = False
            port.rts = True
            time.sleep(0.2)
            port.rts = False

        def sleep_cycle():
            mark("DTR : PRG maintenu 3,4 s, puis relache")
            port.dtr = True
            held = capture(3.4)
            port.dtr = False
            released = capture(1.0)
            require("Appui long valide" in held, "Appui long non detecte")
            require("Entree en veille profonde" not in held,
                    "Veille declenchee avant relachement")
            require("SX1262 en sommeil (code=0)" in released,
                    "Sommeil radio non confirme")
            require("Entree en veille profonde" in released,
                    "Entree en veille non annoncee apres relachement")
            mark("Observation 8 s : aucun journal ni redemarrage attendu")
            quiet = capture(8)
            require(not quiet.strip(), "Sortie serie inattendue pendant la veille")
            mark("PASS : appui long, sommeil radio et silence apres entree en veille")

        keep_asleep = False
        write(f"Heltec sleep test {args.port}, UTC={datetime.now(timezone.utc).isoformat()}\n")
        try:
            port.open()
            port.reset_input_buffer()
            mark("RTS : redemarrage, puis verification du diagnostic v2")
            reset()
            boot = capture(6)
            require("DIAGNOSTIC RX v2" in boot and "radio_rx=OK" in boot,
                    "Demarrage v2 et reception non confirmes")

            mark("DTR : appui court PRG 0,3 s, puis maintien de l'ecoute")
            port.dtr = True
            short = capture(0.3)
            port.dtr = False
            short += capture(5.5)
            require("Appui detecte" in short, "GPIO0/PRG non detecte via DTR")
            require("[VEILLE]" not in short and "radio_rx=OK" in short,
                    "Appui court : veille ou interruption de l'ecoute")
            mark("PASS : appui court sans mise en veille")
            sleep_cycle()

            mark("RTS : equivalent RST pour reprendre apres veille")
            reset()
            restart = capture(6)
            require("DIAGNOSTIC RX v2" in restart and "radio_rx=OK" in restart,
                    "Redemarrage apres veille non confirme")
            require("[ERREUR]" not in restart, "Erreur au redemarrage")
            mark("PASS : redemarrage et reprise de la reception")
            if args.leave_asleep:
                sleep_cycle()
                keep_asleep = True
            mark("SUCCESS : carte laissee " + ("en veille" if keep_asleep else "en reception"))
        finally:
            if port.is_open:
                port.dtr = False
                port.rts = False
                port.close()


if __name__ == "__main__":
    main()
