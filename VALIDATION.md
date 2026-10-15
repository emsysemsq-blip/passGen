# PassGen 0.1.0 — validation

Validated in the current Debian 13 amd64 cloud environment. Native Windows 10/11 execution is **not** verified. No release has been uploaded to GitHub.

## Results

| Check | Result |
|---|---|
| Linux C++17 engine suite | 29 checks passed |
| AddressSanitizer + UndefinedBehaviorSanitizer engine suite | 29 checks passed; no sanitizer findings |
| Windows x64 engine executable under Wine 10 | 29 checks passed |
| Win32 GUI smoke under Wine + Xvfb | Passed preview, exact Unicode output, completed resume, stop, resume, exact 2,097,150-line result and clean close |
| Linux CLI default input `ab` | Exactly `a`, `b`, `aa`, `ab`, `ba`, `bb` |
| Linux CLI one-million-candidate run | Exactly 1,000,000 ordered six-digit lines, 7,000,000 bytes; every line independently checked |
| Windows CLI under Wine | Passed Unicode arguments, Unicode output path and exact content |
| Windows imports | Only Windows system DLLs; C++/thread runtime linked statically |
| Cloud setup script | Executed successfully; Linux tests and Windows cross-compilation succeeded |
| Native Windows PowerShell build and physical Windows UI | Unrun |
| Billion-candidate benchmark, GPU, power-loss testing | Unrun / GPU not implemented |

The engine suite covers character deduplication, indexed count/order, deterministic parallel output, adjacent-token filtering, Unicode paths/content, word concatenation, all ASCII class rules, prefix rules, initial and nonempty checkpoint recovery, removal of uncommitted tails, idempotent completed resume, overwrite refusal, corrupt checkpoints, modified/truncated output, index overflow and invalid Unicode.

The one-million-candidate Linux CLI smoke measured about 0.052 s wall time and 8,576 KiB maximum resident memory for that process in this environment. It used four workers, tiny six-byte candidates, warm binaries and the cloud filesystem. This is **not** a Windows throughput guarantee, sustained physical-disk benchmark or comparison with specialist tools. Durability calls were enabled. Longer tokens, filters, worker counts and storage latency change throughput.

## Reproduce

```sh
bash scripts/build-linux.sh
PATH=/workspace/toolchain/root/usr/bin:$PATH \
CXX_WINDOWS=/workspace/toolchain/root/usr/bin/x86_64-w64-mingw32-g++-posix \
bash scripts/build-windows.sh

g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread -Isrc \
  src/engine.cpp tests/engine_test.cpp -o build/passgen-tests-asan
./build/passgen-tests-asan
```

On Windows, from `dist`, run `passgen-tests.exe` and `passgen-gui-smoke.exe` (the latter opens the application and creates temporary output). The GUI test checks actual controls and resulting files, rather than just whether a window opens. To retain the window briefly for a screenshot, pass any argument to the smoke test.

Optional Linux compatibility check with an initialized Wine installation and display:

```sh
cd dist
wine passgen-tests.exe
wine passgen-gui-smoke.exe
```

For this sandbox, Debian Wine packages were extracted outside the repository. Relocated Wine needed its server/data paths corrected and its packaged `zlib1.dll` available in the test prefix. A fresh prefix then ran the engine and GUI tests. Earlier failures were Wine bootstrap failures, not passing application checks. A nonfatal Wine bootstrap warning about the unavailable 32-bit `rundll32.exe` occurred; the selected 64-bit engine suite completed with exit status 0. These Wine dependencies are optional and are not bundled with the Windows application.

The screenshot is captured from the running GUI in Wine/Xvfb, not a mockup. It shows a completed resumed job and a preview for a new input.
