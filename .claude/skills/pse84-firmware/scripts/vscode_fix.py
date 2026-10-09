#!/usr/bin/env python3
"""Post-fix of `make vscode` output for Infineon ModusToolbox for VS Code (open_ide.sh, stage 2d).

ModusToolbox tools 3.9 `make vscode` writes tasks/settings in an older format than the extension
(1.12) expects, so on first open the ModusToolbox Assistant asks the developer to "Fix Tasks" /
"Fix Settings". This script applies the same fix up front, following the extension's own rules
(dist/extension.js 1.12.0: VSCodeTaskGenerator, VSCodeAppTaskGenerator, VSCodeProjTaskGenerator,
MTBVSCodeSettings.fixSettingsFile / cleanExtensionsFile / patchCodeWorkspaceFile):

  tasks.json      required tasks regenerated (replace by label, append if missing); compared by
                  the extension with JSON.stringify, so key order and spacing matter
                  app:     Rebuild, Clean, Build, Erase Device, Erase All, Build & Program, Quick Program
                  project: Rebuild:<p>, Clean:<p>, Build:<p>, Build & Program:<p>, Quick Program:<p>,
                           Build Application
                  build tasks get "TOOLCHAIN=<tc> CONFIG=<cfg>" (extension defaults GCC_ARM / Debug)
  settings.json   cortex-debug.openocdPath absolute, cortex-debug.gccPath (= armToolchainPath),
                  C_Cpp.intelliSenseEngine "disabled", clangd.detectExtensionConflicts false
  extensions.json ms-vscode.cpptools moved to unwantedRecommendations
  <app>.code-workspace  settings C_Cpp.intelliSenseEngine / clangd.detectExtensionConflicts

All values come from the make vscode output itself (no user paths are invented). If a future
extension changes its rules, it simply offers "Fix Tasks" again - the fix is harmless either way.
Usage: vscode_fix.py <app-dir> [--toolchain GCC_ARM] [--config Debug]
Prints "VSCODE-FIX: done (<n> files)"; exit 1 on unreadable files.
"""
import json
import os
import re
import sys
from pathlib import Path

# the extension's comment stripper: keep strings, drop // and /* */ comments
_COMMENTS = re.compile(r'\\"|"(?:\\"|[^"])*"|(//.*|/\*[\s\S]*?\*/)')
CPPTOOLS = "ms-vscode.cpptools"
APP_TASKS = ["Rebuild", "Clean", "Build", "Erase Device", "Erase All", "Build & Program", "Quick Program"]
PROJ_TASKS = ["Rebuild", "Clean", "Build", "Build & Program", "Quick Program", "Build Application"]


def load_jsonc(path):
    text = _COMMENTS.sub(lambda m: "" if m.group(1) else m.group(0), path.read_text(encoding="utf-8"))
    return json.loads(text)


def dump(path, data):
    path.write_text(json.dumps(data, indent=4, ensure_ascii=False), encoding="utf-8", newline="\n")


class TaskGen:
    TOOLS = "export CY_TOOLS_PATHS=${config:modustoolbox.toolsPath}"

    def __init__(self, toolchain, config, project=None):
        self.args = (f"TOOLCHAIN={toolchain}" if toolchain else "") + (f" CONFIG={config}" if config else "")
        self.project = project

    def name(self, base, proj=None):
        return base + (":" + proj if proj else "")

    def is_build(self, base):
        builds = {"Build", "Rebuild", "Build & Program"} if self.project is None else \
                 {"Build", "Build Application", "Build & Program"}
        return base in builds

    def make_task(self, label, target, extra, matcher, proj):
        base = label.split(":")[0]
        if self.is_build(base):
            extra = (extra + " " if extra else "") + self.args
        line = f"{target}_proj {extra}" if proj else f"{target} {extra}"
        task = {
            "label": label, "type": "process", "command": "bash",
            "args": ["--norc", "-c", f"{self.TOOLS} ; make {line} "],
            "windows": {
                "command": "${config:modustoolbox.toolsPath}/modus-shell/bin/bash.exe",
                "args": ["--norc", "-c", "export PATH=/bin:/usr/bin:$PATH ; " + self.TOOLS
                         + " ; ${config:modustoolbox.toolsPath}/modus-shell/bin/make.exe " + line],
            },
        }
        if matcher:
            task["problemMatcher"] = {"base": "$gcc"} if proj else "$gcc"
        if self.is_build(base):
            task["group"] = {"kind": "build", "isDefault": True}
        return task

    def rebuild(self):
        p = self.project
        return {"label": self.name("Rebuild", p), "dependsOrder": "sequence",
                "dependsOn": [self.name("Clean", p), self.name("Build", p)],
                "group": {"kind": "build", "isDefault": True}}

    def required(self):
        if self.project is None:
            return APP_TASKS
        return [t if t == "Build Application" else self.name(t, self.project) for t in PROJ_TASKS]

    def generate(self, label):
        base, p = label.split(":")[0], self.project
        if base == "Rebuild":
            return self.rebuild()
        spec = {"Build": ("build", "", True), "Clean": ("clean", "", False),
                "Erase Device": ("erase", "", False), "Erase All": ("erase", "MTB_ERASE_EXT_MEM=1", False),
                "Build & Program": ("program", "", True), "Quick Program": ("qprogram", "", False)}
        if base == "Build Application":
            return self.make_task(base, "build", "", True, None)
        target, extra, matcher = spec[base]
        return self.make_task(label, target, extra, matcher, p)


