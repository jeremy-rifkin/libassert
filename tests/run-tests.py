# -*- coding: utf-8 -*-
import os
import subprocess
import sys

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

def main():
    assert(len(sys.argv) == 2 or len(sys.argv) == 3)
    target = sys.argv[1]
    opt = False
    if len(sys.argv) == 3:
        opt = sys.argv[2] == "release" or sys.argv[2] == "opt"
    if target == "self":
        test_critical_difference()
        return
    # canonicalize
    if target.startswith("g++"):
        target = "gcc" + ("_windows" if target.endswith("_windows") else "")
    if target.startswith("clang"):
        target = "clang" + ("_windows" if target.endswith("_windows") else "")
    run_unit_tests(["disambiguation", "literals", "type_handling", "basic_test"])
    run_integration("integration/expected/{}.txt".format(target), opt)
    global ok
    print("Tests " + ("passed 🟢" if ok else "failed 🔴"), flush=True)
    sys.exit(not ok)

main()
