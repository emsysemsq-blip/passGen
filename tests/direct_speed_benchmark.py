#!/usr/bin/env python3
"""Alternating durable-file comparison. Hashing and cleanup are outside timing."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', required=True)
parser.add_argument('--candidate', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--repeats', type=int, default=3)
args = parser.parse_args()
executables = {'baseline': str(Path(args.baseline).resolve()), 'candidate': str(Path(args.candidate).resolve())}
length_counts = [1]+[0]*20
for length in range(1,21):
    length_counts[length] = sum(length_counts[length-width] for width in range(1,5) if width<=length)
cases = [
    ('characters', ['--chars', 'abcd', '--min', '10', '--max', '10'], 4**10),
    ('word_characters', ['--words', 'a b c d', '--min', '10', '--max', '10'], 4**10),
    ('capped_word_characters', ['--words', 'a b c d', '--limit', 'a', '2', '--min', '10', '--max', '10'], sum(math.comb(10, k)*3**(10-k) for k in range(3))),
    ('uniform_words', ['--words', 'aa bb cc dd', '--min', '20', '--max', '20'], 4**10),
    ('mixed_word_widths', ['--words', 'a bb ccc dddd', '--min', '20', '--max', '20'], length_counts[20]),
    ('sparse_capped_words', ['--words', 'a b c d', '--limit', 'a', '1', '--limit', 'b', '1', '--limit', 'c', '1', '--min', '16', '--max', '16'], 4129),
    ('wide_rank_capped_characters', None, sum(math.comb(10, k)*3**(10-k) for k in range(3))),
]
def fingerprint(text):
    value = 14695981039346656037
    for byte in text.encode():
        value = ((value ^ byte)*1099511628211) & (2**64-1)
    return value

report = {'scope': 'Native CLI, four workers, manual resources, CPU; normal durable output and checkpoint writes. Full process timing; hashing excluded. Not native Windows or physical GPU throughput.',
          'executables': {label: {'path': path, 'sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest()} for label, path in executables.items()}, 'cases': []}
with tempfile.TemporaryDirectory(prefix='passgen-direct-speed-') as folder:
    root = Path(folder)
    for name, options, expected_lines in cases:
        item = {'case': name, 'args': options, 'expected_lines': expected_lines, 'runs': []}
        identity = None
        for repeat in range(args.repeats):
            for label, executable in executables.items():
                out = root / f'{name}-{repeat}-{label}.txt'
                if options is None:
                    out.write_bytes(b'')
                    body = f'PASSGEN_7\n{4**128-4**10} 0 0 {fingerprint("")}\n128 128 4 0 0 0 0 0 0 0 0 1\n- -\n4\n61\n62\n63\n64\n1\n61 2\n'
                    Path(str(out)+'.pgstate').write_text(body+f'CHECK {fingerprint(body)}\n')
                    command = [executable, '--resume', '--out', str(out)]
                else:
                    command = [executable, *options, '--threads', '4', '--compute', 'cpu', '--out', str(out)]
                began = time.perf_counter()
                subprocess.run(command, capture_output=True, text=True, check=True)
                elapsed = time.perf_counter()-began
                digest = hashlib.sha256();lines = size = 0
                with out.open('rb') as stream:
                    while data := stream.read(1024*1024):
                        digest.update(data);size += len(data);lines += data.count(b'\n')
                actual = (size, lines, digest.hexdigest())
                assert lines == expected_lines, (name, label, lines, expected_lines)
                if identity is None:
                    identity = actual
                assert actual == identity, (name, label)
                item['runs'].append({'version': label, 'seconds': elapsed})
                out.unlink();Path(str(out)+'.pgstate').unlink()
        item.update(bytes=identity[0], lines=identity[1], sha256=identity[2])
        item['median_seconds'] = {label: statistics.median(run['seconds'] for run in item['runs'] if run['version'] == label) for label in executables}
        item['speedup'] = item['median_seconds']['baseline']/item['median_seconds']['candidate']
        report['cases'].append(item)
        print(name, item['median_seconds'], 'speedup', round(item['speedup'], 2), flush=True)
        output = Path(args.output);output.parent.mkdir(exist_ok=True, parents=True);output.write_text(json.dumps(report, indent=2)+'\n')
