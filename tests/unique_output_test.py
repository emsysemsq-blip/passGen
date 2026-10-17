#!/usr/bin/env python3
"""Independent exhaustive oracle for range lengths, word aliases and unique output."""
import argparse
import itertools
import pathlib
import subprocess
import tempfile


def expected(tokens, minimum, maximum, prefix='', suffix='', token_lengths=False, no_repeat=False):
    seen = set()
    lines = []
    layers = [None] if token_lengths else range(minimum, maximum + 1)
    for layer in layers:
        lengths = range(minimum, maximum + 1) if token_lengths else range(1, maximum + 1)
        for length in lengths:
            for seq in itertools.product(tokens, repeat=length):
                if no_repeat and any(a == b for a, b in zip(seq, seq[1:])):
                    continue
                line = prefix + ''.join(seq) + suffix
                if layer is not None and len(line) != layer:
                    continue
                if line not in seen:
                    seen.add(line)
                    lines.append(line)
    return lines


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--windows', action='store_true')
    p.add_argument('command', nargs='+')
    args = p.parse_args()
    command = [str(pathlib.Path(x).resolve()) for x in args.command]
    count = 0
    with tempfile.TemporaryDirectory(prefix='passgen-unique-') as folder:
        def filename(path):
            return 'Z:' + str(path).replace('/', '\\') if args.windows else str(path)
        for tokens in (['a', 'aa'], ['a', 'ab', 'b'], ['ab', 'a', 'b'], ['آ', 'آب', 'ب']):
            for token_lengths in (False, True):
                for no_repeat in (False, True):
                    count += 1
                    output = pathlib.Path(folder) / f'{count}.txt'
                    cmd = command + ['--words', ' '.join(tokens), '--min', '2', '--max', '5',
                                     '--threads', '3', '--out', filename(output)]
                    if token_lengths:
                        cmd += ['--token-lengths']
                    if no_repeat:
                        cmd += ['--no-repeat']
                    run = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
                    assert run.returncode == 0, (cmd, run.stderr)
                    actual = output.read_text().splitlines()
                    oracle = expected(tokens, 2, 5, token_lengths=token_lengths, no_repeat=no_repeat)
                    assert actual == oracle, (tokens, token_lengths, no_repeat, actual, oracle)
                    assert len(actual) == len(set(actual))
                    if not token_lengths:
                        assert [len(x) for x in actual] == sorted(len(x) for x in actual)
                    # Simulate a stopped partial output from a genuine completed job.
                    state = pathlib.Path(str(output)+'.pgstate')
                    body = state.read_bytes().split(b'CHECK ')[0]
                    rows = body.decode().splitlines()
                    # Independent enumeration of raw candidate ranks is intentionally
                    # not duplicated here: use the initial checkpoint to replay all.
                    rows[1] = f'0 0 0 {fingerprint(b"")}'
                    body = ('\n'.join(rows)+'\n').encode()
                    state.write_bytes(body+f'CHECK {fingerprint(body)}\n'.encode())
                    output.write_bytes(b'uncommitted crash tail')
                    again = subprocess.run(command+['--resume','--out',filename(output)],
                                           capture_output=True, text=True, timeout=60)
                    assert again.returncode == 0 and output.read_text().splitlines() == oracle
        # Exact requested example, duplicate input letters, and complete ordered range.
        output = pathlib.Path(folder)/'range-10-15.txt'
        run = subprocess.run(command+['--chars','aabb','--min','10','--max','15',
                                      '--out',filename(output)],capture_output=True,text=True,timeout=60)
        assert run.returncode == 0, run.stderr
        actual = output.read_text().splitlines()
        assert len(actual) == len(set(actual)) == 64512
        assert {len(x) for x in actual} == set(range(10,16))
        assert [len(x) for x in actual] == sorted(len(x) for x in actual)
        assert actual[0] == 'a'*10 and actual[-1] == 'b'*15
        # Prefix and suffix count towards the final character length, including Unicode.
        output = pathlib.Path(folder)/'affixes.txt'
        run = subprocess.run(command+['--words','آ آب ب','--min','3','--max','5',
                                      '--prefix','A','--suffix','1','--upper','--digit',
                                      '--out',filename(output)],capture_output=True,text=True,timeout=60)
        assert run.returncode == 0, run.stderr
        assert output.read_text().splitlines() == expected(['آ','آب','ب'],3,5,'A','1')
        output = pathlib.Path(folder)/'default-words.txt'
        run = subprocess.run(command+['--words','red blue','--out',filename(output)],
                             capture_output=True,text=True,timeout=60)
        assert run.returncode == 0 and output.read_text().splitlines() == [
            'red','blue','redred','redblue','bluered','blueblue']
        output = pathlib.Path(folder)/'old-ambiguous.txt'
        old_data = b'a\naa\naa\n'
        output.write_bytes(old_data)
        body = (f'PASSGEN_2\n3 3 {len(old_data)} {fingerprint(old_data)}\n'
                '1 3 1 0 0 0 0 0\n- -\n2\n61\n6161\n').encode()
        old_state = body + f'CHECK {fingerprint(body)}\n'.encode()
        state = pathlib.Path(str(output)+'.pgstate')
        state.write_bytes(old_state)
        run = subprocess.run(command+['--resume','--out',filename(output)],
                             capture_output=True,text=True,timeout=60)
        assert run.returncode == 1 and 'Start a new output' in run.stderr
        assert output.read_bytes() == old_data and state.read_bytes() == old_state
        print(f'PASS: {count} word/rule cases, crash-tail replay, exact unique 10..15 range (64,512 lines), final Unicode lengths with affixes, default words, safe legacy-word refusal')


if __name__ == '__main__':
    main()
