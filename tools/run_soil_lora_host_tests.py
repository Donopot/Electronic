"""Tests cibles des vrais firmwares radio SOIL, sans autre suite ni carte USB."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
plant = root / "firmware/PlantStationVeml7700"
xiao = root / "firmware/XiaoSoilDiagnostic"
build = root / "output/soil-lora-targeted-tests"
build.mkdir(parents=True, exist_ok=True)
compiler = ["uv", "run", "--with", "ziglang==0.13.0", "python", "-m", "ziglang", "c++", "-std=c++11", "-O0", "-g", "-Wall", "-Wextra"]
if os.name == "nt":
    compiler += ["-target", "x86_64-windows-gnu"]
protocol = root / "lib/PlantSoilProtocol/src"
sensor = plant / ".pio/libdeps/heltec_v3_text/SparkFun VEML7700 Arduino Library/src"
toolkit = plant / ".pio/libdeps/heltec_v3_text/SparkFun Toolkit/src"
plant_files = ["test/test_soil_radio.cpp", "src/main.cpp", "src/radio/SoilRadio.cpp", "src/display/OledDisplay.cpp", "src/power/SleepPower.cpp", "src/sensors/VEML7700Sensor.cpp", "src/diagnostics/Diagnostics.cpp"]
for name, includes, sources, flags, scenarios in [
    ("heltec", [plant / "test/support", plant / "include", plant / "src", protocol, sensor, toolkit],
     [plant / item for item in plant_files] + [sensor / "sfTk/sfDevVEML7700.cpp", toolkit / "sfTk/sfToolkit.cpp"],
     ["-DPLANT_SOIL_LORA=1"], ["normal", "radio_failed", "sleep_failed", "pending_init"]),
    ("xiao", [xiao / "test/support", protocol], [xiao / "test/test_tx.cpp"], ["-DSOIL_LORA_TX=1"],
     ["cadence", "wrap", "rng", "usb", "disconnected"]),
]:
    executable = build / (name + (".exe" if os.name == "nt" else ""))
    subprocess.run(compiler + ["-I" + str(path) for path in includes] + [str(path) for path in sources] + flags + ["-o", str(executable)], check=True)
    for scenario in scenarios:
        subprocess.run([str(executable), scenario], check=True)
