import os
import platform
import re
import subprocess
import sys
from typing import List

env = os.environ.copy()
lp = "{}/../bin".format(os.path.dirname(os.path.realpath(__file__)))
if "LD_LIBRARY_PATH" in env:
    env["LD_LIBRARY_PATH"] += ":" + lp
else:
    env["LD_LIBRARY_PATH"] = lp

def load_msvc_environment():
    global env
    run_command("powershell.exe", "./dump_msvc_env.ps1")
    with open(f"{env['TEMP']}/vcvars.txt", "r") as f:
        for line in f:
            m = re.match(r"^(.*)=(.*)$", line)
            #if m.group(1) not in env or env[m.group(1)] != m.group(2):
            #    print(f"setting {m.group(1)} = {m.group(2)}")
            env[m.group(1)] = m.group(2)

def run_command(*args: List[str]):
    global env
    p = subprocess.Popen(args, env=env)
    p.wait()
    if p.returncode != 0:
        print("[🔴 Command \"{}\" failed]".format(" ".join(args)))
        sys.exit(1)

def build(compiler: str):
    run_command("make", "-C", "..", "clean")
    run_command("make", "-C", "..", f"COMPILER={compiler}", "-j")
    run_command("make", "clean")
    run_command("make", "integration", f"COMPILER={compiler}", "-j")
    print("\033[0m") # makefile in parallel sometimes messes up colors

def generate(compiler: str, output_name: str):
    global env
    print(f"[🔵 Compiling for {compiler}]")
    build(compiler)
    print(f"[🔵 Running integration {compiler}]")
    p = subprocess.Popen(["bin/integration" + (".exe" if sys.platform == "win32" else "")],
        stdout=subprocess.PIPE, env=env)
    test_output, _ = p.communicate()
    test_output = test_output.decode("utf-8").replace("\r", "")
    with open(f"integration/expected/{output_name}.txt", "w") as f:
        f.write(test_output)
    print(f"[🟢 Done, updated output for {compiler}]")

def main():
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    if platform.system() != "Windows":
        generate("g++", "gcc")
        generate("clang++", "clang")
    else:
        load_msvc_environment()
        generate("g++", "gcc_windows")
        generate("clang++", "clang_windows")
        generate("msvc", "msvc")

main()
