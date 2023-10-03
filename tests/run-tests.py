# -*- coding: utf-8 -*-
import os
import re
import subprocess
import sys
import platform
from typing import List

sys.stdout.reconfigure(encoding='utf-8') # for windows gh runner

from pyutils.utils import critical_difference, icdiff, parse_output

ok = True

MAX_LINE_DIFF = 2

env = os.environ.copy()
lp = "{}/../bin".format(os.path.dirname(os.path.realpath(__file__)))
if "LD_LIBRARY_PATH" in env:
    env["LD_LIBRARY_PATH"] += ":" + lp
else:
    env["LD_LIBRARY_PATH"] = lp

def run_integration(integration_binary: str, expected: str, opt: bool):
    p = subprocess.Popen(
        [integration_binary],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env
    )
    output, err = p.communicate()
    output = output.decode("utf-8").replace("\r", "")
    passed = True
    expected_blocks = parse_output(expected)
    output_blocks = parse_output(output)
    if output_blocks != expected_blocks: # TODO for later: room for improvement for trace handling under opt
        if critical_difference(output, expected, MAX_LINE_DIFF, not opt):
            passed = False
        else:
            print("WARNING: Difference in output but deemed non-critical", flush=True)
        print(os.path.basename(expected))
        if opt:
            expected_blocks = filter(lambda b: b["type"] != "trace", expected_blocks)
            output_blocks = filter(lambda b: b["type"] != "trace", output_blocks)
        icdiff(
            ("\n".join(map(lambda b: "\n".join(b["lines"]), expected_blocks)), "expected"),
            ("\n".join(map(lambda b: "\n".join(b["lines"]), output_blocks)), "output")
        )
    if p.returncode != 0:
        print("p.retruncode = {}".format(p.returncode), flush=True)
        passed = False
    if len(err) != 0:
        print("Warning: Process stderr not empty:\n{}".format(err.decode("utf-8")), flush=True)
    print("[{}]".format("🟢 Passed" if passed else "🔴 Failed"), flush=True)
    return passed

def similarity(name: str, target: List[str]) -> int:
    parts = name.split(".txt")[0].split(".")
    c = 0
    for part in parts:
        if part in target:
            c += 1
        else:
            return -1
    return c

def main():
    if len(sys.argv) < 2:
        print("Expected at least one arg")
        sys.exit(1)

    integration_binary = sys.argv[1]

    target = []

    if sys.argv[2].startswith("gcc") or sys.argv[1].startswith("g++"):
        target.append("gcc")
    elif sys.argv[2].startswith("clang"):
        target.append("clang")
    elif sys.argv[2].startswith("cl"):
        target.append("msvc")

    if platform.system() == "Windows":
        target.append("windows")
    elif platform.system() == "Darwin":
        target.append("macos")
    else:
        target.append("linux")

    other_configs = sys.argv[3:]
    for config in other_configs:
        target.append(config.lower())

    print(f"Searching for expected file best matching {target}")

    expected_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "integration/expected/")
    files = [f for f in os.listdir(expected_dir) if os.path.isfile(os.path.join(expected_dir, f))]
    if len(files) == 0:
        print(f"Error: No expected files to use (searching {expected_dir})", file=sys.stderr)
        sys.exit(1)
    files = list(map(lambda f: (f, similarity(f, target)), files))
    m = max(files, key=lambda entry: entry[1])[1]
    if m <= 0:
        print(f"Error: Could not find match for {target} in {files}", file=sys.stderr)
        sys.exit(1)
    files = [entry[0] for entry in files if entry[1] == m]
    if len(files) > 1:
        print(f"Error: Ambiguous expected file to use ({files})", file=sys.stderr)
        sys.exit(1)

    file = files[0]
    print(f"Reading from {file}")

    with open(os.path.join(os.path.dirname(os.path.realpath(__file__)), "integration/expected/", file), "r") as f:
        expected = f.read()

    if run_integration(integration_binary, expected, "debug" not in target):
        print("Test passed")
    else:
        print("Test failed")
        sys.exit(1)

main()
