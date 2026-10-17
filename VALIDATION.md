# PassGen 0.1.1 — validation

This release fixes the reported `Search space exceeds 64-bit indexing` error.
Validated in Debian 13 amd64 and Wine 10 with Xvfb. Native Windows 10/11 execution remains unverified.

| Check | Result |
|---|---|
| Linux engine suite | 36 checks passed |
| AddressSanitizer + UndefinedBehaviorSanitizer | 36 checks passed; no findings |
| Windows x64 engine suite under Wine | 36 checks passed |
| GUI integration under Wine | Passed exact default-alphabet large count, Unicode output, stop, resume, exact 2,097,150-line output and clean close |
| C++ rank arithmetic against independent Python big integers | 1,021 cases passed, including 64-bit boundaries and values up to 2,199 bits |
| CLI large-space generation/resume on Linux and Windows EXE under Wine | Exact last three candidates of spaces ending at 2^64, 2^65, 2^128; checkpoint positions retained without narrowing; completed resume idempotent |
| Legacy checkpoint migration on both CLI targets | PASSGEN_1 read correctly, remaining line generated, checkpoint upgraded to PASSGEN_2 |

The regression tests use controlled checkpoint positions near the end of each large space. They exercise actual generation, file output, large-rank serialization and resume, without claiming to enumerate those entire spaces. Direct tests also verify the candidate at rank 2^64 and startup/stop of a new job with 2^128 candidates.

Existing checks continue to cover exact order/count, word and Unicode input, ASCII rules, parallel output determinism, corruption/truncation detection and recovery from uncommitted output tails. Candidate positions and search-space totals are arbitrary-precision; actual output byte offsets remain limited by the operating system and storage.

## Reproduce

```sh
bash scripts/build-linux.sh
python3 tests/large_space_test.py ./build/passgen-cli

g++ -std=c++17 -O2 -Isrc tests/index_oracle.cpp -o build/index-oracle
python3 tests/index_oracle_test.py ./build/index-oracle

g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread -Isrc \
  src/engine.cpp tests/engine_test.cpp -o build/passgen-tests-asan
./build/passgen-tests-asan

PATH=/workspace/toolchain/root/usr/bin:$PATH \
CXX_WINDOWS=/workspace/toolchain/root/usr/bin/x86_64-w64-mingw32-g++-posix \
bash scripts/build-windows.sh
```

With an initialized Wine and display, run `passgen-tests.exe` and `passgen-gui-smoke.exe` from `dist`. The CLI regression also accepts a Windows loader:

```sh
python3 tests/large_space_test.py --windows /path/to/wine64 ./dist/passgen-cli.exe
```

No native Windows throughput, full billion-candidate run or power-loss test was performed for this release. Accepting a very large finite search space does not make its full output practical to generate or store. No external big-integer library/runtime is needed.
