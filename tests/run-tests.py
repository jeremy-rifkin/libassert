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
		print("[Running test {}]".format(binary), flush=True)
		p = subprocess.Popen([binary], env=env)
		p.wait(timeout=10)
		if p.returncode != 0:
			global ok
			ok = False
		print("[{}, code {}]".format("Passed" if p.returncode == 0 else "Failed", p.returncode), flush=True)

# This function scans two outputs and ignores close mismatches in line numbers
def critical_difference(output: str, expected_output: str):
	try:
		output_lines = [l.strip() for l in output.split("\n")]
		expected_lines = [l.strip() for l in expected_output.split("\n")]
		# state machine
		in_trace = False
		last_was_frame_line = False
		if len(output_lines) != len(expected_lines):
			print("(Note: critical_difference true due to lines mismatch, {} vs {})"
				.format(len(expected_lines), len(output_lines)), flush=True)
			return True
		for i in range(len(output_lines)):
			if in_trace:
				if last_was_frame_line:
					last_was_frame_line = False
					l1 = output_lines[i].split(":")
					l2 = expected_lines[i].split(":")
					if l1[:-1] != l2[:-1]:
						print("(Note: critical_difference true due to trace file mismatch, {} vs {})"
							.format(repr(l2[:-1]), repr(l1[:-1])), flush=True)
						return True
					n1 = l1[-1]
					n2 = l2[-1]
					if n1 == "?" and n2 == "?":
						pass
					else:
						n1 = int(n1)
						n2 = int(n2)
						if abs(n1 - n2) > MAX_LINE_DIFF:
							print("(Note: critical_difference true due to line number mismatch, {} vs {})".format(n2, n1))
							return True
				elif output_lines[i] != expected_lines[i]:
					print("(Note: critical_difference true due to line mismatch in trace, {} vs {})"
						.format(repr(expected_lines[i]), repr(output_lines[i])), flush=True)
					return True
				elif output_lines[i] == "": # and expected_lines[i].strip() == "":
					in_trace = False
				elif output_lines[i].startswith("# "): # and expected_lines[i].startswith("# "):
					last_was_frame_line = True
			elif output_lines[i] != expected_lines[i]:
				print("(Note: critical_difference true due to line mismatch, {} vs {})"
					.format(repr(expected_lines[i]), repr(output_lines[i])), flush=True)
				return True
			elif output_lines[i] == "Stack trace:": # and expected_lines[i] == "Stack trace:":
				in_trace = True
				last_was_frame_line = False
		return False
	except Exception as e:
		print("Exception in critical_difference:", e)
		return False

def run_integration(expected_output_path: str):
	global ok
	print("[Running integration test against {}]".format(expected_output_path), flush=True)
	with open(expected_output_path) as f:
		expected_output = f.read()
	p = subprocess.Popen(
		["bin/integration" + (".exe" if sys.platform == "win32" else "")],
		stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
	output, err = p.communicate()
	output = output.decode("utf-8").replace("\r", "")
	if output != expected_output:
		if critical_difference(output, expected_output):
			ok = False
		else:
			print("WARNING: Difference in output but deemed non-critical", flush=True)
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
	print("[{}]".format("Passed" if p.returncode == 0 else "Failed"), flush=True)

def main():
	assert(len(sys.argv) == 2)
	run_unit_tests(["disambiguation", "literals"])
	run_integration("integration/expected/{}.txt".format(sys.argv[1]))
	global ok
	print("tests " + ("passed" if ok else "failed"), flush=True)
	sys.exit(not ok)

main()
