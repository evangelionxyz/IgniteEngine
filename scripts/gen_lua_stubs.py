#!/usr/bin/env python3
"""Drift check between lua.cpp bindings and ignite.d.lua (LuaCATS stubs).

Usage: python scripts/gen_lua_stubs.py --check
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CPP = ROOT / "ignite/engine/src/ignite/scripting/lua/lua.cpp"
STUB = ROOT / "ignite/editor/resources/lua/ignite.d.lua"


def cpp_api():
    """Return {namespace: set(names)} registered in RegisterBindings."""
    text = CPP.read_text(encoding="utf-8", errors="replace")
    body = text[text.index("void RegisterBindings(") :]
    # Cut at the end of the function (first line that is exactly '    }').
    end = body.find("\n    }\n")
    body = body[: end if end > 0 else len(body)]
    api, pending, root = {}, [], {}
    tbl_names = ("Math", "Scene", "Physics", "Debug", "Net", "Input", "Key", "Mouse")
    root_pending = []
    for line in body.splitlines():
        if line.strip() == 'lua_newtable(state);':
            root_pending += pending
            pending = []
        for m in re.finditer(r'RegisterSceneClosure\(state, scene, "(\w+)"', line):
            pending.append(m.group(1))
        for m in re.finditer(r'lua_setfield\(state, -2, "(\w+)"\)', line):
            name = m.group(1)
            if name in tbl_names and not line.strip().startswith("lua_pushcfunction"):
                api[name] = set(pending)
                pending = []
            elif line.strip().startswith(("lua_pushcfunction", "lua_pushinteger")):
                pending.append(name)
        for m in re.finditer(r'setKey\("(\w+)"', line):
            pending.append(m.group(1))
    api["Ignite"] = {n for n in set(root_pending + pending) if not n.startswith("__")}
    return api


def stub_names():
    text = STUB.read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(r"^(?:function )?Ignite(?:\.(\w+))?[.:=\s(]", text, re.M):
        pass
    for m in re.finditer(r"^function Ignite\.(\w+)\.(\w+)\(", text, re.M):
        out.setdefault(m.group(1), set()).add(m.group(2))
    for m in re.finditer(r"^function Ignite\.(\w+)\(", text, re.M):
        out.setdefault("Ignite", set()).add(m.group(1))
    for m in re.finditer(r"^---@field (\w+) integer", text, re.M):
        out.setdefault("_fields", set()).add(m.group(1))
    return out


def main():
    cpp, stub = cpp_api(), stub_names()
    fields = stub.get("_fields", set())
    missing = []
    for ns, names in cpp.items():
        have = stub.get(ns, set()) if ns not in ("Key", "Mouse") else fields
        for n in sorted(names - have):
            missing.append(f"{'Ignite' if ns == 'Ignite' else 'Ignite.' + ns}.{n}")
    if missing:
        print("Missing from ignite.d.lua:")
        for m in missing:
            print("  " + m)
        return 1
    print("ignite.d.lua is in sync with lua.cpp")
    return 0


if __name__ == "__main__":
    sys.exit(main() if "--check" in sys.argv else (print(__doc__) or 0))

