# 0.1.1

- Fix the 64-bit search-space overflow with dependency-free arbitrary-precision candidate counts/ranks.
- Preserve exact large ranks in checkpoints; read old PASSGEN_1 and write PASSGEN_2.
- Update GUI counts, estimates and progress for large spaces without narrowing to uint64.
- Add actual generation/resume regressions ending at 2^64, 2^65 and 2^128, arithmetic oracle checks, and default-alphabet GUI preview coverage.

# 0.1.0

- Native Windows x64 GUI and portable C++17 generation engine.
- Character/word input, explicit default lengths, preview, prefix/suffix and ASCII class filters.
- Bounded parallel generation, deterministic output, durable checkpoints and resume.
- UTF-8 output and Unicode Windows paths/CLI arguments.
- Separate source and Windows binary archives with checksums and validation notes.
