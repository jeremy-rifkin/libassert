#!/bin/bash

status=0
while read f
do
    echo checking $f
    clang-tidy $f -p=build
    ret=$?
    if [ $ret -ne 0 ]; then
        status=1
    fi
done <<< $(echo include/assert/assert.hpp && find src -name "*.hpp" -o -name "*.cpp")
exit $status
