# v0.1.15 resource overhead

- Final native resource-engine suite: 286 checks passed. CPU target 25% measured 25.2%, target 70% measured 69.8% process CPU/wall in a single-core synthetic fixture. Stop under a tiny CPU quota and exact Resume passed.
- Direct prefix-pruning suite passed, including impossible branches, 128-bit ranks and exact multiworker Stop/crash/Resume. CPU traversal was retained unchanged after rejecting an accounting-interval experiment.
- Range comparison: 38 baseline cases passed. Large-space 2^64/65/128 and legacy upgrades passed. Unique output: 16 cases plus 64,512 unique length-10..15 lines passed.
- Windows MinGW builds completed. Wine GUI smoke and 62 engine checks passed before the timer-only addition. Final Wine GUI smoke passed; final Wine resource suite passed 286 checks (25% target:21.1%,70% target:44.0%). Wine does not establish native Windows timer performance. No native Windows or physical GPU speed measurement.
- Windows workers reuse a high-resolution waitable timer if available, with standard sleep fallback. The process does not modify the system timer resolution. CPU accounting cadence, target and RAM buffer plan are unchanged.
- Character/legacy checkpoint writes use existing bounded aggregation; first/final/Stop/quota commits remain mandatory. Output-before-checkpoint ordering and crash-tail replay are unchanged.
- Instrumented sparse synthetic character job: fsync calls reduced from 51 to 12, checkpoint renames from 17 to 4, with the same 16,777,216 covered ranks and empty valid output. See checkpoint-syscalls.json.
- Final automatic-resource wall-time comparison was mixed: some cases slower, some faster. These measurements do not demonstrate a general throughput increase.
- Reproduce native automatic-70% comparisons with tests/resource_speed_benchmark.py; see benchmarks/v0.1.15/resource-comparison.json. Short cloud filesystem timings vary and are not a universal speed guarantee.

# v0.1.14 input keyboard regression

Windows GUI rebuilt with MinGW. Full GUI smoke suite passed under Wine, including queued Enter keydown/up through the application dialog loop, insertion of CRLF, subsequent typing on the new line, two refreshed token-table rows and no generation triggered by Enter. This is Wine validation, not a native Windows run. Engine behavior and checkpoint schema are unchanged.

# v0.1.13 validation

- Native engine62 and resource-engine286 checks passed. Actual OpenCL backend76 and engine integration70 checks passed using the explicit PoCL CPU test device. Production does not select that CPU as a GPU; no physical GPU was available.
- All70 independent capped-token ordered-output/crash-tail oracles passed both normally and with AddressSanitizer/UndefinedBehaviorSanitizer, without suppressions/preload. Existing CLI suites passed38 baseline comparisons,16 word/rule/crash cases, the64,512-row unique range and exact2^64/65/128 tails.
- Expanded direct-traversal tests passed with ASAN/UBSAN and under Wine: huge impossible branches still construct zero leaves; first-commit Stop/crash/Resume remains byte-exact; proven-legal fast output equals the reference262,144-row file with correct constructed counters; a constrained128-bit resumed cursor constructs exactly the final10 expected combinations.
- Windows x64 engine62, resource-engine286, direct traversal, GUI integration and actual checkpoint-lock/recovery tests passed under Wine. Custom-word table, scrolling, caps, Unicode and Resume checks remain enabled. This is not native Windows throughput or antivirus validation.
- Three alternating repetitions across seven native CPU workloads matched by independently expected line count, byte count and SHA256. Full-process median ratios: word characters2.93x, capped2.00x, mixed-width words7.22x, uniform words1.98x, sparse caps1.96x, wide-rank tail1.93x. See paired-comparison.json/BENCHMARKS.md for samples. No universal/user-machine speedup is claimed.
- Unrestricted flat dispatch requires fixed-width/prefix-free tokens, no token caps or adjacent-repeat rule, uniform scalar widths when needed, and required classes satisfied by affixes or every token. Thus every generated leaf is legal; constrained cases retain prefix pruning. Prefix-free constrained leaves skip redundant canonicalization only after branch caps/rules are checked.
- Global ranks remain arbitrary precision. Each worker narrows to a bounded prefix block, then uses uint64 offsets inside recursion. The wide saved-rank test checks local-offset clipping, prefix cap counts and exact global cursor restoration.
- Final scalar-length buffer bounds use the maximum ceiling(bytes/scalars) of the valid UTF-8 pool and charge affixes exactly. A low-RAM fixture with an unusable4096-character word and one short token produces the sole valid128-character output within its planned buffer; ASAN/UBSAN passes.
- Sparse direct jobs write/release each bounded text buffer but aggregate checkpoint metadata. First batch, completion, quota, Stop and final loop exit force output flush before saving the cursor. Additional thresholds are512KiB,16,384 constructed combinations,256 waves or50ms, checked between waves. These are not hard real-time deadlines. Exceptions leave the previous durable checkpoint valid; Resume discards/replays an unsaved tail. Progress counters only advance after successful checkpoint save.
- PASSGEN_7 remains unchanged: existing v0.1.12 jobs can Resume directly with the optimized implementation. Independent GUI/CLI MinGW rebuilds were byte-identical, timestamp0/AMD64/unsigned, GUI version0.1.13.0/asInvoker. See build-verification.json. Native MSVC/PowerShell and physical Windows remain unexecuted here.

