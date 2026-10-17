"""Independent CLI oracle for optional emoji, Persian and Shift input pools."""
import itertools
import xml.etree.ElementTree as ET
import re
import pathlib
import subprocess
import sys
import tempfile

binary = str(pathlib.Path(sys.argv[1]).resolve())
fixture = ET.parse(pathlib.Path(__file__).parent / "fixtures/fa-t-k0-windows.xml").getroot()
base = {key.attrib["iso"]: key.attrib["to"] for key in fixture.findall("keyMap")[0]}
shift = {base[key.attrib["iso"]]: re.sub(r"\\u\{([0-9A-Fa-f]+)\}", lambda match: chr(int(match[1], 16)), key.attrib["to"]) for key in fixture.findall("keyMap")[1] if base[key.attrib["iso"]] in "ضصثقفغعهخحجچشسیبلاتنمکگپظطزرذدئو"}
with tempfile.TemporaryDirectory() as folder:
    cases = [
        (["--chars", "aA", "--shift-letters"], ["a", "A"]),
        (["--words", "ab بa", "--shift-letters"], ["ab", "بa", "ّa"]),
        (["--emojis", "😀 👩‍💻 👍🏽 🇮🇷 😀"], ["😀", "👩‍💻", "👍🏽", "🇮🇷"]),
        (["--chars", "a", "--symbols", "!", "--emojis", "😀", "--persian", "--shift-letters"],
         list(dict.fromkeys(["a", "!", "😀"] + list("ابپتثجچحخدذرزژسشصضطظعغفقکگلمنوهیآ") + [shift[letter] for letter in "ابپتثجچحخدذرزژسشصضطظعغفقکگلمنوهیآ" if letter in shift]))),
        (["--emojis", "😀 😀😀"], ["😀", "😀😀"]),
    ]
    cases.append((["--chars", "".join(shift), "--shift-letters"], list(dict.fromkeys(list(shift) + list(shift.values())))))
    for index, (arguments, pool) in enumerate(cases):
        output = pathlib.Path(folder) / f"{index}.txt"
        subprocess.run([binary, *arguments, "--token-lengths", "--min", "1", "--max", "2", "--out", str(output)], check=True, capture_output=True)
        rows = output.read_text(encoding="utf-8").splitlines()
        expected = {"".join(parts) for n in (1, 2) for parts in itertools.product(pool, repeat=n)}
        assert len(rows) == len(set(rows)) and set(rows) == expected, index
        before = output.read_bytes()
        subprocess.run([binary, "--resume", "--emojis", "😎", "--out", str(output)], check=True, capture_output=True)
        assert output.read_bytes() == before
print("PASS: 6 CLI input-pool oracles, exact unique sets, completed resume")
