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
