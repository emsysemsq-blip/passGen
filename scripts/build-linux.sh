#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-g++} -std=c++17 -O3 -Wall -Wextra -Wpedantic -pthread src/engine.cpp src/cli.cpp -o build/passgen-cli
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc src/engine.cpp tests/engine_test.cpp -o build/passgen-tests
./build/passgen-tests
