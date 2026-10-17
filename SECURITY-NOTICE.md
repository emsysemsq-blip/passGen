# Defender review status — v0.1.10

The user reported Microsoft Defender detection `Trojan:Win32/Sabsik.FL.A!ml` (Severe, Removed) against the v0.1.8 download. The detection is unresolved. See audits/passgen-v0.1.8/REPORT.md for artifact reconstruction, toolchain-integrity checks and investigation limits.

The user subsequently reported no Defender alert for v0.1.9 on their system. That observation is specific to that file, endpoint and security intelligence; it is not an official Microsoft review verdict.

This v0.1.10 release adds an optional OpenCL compute backend and dynamically loads the system OpenCL runtime. It has not been scanned with native Microsoft Defender here, has no Authenticode publisher signature, and has not received a Microsoft file-review verdict. Kernel correctness tests and successful functional tests are not antivirus clearance. The application does not install GPU drivers or download executable code.

The distributed executable is a MinGW build. A separate standard MSVC build script is included but has not been executed here.

No obfuscation, executable packing, antivirus exclusions, protection-disabling code, or file submission to an external scanner was added. This release retains the resource controls, exact deduplication and checkpoint recovery.

Keep a Defender-blocked artifact blocked while its finding is unresolved. The legitimate review route for the exact original file and SHA256 is https://www.microsoft.com/en-us/wdsi/filesubmission . No submission was made from this environment. Publisher signing establishes identity, not a malware-clearance verdict.
