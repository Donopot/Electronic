"""Capture USB simultanee bornee et comparaison exacte des diagnostics SOIL-01/02/03 TX/RX."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import threading
import time

PACKET = re.compile(r"^\[SOIL_(TX|RX)\] authenticated=no node=SOIL-(0[123]) session=(\d+) seq=(\d+) uptime_ms=(\d+) status=(ok|adc_unavailable|adc_error|adc_range) address=(\d+) raw=(-?\d+) uv=(-?\d+)$")
ARRIVAL = re.compile(r"^\[HOST t_s=(\d{1,6}\.\d{6})\] (.*)$")
ATTRIBUTED_REJECT = re.compile(r"^\[SOIL_REJECT\] node=SOIL-(0[123]) reason=(duplicate|old_sequence) authenticated=no$")
FIELDS = ("node", "session", "seq", "uptime_ms", "status", "address", "raw", "uv")


def parse_records(lines, direction, nodes=None):
    records, errors = [], []
    for line in lines:
        line = line.rstrip("\r\n")
        arrival = ARRIVAL.fullmatch(line)
        elapsed = float(arrival[1]) if arrival else None
        if arrival:
            line = arrival[2]
        rejected = ATTRIBUTED_REJECT.fullmatch(line)
        if rejected and nodes is not None and int(rejected[1]) not in nodes:
            continue
        match = PACKET.fullmatch(line)
        if match:
            if match[1] != direction:
                errors.append("unexpected_direction: " + line)
                continue
            item = dict(zip(FIELDS, match.groups()[1:]))
            try:
                for key in item:
                    if key != "status":
                        if len(item[key]) > 12:
                            raise ValueError("champ numerique trop long")
                        item[key] = int(item[key])
            except ValueError:
                errors.append("invalid_numeric_log: " + line)
                continue
            valid = (0 < item["session"] <= 0xFFFFFFFF and 0 <= item["seq"] <= 0xFFFFFFFF
                     and 0 <= item["uptime_ms"] <= 0xFFFFFFFF)
            if item["status"] == "ok":
                valid &= 0x48 <= item["address"] <= 0x4B and -8 <= item["raw"] <= 26400 and item["uv"] == item["raw"] * 125
            else:
                valid &= item["raw"] == 0 and item["uv"] == 0 and (item["address"] == 0 if item["status"] == "adc_unavailable" else 0x48 <= item["address"] <= 0x4B)
            if valid:
                if nodes is not None and item["node"] not in nodes:
                    continue
                item["host_elapsed_s"] = elapsed
                records.append(item)
                if item["status"] != "ok":
                    errors.append("adc_fault: " + line)
            else:
                errors.append("invalid_packet_log: " + line)
        elif line.startswith(("[SOIL_TX]", "[SOIL_RX]", "[SOIL_TX_ERROR]", "[SOIL_REJECT]", "[SOIL_LOG]", "[ERREUR]", "[CAPTURE_ERROR]")) or (line.startswith("[SOIL_RADIO]") and ("error=" in line or "state=error" in line or "code=" in line and "code=0" not in line)):
            errors.append(line)
    return records, errors


def compare(tx_lines, rx_lines, minimum=3, duration=None, margin=1.0):
    tx, tx_errors = parse_records(tx_lines, "TX")
    nodes = {packet["node"] for packet in tx}
    all_rx, _ = parse_records(rx_lines, "RX")
    rx, rx_errors = parse_records(rx_lines, "RX", nodes=nodes or None)
    background_rx = [packet for packet in all_rx if nodes and packet["node"] not in nodes]
    def interior(packet):
        when = packet["host_elapsed_s"]
        return duration is None or when is not None and margin <= when <= duration - margin
    def index(records):
        result, duplicates = {}, []
        for packet in records:
            key = (packet["node"], packet["session"], packet["seq"])
            if key in result:
                duplicates.append(list(key))
            result[key] = packet
        return result, duplicates
    sent, tx_duplicate = index(tx)
    received, rx_duplicate = index(rx)
    mismatches, matched, boundary = [], [], []
    for key in sorted(sent.keys() & received.keys()):
        left, right = sent[key], received[key]
        if any(left[field] != right[field] for field in FIELDS):
            mismatches.append({"tx": left, "rx": right})
        elif interior(left) and interior(right):
            matched.append({**{field: left[field] for field in FIELDS},
                            "tx_host_s": left["host_elapsed_s"], "rx_host_s": right["host_elapsed_s"]})
        else:
            boundary.append({"kind": "paired_boundary", "tx": left, "rx": right})
    losses, unmatched_rx = [], []
    for key in sorted(sent.keys() - received.keys()):
        packet = sent[key]
        if interior(packet):
            losses.append(packet)
        else:
            boundary.append({"kind": "tx_incomplete_boundary", "tx": packet})
    for key in sorted(received.keys() - sent.keys()):
        packet = received[key]
        if interior(packet):
            unmatched_rx.append(packet)
        else:
            boundary.append({"kind": "rx_incomplete_boundary", "rx": packet})
    valid_matches = sum(packet["status"] == "ok" for packet in matched)
    matched_by_node = {node: sum(p["status"] == "ok" and p["node"] == node for p in matched) for node in sorted(nodes)}
    errors = tx_errors + rx_errors
    if duration is not None:
        for direction, records in (("TX", tx), ("RX", rx)):
            if any(packet["host_elapsed_s"] is None for packet in records):
                errors.append("missing_host_timestamp: " + direction)
    return {"authenticated": False, "compared_nodes": sorted(nodes), "background_rx": background_rx,
            "tx_count": len(tx), "rx_count": len(rx), "matched": matched,
            "comparison_window_s": [margin, max(margin, duration - margin)] if duration is not None else None,
            "boundary_margin_s": margin, "boundary_pairs": boundary,
            "valid_matched_count": valid_matches, "losses": losses, "unmatched_rx": unmatched_rx,
            "valid_matched_by_node": matched_by_node, "minimum_per_node": minimum,
            "mismatches": mismatches, "tx_duplicates": tx_duplicate, "rx_duplicates": rx_duplicate,
            "errors": errors, "accepted": bool(nodes) and all(count >= minimum for count in matched_by_node.values()) and not (losses or unmatched_rx or mismatches or tx_duplicate or rx_duplicate or errors)}


def record_raw(log, raw, elapsed, errors):
    """Conserver les octets defectueux en hexadecimal, sans remplacement UTF-8."""
    problem = None
    if len(raw) > 8192:
        problem = "line_too_long"
    try:
        line = raw.decode("utf-8").rstrip("\r")
    except UnicodeDecodeError:
        problem = "invalid_utf8" if problem is None else problem + "+invalid_utf8"
        line = ""
    if problem:
        line = "[CAPTURE_ERROR] " + problem + " raw_hex=" + raw.hex()
        errors.append(problem)
    log.write(f"[HOST t_s={elapsed:.6f}] {line}\n")
    log.flush()


def capture(tx_port, rx_port, seconds, tx_path, rx_path):
    import serial
    ports, logs, threads, errors = [], [], [], []
    stop = threading.Event()
    started = time.monotonic()
    interrupted = False
    try:
        # Reserver aussi les journaux avant toute operation sur les ports.
        logs = [tx_path.open("x", encoding="utf-8")]
        logs.append(rx_path.open("x", encoding="utf-8"))
        for name, dtr in [(tx_port, True), (rx_port, False)]:
            port = serial.Serial(port=None, baudrate=115200, timeout=0.1)
            port.dtr, port.rts, port.port = dtr, False, name
            ports.append(port)
            port.open()
        for port in ports:
            port.reset_input_buffer()
        started = time.monotonic()
        deadline = started + seconds
        def read_port(port, log):
            pending = b""
            pending_started = started
            try:
                log.write(f"Capture {port.port}, UTC={datetime.now(timezone.utc).isoformat()}, seconds={seconds}\n")
                while not stop.is_set() and time.monotonic() < deadline:
                    chunk = port.read(min(port.in_waiting or 1, 4096))
                    if chunk and not pending:
                        pending_started = time.monotonic()
                    pending += chunk
                    while b"\n" in pending:
                        raw, pending = pending.split(b"\n", 1)
                        record_raw(log, raw, time.monotonic() - started, errors)
                        pending_started = time.monotonic()
                    if len(pending) > 8192:
                        raise RuntimeError("ligne serie de plus de 8192 octets")
            except Exception as exc:
                errors.append(f"{port.port}: {exc}")
                stop.set()
            finally:
                if pending:
                    if not stop.is_set() and time.monotonic() >= deadline and pending_started >= deadline - 1.0:
                        # Une ligne en cours a la limite normale n'appartient
                        # pas a la fenetre complete. Conserver ses octets.
                        log.write(f"[CAPTURE_BOUNDARY] partial_tail raw_hex={pending.hex()}\n")
                    else:
                        record_raw(log, pending, time.monotonic() - started, errors)
                        errors.append(f"{port.port}: partial_line")
                        log.write("[CAPTURE_ERROR] partial_line\n")
                    log.flush()
        threads = [threading.Thread(target=read_port, args=pair) for pair in zip(ports, logs)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
    except KeyboardInterrupt:
        interrupted = True
        errors.append("interrupted: KeyboardInterrupt")
    except Exception as exc:
        errors.append(str(exc))
    finally:
        stop.set()
        # Aucun port ni journal ferme tant qu'un lecteur peut encore l'utiliser.
        for thread in threads:
            while thread.is_alive():
                try:
                    thread.join(0.2)
                except KeyboardInterrupt:
                    interrupted = True
                    errors.append("interrupted: KeyboardInterrupt")
        for port in ports:
            port.close()
        for log in logs:
            log.close()
    return {"errors": errors, "duration_s": time.monotonic() - started, "interrupted": interrupted}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tx-port", required=True)
    parser.add_argument("--rx-port", required=True)
    parser.add_argument("--seconds", type=float, default=5500)
    parser.add_argument("--boundary-margin", type=float, default=1.0)
    parser.add_argument("--output-prefix", type=Path, required=True)
    args = parser.parse_args()
    if not 0 < args.seconds <= 7200 or args.tx_port.upper() == args.rx_port.upper():
        parser.error("utiliser des ports distincts et 0 < seconds <= 7200")
    if not 0 <= args.boundary_margin <= 2 or args.seconds <= 2 * args.boundary_margin:
        parser.error("marge de 0 a 2 s, inferieure a la demi-duree de capture")
    paths = [Path(str(args.output_prefix) + suffix) for suffix in ("-tx.log", "-rx.log", ".json")]
    args.output_prefix.parent.mkdir(parents=True, exist_ok=True)
    if any(path.exists() for path in paths):
        parser.error("sortie existante ; choisir un nouveau prefixe pour conserver les preuves")
    try:
        reserved_summary = paths[2].open("x", encoding="utf-8")
    except FileExistsError:
        parser.error("bilan reserve par une autre capture ; choisir un nouveau prefixe")
    with reserved_summary as output:
        output.write(json.dumps({"accepted": False, "completed": False, "capture_errors": ["capture_en_cours"]}))
        output.flush()
        result = {"errors": [], "duration_s": args.seconds, "interrupted": False}
        try:
            result = capture(args.tx_port, args.rx_port, args.seconds, paths[0], paths[1])
        except KeyboardInterrupt:
            result["errors"].append("interrupted: KeyboardInterrupt")
            result["interrupted"] = True
        except Exception as exc:
            result["errors"].append(str(exc))
        lines = []
        for path in paths[:2]:
            try:
                lines.append(path.read_text(encoding="utf-8").splitlines() if path.exists() else [])
            except (OSError, UnicodeError) as exc:
                result["errors"].append(f"{path}: {exc}")
                lines.append([])
        summary = compare(*lines, duration=result["duration_s"], margin=args.boundary_margin)
        summary.update({"tx_port": args.tx_port, "rx_port": args.rx_port, "duration_requested_s": args.seconds,
                        "duration_s": result["duration_s"], "capture_errors": result["errors"],
                        "interrupted": result["interrupted"], "completed": not result["errors"] and not result["interrupted"]})
        summary["accepted"] &= summary["completed"]
        output.seek(0)
        output.write(json.dumps(summary, indent=2))
        output.truncate()
    print(json.dumps(summary, indent=2))
    return 0 if summary["accepted"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
