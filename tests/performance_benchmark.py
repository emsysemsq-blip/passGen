#!/usr/bin/env python3
"""Measure durable CLI wordlist generation and compare output bytes.

Run the baseline before replacing/rebuilding an executable:
  python3 tests/performance_benchmark.py --mode baseline \
    --baseline build/perf/passgen-v0.1.6-cli --output build/perf/baseline.json
Then compare without rerunning those samples:
  python3 tests/performance_benchmark.py --mode candidate \
    --candidate build/passgen-cli --baseline-results build/perf/baseline.json \
    --output build/perf/comparison.json

Each invocation is a fresh job, including output fsync and checkpoints. Hashing,
line counting, and deleting the temporary files happen outside the timed region.
No CPU affinity, OS cache clearing, or background workload control is applied.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import tempfile
import time


CASES = [
    {"name": "ascii_binary_20", "args": ["--chars", "ab", "--min", "20", "--max", "20"], "expected_lines": 2 ** 20},
    {"name": "ascii_four_10", "args": ["--chars", "abcd", "--min", "10", "--max", "10"], "expected_lines": 4 ** 10},
    {"name": "ascii_filtered_10", "args": ["--chars", "aA1!", "--min", "10", "--max", "10", "--lower", "--upper", "--digit", "--symbol"], "expected_lines": 4 ** 10 - 4 * 3 ** 10 + 6 * 2 ** 10 - 4},
    {"name": "persian_four_9", "args": ["--chars", "ابپت", "--min", "9", "--max", "9"], "expected_lines": 4 ** 9},
    {"name": "ambiguous_words_10", "args": ["--words", "a aa b", "--min", "10", "--max", "10", "--token-lengths"], "expected_lines": None},
]
SUSTAINED_CASES = [
    {"name": "ascii_four_12_sustained", "args": ["--chars", "abcd", "--min", "12", "--max", "12"], "expected_lines": 4 ** 12},
]
RESULT = re.compile(r"^Complete: scanned=(\d+) written=(\d+) bytes=(\d+) seconds=([0-9.eE+\-]+)$", re.MULTILINE)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def machine():
    result = {"platform": platform.platform(), "python": platform.python_version(), "logical_cpus": os.cpu_count()}
    try:
        result["affinity_cpus"] = len(os.sched_getaffinity(0))
    except AttributeError:
        pass
    for source in ("/sys/fs/cgroup/cpu.max", "/sys/fs/cgroup/cpu/cpu.cfs_quota_us"):
        try:
            value = Path(source).read_text().strip()
            result[source] = value
            if source.endswith("cpu.max"):
                quota, period = value.split()
                if quota != "max":
                    result["cpu_quota_cores"] = int(quota) / int(period)
        except OSError:
            pass
    try:
        result["cpu_model"] = next(line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines() if line.startswith("model name"))
    except (OSError, StopIteration):
        pass
    return result


def inspect_output(path):
    digest = hashlib.sha256()
    lines = size = 0
    with open(path, "rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
            lines += chunk.count(b"\n")
            size += len(chunk)
    return {"sha256": digest.hexdigest(), "lines": lines, "bytes": size}


def run_suite(executable, repeats, threads, cases, working_directory, save):
    executable = Path(executable).resolve(strict=True)
    before_hash = sha256_file(executable)
    suite = {"executable": str(executable), "executable_sha256": before_hash, "repeats": repeats, "results": []}
    # Run one process at a time so these benchmark jobs do not compete with each other.
    for case in cases:
        for workers in threads:
            samples = []
            identity = None
            for repeat in range(repeats):
                with tempfile.TemporaryDirectory(prefix="passgen-perf-", dir=working_directory) as directory:
                    output = Path(directory) / "wordlist.txt"
                    command = [str(executable), *case["args"], "--threads", str(workers), "--out", str(output)]
                    began = time.perf_counter()
                    completed = subprocess.run(command, text=True, encoding="utf-8", capture_output=True, check=True)
                    wall = time.perf_counter() - began
                    match = RESULT.search(completed.stdout)
                    if not match:
                        raise RuntimeError(f"Missing completion statistics: {completed.stdout!r}, {completed.stderr!r}")
                    scanned, written, byte_count = map(int, match.group(1, 2, 3))
                    cli_seconds = float(match.group(4))
                    actual = inspect_output(output)
                    if actual["lines"] != written or actual["bytes"] != byte_count:
                        raise AssertionError(f"CLI statistics disagree with actual file: {case['name']}")
                    if case["expected_lines"] is not None and written != case["expected_lines"]:
                        raise AssertionError(f"Unexpected line count: {case['name']}: {written}")
                    if identity is not None and actual != identity:
                        raise AssertionError(f"Nondeterministic output: {case['name']}, threads={workers}")
                    identity = actual
                    samples.append({"wall_seconds": wall, "cli_seconds": cli_seconds, "scanned": scanned,
                                    "lines_per_second": written / wall, "mib_per_second": byte_count / 1048576 / wall})
                    print(f"{executable.name}: {case['name']} threads={workers} sample={repeat + 1}/{repeats} "
                          f"wall={wall:.4f}s cli={cli_seconds:.4f}s lines/s={written / wall:,.0f}", flush=True)
            result = {"case": case["name"], "args": case["args"], "threads": workers, **identity, "samples": samples,
                      "median_wall_seconds": statistics.median(sample["wall_seconds"] for sample in samples),
                      "median_cli_seconds": statistics.median(sample["cli_seconds"] for sample in samples),
                      "median_lines_per_second": statistics.median(sample["lines_per_second"] for sample in samples),
                      "median_mib_per_second": statistics.median(sample["mib_per_second"] for sample in samples)}
            suite["results"].append(result)
            save(suite)
    if sha256_file(executable) != before_hash:
        raise RuntimeError(f"Executable changed during benchmark: {executable}")
    # Worker count changes must also produce exactly the same ordered bytes.
    for case in cases:
        cases = [result for result in suite["results"] if result["case"] == case["name"]]
        if len({result["sha256"] for result in cases}) != 1:
            raise AssertionError(f"Different output across worker counts: {case['name']}")
    return suite


def compare(baseline, candidate):
    previous = {(result["case"], result["threads"]): result for result in baseline["results"]}
    compared = []
    for result in candidate["results"]:
        key = result["case"], result["threads"]
        if key not in previous:
            raise ValueError(f"Baseline does not contain {key}")
        old = previous[key]
        if result["args"] != old["args"]:
            raise AssertionError(f"Different benchmark arguments for {key}")
        for field in ("sha256", "lines", "bytes"):
            if result[field] != old[field]:
                raise AssertionError(f"Baseline/candidate {field} mismatch for {key}")
        speedup = old["median_wall_seconds"] / result["median_wall_seconds"]
        compared.append({"case": key[0], "threads": key[1], "identical_bytes": True, "wall_speedup": speedup,
                         "baseline_median_seconds": old["median_wall_seconds"],
                         "candidate_median_seconds": result["median_wall_seconds"],
                         "candidate_lines_per_second": result["median_lines_per_second"],
                         "candidate_mib_per_second": result["median_mib_per_second"]})
        print(f"COMPARE {key[0]} threads={key[1]}: {speedup:.2f}x "
              f"({old['median_wall_seconds']:.4f}s -> {result['median_wall_seconds']:.4f}s), identical bytes", flush=True)
    return compared


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline", default="build/perf/passgen-v0.1.6-cli")
    parser.add_argument("--candidate", default="build/passgen-cli")
    parser.add_argument("--baseline-results", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/perf/comparison.json"))
    parser.add_argument("--work-dir", type=Path, default=Path("build/perf"))
    parser.add_argument("--mode", choices=("baseline", "candidate", "both"), default="both")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--threads", default="1,4")
    parser.add_argument("--sustained", action="store_true", help="Use a 16,777,216-row ASCII workload instead of the five default cases")
    args = parser.parse_args()
    if args.repeats < 3:
        parser.error("Use at least three repetitions for a meaningful median")
    threads = [int(value) for value in args.threads.split(",")]
    if not threads or any(value < 1 or value > 32 for value in threads) or len(set(threads)) != len(threads):
        parser.error("--threads requires distinct comma-separated worker counts between 1 and 32")
    if args.mode == "candidate" and not args.baseline_results:
        parser.error("--mode candidate requires --baseline-results")
    args.work_dir.mkdir(parents=True, exist_ok=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    cases = SUSTAINED_CASES if args.sustained else CASES
    report = {"created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(), "machine": machine(),
              "timing_scope": "Fresh CLI process through exit, durable file and checkpoints; output inspection excluded", "cases": cases}

    def write():
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def save_stage(name):
        def save(suite):
            report[name] = suite
            write()
        return save

    if args.mode in ("baseline", "both"):
        report["baseline"] = run_suite(args.baseline, args.repeats, threads, cases, args.work_dir, save_stage("baseline"))
    else:
        previous = json.loads(args.baseline_results.read_text(encoding="utf-8"))
        report["baseline"] = previous["baseline"]
        report["baseline_machine"] = previous["machine"]
        report["baseline_created_utc"] = previous["created_utc"]
    if args.mode in ("candidate", "both"):
        report["candidate"] = run_suite(args.candidate, args.repeats, threads, cases, args.work_dir, save_stage("candidate"))
        report["comparison"] = compare(report["baseline"], report["candidate"])
    write()
    print(f"Saved {args.output}", flush=True)


if __name__ == "__main__":
    main()
