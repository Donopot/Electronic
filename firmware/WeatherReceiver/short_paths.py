"""Keep GCC child command lines below the Windows length limit."""
import os

Import("env")

def include_flags(target, source, env, for_signature):
    project = env.subst("$PROJECT_DIR")
    flags = []
    for entry in env.get("CPPPATH", []):
        value = env.Dir(env.subst(str(entry))).get_abspath()
        flags.append("-I" + os.path.relpath(value, project).replace(os.sep, "/"))
    return flags


# Preserve CPPPATH for dependency scanning; shorten only compiler arguments.
env.Replace(_CPPINCFLAGS=include_flags)
