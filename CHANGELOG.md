# 0.1.14

- Make plain Enter insert a new line in the Input editor instead of invoking the default Generate button. Verify keyboard-message handling, typing on the next line and token-table refresh.

# 0.1.13

- Restore the buffered flat renderer for direct jobs whose every combination is provably legal. Retain prefix pruning for caps, adjacency, ambiguous words and other constrained cases; do not generate rejected passwords to regain speed.
- Size character-length batches using a safe UTF-8 bytes-per-scalar bound instead of multiplying word bytes by final scalar length.
- Narrow arbitrary-precision cursors to bounded prefix blocks once, use uint64 offsets inside recursive traversal, and avoid redundant cap canonicalization for prefix-free tokens.
- Aggregate sparse direct-job checkpoints with bounded thresholds and always commit the first batch, completion, quota and Stop. Preserve output-before-checkpoint order and replay unsaved crash tails.
- Keep PASSGEN_7 and byte-exact raw-rank order, including a constrained resumed tail ending at 2^128. Add fast-path counter and wide-prefix tests plus a reproducible alternating benchmark.
- Native Linux three-pair durable-output measurements: word-character sample2.93x, capped2.00x, mixed-width words7.22x, uniform words1.98x, sparse caps1.96x, wide-rank tail1.93x. Files match by count/bytes/SHA256. These are not native Windows or user-system speeds.

# 0.1.12

- Default the GUI to whole-word input with an empty editor. Synchronize the token table after actual text/mode changes, including multiline WM_SETTEXT/paste; invalid character-mode multiline input clears old rows and explains the mode. Keep the selected dropdown visible when navigating to scrolled rows.
- Add direct whole-word prefix traversal for new GUI word jobs and CLI --words. Reject capped, adjacent-repeat, impossible-length and impossible-class branches before constructing their leaves. Use exact reachable scalar-length tables and safe class feasibility bounds.
- Skip impossible arbitrary-precision rank intervals before parallel dispatch, including subtrees larger than 64 bits. Keep exact ordered output, bounded CPU buffers, quota replay and durable raw-rank checkpoints.
- Display covered raw ranks separately from committed constructed-combination rates; skipped ranks are not advertised as generated-password throughput. Word traversal uses CPU; character/GPU behavior remains separate.
- Save PASSGEN_7 direct-combination policy. Read schemas 1..6 with their saved legacy policy; begin a new job to enable direct traversal for old jobs. Retain schema-6 token caps.
- Validate synthetic multiline custom entries, editable/scrolled per-word caps, exhaustive output parity, huge impossible subtrees, multiworker Stop/crash/Resume and sanitizer checks. Native Windows/GPU throughput remains unverified.

# 0.1.11

- Add an editable per-token table beside Input, with Always (unlimited) or a maximum of 1..10 selections per generated row. The same token may be absent; no presence requirement is introduced.
- Track bounded-token frequencies incrementally in the CPU odometer. Preserve the unrestricted fast path when every token is Always. Capped jobs explicitly select CPU; the OpenCL path remains available for uncapped jobs.
- Make ambiguous-word canonicalization respect token caps, so a forbidden earlier spelling cannot suppress an allowed equivalent output. Exact line deduplication remains mandatory.
- Add repeatable CLI --limit TOKEN N and save caps in PASSGEN_6. Read schemas 1..5 as unlimited; Resume preserves saved caps independently of current form edits.
- Test capped-token order, ambiguous aliases, Unicode, absence, Always, cap 10, quota/resume, crash-tail recovery and the native Windows dropdown through Wine. No new physical GPU, native Windows or Defender verdict is claimed.

# 0.1.10

