# v0.1.15 resource overhead

See benchmarks/v0.1.15/resource-comparison.json for alternating three-run automatic-70% comparisons with byte/line/SHA256 parity. Results are mixed; no general throughput improvement is established on this cloud filesystem. The word traversal and RAM plan are unchanged. A separate instrumented synthetic sparse-character job reduced fsync calls from 51 to 12 and checkpoint replacements from 17 to 4 (checkpoint-syscalls.json). Windows high-resolution timers require native Windows measurement; Wine functional tests do not establish speed.

# PassGen v0.1.13 speed regression fix

Three alternating baseline/candidate repetitions, four CPU workers, manual resources, fresh normal file output through process exit including durable checkpoint/output writes. Hashing is outside timing. All paired files match by SHA256, byte count and independently expected line count.

| Workload | Lines | v0.1.12 median | v0.1.13 median | Ratio |
| --- | ---: | ---: | ---: | ---: |
| Separate character mode | 1,048,576 | 0.015006 s | 0.011532 s | 1.30x |
| Single-character word tokens | 1,048,576 | 0.032236 s | 0.010992 s | 2.93x |
| Word characters with a maximum-two token | 551,124 | 0.032014 s | 0.016020 s | 2.00x |
| Uniform two-character words | 1,048,576 | 0.033132 s | 0.016747 s | 1.98x |
| Words with mixed scalar lengths | 283,953 | 0.415861 s | 0.057587 s | 7.22x |
| Sparse capped words | 4,129 | 0.020541 s | 0.010500 s | 1.96x |
| Capped tail near rank 4^128 | 551,124 | 0.068516 s | 0.035533 s | 1.93x |

Native Linux cloud-overlay storage, effective four-core quota. Short jobs include process/setup/cache effects. The sparse job's raw space is 4^16; the wide job resumes token length128 at 4^128-4^10 and writes 71,094,996 bytes. No native Windows, user-workload, physical SSD or GPU speed claim. Short-job character timing varies with process/setup and cache effects; these samples do not establish a universal speedup.

Raw observations: benchmarks/v0.1.13/paired-comparison.json. Preserve a v0.1.12 native CLI and run:

```sh
python3 tests/direct_speed_benchmark.py --baseline build/perf/passgen-v0.1.12-cli --candidate build/passgen-cli --output build/perf/direct-speed.json
```

# PassGen v0.1.12 direct word traversal

A functional pruning fixture uses two words, maximum one use of each, token lengths 1..128. Exactly four complete combinations are constructed (`a`, `b`, `ab`, `ba`) while the raw rank bound is 2^129-2. Another fixture at length128 constructs zero leaves when caps or a missing required class make the tree impossible. These establish skipped work, not a sustained throughput or native Windows benchmark.

Covered raw ranks include proven-impossible subtrees; dividing that count by time would be misleading. The GUI word-mode rate counts constructed combinations in committed ranges of the current run. Direct word traversal runs on CPU. If most branches are valid, traversal may cost more than the old unrestricted renderer; no universal speedup is claimed.

# PassGen v0.1.11 repetition caps

No new throughput claim is made. Unlimited jobs retain the unrestricted CPU fast path and existing GPU eligibility. Capped jobs use incremental frequency tracking on CPU; ambiguous words also need cap-aware canonicalization and may be slower. Historical timings below do not measure these new capped workloads.

# PassGen v0.1.10 compute selection

The new Auto mode compares a warmed OpenCL sample, including transfer and compaction, with the CPU renderer. It enables the GPU only if that sample is faster. GPU-preferred mode bypasses that timing choice while retaining correctness checks and CPU fallback.

This environment has no physical GPU. Real OpenCL kernel tests used PoCL on a CPU through an explicit test hook; they establish functional coverage, not GPU acceleration or RTX 4090 throughput. No GPU speedup is claimed for v0.1.10. Hybrid generation uses bounded waves and waits for each wave's CPU results before committing; a faster GPU can be idle during that wait. Storage and checkpoint writes can also limit total speed.

The CPU measurements below are historical results for the named versions and workloads. They do not measure v0.1.10's Auto mode or predict native Windows performance.

