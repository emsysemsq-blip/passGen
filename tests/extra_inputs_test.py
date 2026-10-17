"""Independent CLI oracle for optional emoji, Persian and uppercase input pools."""
import itertools
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as folder:
    cases = [
        (["--chars", "aA", "--shift-letters"], ["a", "A"]),
        (["--words", "ab بa", "--shift-letters"], ["ab", "بa", "AB", "بA"]),
        (["--emojis", "😀 👩‍💻 👍🏽 🇮🇷 😀"], ["😀", "👩‍💻", "👍🏽", "🇮🇷"]),
        (["--chars", "a", "--symbols", "!", "--emojis", "😀", "--persian", "--shift-letters"],
         list(dict.fromkeys(["a", "!", "😀"] + list("ابپتثجچحخدذرزژسشصضطظعغفقکگلمنوهیآ") + ["A"]))),
        (["--emojis", "😀 😀😀"], ["😀", "😀😀"]),
    ]
    for index, (arguments, pool) in enumerate(cases):
        output = pathlib.Path(folder) / f"{index}.txt"
        subprocess.run([binary, *arguments, "--token-lengths", "--min", "1", "--max", "2", "--out", str(output)], check=True, capture_output=True)
        rows = output.read_text(encoding="utf-8").splitlines()
        expected = {"".join(parts) for n in (1, 2) for parts in itertools.product(pool, repeat=n)}
        assert len(rows) == len(set(rows)) and set(rows) == expected, index
        before = output.read_bytes()
        subprocess.run([binary, "--resume", "--emojis", "😎", "--out", str(output)], check=True, capture_output=True)
        assert output.read_bytes() == before
print("PASS: 5 CLI input-pool oracles, exact unique sets, completed resume")
