#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-g++} -std=c++17 -O3 -Wall -Wextra -Wpedantic -pthread src/engine.cpp src/cli.cpp -o build/passgen-cli
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc src/engine.cpp tests/engine_test.cpp -o build/passgen-tests
./build/passgen-tests

${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -Isrc tests/checkpoint_retry_test.cpp -o build/checkpoint-retry-tests
./build/checkpoint-retry-tests

${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc tests/resource_test.cpp -o build/resource-tests
./build/resource-tests
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -Isrc src/engine.cpp tests/resource_engine_test.cpp -o build/resource-engine-tests
./build/resource-engine-tests