Historical validation follows.

# v0.1.12 validation

- Native engine 62 and resource-engine 286 checks passed. Capped quota/replay fixtures now explicitly use direct traversal. Existing CLI suites passed 70 independent capped-token ordered oracles/crash-tail resumes, all 16 word/rule cases, 38 baseline range comparisons, the 64,512-line unique 10..15 range, and exact tails ending at 2^64/65/128.
- New direct-traversal suite passed: lengths1..128 over two maximum-once tokens construct exactly four feasible combinations while covering a raw bound of 2^129-2. Impossible capped length128 and impossible required-class length128 each construct zero complete combinations. A nonempty multiworker Stop, appended crash tail and Resume produce the exact uninterrupted 25-line oracle with no lost or duplicate rows.
- AddressSanitizer/UndefinedBehaviorSanitizer passed the direct-traversal suite and all 70 independent capped-token cases/resumes, without suppressions or preload.
- Actual OpenCL backend 76 and engine integration 70 checks passed on the explicit PoCL CPU test device. Word traversal uses CPU; legacy/character GPU tests retain their previous backend. No physical GPU or native Windows throughput validation was available.
- Windows MinGW build passed. Under Wine: engine62, resource-engine286, direct pruning/Stop/crash/Resume, GUI integration and actual temporary/persistent checkpoint-lock tests passed. The final GUI smoke also selects and edits the last custom word after scrolling, verifying that its dropdown remains visible.
- GUI regression uses synthetic multiline number/name entries, not personal data from the user's screenshot: default Whole words, live table replacement even without edit notifications, stale-table clearing on invalid character-mode text, and a custom last-word maximum producing exactly41 of42 one-/two-word combinations. Existing Unicode, manual-symbol, duplicate, preview, large-rank, Stop/Resume and saved-cap cases still pass.
- Prefix feasibility uses cap/adjacency checks, exact reachable scalar-length tables and safe ASCII-class over-approximations. Only feasible complete combinations reach exact ambiguous-word canonicalization. The direct raw-rank cursor skips impossible intervals before dispatch and preserves order and quota boundaries.
- Covered ranks include skipped subtrees. Constructed-combination counters describe committed ranges of the current run, not skipped ranks or speculative worker results discarded on Stop/quota. The GUI does not turn a huge skipped-rank interval into a generated-password throughput claim.
- PASSGEN_7 persists the direct-combination policy. Schemas1..6 retain their saved legacy policy, including schema6 caps; use a new output to enable direct traversal on an old job. This policy avoids silently changing saved execution preferences.
- Independent second GUI/CLI builds were fully byte-identical. PE inspection confirms AMD64, timestamp0, unsigned executables and GUI version0.1.12.0/asInvoker. See benchmarks/v0.1.12/build-verification.json. MSVC/PowerShell scripts were updated but remain unexecuted here.
- No sustained speedup, physical-GPU, native Windows or Defender-clearance claim. Pruning benefits workloads with impossible branches; unconstrained output can still be huge and traversal may cost more than the old unrestricted renderer.

