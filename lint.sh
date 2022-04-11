#!/bin/bash

status=0
while read f
do
    echo checking $f
    flags=
    if [ $f != "tests/unit/basic_test.cpp" ]; then
        flags=-DASSERT_LOWERCASE
    fi
    clang-tidy $f -- -std=c++17 -Iinclude $flags
    ret=$?
    if [ $ret -ne 0 ]; then
        status=1
    fi
done <<< $(find include src tests -name "*.hpp" -o -name "*.cpp")
exit $status
