#!/usr/bin/env python3
"""Independent exhaustive capped-token oracle, including ambiguous spellings."""
import itertools
import pathlib
import random
import subprocess
import sys
import tempfile

cli = str(pathlib.Path(sys.argv[1]).resolve())
rng = random.Random(111)
checks = 0
with tempfile.TemporaryDirectory(prefix='passgen-limits-') as folder:
    root = pathlib.Path(folder)
    def run(tokens, caps, minimum, maximum, characters=False, adjacent=False, prefix='', suffix='', workers=3):
        global checks
        path = root / f'{checks}.txt'
        args = [cli, '--words', ' '.join(tokens), '--min', str(minimum), '--max', str(maximum),
                '--threads', str(workers), '--compute', 'gpu', '--out', str(path), '--prefix', prefix, '--suffix', suffix]
        if not characters:
            args += ['--token-lengths']
        if adjacent:
            args += ['--no-repeat']
        for token, cap in caps.items():
            args += ['--limit', token, str(cap)]
        expected, seen = [], set()
        for target in range(minimum, maximum + 1):
            lengths = range(1, maximum + 1) if characters else [target]
            for length in lengths:
                for digits in itertools.product(range(len(tokens)), repeat=length):
                    if any(digits.count(tokens.index(token)) > cap for token, cap in caps.items()):
                        continue
                    if adjacent and any(a == b for a, b in zip(digits, digits[1:])):
                        continue
                    text = prefix + ''.join(tokens[i] for i in digits) + suffix
                    if characters and len(text) != target:
                        continue
                    if text not in seen:
                        seen.add(text)
                        expected.append(text)
        result = subprocess.run(args, capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
        if caps:
            assert "CPU: direct word combinations" in result.stderr
        actual = path.read_text().splitlines()
        assert actual == expected, (tokens, caps, minimum, maximum, characters, adjacent, actual, expected)
        state = pathlib.Path(str(path) + '.pgstate')
        assert state.read_text().startswith('PASSGEN_7\n')
        # A committed output ignores newly supplied CLI settings on Resume.
        with path.open('ab') as stream:
            stream.write(b'uncommitted crash tail\n')
        resumed = subprocess.run([cli, '--resume', '--out', str(path), '--limit', 'ignored', '10'], capture_output=True, text=True)
        assert resumed.returncode == 0, resumed.stderr
        assert path.read_text().splitlines() == expected
        checks += 1
    run(['a', 'b'], {'a': 2}, 1, 5)
    run(['word'], {'word': 10}, 1, 12)
    run(['word'], {}, 1, 12)
    run(['a', 'aa', 'aaa', 'b'], {'a': 1, 'aa': 1}, 2, 4)
    run(['a', 'aa', 'aaa', 'b'], {'a': 1, 'aa': 1}, 2, 6, True)
    run(['آ', 'ب', '😀'], {'آ': 2, '😀': 1}, 1, 5, True, True, 'آ', '!')
    for _ in range(64):
        tokens = rng.sample(['a', 'aa', 'aaa', 'b', 'ab', 'ba', 'ب', '😀'], rng.randint(2, 3))
        caps = {token: rng.randint(1, 3) for token in tokens if rng.randrange(3)}
        run(tokens, caps, rng.randint(1, 2), rng.randint(3, 5), bool(rng.randrange(2)), bool(rng.randrange(2)))
    for cap in ['0', '11']:
        invalid = root / f'invalid-{cap}.txt'
        result = subprocess.run([cli, '--chars', 'a', '--limit', 'a', cap, '--out', str(invalid)], capture_output=True)
        assert result.returncode == 1 and not invalid.exists()
    invalid = root / 'missing.txt'
    result = subprocess.run([cli, '--chars', 'a', '--limit', 'b', '2', '--out', str(invalid)], capture_output=True)
    assert result.returncode == 1 and not invalid.exists()
print(f'PASS: {checks} exhaustive capped-token ordered oracles and crash-tail resumes; invalid caps rejected')
