"""Audit the delivered APK, signature, permissions and actual optimized DEX.

Only reads build artifacts; no ADB, phone, serial, installation or network.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--apk", type=Path, default=root / "android/PlantStation/app/build/outputs/apk/release/app-release.apk")
parser.add_argument("--sdk", type=Path)
parser.add_argument("--expected-cert", help="Optional expected SHA256 signer fingerprint")
parser.add_argument("--output", type=Path, default=root / "diagnostics/plant-android-release-audit.json")
args = parser.parse_args()
environment = os.environ.copy()
bootstrap = root / ".tools/android-bootstrap/environment.json"
configuration = json.loads(bootstrap.read_text(encoding="utf-8")) if bootstrap.exists() else {}
sdk = args.sdk or Path(environment.get("ANDROID_HOME", configuration.get("ANDROID_HOME", "")))
if configuration.get("JAVA_HOME"):
    environment["JAVA_HOME"] = configuration["JAVA_HOME"]
    environment["PATH"] = str(Path(environment["JAVA_HOME"]) / "bin") + os.pathsep + environment["PATH"]
build = sdk / "build-tools/35.0.0"
apk = args.apk.resolve()

def run(tool, *arguments):
    result = subprocess.run([str(build / tool), *arguments], env=environment, capture_output=True, text=True, encoding="utf-8", errors="replace", check=True)
    return result.stdout

signature = run("apksigner.bat" if os.name == "nt" else "apksigner", "verify", "--verbose", "--print-certs", str(apk))
fingerprint = re.search(r"certificate SHA-256 digest: ([a-fA-F0-9]+)", signature).group(1).lower()
if args.expected_cert:
    assert fingerprint == args.expected_cert.replace(":", "").lower(), "Unexpected signer"
badging = run("aapt.exe" if os.name == "nt" else "aapt", "dump", "badging", str(apk))
assert "package: name='local.plantstation.kit'" in badging and "sdkVersion:'26'" in badging and "targetSdkVersion:'35'" in badging
permissions = run("aapt.exe" if os.name == "nt" else "aapt", "dump", "permissions", str(apk))
assert "android.permission.INTERNET" not in permissions, "Remote-network permission inherited"
assert "android.permission.ACCESS_COARSE_LOCATION" in permissions and "android.permission.ACCESS_FINE_LOCATION" in permissions
sdk_log_calls = []
sdk_trace_calls = []
other_log_writes = []
with tempfile.TemporaryDirectory(prefix="plant-apk-") as folder, zipfile.ZipFile(apk) as archive:
    dexes = [name for name in archive.namelist() if re.fullmatch(r"classes\d*\.dex", name)]
    assert dexes
    for name in dexes:
        target = Path(folder) / name
        target.write_bytes(archive.read(name))
        process = subprocess.Popen([str(build / ("dexdump.exe" if os.name == "nt" else "dexdump")), "-d", str(target)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
        owner = ""
        for line in process.stdout:
            if "Class descriptor" in line:
                owner = line.strip()
            if "invoke-" not in line:
                continue
            log = "Landroid/util/Log;" in line or "android.util.Log." in line
            if log and "espressif" in owner:
                sdk_log_calls.append(line.strip())
            if "printStackTrace" in line and "espressif" in owner:
                sdk_trace_calls.append(line.strip())
            if log and not any(readonly in line for readonly in ("isLoggable", "getStackTraceString")):
                other_log_writes.append({"class": owner, "instruction": line.strip()})
        assert process.wait() == 0, "Invalid DEX"
assert not sdk_log_calls and not sdk_trace_calls, "SDK secret logging survived release optimization"
assert not other_log_writes, "Unexpected Android log writing in release"
try:
    recorded_apk = str(apk.relative_to(root))
except ValueError:
    recorded_apk = str(apk)
report = {
    "apk": recorded_apk, "sha256": hashlib.sha256(apk.read_bytes()).hexdigest(), "bytes": apk.stat().st_size,
    "signature_verified": True, "certificate_sha256": fingerprint, "min_sdk": 26, "target_sdk": 35,
    "internet_permission": False, "espressif_log_invocations": 0, "espressif_stacktrace_invocations": 0,
    "android_log_writes": 0, "physical_android_test": "not performed by this audit",
}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report))