def fix_tasks(path, gen):
    data = load_jsonc(path)
    tasks = data.setdefault("tasks", [])
    for label in gen.required():
        want = gen.generate(label)
        idx = next((i for i, t in enumerate(tasks) if t.get("label") == label), None)
        if idx is None:
            tasks.append(want)
        else:
            tasks[idx] = want
    dump(path, {"version": data.get("version", "2.0.0"), "tasks": tasks})


def fix_settings(path):
    s = load_jsonc(path)
    tools = s.get("modustoolbox.toolsPath", "")
    ocd = s.get("cortex-debug.openocdPath", "")
    if "${config:modustoolbox.toolsPath}" in ocd and tools:
        ocd = os.path.normpath(ocd.replace("${config:modustoolbox.toolsPath}", tools)).replace("\\", "/")
    if ocd:
        s["cortex-debug.openocdPath"] = ocd
    if s.get("cortex-debug.armToolchainPath"):
        s["cortex-debug.gccPath"] = s["cortex-debug.armToolchainPath"]
    s["C_Cpp.intelliSenseEngine"] = "disabled"
    s["clangd.detectExtensionConflicts"] = False
    dump(path, s)


def fix_extensions(path):
    e = load_jsonc(path)
    e["recommendations"] = [r for r in e.get("recommendations", []) if str(r).lower() != CPPTOOLS]
    unwanted = e.setdefault("unwantedRecommendations", [])
    if CPPTOOLS not in [str(u).lower() for u in unwanted]:
        unwanted.append(CPPTOOLS)
    dump(path, e)


def fix_workspace(path):
    w = load_jsonc(path)
    st = w.setdefault("settings", {})
    st["C_Cpp.intelliSenseEngine"] = "disabled"
    st["clangd.detectExtensionConflicts"] = False
    dump(path, w)


def main():
    args = sys.argv[1:]
    if not args or args[0].startswith("-"):
        sys.exit(__doc__)
    app = Path(args[0])
    opts = dict(zip(args[1::2], args[2::2]))
    tc, cfg = opts.get("--toolchain", "GCC_ARM"), opts.get("--config", "Debug")
    n = 0
    try:
        for vs in sorted(app.glob("*/.vscode")) + [app / ".vscode"]:
            proj = None if vs.parent == app else vs.parent.name
            if (vs / "tasks.json").is_file():
                fix_tasks(vs / "tasks.json", TaskGen(tc, cfg, proj)); n += 1
            if (vs / "settings.json").is_file():
                fix_settings(vs / "settings.json"); n += 1
            if (vs / "extensions.json").is_file():
                fix_extensions(vs / "extensions.json"); n += 1
        for ws in app.glob("*.code-workspace"):
            fix_workspace(ws); n += 1
    except (OSError, ValueError) as e:
        print(f"VSCODE-FIX: FAIL ({e})")
        sys.exit(1)
    print(f"VSCODE-FIX: done ({n} files, TOOLCHAIN={tc} CONFIG={cfg})")


if __name__ == "__main__":
    main()
