#!/usr/bin/env python3
"""Compare optimized CLI ranges with the previous release and small independent oracles.

Example:
  python3 tests/range_generation_test.py \
    --baseline build/perf/passgen-v0.1.6-cli --candidate build/passgen-cli
Commands may include a launcher; --windows translates temporary paths for Wine.
"""
import argparse
import pathlib
import random
import re
import subprocess
import tempfile


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def checkpoint(output, length, start):
    """Create an independently checksummed, empty-output PASSGEN_3 checkpoint."""
    output.write_bytes(b'')
    body = (f'PASSGEN_3\n{start} 0 0 {fingerprint(b"")}\n'
            f'{length} {length} 3 0 0 0 0 0 0\n- -\n2\n61\n62\n').encode()
    pathlib.Path(str(output) + '.pgstate').write_bytes(
        body + f'CHECK {fingerprint(body)}\n'.encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--baseline', nargs='+', required=True)
    parser.add_argument('--candidate', nargs='+', required=True)
    args = parser.parse_args()
    commands = {name: [str(pathlib.Path(part).resolve()) for part in getattr(args, name)]
                for name in ('baseline', 'candidate')}
    randomizer = random.Random(0x50617373)
    cases = [
        ('scalar-length-boundaries', ['--chars', 'ab', '--min', '1', '--max', '7']),
        ('unequal-token-lengths', ['--words', 'a xyz b', '--min', '2', '--max', '7']),
        ('word-aliases', ['--words', 'ab a b aa', '--min', '1', '--max', '5']),
        ('word-aliases-no-repeat', ['--words', 'a aa aaa b', '--min', '1', '--max', '5',
                                   '--no-repeat']),
        ('unicode-and-affixes', ['--words', 'آ آب ب', '--min', '3', '--max', '7',
                                '--prefix', 'A', '--suffix', '1', '--upper', '--digit']),
        ('emoji-sequence-aliases', ['--emojis', '😀 😀😀 👩‍💻 👍🏽 🇮🇷', '--min', '1',
                                    '--max', '5']),
        ('legacy-token-lengths', ['--words', 'a aa 😀', '--token-lengths', '--min', '1',
                                 '--max', '5', '--prefix', 'آ', '--suffix', '!']),
        ('all-required-classes', ['--chars', 'aA1!', '--min', '1', '--max', '5',
                                  '--lower', '--upper', '--digit', '--symbol']),
        ('requirements-in-affixes', ['--words', 'a aa b', '--min', '4', '--max', '7',
                                     '--prefix', 'A1', '--suffix', '!', '--upper',
                                     '--digit', '--symbol', '--lower', '--no-repeat']),
        ('impossible-requirement', ['--chars', 'ab', '--min', '1', '--max', '6',
                                    '--upper']),
        ('no-fitting-length', ['--words', 'abc def', '--min', '1', '--max', '2']),
        ('one-symbol-full-length-range', ['--chars', 'آ', '--min', '1', '--max', '128']),
        ('one-symbol-no-repeat', ['--chars', 'x', '--min', '1', '--max', '128',
                                 '--no-repeat']),
    ]
    pools = [['a', 'aa', 'b'], ['ab', 'a', 'b'], ['a', 'A', '1', '!'],
             ['آ', 'آب', 'ب'], ['😀', '😀😀', 'آ'], ['x', 'yz', '!']]
    for number in range(24):
        pool = list(randomizer.choice(pools))
        randomizer.shuffle(pool)
        # Duplicate inputs exercise normalization before range assignment.
        if number % 4 == 0:
            pool.append(pool[0])
        minimum = randomizer.randint(1, 3)
        maximum = randomizer.randint(minimum, 5)
        options = ['--words', ' '.join(pool), '--min', str(minimum), '--max', str(maximum)]
        if number % 2:
            options += ['--token-lengths']
        if number % 3 == 0:
            options += ['--no-repeat']
        if number % 4 == 0:
            options += ['--prefix', randomizer.choice(['A', 'آ', '1!'])]
        if number % 5 == 0:
            options += ['--suffix', randomizer.choice(['b', '😀', '!'])]
        if number % 3 == 1:
            options += [randomizer.choice(['--lower', '--upper', '--digit', '--symbol'])]
        cases.append((f'random-{number:02}', options))
    # More than one optimized worker range/batch, including a token-length boundary.
    cases.append(('worker-and-length-boundaries', ['--chars', 'ab', '--min', '1',
                                                   '--max', '19']))

    with tempfile.TemporaryDirectory(prefix='passgen-ranges-') as folder:
        def filename(output):
            return 'Z:' + str(output).replace('/', '\\') if args.windows else str(output)

        def invoke(name, options, output, workers):
            process = subprocess.run(commands[name] + options + ['--threads', str(workers),
                                     '--out', filename(output)], capture_output=True,
                                     text=True, encoding='utf-8', timeout=90)
            assert process.returncode == 0, (name, options, process.stdout, process.stderr)
            stats = re.search(r'Complete: scanned=(\d+) written=(\d+) bytes=(\d+)',
                              process.stdout)
            assert stats, (name, process.stdout, process.stderr)
            data = output.read_bytes()
            scanned, written, size = map(int, stats.groups())
            assert written == data.count(b'\n') and size == len(data), (name, options)
            return data, scanned, written

        for number, (label, options) in enumerate(cases):
            results = []
            for name in commands:
                output = pathlib.Path(folder) / f'{number}-{name}.txt'
                # Deliberately change worker count; ordered output must remain identical.
                workers = 3 if name == 'baseline' else 1 + number % 4
                results.append(invoke(name, options, output, workers))
            assert results[0] == results[1], label
            if label == 'one-symbol-full-length-range':
                assert results[1][0] == ''.join('آ' * n + '\n' for n in range(1, 129)).encode()
            if label == 'one-symbol-no-repeat':
                assert results[1][0] == b'x\n'
            if label == 'worker-and-length-boundaries':
                assert results[1][2] == (1 << 20) - 2
                rows = results[1][0].splitlines()
                assert rows[0] == b'a' and rows[-1] == b'b' * 19
                assert len(rows) == len(set(rows))

        for length in (64, 65, 128):
            total = 1 << length
            start = total - 5
            expected = ''.join(format(rank, f'0{length}b').translate(str.maketrans('01', 'ab'))
                               + '\n' for rank in range(start, total)).encode()
            for name in commands:
                output = pathlib.Path(folder) / f'large-{length}-{name}.txt'
                checkpoint(output, length, start)
                actual, scanned, written = invoke(name, ['--resume'], output, 2)
                assert actual == expected and scanned == total and written == 5, (name, length)
                # Completed resume also exercises persistent-worker teardown with no tasks.
                assert invoke(name, ['--resume'], output, 4) == (expected, total, 5)

    print(f'PASS: {len(cases)} baseline comparisons with varied workers; single-symbol 1..128, '
          'large worker/length boundaries, exact ranks ending at 2^64/65/128 and idempotent resume')


if __name__ == '__main__':
    main()
