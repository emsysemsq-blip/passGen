# PassGen 0.1.2 — validation

Selected workflow: exhaustive, unique wordlist generation across every selected final-character length, with durable stop/resume. Tested in Debian 13 amd64 and Wine 10/Xvfb. Native Windows 10/11 remains unverified.

## Evidence

- **40 C++ engine checks** passed on Linux and on the Windows executable under Wine. These include deterministic output, Unicode, class rules, large ranks, crash-tail recovery, and nonempty mid-job recovery of ambiguous word input. A resumed ambiguous-word job matched a complete run byte for byte and had no duplicate lines.
- The C++ suite also passed with AddressSanitizer and UndefinedBehaviorSanitizer. The CLI uniqueness and large-rank suites passed with both sanitizers enabled; no findings were reported.
- An independent Python exhaustive oracle checked **16 word/rule cases**: four token sets, character/token length policies, and adjacent-token rejection on/off. Cases include cross-token-count aliases, same-count aliases, input-order permutations and Persian words. Actual files matched the independently enumerated, deduplicated lists exactly.
- Each word/rule case was replayed from its initial checkpoint after appending an uncommitted crash tail. No missing or duplicate output resulted. The separate C++ tests cover recovery from a nonempty committed checkpoint.
- With input `aabb`, final lengths **10 through 15** produced exactly **64,512 unique lines**; all six lengths occurred in ascending order, from `aaaaaaaaaa` through `bbbbbbbbbbbbbbb`. Duplicate input characters did not increase the search alphabet.
- Prefix/suffix Unicode length counting, default word lengths, and safe refusal of an ambiguous old word checkpoint were exercised. Refusal preserved both old files unchanged.
- Large-space generation/resume still produced the exact final three candidates ending at **2^64, 2^65 and 2^128**, with idempotent completed resume. Compatible v0.1.0 checkpoints upgraded to PASSGEN_3 without missing/duplicate lines.
- The actual Windows GUI under Wine passed: unique word aliases, final character lengths including prefix/suffix, large default alphabet preview, exact Unicode output, stop/resume, exact 2,097,150-line character output and clean close.

The release does not claim a full billion-candidate run, physical power-loss test, competitive throughput benchmark or native Windows validation. Very large finite spaces remain constrained by time and storage. Selection is currently limited to lengths 1..128; there is no output-count quota and no 64-bit candidate-count limit.

## Reproduce

```sh
bash scripts/build-linux.sh
python3 tests/unique_output_test.py ./build/passgen-cli
python3 tests/large_space_test.py ./build/passgen-cli

g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread -Isrc \
  src/engine.cpp tests/engine_test.cpp -o build/passgen-tests-asan
./build/passgen-tests-asan

g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread \
  src/engine.cpp src/cli.cpp -o build/passgen-cli-asan
python3 tests/unique_output_test.py ./build/passgen-cli-asan
python3 tests/large_space_test.py ./build/passgen-cli-asan

PATH=/workspace/toolchain/root/usr/bin:$PATH \
CXX_WINDOWS=/workspace/toolchain/root/usr/bin/x86_64-w64-mingw32-g++-posix \
bash scripts/build-windows.sh
```

With a configured Wine and display, run the engine and GUI suites from `dist`:

```sh
wine passgen-tests.exe
wine passgen-gui-smoke.exe
```

The independent CLI oracles also support a Windows loader from the repository root:

```sh
python3 tests/unique_output_test.py --windows /path/to/wine64 ./dist/passgen-cli.exe
python3 tests/large_space_test.py --windows /path/to/wine64 ./dist/passgen-cli.exe
```

## Implementation limits

Uniqueness compares exact UTF-8 text, with no case folding or Unicode normalization. Prefix-free token alphabets (including character inputs) are inherently unique. Ambiguous word alphabets use exact canonical parsing, without a growing set of prior output strings or a probabilistic filter. This avoids history-dependent duplicates after resume but requires extra CPU and temporary per-candidate parsing memory for ambiguous words.

The displayed candidate count is the number of raw ranks scanned, not a promise of that many accepted unique lines. In final-character word mode the engine visits each selected character length separately and filters token sequences to that exact length. Adjacent-token and class rules may further reduce the actual output.

Old checkpoints retain their original token-length semantics. Ambiguous older word checkpoints cannot guarantee that already-written output is unique, so they require a new output file. This release deliberately preserves those existing files rather than rewriting them silently.
