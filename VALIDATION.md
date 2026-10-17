# v0.1.7 validation

- Linux engine suite: 62 checks passed; same 62 checks passed with AddressSanitizer and UndefinedBehaviorSanitizer.
- Windows x64 binaries cross-compiled; Windows engine suite passed 62 checks under Wine. Windows GUI integration and real temporary/persistent checkpoint-lock recovery tests passed under Wine.
- New range regression: 38 exact comparisons against v0.1.6 across Unicode, aliases, rules, affixes, scalar/token lengths and worker counts. Also checks singleton lengths 1..128, 1,048,574-line worker/length boundaries, independently checksummed v3 tails at 2^64/65/128 and completed resume.
- Existing CLI suites passed: 16 word/rule cases and crash replay, exact unique 64,512-row length range 10..15, six extra-input oracles, huge-rank and legacy-checkpoint upgrade tests.
- Performance measurements: all 60 short-workload and 12 larger-workload files matched across versions by SHA256, byte count and line count. Larger workload produced 16,777,216 lines / 208 MiB per run; four-worker median fell from 2.1727 s to 0.1638 s (13.26x). See BENCHMARKS.md and benchmarks/v0.1.7 for scope and raw measurements.
- Performance tests used native Linux CLI with four-core quota and cloud overlay storage. GPU was not exposed; GPU execution is not implemented. These are not native Windows or physical SSD results; billion-line sustained throughput is unverified.
- Worker output buffers are bounded at about 4 MiB each; exact canonical deduplication, raw candidate rank order and durable output-before-checkpoint commit order remain enabled. Stop waits for the current bounded batch; a crash replays uncommitted data from the last checkpoint.
- GUI rates use starting rank/bytes loaded from the checkpoint and actual completion duration; status includes MiB/sec. Completed Resume does not count earlier work as new throughput.
- No checkpoint schema or runtime dependency change. Native Windows 10/11 and user-system performance remain unverified.

Historical validation follows.

# v0.1.6 validation

- Linux engine: 62 checks passed; Windows engine under Wine: 62 checks passed.
- Six independent CLI set oracles passed, including all 32 mapped Persian letter keys compared with the committed Unicode CLDR Windows Persian reference, exact uniqueness and idempotent completed resume.
- Windows GUI integration passed in Wine: combined Persian/emoji/Shift alphabet has 65 unique tokens, emits Persian Shift marks and intact emoji sequences, and passes the prior symbol, Unicode, preview and stop/resume checks.
- Windows x64 executables successfully cross-compiled. No new runtime dependency or checkpoint schema change.
- Shift now means Windows Persian (00000429), not English uppercase or Persian (Standard). Each original token remains and a fully Persian-shifted variant is added; nonmapped scalar values remain unchanged. This mapping is explicit and independent of the host keyboard layout.
- Existing checkpoints preserve their saved alphabet, including old uppercase variants. Start a new output to apply the corrected mapping.
- Reference: Unicode CLDR release-32 keyboards/windows/fa-t-k0-windows.xml; fixture and Unicode license are included in tests/fixtures.

Native Windows 10/11 and live keyboard comparison on native Windows remain unverified. Wine is a compatibility test environment.

Historical validation follows.

# v0.1.5 validation

- Linux engine suite: 61 checks passed, including complete emoji sequence preservation, Persian alphabet uniqueness, uppercase word variants, emoji aliases and checkpoint replay with Unicode scalar lengths.
- CLI: 5 independent exhaustive input-pool set oracles passed with no duplicate rows and unchanged completed resume (tests/extra_inputs_test.py).
- Windows x64 GUI, CLI and test executables cross-compiled successfully using the existing MinGW toolchain.
- Wine: Windows engine suite passed all 61 checks. GUI integration passed, including the new combined pool (37 unique tokens), complete ZWJ output, Persian letters and uppercase variants, plus existing symbols, large-space preview and stop/resume tests.
- Default lengths count Unicode scalars, including ZWJ and modifiers. Whitespace-delimited manual Emoji entries remain complete generation tokens; token-length mode treats each as one unit. Emoji input is literal and does not enforce Unicode emoji categories.
- Shift + letter adds ASCII uppercase counterparts (whole-word uppercase in word mode), not a Persian keyboard Shift mapping. Persian preset contains 32 letters plus آ. Exact line deduplication remains mandatory.
- Existing PASSGEN_3 checkpoints persist the merged alphabet; Resume ignores current form edits.

