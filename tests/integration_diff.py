#!/bin/python3
import itertools
import os
import pprint
import sys

from pyutils.utils import parse_output, icdiff

def extract_lines(blocks, type_filter):
	return sum(list(map(lambda block: block["lines"], filter(lambda block: type_filter(block["type"]), blocks))), [])

def diff(a: str, b: str, a_name: str, b_name: str):
	a = parse_output(a)
	b = parse_output(b)
	a_seq = list(map(lambda block: block["type"], a))
	b_seq = list(map(lambda block: block["type"], b))
	if a_seq == b_seq:
		a_traces = extract_lines(a, lambda x: x != "message")
		b_traces = extract_lines(b, lambda x: x != "message")
		print("[[ Trace Diff ]]")
		icdiff(
			("\n".join(a_traces), a_name),
			("\n".join(b_traces), b_name)
		)
		a_messages = extract_lines(a, lambda x: x != "trace")
		b_messages = extract_lines(b, lambda x: x != "trace")
		print("[[ Non-Trace Diff ]]")
		icdiff(
			("\n".join(a_messages), a_name),
			("\n".join(b_messages), b_name)
		)
		print("[[ Done ]]")
	else:
		pprint.PrettyPrinter(indent=4).pprint(list(map(lambda p: (p[0], p[1], "*" * 10 if p[0] != p[1] else ""),
		                                             itertools.zip_longest(a_seq, b_seq))))
		print("Error: Critical difference between the two")
		# Can provide better diagnostics here... if needed later... would be a pain

def main():
	assert(len(sys.argv) == 3)
	_, a, b = sys.argv
	a_name = os.path.basename(a)
	b_name = os.path.basename(b)
	with open(a, "r") as f:
		a = f.read()
	with open(b, "r") as f:
		b = f.read()
	diff(a, b, a_name, b_name)

main()
