# -*- coding: utf-8 -*-
import os
import re
import subprocess
import sys
from typing import List

sys.stdout.reconfigure(encoding='utf-8') # for windows gh runner

from pyutils.utils import critical_difference, test_critical_difference, icdiff, parse_output

ok = True

MAX_LINE_DIFF = 2

env = os.environ.copy()
lp = "{}/../bin".format(os.path.dirname(os.path.realpath(__file__)))
if "LD_LIBRARY_PATH" in env:
    env["LD_LIBRARY_PATH"] += ":" + lp
else:
    env["LD_LIBRARY_PATH"] = lp

def run_unit_tests(tests):
    for test in tests:
        binary = "bin/" + test + (".exe" if sys.platform == "win32" else "")
        print("[🔵 Running test {}]".format(binary), flush=True)
        p = subprocess.Popen([binary], env=env)
        p.wait(timeout=10)
        if p.returncode != 0:
            global ok
            ok = False
        print("[{}, code {}]".format("🟢 Passed" if p.returncode == 0 else "🔴 Failed", p.returncode), flush=True)

def run_integration(expected_output_path: str, opt: bool):
    print("[🔵 Running integration test against {}]".format(expected_output_path), flush=True)
    with open(expected_output_path) as f:
        expected_output = f.read()
    p = subprocess.Popen(
        ["bin/integration" + (".exe" if sys.platform == "win32" else "")],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    output, err = p.communicate()
    output = output.decode("utf-8").replace("\r", "")
    passed = True
    expected_blocks = parse_output(expected_output)
    output_blocks = parse_output(output)
    if output_blocks != expected_blocks: # TODO for later: room for improvement for trace handling under opt
        if critical_difference(output, expected_output, MAX_LINE_DIFF, not opt):
            passed = False
        else:
            print("WARNING: Difference in output but deemed non-critical", flush=True)
        print(os.path.basename(expected_output_path))
        if opt:
            expected_blocks = filter(lambda b: b["type"] != "trace", expected_blocks)
            output_blocks = filter(lambda b: b["type"] != "trace", output_blocks)
        icdiff(
            ("\n".join(map(lambda b: "\n".join(b["lines"]), expected_blocks)), "expected"),
            ("\n".join(map(lambda b: "\n".join(b["lines"]), output_blocks)), "output")
        )
    elif p.returncode != 0:
        print("p.retruncode = {}".format(p.returncode), flush=True)
        passed = False
    elif len(err) != 0:
        print("Warning: Process stderr not empty:\n{}".format(err.decode("utf-8")), flush=True)
    print("[{}]".format("🟢 Passed" if passed else "🔴 Failed"), flush=True)
    if not passed:
        global ok
        ok = False

# Note: duplicate code with generate_outputs.py
def load_msvc_environment():
    global env
    run_command("powershell.exe", "./dump_msvc_env.ps1")
    with open(f"{env['TEMP']}/vcvars.txt", "r") as f:
        for line in f:
            m = re.match(r"^(.*)=(.*)$", line)
            #if m.group(1) not in env or env[m.group(1)] != m.group(2):
            #    print(f"setting {m.group(1)} = {m.group(2)}")
            env[m.group(1)] = m.group(2)

# Note: duplicate code with generate_outputs.py
def run_command(*args: List[str]):
    global env
    p = subprocess.Popen(args, env=env)
    p.wait()
    print("\033[0m") # makefile in parallel sometimes messes up colors
    if p.returncode != 0:
        print("[🔴 Command \"{}\" failed]".format(" ".join(args)))
        sys.exit(1)

def main():
    assert(len(sys.argv) >= 2)
    # usage: python3 run-tests.py [release|opt] [build] <compiler name>
    # in any order
    opt = False
    build_and_run = False
    for arg in sys.argv:
        if arg == "release" or arg == "opt":
            opt = True
        elif arg == "debug":
            pass
        elif arg == "build":
            build_and_run = True
        else:
            target = arg
    print(f"Running tests for {target}")
    if target == "self":
        test_critical_difference()
        return
    # canonicalize
    if target.startswith("g++") or target.startswith("gcc"):
        target_file = "gcc" + ("_windows" if target.endswith("_windows") else "")
        compiler_name = target
    elif target.startswith("clang"):
        target_file = "clang" + ("_windows" if target.endswith("_windows") else "")
        compiler_name = target
    else:
        assert(target == "msvc")
        target_file = "msvc"
        compiler_name = target
    # build if needed
    if build_and_run:
        run_command("make", "-C", "..", "clean")
        run_command("make", "-C", "..", f"COMPILER={compiler_name}", "-j")
        run_command("make", "clean")
        run_command("make", f"COMPILER={compiler_name}", "-j")
    # run everything
    run_unit_tests([
        "disambiguation",
        "literals",
        "test_public_utilities",
        "type_handling",
        "basic_test",
        "test_type_prettier"
    ])
    run_integration("integration/expected/{}.txt".format(target_file), opt)
    global ok
    print("Tests " + ("passed 🟢" if ok else "failed 🔴"), flush=True)
    sys.exit(not ok)

main()
