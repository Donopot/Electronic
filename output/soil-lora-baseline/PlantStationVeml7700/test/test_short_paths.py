"""Verifier le script SCons reel avec des chemins Windows sur deux lecteurs."""
import ntpath
from pathlib import Path
import runpy
from unittest.mock import patch


class Directory:
    def __init__(self, path):
        self.path = path

    def get_abspath(self):
        return self.path


class Environment:
    def subst(self, value):
        return r"C:\work\PlantStationVeml7700" if value == "$PROJECT_DIR" else value

    def Dir(self, value):
        return Directory(value)

    def get(self, name, default):
        assert name == "CPPPATH"
        return [r"C:\work\PlantStationVeml7700\include", r"D:\PlatformIO Packages\framework\include"]

    def Replace(self, **kwargs):
        self.flags = kwargs["_CPPINCFLAGS"]


env = Environment()
runpy.run_path(str(Path(__file__).resolve().parents[1] / "short_paths.py"),
               init_globals={"env": env, "Import": lambda name: None})
windows_relpath = ntpath.relpath
with patch("os.path.relpath", side_effect=windows_relpath):
    flags = env.flags(None, None, env, False)
assert flags == ["-Iinclude", "-ID:/PlatformIO Packages/framework/include"], flags
print("PASS short_paths reel : include relatif meme lecteur, absolu autre lecteur")
