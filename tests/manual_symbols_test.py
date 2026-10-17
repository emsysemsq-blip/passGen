#!/usr/bin/env python3
"""End-to-end optional custom-symbol alphabet tests for the CLI."""
import argparse
import itertools
import pathlib
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('command', nargs='+')
    args = parser.parse_args()
    command = [str(pathlib.Path(x).resolve()) for x in args.command]
    with tempfile.TemporaryDirectory(prefix='passgen-manual-symbols-') as folder:
        def filename(path):
            return 'Z:' + str(path).replace('/', '\\') if args.windows else str(path)
        cases = [
            (['--chars','ab','--symbols','!@!!'], 2,
             [''.join(x) for n in (1,2) for x in itertools.product('ab!@', repeat=n)]),
            (['--symbols','€؟'], 1, ['€','؟']),
            (['--words','a aa','--symbols','!'], 2, ['a','!','aa','a!','!a','!!']),
            (['--chars','ab','--symbols',''], 2, ['a','b','aa','ab','ba','bb']),
            (['--chars','a!','--symbols','!!!'], 2, ['a','!','aa','a!','!a','!!']),
        ]
        for i,(options,maximum,expected) in enumerate(cases):
            out = pathlib.Path(folder)/f'{i}.txt'
            result = subprocess.run(command+options+['--min','1','--max',str(maximum),
                                                    '--out',filename(out)],
                                    capture_output=True,text=True,timeout=60)
            assert result.returncode == 0, result.stderr
            actual = out.read_text().splitlines()
            assert actual == expected and len(actual) == len(set(actual)), (i,actual)
        out = pathlib.Path(folder)/'0.txt'
        original = out.read_bytes()
        resumed = subprocess.run(command+['--resume','--symbols','$','--out',filename(out)],
                                 capture_output=True,text=True,timeout=60)
        assert resumed.returncode == 0 and out.read_bytes() == original
        invalid = pathlib.Path(folder)/'invalid.txt'
        result = subprocess.run(command+['--chars','ab','--symbols',' A','--out',filename(invalid)],
                                capture_output=True,text=True,timeout=60)
        assert result.returncode == 1 and 'literal symbols' in result.stderr
        assert not invalid.exists()
    print('PASS: optional exact manual symbols, no unselected additions, Unicode, symbols-only input, word aliases, empty/duplicate symbols, stored-alphabet resume and invalid-input preservation')


if __name__ == '__main__':
    main()