Reproduce with scripts/build-linux.sh, tests/token_limits_test.py and tests/direct_combinations_test.cpp. Windows scripts also build the direct-traversal executable. Native Windows and physical GPU behavior still require their actual platforms.

Historical validation follows.

# v0.1.11 validation

- Native Linux engine: 62 checks passed. Resource engine: 286 checks passed, including new exact capped-token disk-quota prefixes and partial Resume for both prefix-free and ambiguous word pools.
- New independent Python oracle: 70 exhaustive cases passed, with exact ordered output and crash-tail Resume. Covers random ambiguous word pools, ASCII/Persian/emoji, per-token maxima, optional absence, unlimited tokens, limit 10, final-character/token lengths, adjacent-token rules and affixes. Invalid caps and absent-token limits are rejected before output creation.
- The same 70 independent cases and resumes passed under AddressSanitizer/UndefinedBehaviorSanitizer, without suppressions or preload.
- Actual OpenCL backend 76 checks and hybrid integration 70 checks passed using PoCL's explicitly allowed CPU test device. Uncapped jobs retain the GPU path; capped jobs report CPU selection. This is not a physical GPU performance test.
- Windows x64 binaries cross-compiled. Under Wine, engine 62 and resource-engine 286 checks passed, together with actual checkpoint-lock/recovery tests. GUI integration passed the existing Unicode, Stop/Resume and dedup cases plus the new 11-choice token dropdown, maximum-two output, unlimited alternative token, and Resume preserving saved caps after form edits. Native Windows remains untested.
- Existing independent CLI suites passed all 16 word/rule/crash-replay cases, the 64,512-row unique 10..15 range, legacy checkpoint upgrades, and exact tails ending at 2^64/65/128.
- Independent second GUI and CLI MinGW builds were byte-identical. Inspection confirms AMD64, timestamp zero, no Authenticode certificate, and GUI version 0.1.11.0/asInvoker. See benchmarks/v0.1.11/build-verification.json. The MSVC/PowerShell paths remain unexecuted here.
- PASSGEN_6 stores per-token limits. Schemas 1..5 load with unlimited token repetition and preserve their other saved settings. Caps count generated token selections, not substrings inside other words or affix text. Exact deduplication chooses the first feasible tokenization under the saved caps.
- No new throughput or Defender-clearance claim. Ambiguous capped words may require more CPU work than unrestricted characters; the unlimited fast path remains enabled. No physical GPU or native Windows validation was available.

Reproduce: build normally, then run `python3 tests/token_limits_test.py ./build/passgen-cli`. Sanitized builds must link src/engine.cpp, src/gpu.cpp and src/cli.cpp with `-fsanitize=address,undefined -fno-omit-frame-pointer -pthread -ldl`.

Historical validation follows.

# v0.1.10 validation

