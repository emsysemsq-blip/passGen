#!/usr/bin/env python3
"""End-to-end regression: genuine ranks beyond 64 bits, without an enormous output."""
import argparse
import pathlib
import subprocess
import tempfile


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def checkpoint(path, length, next_rank, content=b'', version=2):
    path.write_bytes(content)
    body = (f'PASSGEN_{version}\n{next_rank} {content.count(bytes([10]))} '
            f'{len(content)} {fingerprint(content[-65536:])}\n'
            f'{length} {length} 2 0 0 0 0 0\n- -\n2\n61\n62\n').encode()
    pathlib.Path(str(path) + '.pgstate').write_bytes(
        body + f'CHECK {fingerprint(body)}\n'.encode())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('command', nargs='+')
    args = parser.parse_args()
    command = [str(pathlib.Path(p).resolve()) for p in args.command]
    with tempfile.TemporaryDirectory(prefix='passgen-large-') as folder:
        for length in (64, 65, 128):
            path = pathlib.Path(folder) / f'{length}.txt'
            count = 1 << length
            checkpoint(path, length, count - 3)
            filename = 'Z:' + str(path).replace('/', '\\') if args.windows else str(path)
            result = subprocess.run(command + ['--resume', '--out', filename],
                                    capture_output=True, text=True, timeout=60)
            assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
            expected = ''.join(format(i, f'0{length}b').translate(str.maketrans('01', 'ab')) + '\n'
                               for i in range(count - 3, count)).encode()
            assert path.read_bytes() == expected, (length, path.read_bytes())
            assert f'scanned={count} written=3' in result.stdout, result.stdout
            state = pathlib.Path(str(path) + '.pgstate').read_text()
            assert state.splitlines()[1].split()[0] == str(count)
            again = subprocess.run(command + ['--resume', '--out', filename],
                                   capture_output=True, text=True, timeout=60)
            assert again.returncode == 0 and path.read_bytes() == expected
            print(f'PASS: exact generation and idempotent resume ending at 2^{length}')
        path = pathlib.Path(folder) / 'legacy.txt'
        checkpoint(path, 2, 3, b'aa\nab\nba\n', version=1)
        filename = 'Z:' + str(path).replace('/', '\\') if args.windows else str(path)
        result = subprocess.run(command + ['--resume', '--out', filename], capture_output=True,
                                text=True, timeout=60)
        assert result.returncode == 0 and path.read_bytes() == b'aa\nab\nba\nbb\n'
        assert pathlib.Path(str(path)+'.pgstate').read_text().startswith('PASSGEN_3\n')
        print('PASS: v0.1.0 checkpoint reads and upgrades without lost/duplicate lines')


if __name__ == '__main__':
    main()
