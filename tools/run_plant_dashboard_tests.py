"""Portable tests for the real dashboard model, flash journal, API validators and PRG."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
project = root / "firmware/PlantStationVeml7700"
build = root / "output/plant-dashboard-tests"
build.mkdir(parents=True, exist_ok=True)
binary = build / ("dashboard.exe" if os.name == "nt" else "dashboard")
command = ["uv", "run", "--with", "ziglang==0.13.0", "python", "-m", "ziglang", "c++",
           "-std=c++11", "-O0", "-g", "-Wall", "-Wextra", "-Werror=return-type",
           "-DPLANT_DASHBOARD=1", "-I" + str(project / "include"),
           "-I" + str(project / "src"), "-I" + str(root / "lib/PlantSoilProtocol/src"),
           str(project / "test/test_dashboard.cpp"), str(project / "src/ui/PlantView.cpp"),
           str(project / "src/storage/PlantHistory.cpp"), "-o", str(binary)]
if os.name == "nt":
    command += ["-target", "x86_64-windows-gnu"]
subprocess.run(command, check=True)
subprocess.run([str(binary)], check=True)

# Real main/radio/OLED integration with an explicitly simulated network-worker boundary.
sensor = project / ".pio/libdeps/heltec_v3_text/SparkFun VEML7700 Arduino Library/src"
toolkit = project / ".pio/libdeps/heltec_v3_text/SparkFun Toolkit/src"
integration = build / ("integration.exe" if os.name == "nt" else "integration")
integration_command = command[:command.index(str(project / "test/test_dashboard.cpp"))]
integration_command += ["-DPLANT_SOIL_LORA=1", "-I" + str(project / "test/support"),
                        "-I" + str(sensor), "-I" + str(toolkit)]
integration_command += [str(project / item) for item in [
    "test/test_dashboard_orchestration.cpp", "src/main.cpp", "src/ui/PlantView.cpp",
    "src/radio/SoilRadio.cpp", "src/display/OledDisplay.cpp", "src/power/SleepPower.cpp",
    "src/sensors/VEML7700Sensor.cpp", "src/diagnostics/Diagnostics.cpp"]]
integration_command += [str(sensor / "sfTk/sfDevVEML7700.cpp"), str(toolkit / "sfTk/sfToolkit.cpp"),
                        "-o", str(integration)]
if os.name == "nt":
    integration_command += ["-target", "x86_64-windows-gnu"]
subprocess.run(integration_command, check=True)
for scenario in ["normal", "no_oled"]:
    subprocess.run([str(integration), scenario], check=True)