- Native Linux: 62 engine checks and 267 resource-engine checks passed. Resource discovery/planning and checkpoint retry suites passed. The latest one-core pacing fixture measured 25.56% and 69.69% process CPU/wall for 25% and 70% targets; these are approximate worker-generation controls.
- Real OpenCL execution: 76 backend/discovery checks and 70 integrated engine checks passed on PoCL 6 / LLVM 18, using the explicit CPU-device test hook. Production discovery excludes CPU OpenCL devices. No physical GPU was available; these results do not measure GPU speed or RTX 4090 compatibility.
- Integrated oracles cover ordered CPU parity, ASCII requirements, affixes, Persian and emoji UTF-8, mandatory deduplication, unsupported-input fallback, transfer-inclusive Auto calibration, exact filtered disk-quota boundaries, nonempty Stop/Resume, crash-tail replay, a failed GPU batch replayed by CPU, and final ranks ending at 2^128.
- AddressSanitizer/UndefinedBehaviorSanitizer passed all 76 backend and 70 integration checks without suppressions or preload. Repeated discovery (100 iterations) and OpenCL create/generate/destroy (10 iterations) also passed. The OpenCL loader is cached once per process; per-job queues, events, buffers, kernel, program and context are released. This avoids stranding the ICD loader's process-global vendor tables on repeated unloading. Installing or changing a driver requires restarting the application.
- Review corrected calibration CPU-buffer sizing and reserved the GPU aggregate once to avoid transient string growth beyond its assigned buffer budget. Stopping during calibration does not report a false output mismatch or commit probe output.
- Windows x64 GUI, CLI and test executables cross-compiled. Under Wine, engine 62 and resource-engine 267 checks passed, as did actual GUI generation/Stop/Resume and temporary/persistent checkpoint-lock recovery. Windows GPU tests explicitly skipped real-kernel execution because no usable OpenCL GPU was exposed; each passed its two fallback/discovery checks. Wine utilization and functional results are not native Windows GPU measurements.
- Existing independent CLI suites passed: 16 word/rule/crash-replay cases, six extra-input oracles, huge-rank and legacy-checkpoint cases, and 38 byte-exact baseline range comparisons with varied worker counts and boundaries through 2^128.
- Independent second MinGW builds of the GUI and CLI were fully byte-identical. PE inspection confirmed AMD64, link timestamp zero, no Authenticode certificate, and GUI version 0.1.10.0 with an asInvoker manifest. Artifact hashes and inspection results are in benchmarks/v0.1.10/build-verification.json. The separate MSVC/PowerShell paths remain unexecuted in this environment.
- PASSGEN_5 persists the compute selection. Schemas 1..4 retain their previous CPU execution policy. Unsupported inputs or reportable GPU failures use CPU; a failed GPU range is replayed before checkpoint advancement. This does not guarantee recovery from an operating-system or vendor-driver crash.
- One GPU lane can run beside the CPU workers. Bounded waves wait for CPU results, so GPU utilization and whole-job speedup are workload-dependent. No new physical-GPU, native-Windows or billion-line performance result is claimed; historical CPU benchmarks remain labelled by version.
- The user reported no Defender alert for v0.1.9 on their endpoint. v0.1.10 is unsigned and has no native Defender scan or Microsoft review verdict here; see SECURITY-NOTICE.md.

Reproduce the OpenCL checks with scripts/setup-opencl-tests.sh and the commands in README.md. A test reporting SKIP is not evidence that its kernel ran.

Historical validation follows.

# v0.1.9 validation

- Linux and Windows-under-Wine engine suites passed62checks. Resource-engine suites passed267checks on Linux and Wine, preserving CPU pacing, exact disk quota boundaries and Resume. Native Linux70%sample measured69.86%; Wine timing is not a native Windows utilization claim.
- GUI smoke and real Windows checkpoint-lock/recovery tests passed under Wine with the new embedded common-controls/asInvoker manifest.
- Optimization validation:38 baseline/candidate Unicode, mixed-width, word and rule cases matched byte-for-byte; independent word/dedup/crash-replay and2^64/65/128 tail suites passed. Engine ASAN/UBSAN passed62checks.
- Five alternating normal durable-output runs at abcd length12/workers4/resource0 produced16,777,216 rows and218,103,808bytes each, all with the sameSHA256. Full-process medians0.167432→0.131254seconds (~22% less); Linux cloud-overlay only. See benchmarks/v0.1.9/paired-comparison.json.
- Independent second MinGW builds of the GUI and CLI were entirely byte-identical (not just normalized). PE inspection confirmed linktimestamp0, embedded asInvoker manifest, version0.1.9.0, and no Authenticode certificate. Raw artifact hashes and checks are in benchmarks/v0.1.9/build-verification.json.
- The separately added MSVC x64 script was reviewed but not executed: this Linux environment has neither MSVC nor PowerShell. Distributed executables are MinGW builds, not MSVC builds.
- The user's Defender finding againstv0.1.8 remains unresolved. v0.1.9 has not received native Defender scanning, publisher signing, or Microsoft file-review clearance. Functional/reproducibility tests do not establish antivirus acceptance. See SECURITY-NOTICE.md.
- GPU remains unimplemented. Native Windows throughput and billion-line sustained performance are unverified. No checkpoint schema change beyondv0.1.8's PASSGEN_4.

