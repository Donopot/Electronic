"""Capture bornee du diagnostic USB XIAO/ADS1115, sans redemarrage."""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=float, default=25)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 0 < args.seconds <= 300:
        parser.error("--seconds doit etre compris entre 0 et 300")
    summary_path = args.output.with_suffix(".json")
    if summary_path.resolve() == args.output.resolve():
        parser.error("--output doit designer un journal, pas un fichier .json")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    pattern = re.compile(r"^\[MESURE\] t_ms=(\d+) adresse=(0x4[89AB]) A0_brut=(-?\d+) A0_V=(-?\d+\.\d+) etat=(\w+)$")
    samples = []
    errors = []
    addresses = set()
    pending = b""
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    port.dtr, port.rts, port.port = True, False, args.port
    started = time.monotonic()
    completed = False
    with args.output.open("w", encoding="utf-8") as log:
        header = f"Capture {args.port}, UTC={datetime.now(timezone.utc).isoformat()}, duree={args.seconds}s\n"
        log.write(header)
        print(header, end="", flush=True)
        try:
            port.open()
            started = time.monotonic()
            while time.monotonic() - started < args.seconds:
                pending += port.read(min(port.in_waiting or 1, 4096))
                while b"\n" in pending:
                    raw, pending = pending.split(b"\n", 1)
                    line = raw.decode("utf-8", errors="replace").rstrip("\r")
                    log.write(line + "\n")
                    log.flush()
                    print(line, flush=True)
                    match = pattern.fullmatch(line)
                    if match:
                        samples.append({"t_ms": int(match[1]), "address": match[2], "raw": int(match[3]),
                                        "volts": float(match[4]), "state": match[5],
                                        "host_elapsed_s": time.monotonic() - started})
                    elif line.startswith("[MESURE]"):
                        errors.append("[ERREUR] Mesure serie malformee : " + line)
                    if line.startswith("[ERREUR]"):
                        errors.append(line)
                    if line.startswith("[I2C] ACK adresse="):
                        addresses.add(line.split("=", 1)[1])
                if len(pending) > 8192:
                    raise RuntimeError("Sortie incompatible : ligne serie trop longue")
            completed = True
        except (serial.SerialException, OSError, RuntimeError, ValueError) as exc:
            message = f"[ERREUR] Capture interrompue : {exc}"
            errors.append(message)
            log.write(message + "\n")
            print(message, flush=True)
        finally:
            port.close()
        if pending:
            log.write("Ligne partielle : " + pending.decode("utf-8", errors="replace") + "\n")
    duration = time.monotonic() - started
    arrivals = [0.0] + [s["host_elapsed_s"] for s in samples] + [duration]
    max_gap = max(b - a for a, b in zip(arrivals, arrivals[1:]))
    coverage_ok = bool(samples) and max_gap <= 5.0
    summary = {
        "port": args.port,
        "completed": completed,
        "duration_requested_s": args.seconds,
        "duration_s": duration,
        "max_gap_s": max_gap,
        "coverage_ok": coverage_ok,
        "sample_count": len(samples),
        "errors": errors,
        "addresses": sorted(addresses),
        "samples": samples,
        "volts_min": min((s["volts"] for s in samples), default=None),
        "volts_max": max((s["volts"] for s in samples), default=None),
        "physical_sensor_validated": False,
        "note": "Les conversions ne prouvent pas la presence de la sonde ni sa sensibilite a l'humidite.",
    }
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print("BILAN", json.dumps({k: v for k, v in summary.items() if k != "samples"}), flush=True)
    accepted_states = {"DANS_PLAGE_NOMINALE", "PROCHE_ZERO_OFFSET_POSSIBLE"}
    return 0 if completed and coverage_ok and not errors and all(s["state"] in accepted_states for s in samples) else 2


if __name__ == "__main__":
    raise SystemExit(main())