Historical measurements follow.

# PassGen v0.1.9 comparison

Uniform-width, prefix-free generation skips redundant scalar/rule work when every output passes those rules. Five alternating repetitions per build, abcd at final length12, workers4, resource-percent0, produced exactly16,777,216 lines and218,103,808bytes per run, with the same SHA256 in all ten runs.

| Timing | v0.1.8 | v0.1.9 |
| --- | ---: | ---: |
| Median full CLI process |0.167432s|0.131254s|
| Median reported run time |0.164275s|0.127783s|

Full-process median decreased21.6%; reported run median decreased22.2%. These are normal durable file writes on the same Linux cloud-overlay environment described below; hashing is outside timing. Native Windows performance and billion-line sustained rates are unverified. CPU70 mode retains pacing and is not the mode measured by this comparison. Raw samples: benchmarks/v0.1.9/paired-comparison.json.

Earlier measurements follow.

# PassGen v0.1.7 performance

The optimized CPU engine is faster than v0.1.6 while producing exactly the same ordered output bytes. GPU execution is not implemented.

## Larger workload

Four ASCII choices (`abcd`), final character length 12: 16,777,216 lines, 218,103,808 bytes (208 MiB). Medians of three fresh, sequential runs for each version/worker count:

| Workers | v0.1.6 | v0.1.7 | Speedup | v0.1.7 lines/second |
| --- | ---: | ---: | ---: | ---: |
| 1 | 6.7681 s | 0.3923 s | 17.25x | 42.77 million |
| 4 | 2.1727 s | 0.1638 s | 13.26x | 102.40 million |

All 12 output files matched by SHA256, byte count and line count. Timing covers a fresh CLI process through exit, including actual normal output writes, fsync/checkpoint flush, checkpoint replacement and worker shutdown. Output hashing occurs outside the measured interval. Files were removed between runs. The test does not use a null sink or disable durability.

Machine: AMD EPYC 9V74 virtual CPU, five logical CPUs exposed, cgroup quota four CPU cores, 32 GiB memory limit, Linux cloud overlay storage. The filesystem/cache behavior is environment-specific; these results are not native Windows or physical SSD/NVMe measurements. The workload is much shorter than a billion-line production job, despite the larger file. The old 50 ms CLI shutdown delay contributes to short-case gains; it does not explain the multi-second baseline result above.

## Other workloads

Three repetitions per worker count (1 and 4). All 60 baseline/candidate outputs were byte-identical. Four-worker speedups in the shorter workloads:

| Workload | Rows | Speedup |
| --- | ---: | ---: |
| Binary choices, length 20 | 1,048,576 | 8.46x |
| Four ASCII choices, length 10 | 1,048,576 | 8.54x |
| Required ASCII classes, length 10 | 818,520 | 9.76x |
| Four Persian choices, length 9 | 262,144 | 4.82x |
| Ambiguous word choices | 12,836 | 1.38x |

Ambiguous words/emoji retain exact canonical-tokenization checks and benefit less. Disk limits, rule complexity, token lengths, hardware, antivirus and other activity can change observed throughput.

## Reproduce

Build v0.1.6 CLI from branch `release/passgen-v0.1.6`, preserve it as `build/perf/passgen-v0.1.6-cli`, then build the current version. Do not benchmark while compiling or running other tests.

```sh
python3 tests/performance_benchmark.py --sustained \
  --baseline build/perf/passgen-v0.1.6-cli --candidate build/passgen-cli \
  --output build/perf/sustained.json
python3 tests/performance_benchmark.py \
  --baseline build/perf/passgen-v0.1.6-cli --candidate build/passgen-cli \
  --output build/perf/comparison.json
python3 tests/range_generation_test.py \
  --baseline build/perf/passgen-v0.1.6-cli --candidate build/passgen-cli
```

Raw observations are saved in `benchmarks/v0.1.7/sustained.json` and `benchmarks/v0.1.7/comparison.json`. Timing uses the normal file-producing CLI; default benchmark commands are native-host commands. The Windows executable is validated separately under Wine, with no Windows speed claim.
