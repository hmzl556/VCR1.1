"""Analyze all enabled Keil C units; keep raw diagnostics and parser limits.

Cppcheck 2.10 on Windows cannot open this workspace's Unicode paths. Analyze
an unmodified temporary ASCII-path mirror with the actual project defines.
ARM assembly and proprietary libraries still require Keil/hardware validation.
"""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
project_dir = root / "bleuart/prj"
tree = ET.parse(project_dir / "ble_dtss.uvprojx")
include_dirs = [
    (project_dir / x).resolve()
    for x in tree.findtext(".//Cads/VariousControls/IncludePath").split(";")
]
units = []
for entry in tree.findall(".//Groups/Group/Files/File"):
    path = (project_dir / entry.findtext("FilePath")).resolve()
    if path.suffix.lower() == ".c" and entry.findtext(".//IncludeInBuild") != "0":
        units.append(path)

with tempfile.TemporaryDirectory(prefix="ir_static_") as td:
    mirror = Path(td) / "IR_Project"
    for source in root.rglob("*"):
        if (source.is_file() and source.suffix.lower() in {".c", ".h"}
                and not {"Objects", "Listings", "tools"}.intersection(source.parts)):
            target = mirror / source.relative_to(root)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    database = []
    for source in units:
        args = ["armcc", "-DBR1601A01", "-DUSB_STDPERIPH_DRIVER",
                "-D__CC_ARM", "-D__ARMCC_VERSION=5060750"]
        args += ["-I" + str(mirror / path.relative_to(root)) for path in include_dirs]
        args += ["-c", str(mirror / source.relative_to(root))]
        database.append({"directory": str(mirror / "bleuart/prj"),
                         "file": args[-1], "arguments": args})
    db = Path(td) / "compile_commands.json"
    db.write_text(json.dumps(database), encoding="utf-8")
    result = subprocess.run([
        "cppcheck", "--project=" + str(db), "--enable=warning,performance,portability",
        "--inconclusive", "--force", "--max-configs=8", "--platform=unix32",
        "--std=c99", "--template=gcc", "--quiet"
    ], capture_output=True)
    raw = (result.stdout + result.stderr).decode("utf-8", errors="replace")
    raw = raw.replace(str(mirror), str(root))
    (root / "tools/static_analysis.log").write_text(raw, encoding="utf-8")
    diagnostics = [line for line in raw.splitlines()
                   if ": warning:" in line or ": error:" in line]
    summary = {
        "tool": "Cppcheck 2.10",
        "translation_unit_count": len(units),
        "translation_units": [source.relative_to(root).as_posix() for source in units],
        "process_exit_code": result.returncode,
        "diagnostic_count": len(diagnostics),
        "diagnostics": diagnostics,
        "limits": ["ARM inline assembly in FreeRTOS port is not parsed",
                   "FreeRTOS configASSERT diagnostics require caller-contract review",
                   "Proprietary BLE libraries and actual hardware are outside host analysis"]
    }
    (root / "tools/static_analysis.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Analyzed {len(units)} Keil C units; {len(diagnostics)} raw diagnostics; exit {result.returncode}")
    if result.returncode:
        raise SystemExit("Static analysis could not complete; see tools/static_analysis.log")
