import subprocess
import os
import sys

ok = True

env = os.environ.copy()
lp = "{}/../bin".format(os.path.dirname(os.path.realpath(__file__)))
if "LD_LIBRARY_PATH" in env:
	env["LD_LIBRARY_PATH"] += ":" + lp
else:
	env["LD_LIBRARY_PATH"] = lp

def run_unit_tests(tests):
	for test in tests:
		binary = "bin/" + test + (".exe" if sys.platform == "win32" else "")
		print("[Running test {}]".format(binary))
		p = subprocess.Popen([binary], env=env)
		p.wait(timeout=10)
		if p.returncode != 0:
			global ok
			ok = False
		print("[{}, code {}]".format("Passed" if p.returncode == 0 else "Failed", p.returncode))

def run_integration(expected_output_path):
	global ok
	print("[Running integration test against {}]".format(expected_output_path))
	with open(expected_output_path) as f:
		expected_content = f.read().strip()
	p = subprocess.Popen(
		["bin/integration" + (".exe" if sys.platform == "win32" else "")],
		stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
	out, err = p.communicate()
	out = out.decode("utf-8").replace("\r", "").strip()
	if out != expected_content:
		ok = False
		with open("output.txt", "w", newline="\n") as f:
			f.write(out)
		dp = subprocess.Popen([
			"icdiff",
			expected_output_path,
			"output.txt",
			"--cols",
			"100"
		])
		dp.wait()
	elif p.returncode != 0 or len(err) != 0:
		ok = False
	print("[{}]".format("Passed" if p.returncode == 0 else "Failed"))

def main():
	assert(len(sys.argv) == 2)
	run_unit_tests(["disambiguation", "literals"])
	run_integration("integration/expected/{}.txt".format(sys.argv[1]))
	global ok
	print("tests " + ("passed" if ok else "failed"))
	sys.exit(not ok)

main()
