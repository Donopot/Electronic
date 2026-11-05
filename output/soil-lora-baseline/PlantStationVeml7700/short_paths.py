"""Raccourcir les arguments d'inclusion du compilateur sous Windows."""
import os

Import("env")


def include_flags(target, source, env, for_signature):
    project = env.subst("$PROJECT_DIR")
    flags = []
    for entry in env.get("CPPPATH", []):
        absolute = env.Dir(env.subst(str(entry))).get_abspath()
        try:
            path = os.path.relpath(absolute, project)
        except ValueError:
            # Windows : aucun chemin relatif possible entre deux lecteurs.
            path = absolute
        flags.append("-I" + path.replace("\\", "/"))
    return flags


env.Replace(_CPPINCFLAGS=include_flags)
