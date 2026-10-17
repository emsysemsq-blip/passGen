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
