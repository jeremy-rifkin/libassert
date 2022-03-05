import subprocess
import os
import sys

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
		print("[Running test {}]".format(binary))
		p = subprocess.Popen([binary], env=env)
		p.wait(timeout=10)
		if p.returncode != 0:
			global ok
			ok = False
		print("[{}, code {}]".format("Passed" if p.returncode == 0 else "Failed", p.returncode))

# This function scans two outputs and ignores close mismatches in line numbers
def critical_difference(output, expected_output):
	output_lines = [l.strip() for l in "\n".split(output)]
	expected_lines = [l.strip() for l in "\n".split(expected_output)]
	# state machine
	in_trace = False
	last_was_frame_line = False
	for i in range(min(len(output_lines), len(expected_lines))):
		if output_lines[i] == "Stack trace:" and expected_lines[i] == "Stack trace:":
			if in_trace:
				# this is an error
				return True
			else:
				in_trace = True
				last_was_frame_line = False
		elif in_trace:
			if output_lines[i].strip() == "" and expected_lines[i].strip() == "":
				in_trace = False
			elif output_lines[i].startswith("# ") and expected_lines[i].startswith("# "):
				if last_was_frame_line:
					# this is an error
					return True
				else:
					last_was_frame_line = True
			else:
				assert(last_was_frame_line)
				n1 = ":".split(output_lines[i])[-1]
				n2 = ":".split(output_lines[i])[-1]
				if n1 == "?" and n2 == "?":
					pass
				else:
					n1 = int(n1)
					n2 = int(n2)
					if abs(n1 - n2) > MAX_LINE_DIFF:
						return True
	return False

def run_integration(expected_output_path):
	global ok
	print("[Running integration test against {}]".format(expected_output_path))
	with open(expected_output_path) as f:
		expected_output = f.read().strip()
	p = subprocess.Popen(
		["bin/integration" + (".exe" if sys.platform == "win32" else "")],
		stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
	output, err = p.communicate()
	output = output.decode("utf-8").replace("\r", "").strip()
	if output != expected_output:
		if critical_difference(output, expected_output):
			ok = False
		else:
			print("WARNING: Difference in output but deemed non-critical")
		with open("output.txt", "w", newline="\n") as f:
			f.write(output)
		dp = subprocess.Popen([
			"icdiff",
			expected_output_path,
			"output.txt",
			"--cols",
			"150"
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
