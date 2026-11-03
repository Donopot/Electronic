"""Exécuter le vrai initVariant() avec une trace GPIO, avant essai matériel."""
from pathlib import Path
import configparser
import os
import re
import subprocess
import tempfile
import unittest


def main():
    root = Path(__file__).resolve().parents[1]
    variant = root / "firmware/XiaoSoilDiagnostic/variants/Seeed_XIAO_nRF52840"
    (root / "tmp").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="xiao-battery-", dir=root / "tmp") as folder:
        build = Path(folder)
        for name, content in {
            "WVariant.h": "#pragma once\n#include <stdint.h>\n",
            "wiring_constants.h": "#pragma once\nenum {INPUT=0, OUTPUT=1, LOW=0, HIGH=1};\n",
            "wiring_digital.h": "#pragma once\nvoid pinMode(uint32_t, uint32_t);\nvoid digitalWrite(uint32_t, uint32_t);\n",
            "nrf.h": "#pragma once\n",
        }.items():
            (build / name).write_text(content, encoding="utf-8")
        harness = build / "main.cpp"
        harness.write_text(r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include "variant.cpp"
struct Event { char kind; uint32_t pin, value; };
std::vector<Event> events;
void pinMode(uint32_t pin, uint32_t value) { events.push_back({'m',pin,value}); }
void digitalWrite(uint32_t pin, uint32_t value) { events.push_back({'w',pin,value}); }
int main() {
  const uint32_t expected[] = {
    2,3,28,29,4,5,43,44,45,46,47,26,6,30,14,40,27,7,11,42,32,16,
    13,17,21,25,20,24,22,23,9,10,31
  };
  static_assert(sizeof(g_ADigitalPinMap)==sizeof(expected), "taille de brochage");
  for (unsigned i=0; i<PINS_COUNT; ++i) assert(g_ADigitalPinMap[i]==expected[i]);
  assert(g_ADigitalPinMap[VBAT_ENABLE]==14);
  assert(g_ADigitalPinMap[PIN_CHARGING_CURRENT]==13);
  for (int boot=0; boot<2; ++boot) {
    events.clear();
    initVariant();
    assert(events.size()==11);
    // LOW doit être préchargé avant la prise de la sortie, sans impulsion HIGH.
    assert(events[0].kind=='w' && events[0].pin==VBAT_ENABLE && events[0].value==LOW);
    assert(events[1].kind=='m' && events[1].pin==VBAT_ENABLE && events[1].value==OUTPUT);
    assert(events[2].kind=='m' && events[2].pin==PIN_CHARGING_CURRENT && events[2].value==INPUT);
    for (const auto& e:events) {
      if (e.pin==VBAT_ENABLE && e.kind=='w') assert(e.value==LOW);
      if (e.pin==PIN_CHARGING_CURRENT) assert(e.kind=='m' && e.value==INPUT);
      // Aucune broche de radio, bus ADS ou bouton D0 ne doit être prise ici.
      assert(e.pin>10);
    }
    const uint32_t highPins[]={25,11,13,12};
    for (unsigned i=0;i<4;++i) {
      const auto& mode=events[3+2*i]; const auto& level=events[4+2*i];
      assert(mode.kind=='m' && mode.pin==highPins[i] && mode.value==OUTPUT);
      assert(level.kind=='w' && level.pin==highPins[i] && level.value==HIGH);
    }
  }
}
''', encoding="utf-8")
        executable = build / ("variant.exe" if os.name == "nt" else "variant")
        command = ["uv", "run", "--with", "ziglang==0.13.0", "python", "-m",
                   "ziglang", "c++", "-std=c++11", "-O0", "-Wall", "-Wextra"]
        if os.name == "nt":
            command += ["-target", "x86_64-windows-gnu"]
        subprocess.run(command + ["-I" + str(build), "-I" + str(variant),
                                  str(harness), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    print("PASS: initVariant réel, ordre LOW/OUTPUT, courant faible, brochage et autres sorties conservés")


class BatteryVariantTest(unittest.TestCase):
    def test_gpio_initialization(self):
        main()

    def test_platformio_selection(self):
        root = Path(__file__).resolve().parents[1]
        config = configparser.ConfigParser()
        config.read(root / "firmware/XiaoSoilDiagnostic/platformio.ini", encoding="utf-8")
        self.assertEqual(config["env"]["board_build.variants_dir"], "variants")
        self.assertNotIn("board_build.variants_dir", config["env:xiao_soil_diagnostic"])
        self.assertNotIn("board_build.variants_dir", config["env:xiao_soil_lora_tx"])

    def test_both_linked_firmwares(self):
        root = Path(__file__).resolve().parents[1]
        suffix = ".exe" if os.name == "nt" else ""
        toolchain = root / ".tools/platformio/packages/toolchain-gccarmnoneeabi/bin"
        for profile in ("xiao_soil_diagnostic", "xiao_soil_lora_tx"):
            with self.subTest(profile=profile):
                elf = root / "firmware/XiaoSoilDiagnostic/.pio/build" / profile / "firmware.elf"
                self.assertTrue(elf.is_file(), "Compiler les deux profils avant le test")
                nm = subprocess.check_output([str(toolchain / ("arm-none-eabi-nm" + suffix)),
                                              "-C", str(elf)], text=True)
                self.assertEqual(sum(" T initVariant()" in line for line in nm.splitlines()), 1)
                disassembly = subprocess.check_output([
                    str(toolchain / ("arm-none-eabi-objdump" + suffix)), "-d", "-C", str(elf)
                ], text=True)
                function = re.search(r"^[0-9a-f]+ <initVariant\(\)>:\n.*?(?=\n\n)",
                                     disassembly, re.MULTILINE | re.DOTALL)
                self.assertIsNotNone(function)
                # Lecture des arguments r0/r1 du code ARM réellement lié.
                registers, calls = {}, []
                for line in function.group().splitlines():
                    move = re.search(r"\bmovs?\s+r([01]),\s*#(\d+)", line)
                    if move:
                        registers[int(move[1])] = int(move[2])
                    call = re.search(r"\bbl(?:\.w)?\s+[0-9a-f]+\s+<(digitalWrite|pinMode)>", line)
                    if call:
                        calls.append((call[1], registers.get(0), registers.get(1)))
                self.assertEqual(calls[:3], [("digitalWrite", 14, 0),
                                            ("pinMode", 14, 1), ("pinMode", 22, 0)])
                self.assertNotIn(("digitalWrite", 14, 1), calls)


if __name__ == "__main__":
    unittest.main(verbosity=2)
