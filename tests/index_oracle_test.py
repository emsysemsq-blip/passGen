#!/usr/bin/env python3
"""Compare the C++ rank arithmetic with Python's independent big integers."""
import random
import subprocess
import sys

rng = random.Random(1011)
cases = [(rng.getrandbits(rng.randrange(1, 2200)),
          rng.getrandbits(rng.randrange(1, 2200)), rng.randrange(1, 100001))
         for _ in range(1000)]
cases += [(x, x, divisor)
          for x in (0, 1, 10**9-1, 10**9, 2**64-1, 2**64, 2**128)
          for divisor in (1, 2, 100000)]
result = subprocess.run([sys.argv[1]], input=''.join(f'{a} {b} {n}\n' for a,b,n in cases),
                        text=True, capture_output=True, check=True)
lines = result.stdout.splitlines()
assert len(lines) == len(cases)
for (a,b,n), line in zip(cases, lines):
    assert list(map(int, line.split())) == [a+b, abs(a-b), a*n, a//n, a%n,
                                          int(a<b), int(a <= 2**64-1)]
print(f'PASS: {len(cases)} arithmetic cases compared with Python arbitrary-precision integers')
