import itertools
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
                line = output_lines[i]
                if re.match(r"^\w+ failed at .+\.cpp:\d+: .+$", line):
                    line = re.sub(r"(?<= failed at ).+\.cpp(?=:\d+: .+$)", "integration/integration.cpp", line)
                e["lines"].append(line)
                i += 1
            # read until non blank line
            while i < len(output_lines) and output_lines[i] == "":
                e["lines"].append(output_lines[i])
                i += 1
            tokens.append(e)
    return tokens

# This function scans two outputs and ignores close mismatches in line numbers
def critical_difference(raw_output: str, raw_expected: str, max_line_diff: int, check_trace: bool = True):
    try:
        expected = parse_output(raw_expected)
        output = parse_output(raw_output)
        if len(output) != len(expected):
            print("Note: critical_difference true due token count mismatch, {} vs {}"
                .format(len(expected), len(output)), flush=True)
            return True
        for (a, b) in zip(expected, output):
            if a["type"] != b["type"]:
                print("Note: critical_difference true due to line mismatch in parsed trace blocks: {} vs {}"
                    .format(a["type"], b["type"]), flush=True)
                return True
            if a["type"] == "sector" or a["type"] == "message":
                if a["lines"] != b["lines"]:
                    print("Note: critical_difference true due to line mismatch in message:")
                    print("/" * 40)
                    print(a["lines"])
                    print("/" * 40 + " vs:")
                    print(b["lines"])
                    print("/" * 40, flush=True)
                    return True
            elif a["type"] == "trace":
                if check_trace:
                    for (line_a, line_b) in itertools.zip_longest(a["lines"], b["lines"]):
                        if line_a != line_b:
                            m_a = re.match(r"^(\s+.+):(\d+)$", line_a)
                            m_b = re.match(r"^(\s+.+):(\d+)$", line_b)
                            if m_a and m_b:
                                if m_a.group(1) != m_b.group(1):
                                    print("Note: critical_difference true due to file mismatch, {} vs {}"
                                        .format(m_a.group(1), m_b.group(1)), flush=True)
                                    return True
                                l_a = int(m_a.group(2))
                                l_b = int(m_b.group(2))
                                if abs(l_a - l_b) > max_line_diff:
                                    print("Note: critical_difference true due to line number mismatch, {} vs {}"
                                        .format(l_a, l_b), flush=True)
                                    return True
                            else:
                                print("Note: critical_difference true due to line mismatch in trace:")
                                print("/" * 40)
                                print(a["lines"])
                                print("/" * 40 + " vs:")
                                print(b["lines"])
                                print("/" * 40, flush=True)
                                return True
            else:
                assert(False)
        return False
    except Exception as e:
        print("Exception in critical_difference:", e, flush=True)
        return True

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
    def launch_ic(*args):
        dp = subprocess.Popen([
            *args,
            a_path,
            b_path,
            "--cols",
            "170"
        ])
        dp.wait()
    try:
        launch_ic("icdiff")
    except FileNotFoundError:
        try:
            launch_ic("python3", "./icdiff")
        except FileNotFoundError:
            print("FileNotFoundError while spawning subprocess")
    print("{x}============={x}".format(x = "=" * 40))
    print("{x} basic diff: {x}".format(x = "=" * 40))
    print("{x}============={x}".format(x = "=" * 40))
    try:
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
    except FileNotFoundError:
        print("FileNotFoundError while spawning subprocess")
    for path in to_delete:
        os.remove(path)




# Test cases

def test_critical_difference(): # self test cases
    max_line_diff = 2
    # test equal
    a = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(not critical_difference(a, a, max_line_diff))
    # test message difference
    a = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    b = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::somethfng_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(critical_difference(a, b, max_line_diff))
    # test trace difference
    a = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    b = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<char>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(critical_difference(a, b, max_line_diff))
    # test non-critical line difference
    a = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    b = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1410
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(not critical_difference(a, b, max_line_diff))
    # test critical line difference
    a = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    b = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1501
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(critical_difference(a, b, max_line_diff))
    # test ignore-trace
    a = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1501
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    b = """
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_asdfpp:1501
# 2 void teasdfon.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(not critical_difference(a, b, max_line_diff, False))
    # test section difference
    a = """
===================== [value printing: floating point] =====================
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    b = """
===================== [value printing: flopping point] =====================
Assertion failed at integration/integration.cpp:1408: void test_class<T>::something_else() [with T = int]:
    assert(.1f == .1);
    Where:
        .1f => 0.100000001
        .1  => 0.10000000000000001

Stack trace:
# 1 test_class<int>::something_else()
      at integration.cpp:1408
# 2 void test_class<int>::something<N>(std::pair<N, int>)
      at integration.cpp:1005
# 3 main
      at integration.cpp:403
""".strip()
    assert(critical_difference(a, b, max_line_diff))

if __name__ == "main":
    test_critical_difference()
