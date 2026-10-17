# Defender review status — v0.1.9

The user reported Microsoft Defender detection `Trojan:Win32/Sabsik.FL.A!ml` (Severe, Removed) against the v0.1.8 download. The detection is unresolved. See audits/passgen-v0.1.8/REPORT.md for artifact reconstruction, toolchain-integrity checks and investigation limits.

This v0.1.9 release contains a real CPU-generation optimization, transparent version information and a standard asInvoker manifest. It has not been scanned with native Microsoft Defender in this environment, has no Authenticode publisher signature, and has not received a Microsoft file-review verdict. Do not interpret the new version/hash, metadata, reproducible build, or successful functional tests as antivirus clearance.

The distributed executable is a MinGW build. A separate standard MSVC build script is included as a supported development path, but it has not been executed here. There is no claim that either compiler resolves a particular antivirus detection.

No obfuscation, executable packing, antivirus exclusions, protection-disabling code, or file submission to an external scanner was added. This release retains the resource controls, exact deduplication and checkpoint recovery.

Keep a Defender-blocked artifact blocked while its finding is unresolved. The legitimate review route for the exact original file and SHA256 is https://www.microsoft.com/en-us/wdsi/filesubmission . No submission was made from this environment. Publisher signing establishes identity, not a malware-clearance verdict.
