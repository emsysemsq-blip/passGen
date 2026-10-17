# 0.1.6

- Correct Shift + letter to Windows Persian (00000429) instead of ASCII uppercase, following the user's clarification.
- Apply explicit CLDR Windows Persian letter-key mappings, preserving combining marks and multi-scalar ريال output; leave non-Persian-letter input unchanged.
- Word mode adds the fully shifted word; existing letters and exact mandatory deduplication remain intact.
- Store the Unicode CLDR reference fixture and license for independent all-key oracle tests. Source: https://raw.githubusercontent.com/unicode-org/cldr/release-32/keyboards/windows/fa-t-k0-windows.xml
- Existing checkpoints retain their original stored alphabet; use a new output for the corrected Shift option.

# 0.1.5

- Add separate manual Emoji input and CLI --emojis with whitespace-delimited complete tokens, preserving ZWJ, flag and modifier sequences.
- Add optional Persian alphabet preset (32 letters plus آ) and Shift + letter uppercase variants for ASCII letters, including word tokens.
- Keep exact output deduplication mandatory across every input pool and resume; no checkpoint schema change is needed.
- Retain Unicode scalar lengths; token-length mode counts each complete emoji entry as one unit.

# 0.1.4

- Replace the GUI's required-ASCII-symbol checkbox with a manual Symbols edit field.
- Add only the exact typed symbol characters to the generation alphabet; symbols are optional in each password as requested.
- Deduplicate repeated symbols and existing input overlap; support literal Unicode symbols, word input and symbol-only input.
- Add CLI --symbols TEXT; preserve the legacy --symbol flag and existing checkpoint behavior.
- Keep schema PASSGEN_3 unchanged: the merged token alphabet already persists all manual symbol choices.
- Verify GUI generation of all 20 combinations of ab!@ at lengths 1..2, plain-password retention, mixed word aliases and resume ignoring edited UI choices.

# 0.1.3

- Retry transient Windows checkpoint replacement errors with bounded exponential backoff; keep the existing committed checkpoint instead of deleting it to bypass a lock.
- Surface the original Windows error code and localized system reason when replacement still fails. Do not assert a known root cause for the user's older generic error.
- Replace the sequential first-1000 preview with bounded representative rank sampling at token-length boundaries, reusing the prepared generation plan.
- Reproduce temporary/persistent Windows sharing locks and validate exact recovery of the uncommitted tail; add preview regression for mixed word lengths 14..30.
- Keep schema-3 checkpoints and generation order compatible with v0.1.2.

# 0.1.2

- Scan every selected final-character length in ascending order; include prefix/suffix in length and keep explicit legacy token-count mode.
- Guarantee exact unique output across character/word aliases, token counts, parallel batches and checkpoint resume.
- Add canonical tokenization without storing output history; the character path stays naturally unique.
- Normalize repeated inputs before assigning new job ranks.
- Save PASSGEN_3 with length policy; preserve compatible legacy semantics and reject ambiguous old word checkpoints that may already contain duplicates.
- Add independent exhaustive uniqueness/range oracles, ambiguous-word mid-job recovery and GUI word/length coverage.

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
