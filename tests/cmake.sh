#!/usr/bin/env bash

set -e

rm -rf ./install*
rm -rf ./build*
rm -rf ./cmake_install/build*

cmake -S .. -B build_shared -DCMAKE_INSTALL_PREFIX=./install_shared -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=TRUE
cmake -S .. -B build_static -DCMAKE_INSTALL_PREFIX=./install_static -DCMAKE_BUILD_TYPE=Release

cmake --build build_shared --target install
cmake --build build_static --target install

cmake -S cmake_client -B cmake_client/build_shared -DCMAKE_PREFIX_PATH=../install_shared -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DCMAKE_BUILD_TYPE=Release
cmake -S cmake_client -B cmake_client/build_static -DCMAKE_PREFIX_PATH=../install_static -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE -DCMAKE_BUILD_TYPE=Release

cmake --build cmake_client/build_shared
cmake --build cmake_client/build_static

./cmake_client/build_shared/main
./cmake_client/build_static/main
