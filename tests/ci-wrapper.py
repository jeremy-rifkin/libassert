# -*- coding: utf-8 -*-
import os
import subprocess
import sys

env = os.environ.copy()
lp = "{}/../bin".format(os.path.dirname(os.path.realpath(__file__)))
if "LD_LIBRARY_PATH" in env:
    env["LD_LIBRARY_PATH"] += ":" + lp
else:
    env["LD_LIBRARY_PATH"] = lp

# This script is just a poor workaround for https://github.com/actions/runner-images/issues/8803
buggy_path = "C:\\Program Files\\Git\\mingw64\\bin;C:\\Program Files\\Git\\usr\\bin;C:\\Users\\runneradmin\\bin;"
if env["PATH"].startswith(buggy_path):
    env["PATH"] = env["PATH"].replace(buggy_path, "", 1)

p = subprocess.Popen(
    sys.argv[1:],
    env=env
)
p.wait()
sys.exit(p.returncode)
