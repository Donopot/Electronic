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

# Verify the actual pinned SDK contract that motivated direct nonblocking sends.
sdk = root / ".tools/platformio/packages/framework-arduinoespressif32"
wifi_header = (sdk / "libraries/WiFi/src/WiFiClient.h").read_text(encoding="utf-8")
wifi_source = (sdk / "libraries/WiFi/src/WiFiClient.cpp").read_text(encoding="utf-8")
print_header = (sdk / "cores/esp32/Print.h").read_text(encoding="utf-8")
socket_header = (sdk / "tools/sdk/esp32s3/include/lwip/lwip/src/include/lwip/sockets.h").read_text(encoding="utf-8")
assert "virtual int fd() const;" in wifi_header
assert "availableForWrite" not in wifi_header
assert "virtual int availableForWrite() { return 0; }" in print_header
assert "#define MSG_DONTWAIT   0x08" in socket_header
assert "send(socketFileDescriptor, (void*) buf, bytesRemaining, MSG_DONTWAIT)" in wifi_source
dashboard_source = (project / "src/web/PlantDashboard.cpp").read_text(encoding="utf-8")
assert "client.availableForWrite()" not in dashboard_source and "client.write(" not in dashboard_source
assert "sendHttpChunk(client.fd(),data,remain)" in dashboard_source
generic_source = (sdk / "libraries/WiFi/src/WiFiGeneric.cpp").read_text(encoding="utf-8")
sta_source = (sdk / "libraries/WiFi/src/WiFiSTA.cpp").read_text(encoding="utf-8")
ap_source = (sdk / "libraries/WiFi/src/WiFiAP.cpp").read_text(encoding="utf-8")
udp_source = (sdk / "libraries/WiFi/src/WiFiUdp.cpp").read_text(encoding="utf-8")
assert "_persistent = persistent;" in generic_source
assert "strlen(ssid) > 32" in sta_source and "strlen(passphrase) > 64" in sta_source
assert "WIFI_AUTH_WPA2_PSK" in ap_source
assert "fcntl(udp_server, F_SETFL, O_NONBLOCK)" in udp_source
assert "esp_wifi_set_storage(WIFI_STORAGE_FLASH)" in dashboard_source
assert "esp_wifi_set_storage(WIFI_STORAGE_RAM)" in dashboard_source
assert "DNSServer" not in dashboard_source
preferences_source = (sdk / "libraries/Preferences/src/Preferences.cpp").read_text(encoding="utf-8")
put_bytes = preferences_source.split("size_t Preferences::putBytes(")[1].split("PreferenceType Preferences::getType")[0]
assert "nvs_set_blob(" in put_bytes and "nvs_commit(" in put_bytes
print("PASS installed Arduino-ESP32 SDK: WiFiClient fd public, inherited availableForWrite=0, lwIP MSG_DONTWAIT available; actual serviceHttp uses the tested sender", flush=True)
http_binary = build / ("http-transport.exe" if os.name == "nt" else "http-transport")
http_command = command[:command.index(str(project / "test/test_dashboard.cpp"))]
http_command += ["-I" + str(project / "test/support"), str(project / "test/test_http_transport.cpp"), "-o", str(http_binary)]
if os.name == "nt":
    http_command += ["-target", "x86_64-windows-gnu"]
subprocess.run(http_command, check=True)
subprocess.run([str(http_binary)], check=True)

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
# Variante sans capteur de lumiere : profil heltec_v3_soil_dashboard.
no_lux = build / ("integration-nolux.exe" if os.name == "nt" else "integration-nolux")
no_lux_command = [str(no_lux) if item == str(integration) else item for item in integration_command]
no_lux_command.append("-DPLANT_LUX=0")
subprocess.run(no_lux_command, check=True)
for scenario in ["normal", "no_oled"]:
    subprocess.run([str(no_lux), scenario], check=True)

worker_binary = build / ("worker.exe" if os.name == "nt" else "worker")
worker_command = command[:command.index(str(project / "test/test_dashboard.cpp"))]
worker_command += ["-I" + str(project / "test/dashboard-support"), "-I" + str(project / "test/support"),
                   str(project / "test/test_dashboard_worker.cpp"), str(project / "src/ui/PlantView.cpp"),
                   str(project / "src/storage/PlantHistory.cpp"), "-o", str(worker_binary)]
if os.name == "nt":
    worker_command += ["-target", "x86_64-windows-gnu"]
subprocess.run(worker_command, check=True)
subprocess.run([str(worker_binary)], check=True)
subprocess.run([str(worker_binary), "worker-loop"], check=True)

subprocess.run([str(worker_binary), "ble-worker-loop"], check=True)

for scenario in ["ble-ap-failure", "ble-http-handoff", "saved-connection-recovery"]:
    subprocess.run([str(worker_binary), scenario], check=True)

# AP permanent : tableau de bord servi sur le Wi-Fi du kit, sans configuration.
ap_binary = build / ("always-ap.exe" if os.name == "nt" else "always-ap")
ap_command = command[:command.index(str(project / "test/test_dashboard.cpp"))]
ap_command += ["-DPLANT_ALWAYS_AP=1", "-I" + str(project / "test/dashboard-support"),
               "-I" + str(project / "test/support"),
               str(project / "test/test_dashboard_always_ap.cpp"), str(project / "src/ui/PlantView.cpp"),
               str(project / "src/storage/PlantHistory.cpp"), "-o", str(ap_binary)]
if os.name == "nt":
    ap_command += ["-target", "x86_64-windows-gnu"]
subprocess.run(ap_command, check=True)
subprocess.run([str(ap_binary)], check=True)
