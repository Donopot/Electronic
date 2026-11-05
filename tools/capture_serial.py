"""Capture bornee du diagnostic USB serie (redemarrage USB optionnel)."""

import argparse
from datetime import datetime, timezone
from pathlib import Path
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--seconds", type=float, default=65)
    parser.add_argument("--max-errors", type=int, default=3, help="Arreter apres N erreurs ; 0 pour poursuivre")
    parser.add_argument("--reset", action="store_true", help="Redemarrer le C3 via USB RTS avant la capture")
    parser.add_argument("--output", type=Path, default=Path("diagnostics/bmp180-com3.log"))
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds doit etre positif")
    if args.max_errors < 0:
        parser.error("--max-errors doit etre positif ou nul")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    readings = []
    errors = 0
    pattern = re.compile(
        r"t=(\d+) ms \| Temperature : (-?[\d.]+).*Pression : ([\d.]+) hPa"
    )
    # Fixer les lignes avant l'ouverture pour eviter une demande de mode BOOT.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.3)
    port.dtr = True
    port.rts = False
    port.port = args.port
    with args.output.open("w", encoding="utf-8") as log:
        header = f"Capture {args.port}, 115200 bauds, UTC={datetime.now(timezone.utc).isoformat()}\n"
        log.write(header)
        print(header, end="", flush=True)
        try:
            port.open()
            if args.reset:
                # Sequence HardReset USB d'esptool : RTS, puis DTR pour que
                # usbser.sys propage le changement de lignes sous Windows.
                port.dtr = False
                port.rts = True
                port.dtr = False
                time.sleep(0.2)
                port.rts = False
                port.dtr = False
                time.sleep(0.2)
            started = time.monotonic()
            pending = b""
            while time.monotonic() - started < args.seconds:
                pending += port.read(port.in_waiting or 1)
                while b"\n" in pending:
                    raw, pending = pending.split(b"\n", 1)
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    log.write(line + "\n")
                    log.flush()
                    print(line, flush=True)
                    match = pattern.search(line)
                    if match:
                        readings.append((int(match[1]), float(match[2]), float(match[3])))
                    if "[ERREUR]" in line:
                        errors += 1
                if args.max_errors and errors >= args.max_errors:
                    break
            if pending:
                log.write(pending.decode("utf-8", errors="replace") + "\n")
            summary = f"BILAN : {len(readings)} mesures, {errors} erreurs, {time.monotonic() - started:.1f} s de capture."
            if readings:
                temperatures = [row[1] for row in readings]
                pressures = [row[2] for row in readings]
                summary += (
                    f" Temperature={min(temperatures):.1f}..{max(temperatures):.1f} C;"
                    f" pression={min(pressures):.2f}..{max(pressures):.2f} hPa."
                )
            if len(readings) > 1:
                intervals = [b[0] - a[0] for a, b in zip(readings, readings[1:])]
                summary += f" Intervalles={min(intervals)}..{max(intervals)} ms."
            print(summary, flush=True)
            log.write(summary + "\n")
        finally:
            port.close()
    return 0 if readings and errors == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