- Add a real optional OpenCL 1.2 generation backend, loaded dynamically, selecting GPU devices only in production. No SDK/runtime/driver is downloaded by the application.
- Default new GUI/CLI jobs to Auto: validate first GPU rows against the CPU and compare transfer-inclusive sample timing before selecting CPU+GPU. Add CPU-only and GPU-preferred modes and --list-gpus inventory.
- Partition rank ranges across persistent CPU workers and one GPU lane, commit output in order, and replay failed/uncommitted GPU ranges on CPU. Preserve exact deduplication and arbitrary-precision global ranks.
- Support fixed-byte-width prefix-free token pools, UTF-8, ASCII requirements and adjacent-repeat rules. Mixed-width/ambiguous inputs retain the exact CPU path.
- Bound RAM/VRAM buffers, account for shared GPU memory and target approximate GPU duty at the selected resource percentage. Report actual backend reason and committed GPU scanned count.
- Save PASSGEN_5 compute preference and read schemas1..4 with their previous CPU policy. Resume restores saved compute mode; no silent change of old jobs.
- Add real-kernel tests through explicitly permitted PoCL CPU devices, integration/failure/quota/stop/resume/128-bit oracles, and reproducible optional test-runtime setup.
- No physical GPU or native Windows GPU throughput validation was available. User feedback: v0.1.9 produced no Defender alert on that user's system; v0.1.10 has no new Defender verdict or publisher signature.

# 0.1.9

- Optimize uniform-width, prefix-free output generation by removing redundant per-result scalar accounting and rule dispatch when no checks remain; retain bounded CPU pacing/stop checks and exact line-byte quota boundaries.
- Five alternating native-Linux durable-output runs (16,777,216 rows, four workers, manual mode) had identical SHA256; median full-process time improved from0.167432 to0.131254 seconds (~22% lower). No native Windows throughput claim.
- Add centralized version macros, native VERSIONINFO and an explicit asInvoker/Windows10+ manifest with standard common-controls dependency.
- Make the GUI and CLI MinGW link timestamps deterministic. Add a separate conventional MSVC x64 build/test script; native MSVC execution remains unverified here.
- The reported Defender detection againstv0.1.8 remains unresolved. v0.1.9 is unsigned and not Defender-cleared; see SECURITY-NOTICE.md. Metadata and compiler options are for traceability/standard packaging, not proof of a false positive.

# 0.1.8

- Detect effective CPU capacity, available RAM and output filesystem space before generation (Windows APIs; Linux affinity and cgroup v1/v2 limits).
- Default GUI/CLI new jobs to Auto 70%: select worker count automatically and pace worker CPU time. RAM is a bounded buffer-planning ceiling, not deliberate memory filling or a whole-process hard cap.
- Limit each run's new output to 70% of initial free disk space, stop before the first accepted line that cannot fit, and checkpoint only the exact contiguous scanned prefix. Resume recalculates current free-space budget.
- Preserve exact deduplication and UTF-8, including partial worker results and Stop during CPU waits. Reject missing-resource/insufficient-RAM plans before creating files.
- Add CLI --resource-percent 0..100. Explicit --threads retains manual mode unless resource percentage is also explicit; GUI manual mode remains available by clearing Auto 70%.
- Save PASSGEN_4 policy metadata; read schemas 1..3 with their previous manual execution policy. Resource controls never change candidate order/alphabet.
- GPU execution remains unsupported. CPU target is approximate worker-generation utilization, not a whole-system or real-time hard limit. Disk budget means bytes, not disk I/O load.

# 0.1.7

- Unrank once per bounded range and increment token digits; eliminate per-candidate arbitrary-precision division and segment/power reconstruction.
- Reuse worker threads for the whole run. Use bounded 4 MiB worker output buffers (smaller candidate spans for ambiguous tokens) to reduce durable commit overhead.
- Reuse fixed-byte-width line templates and update only changed tokens; precompute ASCII class masks and update length/rule state incrementally.
- Preserve exact canonical word/emoji deduplication, raw rank order, legacy checkpoint schemas and output-before-checkpoint durability.
- Wake CLI signal watcher immediately on completion; retain SIGINT stop handling. Correct GUI initial/resumed rate baselines and show MiB/sec with actual completion timing.
- Add exact baseline range regressions and repeatable end-to-end benchmarks with verified byte-identical files. See BENCHMARKS.md for machine, timing scope and limits.
- GPU execution remains unimplemented; these optimizations use CPU and bounded RAM.

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