Native Windows 10/11, font rendering on the user's machine and billion-candidate throughput have not been verified. Wine checks are compatibility tests, not native Windows testing.

Previous validation follows for historical context.

# PassGen 0.1.4 — validation

The Symbols checkbox is replaced by a manual text field. The user's explicit choice is implemented: typed symbols extend the input alphabet and are optional in each password. There is no new required-symbol rule or global punctuation expansion.

## Verified

- **54 C++ engine checks passed on Linux and on the Windows executable under Wine.** New checks cover exact/deduplicated symbol merging, empty-field no-op, ordinary passwords remaining valid, complete optional-symbol output, input preservation on rejected text, Unicode symbols, stored-alphabet resume and the original required-ASCII-symbol rule in legacy checkpoints.
- **54 engine checks passed with AddressSanitizer and UndefinedBehaviorSanitizer**, with no reported findings.
- **CLI integration passed on Linux and Windows/Wine:** all 20 ordered combinations of `ab!@` at lengths 1..2; repeated entered symbols; input/symbol overlap; empty symbol argument; Unicode `€` and `؟`; symbols-only input; mixed words `a aa` with `!`; saved-alphabet resume ignoring newly supplied symbols; and invalid input rejected before creating an output file. Outputs were exact, unique and included passwords without symbols.
- **Actual Windows GUI integration passed under Wine/Xvfb:** the new edit field accepted `!@!!`, preview counted 20 raw combinations, and the generated output matched all 20 expected lines, including `a`, `b`, `aa` and `ab`. Editing the field to `$` and choosing Resume preserved the original completed file. Mixed word input with `!` produced exactly `a`, `!`, `aa`, `a!`, `!a`, `!!`.
- The GUI suite also retained Unicode output, unique word aliases, prefix/suffix length counting, later-rank preview, large alphabet counts, stop/resume with the exact 2,097,150-line character output and clean close.
- Linux checkpoint retry policy tests continued to pass. The actual checkpoint replacement/generation code is unchanged in this release.

## Persistence and compatibility

No checkpoint schema change was needed. New jobs save the complete merged token alphabet in the existing PASSGEN_3 format. Resume continues the saved alphabet and rules rather than applying edits in the current form. A legacy checkpoint that required an ASCII symbol retains that requirement; the new manual field does not enable it for new jobs. The legacy CLI `--symbol` flag is kept separately for compatibility.

Input punctuation and prefix/suffix text explicitly provided by the user remain part of the input. The manual field does not silently remove them. Only typed extra symbol characters are added, with no automatic ASCII-symbol collection. Characters are literal Unicode scalars, not regex syntax; there is no Unicode category normalization.

Native Windows 10/11, actual antivirus behavior, power-loss durability and competitive/billion-candidate throughput remain unverified. The UI tests exercise Wine's Windows compatibility implementation, not a physical Windows machine.

## Reproduce

```sh
bash scripts/build-linux.sh
python3 tests/manual_symbols_test.py ./build/passgen-cli

g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread -Isrc \
  src/engine.cpp tests/engine_test.cpp -o build/passgen-tests-asan
./build/passgen-tests-asan

PATH=/workspace/toolchain/root/usr/bin:$PATH \
CXX_WINDOWS=/workspace/toolchain/root/usr/bin/x86_64-w64-mingw32-g++-posix \
bash scripts/build-windows.sh
```

With initialized Wine and a display, run `passgen-tests.exe` and `passgen-gui-smoke.exe` from `dist`. From the repository root:

```sh
python3 tests/manual_symbols_test.py --windows /path/to/wine64 ./dist/passgen-cli.exe
```

The CLI and GUI tests use their own temporary output files, and clean them after success. The native PowerShell build/test script remains unverified on physical Windows.
