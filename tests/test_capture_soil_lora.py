"""Tests hors ligne de l'appariement exact ; aucun port serie ouvert."""
import importlib.util
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import types
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("capture", Path(__file__).resolve().parents[1] / "tools/capture_soil_lora.py")
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)

def line(direction, seq, raw=21565, status="ok", address=72):
    return f"[SOIL_{direction}] authenticated=no node=SOIL-01 session=1234 seq={seq} uptime_ms={seq*30000} status={status} address={address} raw={raw} uv={raw*125}"


def stamped(direction, seq, when):
    return f"[HOST t_s={when:.6f}] " + line(direction, seq)

class CaptureTests(unittest.TestCase):
    def test_exact_pairing(self):
        result = capture.compare([line("TX", i) for i in range(3)], [line("RX", i) for i in range(3)])
        self.assertTrue(result["accepted"])
        self.assertEqual(result["valid_matched_count"], 3)

    def test_losses_mismatch_duplicates(self):
        tx = [line("TX", i) for i in range(3)]
        rx = [line("RX", 0), line("RX", 0), line("RX", 1, 8000), line("RX", 4)]
        result = capture.compare(tx, rx)
        self.assertFalse(result["accepted"])
        self.assertEqual(len(result["losses"]), 1)
        self.assertEqual(len(result["mismatches"]), 1)
        self.assertEqual(len(result["unmatched_rx"]), 1)
        self.assertEqual(len(result["rx_duplicates"]), 1)

    def test_fault_and_malformed(self):
        records, errors = capture.parse_records([line("RX", 1, 0, "adc_error"), line("RX", 2) + " extra",
                                                line("RX", 3).replace("uv=2695625", "uv=1"),
                                                "[SOIL_REJECT] reason=duplicate authenticated=no"], "RX")
        self.assertEqual(len(records), 1)
        self.assertEqual(len(errors), 4)

    def test_boundary_pairs_and_real_loss(self):
        tx = [stamped("TX", i, when) for i, when in enumerate((0.1, 10, 40, 70, 104.5))]
        rx = [stamped("RX", i, when) for i, when in enumerate((0.2, 10.1, 40.1, 70.1, 104.6))]
        # Le premier TX et dernier RX manquent uniquement dans la marge.
        result = capture.compare(tx[1:], rx[:-1], duration=105)
        self.assertTrue(result["accepted"])
        self.assertEqual(result["valid_matched_count"], 3)
        self.assertEqual(len(result["boundary_pairs"]), 2)
        result = capture.compare(tx[1:], [rx[0], rx[1], rx[3]], duration=105)
        self.assertFalse(result["accepted"])
        self.assertEqual([p["seq"] for p in result["losses"]], [2])

    def test_capture_window_requires_timestamps(self):
        result = capture.compare([line("TX", i) for i in range(3)],
                                 [line("RX", i) for i in range(3)], duration=105)
        self.assertFalse(result["accepted"])
        self.assertIn("missing_host_timestamp: TX", result["errors"])

    def test_oversized_completed_line_and_invalid_utf8(self):
        output, errors = io.StringIO(), []
        capture.record_raw(output, b"A" * 8193, 1, errors)
        capture.record_raw(output, b"\xff[SOIL_TX]", 2, errors)
        self.assertEqual(errors, ["line_too_long", "invalid_utf8"])
        self.assertIn("raw_hex=ff5b534f494c5f54585d", output.getvalue())
        _, parsed_errors = capture.parse_records(output.getvalue().splitlines(), "TX")
        self.assertEqual(len(parsed_errors), 2)

    def test_giant_numeric_field_is_an_explicit_error(self):
        malformed = line("TX", 1).replace("session=1234", "session=" + "9" * 5000)
        records, errors = capture.parse_records([malformed], "TX")
        self.assertEqual(records, [])
        self.assertEqual(len(errors), 1)
        self.assertIn("invalid_numeric_log", errors[0])

    def run_main_with_capture_failure(self, interruption=False):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "evidence"
            def fake_capture(tx_port, rx_port, seconds, tx_path, rx_path):
                for direction, path in (("TX", tx_path), ("RX", rx_path)):
                    path.write_text("\n".join(stamped(direction, i, 10 + 30 * i)
                                              for i in range(3)) + "\n", encoding="utf-8")
                if interruption:
                    raise KeyboardInterrupt
                return {"errors": ["serial disconnected"], "duration_s": 105, "interrupted": False}
            argv = ["capture", "--tx-port", "COM8", "--rx-port", "COM5", "--seconds", "105",
                    "--output-prefix", str(prefix)]
            with mock.patch.object(sys, "argv", argv), mock.patch.object(capture, "capture", fake_capture), contextlib.redirect_stdout(io.StringIO()):
                result = capture.main()
            summary = json.loads(Path(str(prefix) + ".json").read_text(encoding="utf-8"))
            self.assertEqual(result, 2)
            self.assertEqual(summary["valid_matched_count"], 3)
            self.assertFalse(summary["accepted"])
            self.assertFalse(summary["completed"])
            self.assertTrue(summary["capture_errors"])
            self.assertEqual(summary["interrupted"], interruption)

    def test_main_capture_error_overrides_matching_pairs(self):
        self.run_main_with_capture_failure()

    def test_main_interrupt_preserves_report(self):
        self.run_main_with_capture_failure(interruption=True)

    def test_exclusive_summary_reservation_wins_the_race(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "evidence"
            summary = Path(str(prefix) + ".json")
            summary.write_text("preuve existante", encoding="utf-8")
            argv = ["capture", "--tx-port", "COM8", "--rx-port", "COM5", "--output-prefix", str(prefix)]
            # Simuler un autre processus creant le fichier apres le precontrole.
            with mock.patch.object(sys, "argv", argv), mock.patch.object(Path, "exists", return_value=False), mock.patch.object(capture, "capture") as run_capture, contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    capture.main()
            self.assertEqual(raised.exception.code, 2)
            run_capture.assert_not_called()
            self.assertEqual(summary.read_text(encoding="utf-8"), "preuve existante")

    def test_interrupt_joins_readers_before_closing_ports(self):
        ports = []
        class FakePort:
            def __init__(self, **kwargs):
                self.active_read = False
                self.closed = False
                ports.append(self)
            def open(self):
                pass
            def reset_input_buffer(self):
                pass
            @property
            def in_waiting(self):
                return 0
            def read(self, count):
                self.active_read = True
                time.sleep(0.002)
                self.active_read = False
                return b""
            def close(self):
                self.closed_while_reading = self.active_read
                self.closed = True
        original_join = threading.Thread.join
        interrupted_once = [False]
        def interrupted_join(thread, *args, **kwargs):
            if not interrupted_once[0]:
                interrupted_once[0] = True
                raise KeyboardInterrupt
            return original_join(thread, *args, **kwargs)
        with tempfile.TemporaryDirectory() as directory, mock.patch.dict(sys.modules, {"serial": types.SimpleNamespace(Serial=FakePort)}), mock.patch.object(threading.Thread, "join", interrupted_join):
            result = capture.capture("COM8", "COM5", 1, Path(directory) / "tx.log", Path(directory) / "rx.log")
        self.assertTrue(result["interrupted"])
        self.assertTrue(result["errors"])
        self.assertEqual(len(ports), 2)
        self.assertTrue(all(port.closed and not port.closed_while_reading for port in ports))

    def test_reader_checks_completed_long_lines_and_utf8(self):
        created = []
        class FakePort:
            def __init__(self, **kwargs):
                self.data = b"A" * 8193 + b"\n" if not created else b"\xffbroken\n"
                created.append(self)
            def open(self):
                pass
            def reset_input_buffer(self):
                pass
            @property
            def in_waiting(self):
                return len(self.data)
            def read(self, count):
                chunk, self.data = self.data[:count], self.data[count:]
                if not chunk:
                    time.sleep(0.001)
                return chunk
            def close(self):
                pass
        with tempfile.TemporaryDirectory() as directory, mock.patch.dict(sys.modules, {"serial": types.SimpleNamespace(Serial=FakePort)}):
            result = capture.capture("COM8", "COM5", 0.03, Path(directory) / "tx.log", Path(directory) / "rx.log")
            self.assertIn("line_too_long", result["errors"])
            self.assertIn("invalid_utf8", result["errors"])
            self.assertIn("raw_hex=ff62726f6b656e", (Path(directory) / "rx.log").read_text(encoding="utf-8"))

    def test_only_a_recent_partial_tail_is_a_capture_boundary(self):
        created = []
        class FakePort:
            def __init__(self, **kwargs):
                self.wait = 0 if not created else 1.5
                self.data = b"tail"
                self.started = time.monotonic()
                created.append(self)
            def open(self):
                pass
            def reset_input_buffer(self):
                pass
            @property
            def in_waiting(self):
                return len(self.data) if time.monotonic() - self.started >= self.wait else 0
            def read(self, count):
                time.sleep(0.001)
                if time.monotonic() - self.started < self.wait:
                    return b""
                chunk, self.data = self.data[:count], self.data[count:]
                return chunk
            def close(self):
                pass
        with tempfile.TemporaryDirectory() as directory, mock.patch.dict(sys.modules, {"serial": types.SimpleNamespace(Serial=FakePort)}):
            result = capture.capture("COM8", "COM5", 2.05, Path(directory) / "tx.log", Path(directory) / "rx.log")
            self.assertIn("COM8: partial_line", result["errors"])
            self.assertNotIn("COM5: partial_line", result["errors"])
            self.assertIn("[CAPTURE_BOUNDARY] partial_tail", (Path(directory) / "rx.log").read_text(encoding="utf-8"))

if __name__ == "__main__":
    unittest.main()
