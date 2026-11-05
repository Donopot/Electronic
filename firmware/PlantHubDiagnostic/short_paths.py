"""Shorten include arguments for Windows compiler child processes."""
import os

Import("env")


def include_flags(target, source, env, for_signature):
    project = env.subst("$PROJECT_DIR")
    return [
        "-I" + os.path.relpath(
            env.Dir(env.subst(str(entry))).get_abspath(), project
        ).replace(os.sep, "/")
        for entry in env.get("CPPPATH", [])
    ]


env.Replace(_CPPINCFLAGS=include_flags)
