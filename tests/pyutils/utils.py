import os
import re
import subprocess
from typing import Union

sectionre = r"^={21} \[.*\] ={21}$"

def parse_output(output: str):
	output_lines = [l for l in output.split("\n")]
	tokens = []
	i = 0
	while i < len(output_lines):
		line = output_lines[i]
		if re.match(sectionre, line):
			tokens.append({
				"lines": [line],
				"type": "sector"
			})
			i += 1
		elif line == "Stack trace:":
			e = {
				"lines": [line],
				"type": "trace"
			}
			i += 1
			# read until blank line
			while i < len(output_lines) and output_lines[i] != "":
				e["lines"].append(output_lines[i])
				i += 1
			# read until non blank line
			while i < len(output_lines) and output_lines[i] == "":
				e["lines"].append(output_lines[i])
				i += 1
			tokens.append(e)
		else:
			e = {
				"lines": [],
				"type": "message"
			}
			# read until blank line
			while i < len(output_lines) and output_lines[i] != "":
				e["lines"].append(output_lines[i])
				i += 1
			# read until non blank line
			while i < len(output_lines) and output_lines[i] == "":
				e["lines"].append(output_lines[i])
				i += 1
			tokens.append(e)
	return tokens

def icdiff(a: Union[str, tuple], b: Union[str, tuple]):
	to_delete = []
	a_path = a
	b_path = b
	if type(a) is tuple:
		a_path = a[1]
		to_delete.append(a_path)
		with open(a_path, "w", newline="\n") as f:
			f.write(a[0])
	if type(b) is tuple:
		b_path = b[1]
		to_delete.append(b_path)
		with open(b_path, "w", newline="\n") as f:
			f.write(b[0])
	# TODO: Some issues with icdiff's edit distance...... For now just do both
	print()
	print("{x}========={x}".format(x = "=" * 40))
	print("{x} icdiff: {x}".format(x = "=" * 40))
	print("{x}========={x}".format(x = "=" * 40))
	dp = subprocess.Popen([
		"icdiff",
		a_path,
		b_path,
		"--cols",
		"170"
	])
	dp.wait()
	print("{x}============={x}".format(x = "=" * 40))
	print("{x} basic diff: {x}".format(x = "=" * 40))
	print("{x}============={x}".format(x = "=" * 40))
	dp = subprocess.Popen([
		"diff",
		"-y",
		"--suppress-common-lines",
		"--width",
		"200",
		a_path,
		b_path,
	])
	dp.wait()
	for path in to_delete:
		os.remove(path)
