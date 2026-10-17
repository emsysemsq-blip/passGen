#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p dist
compiler="${CXX_WINDOWS:-x86_64-w64-mingw32-g++-posix}"
"$compiler" -std=c++17 -O3 -Wall -Wextra -Wpedantic -pthread -static -static-libgcc -static-libstdc++ -municode -mwindows src/engine.cpp src/winmain.cpp -lcomdlg32 -lcomctl32 -o dist/PassGen.exe
"$compiler" -std=c++17 -O3 -Wall -Wextra -Wpedantic -pthread -static -static-libgcc -static-libstdc++ -municode src/engine.cpp src/cli.cpp -o dist/passgen-cli.exe
"$compiler" -std=c++17 -O2 -pthread -static -static-libgcc -static-libstdc++ -Isrc src/engine.cpp tests/engine_test.cpp -o dist/passgen-tests.exe

"$compiler" -std=c++17 -O2 -pthread -static -static-libgcc -static-libstdc++ -Isrc tests/gui_smoke.cpp -o dist/passgen-gui-smoke.exe

"$compiler" -std=c++17 -O2 -pthread -static -static-libgcc -static-libstdc++ -Isrc src/engine.cpp tests/checkpoint_lock_test.cpp -o dist/passgen-checkpoint-lock-tests.exe
"$compiler" -std=c++17 -O2 -static -Isrc tests/checkpoint_retry_test.cpp -o dist/passgen-checkpoint-retry-tests.exe
