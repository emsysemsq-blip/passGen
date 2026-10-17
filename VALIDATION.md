# PassGen 0.1.3 — validation

This release addresses the generic Windows `Checkpoint replace failed` report and the empty sequential first-1000 preview. The exact error code/root cause on the user's Windows machine was unavailable in the older screenshot; the release does not claim to have reproduced that specific machine's cause.

## Verified results

- **44 engine checks passed on Linux and on the Windows executable under Wine.** These retain unique-output, length, rule, Unicode, durable resume and large-rank coverage, plus four preview regressions.
- **44 engine checks passed with AddressSanitizer and UndefinedBehaviorSanitizer**, without reported findings.
- The preview regression reproduces mixed words `e`, `l`, `I` and a 20-character word at final lengths 14..30. Its first 1,000 sequential ranks produce no accepted line. The new bounded sampler finds valid later ranks. Samples satisfy the selected final lengths/rules, stay unique and remain within the inspection budget. An impossible ASCII class requirement produces a bounded empty preview.
- **Checkpoint retry policy tests passed on Linux and Windows/Wine:** transient sharing failures followed by success, bounded access-denied retries, immediate disk-full reporting, and no wait on first-attempt success.
- **Real Windows API sharing-lock integration passed under Wine:** a checkpoint was opened without delete-sharing; releasing that lock after 250 ms allowed normal completion. A persistent lock exhausted retries, preserved the last committed checkpoint, left confirmed progress unchanged, and reported the actual Windows error code. After unlocking, Resume regenerated the uncommitted tail and matched the successful reference output byte for byte.
- The integration test compares the application's reported error code with a direct `MoveFileExW` reference call under the same lock rather than assuming every Windows implementation maps the lock to code 32. The initial hardcoded-code assertion was replaced by that direct comparison; the final assertion passed.
- **Actual GUI integration passed under Wine/Xvfb:** representative preview of later mixed-word ranks, unique word aliases, final lengths including prefix/suffix, large-alphabet preview, exact Unicode output, stop/resume, exact 2,097,150-line character output and clean close.

Native Windows 10/11, the user's actual Downloads folder, antivirus interaction, hardware power loss and a full billion-candidate run remain unverified. Wine sharing-lock tests validate the exercised compatibility implementation, not every native Windows filesystem/security product.

## Changes and limits

Transient Windows errors 5, 32, 33 and 1224 receive up to eight replacement attempts with waits of 25, 50, 100, 200, 400, 400 and 400 ms (about 1.575 seconds total programmed delay). Other error codes return immediately. The committed checkpoint is not explicitly deleted or truncated to bypass the lock. Persistent errors keep the Windows code and UTF-8-converted localized system explanation in the application error.

The sampler inspects at most 4,096 unique ranks and returns up to 12 matches. It prioritizes token-length boundaries, end ranks and cycling token patterns, then initial sequential ranks. It prepares the generation plan once. Sampling is not exhaustive or statistically uniform and an empty sample does not prove there are no valid outputs. Its inspection budget does not limit actual generation.

Schema PASSGEN_3 and generation ordering are unchanged from v0.1.2. Resume reads the same committed rank, verifies the saved output tail and reconstructs uncommitted bytes as before. Keep output and checkpoint together; do not edit the output manually. A generic old error cannot establish that every user's checkpoint/output is intact; Resume performs the consistency checks.

## Reproduce

```sh
bash scripts/build-linux.sh

g++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread -Isrc \
  src/engine.cpp tests/engine_test.cpp -o build/passgen-tests-asan
./build/passgen-tests-asan

PATH=/workspace/toolchain/root/usr/bin:$PATH \
CXX_WINDOWS=/workspace/toolchain/root/usr/bin/x86_64-w64-mingw32-g++-posix \
bash scripts/build-windows.sh
```

On native Windows, or under an initialized Wine with a display, run from `dist`:

```text
passgen-checkpoint-lock-tests.exe
passgen-checkpoint-retry-tests.exe
passgen-tests.exe
passgen-gui-smoke.exe
```

The checkpoint integration uses only its own new temporary directory. A failure retains its artifacts for investigation; success removes them. The GUI test also creates and cleans its own outputs. The native PowerShell build/test script remains unverified on physical Windows.
