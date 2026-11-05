"""Compile et execute le vrai pilote, son transport et les diagnostics avec Zig 0.13.0."""
import json
import os
from pathlib import Path
import subprocess
import sys

project = Path(__file__).resolve().parents[1]
libraries = project / ".pio/libdeps/heltec_v3_text"
sensor = libraries / "SparkFun VEML7700 Arduino Library/src"
toolkit = libraries / "SparkFun Toolkit/src"
if not sensor.exists() or not toolkit.exists():
    sys.exit("Compiler d'abord heltec_v3_text pour installer les dependances epinglees.")
build = project / ".pio/host-tests"
build.mkdir(parents=True, exist_ok=True)
executable = build / ("tests.exe" if os.name == "nt" else "tests")
command = ["uv", "run", "--with", "ziglang==0.13.0", "python", "-m", "ziglang", "c++",
           "-std=c++11", "-O0", "-g", "-Wall", "-Wextra", "-Werror=return-type",
           "-I" + str(project / "include"), "-I" + str(project / "src"),
           "-I" + str(project / "test/support"), "-I" + str(sensor), "-I" + str(toolkit),
           str(project / "test/test_main.cpp"), str(project / "src/sensors/VEML7700Sensor.cpp"),
           str(project / "src/diagnostics/Diagnostics.cpp"), str(sensor / "sfTk/sfDevVEML7700.cpp"),
           str(toolkit / "sfTk/sfToolkit.cpp"), "-o", str(executable)]
if os.name == "nt":
    command.extend(["-target", "x86_64-windows-gnu"])
subprocess.run(command, check=True)
result = subprocess.run([str(executable)], text=True, capture_output=True, check=True)
print(result.stdout, end="")
records = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
assert len(records) == 11
keys = {"type", "sensor", "event", "version", "uptime_ms", "status", "raw", "lux", "address", "message"}
for record in records:
    assert set(record) == keys
    assert record["type"] == "plant_station_veml7700"
    assert record["sensor"] == "VEML7700"
    assert isinstance(record["uptime_ms"], int)
    assert isinstance(record["status"], str)
    if record["event"] == "mesure" and record["status"] == "ok":
        assert isinstance(record["lux"], (int, float)) and record["lux"] >= 0
    else:
        assert record["lux"] is None
        if record["status"] != "saturation":
            assert record["raw"] is None
assert [record["status"] for record in records] == [
    "ok", "conversion_en_cours", "i2c_nack", "lecture_courte", "erreur_bus",
    "configuration_perdue", "saturation", "erreur_bus", "erreur_bus", "erreur_bus", "ok"]
assert records[0]["uptime_ms"] == 4294967316
assert records[0]["message"] == 'test "guillemet"\\ et\ncontrole'
print("PASS Python json.loads : 11 lignes reelles, schema identique, lux nombre/null, uptime entier, echappement")

# Lier et executer le vrai main.cpp sans ajouter d'API de test au firmware.
orchestration = build / ("orchestration.exe" if os.name == "nt" else "orchestration")
main_command = [str(project / "test/test_orchestration.cpp") if item == str(project / "test/test_main.cpp")
                else str(orchestration) if item == str(executable) else item for item in command]
main_command.extend([str(project / "src/main.cpp"), str(project / "src/display/OledDisplay.cpp"), str(project / "src/power/SleepPower.cpp"), "-DPLANT_JSON=1"])
subprocess.run(main_command, check=True)
headless = build / ("headless.exe" if os.name == "nt" else "headless")
headless_command = [str(headless) if item == str(orchestration) else item for item in main_command] + ["-DPLANT_OLED=0"]
subprocess.run(headless_command, check=True)
for scenario, base in [("normal", 0), ("wrap", 0xFFFFFE0B), ("no_oled", 0), ("headless", 0)]:
    result = subprocess.run([str(headless if scenario == "headless" else orchestration), scenario], text=True, capture_output=True, check=True)
    print(result.stdout, end="")
    app = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
    enabled = scenario != "headless"
    assert len(app) == (13 if enabled else 12)
    assert [record["event"] for record in app] == [
        "demarrage"] + (["oled"] if enabled else []) + ["scan_ack", "scan_fin", "configuration"] + ["mesure"] * 8
    assert [record["status"] for record in app] == [
        "conversion_en_cours"] + (["erreur_bus" if scenario == "no_oled" else "ok"] if enabled else []) + ["ok", "conversion_en_cours", "conversion_en_cours",
        "ok", "ok", "ok", "i2c_nack", "i2c_nack", "i2c_nack", "conversion_en_cours", "ok"]
    assert [record["uptime_ms"] for record in app] == [base] * (5 if enabled else 4) + [
        base + delta for delta in [1000, 5500, 6500, 7500, 8500, 9500, 10500, 11500]]
    for record in app:
        assert set(record) == keys
        if record["event"] != "mesure" or record["status"] != "ok":
            assert record["raw"] is None and record["lux"] is None
        else:
            assert record["raw"] == 1000 and record["lux"] == 537.6
    print(f"PASS Python vrai setup/loop ({scenario}) : evenements, statuts exacts, cadence et uptime")
subprocess.run([sys.executable, str(project / "test/test_short_paths.py")], check=True)

# Module OLED reel : test des commandes d'alimentation et d'une panne transitoire.
oled_test = build / ("oled.exe" if os.name == "nt" else "oled")
oled_command = [str(project / "test/test_oled.cpp") if item == str(project / "test/test_main.cpp")
                else str(oled_test) if item == str(executable) else item for item in command]
oled_command.append(str(project / "src/display/OledDisplay.cpp"))
subprocess.run(oled_command, check=True)
subprocess.run([str(oled_test)], check=True)

# Politique PRG et sequence de sommeil reelles, sans materiel.
power_test = build / ("power.exe" if os.name == "nt" else "power")
power_command = [str(project / "test/test_power.cpp") if item == str(project / "test/test_main.cpp")
                 else str(power_test) if item == str(executable) else item for item in command]
power_command.append(str(project / "src/power/SleepPower.cpp"))
subprocess.run(power_command, check=True)
subprocess.run([str(power_test)], check=True)
sleep_test = build / ("sleep.exe" if os.name == "nt" else "sleep")
sleep_command = [str(project / "test/test_sleep_orchestration.cpp") if item == str(project / "test/test_orchestration.cpp")
                 else str(sleep_test) if item == str(orchestration) else item for item in main_command]
subprocess.run(sleep_command, check=True)
subprocess.run([str(sleep_test)], check=True)
for scenario in ["sensor_absent", "absent_boot", "bus_start_failed", "readback_clear", "short_read", "request_failed"]:
    subprocess.run([str(sleep_test), scenario], check=True)