Historical validation follows.

# v0.1.8 validation

- Linux engine: 62 checks passed. Resource discovery/planning tests passed, including CPU quota parsing, affinity, cgroup memory, overflow and live detection.
- New resource-engine suite: 267 checks passed natively and with AddressSanitizer/UndefinedBehaviorSanitizer. Covers exact disk-byte prefixes below one line, at one line, in a worker and between workers; filters; Unicode byte widths; ambiguous-token uniqueness; repeated small-budget Resume; crash tails; schemas 1..3; missing resources and insufficient-memory refusal before output creation.
- Native CPU measurements use a fixed one-core ResourceSnapshot fixture and 16,777,216 impossible-uppercase candidates. Target25%: process CPU/wall25.44%; target70%:69.82%. Sanitized run measured25.48% and70.34%. These include coordinator/setup CPU and are approximate, not real-time hard caps.
- Live Linux detection: five affinity CPUs, four effective cores from cgroup quota. Auto70 selects three workers with per-workerCPUfraction0.933333, or2.8 cores of generation budget. RAM and disk available quantities are detected at the output location.
- Windows x64 GUI/CLI/tests cross-compiled. Under Wine, core62, resource planning, resource engine267, GUI auto70/default/manual controls, Unicode generation, Stop/Resume and actual checkpoint-lock recovery passed. Wine reported five Windows-visible cores (Linux cgroup quota is not exposed by Wine's Windows APIs); its pacing samples were below target under host contention. No native Windows utilization claim.
- Existing CLI suites passed: 16 word/rule/crash-replay cases, six extra-input oracles, huge-rank/legacy-upgrade tests and 38 exact baseline comparisons with varied workers and boundaries through2^128.
- PASSGEN_4 persists resource percentage. Legacy schemas retain manual policy. Each Resume redetects resources and recalculates70% of current free disk space, so repeated Resume is not a permanent reserve of30% of the initial free space.
- RAM policy budgets output buffers plus a planning allowance; it does not hard-limit total process memory or fill RAM to70%. CPU pacing controls worker generation time and does not control other apps, overall system CPU or disk I/O bandwidth. GPU execution remains unavailable.
- The v0.1.7 speed benchmarks describe manual mode; the new Auto70 intentionally spends less CPU. No new maximum-speed claim or billion-line run was made for this version.

Historical validation follows.

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
# v0.1.16 known parts and independent inputs

- Native Linux build and tests passed: 62 engine checks, fixed-only/placement regressions, checkpoint retries, resource planning, 286 resource-engine checks and direct prefix-pruning/Stop/crash/Resume tests.
- New regressions exercise fixed-only inputs in character/token length modes, direct/legacy policies and all three compute preferences; Unicode, final-length bounds, active class rules, pre-output Stop, crash-tail truncation and idempotent Resume passed. Fixed-only jobs deliberately use CPU.
- MinGW Windows x64 GUI, CLI and test executables built successfully. Expanded GUI smoke tests were compiled, but not executed for this release: virtual display creation was blocked by the environment and the escalation request was cancelled. Native Windows UI, physical GPU and Microsoft Defender were not tested here.
- Actual OpenCL execution was skipped because no usable GPU was detected. CPU fallback tests passed. PASSGEN_7 remains unchanged.

