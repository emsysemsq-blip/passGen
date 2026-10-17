# Antivirus investigation: PassGen v0.1.8

Date: 2026-10-17. Status: investigation incomplete; no antivirus clearance is claimed.

## Confirmed endpoint report

The supplied Windows Security screenshot confirms Microsoft Defender detected `Trojan:Win32/Sabsik.FL.A!ml`, severity Severe, status Removed, against `PassGen-v0.1.8-win64.exe` in Downloads. The displayed download origin points to this repository's `release/passgen-v0.1.8` artifact. The filename resolves the earlier version ambiguity to v0.1.8. The local downloaded file's hash has not been provided; the screenshot confirms the name/origin, not byte identity.

This is a malware detection, not a SmartScreen unknown-publisher prompt or a HackTool/PUA label. The `!ml` suffix indicates a machine-learning-based label; it does not establish that the result is a false positive. The generic family description in the UI is not a detailed trace of this program's observed behavior. Detection timing (download versus first execution) and Defender engine/security-intelligence versions remain unknown.

## Exact file examined

`releases/v0.1.8/PassGen-v0.1.8-win64.exe`, 3,137,853 bytes.

SHA256:

```text
a32d6582075869014fb750e58f993d1f434998bb8c2842a16e464059134e93b8
```

The release executable matches `dist/PassGen.exe`. The executable has no Authenticode certificate. Lack of a certificate is an observed property, not proof of malware or proof of the reason for this warning. No code-signing certificate was available; no signature was fabricated.

## Source-to-binary reconstruction

Rebuilt from the current release source using the same MinGW compiler and documented GUI flags, writing only to ignored build output. All18PE sections match byte-for-byte. After excluding the COFF link timestamp and PE checksum, the entire original and rebuilt files match byte-for-byte, including the symbol/debug data. Raw hashes are in `rebuild-comparison.json`.

This establishes consistency with this source and toolchain. It does not establish absence of every possible vulnerability or malicious dependency.

## Toolchain integrity

Refreshed APT metadata using the configured HTTPS Debian trixie repository and normal signature verification. SHA256 of the seven cached MinGW/binutils development/runtime packages matches that metadata. Compared every regular file from those package archives with its installed copy:4,225 files matched, zero changed or missing. Details are in `toolchain-integrity.json`.

## Source and imports review

No application source path for networking, downloads, command/shell execution, persistence/autorun, credential extraction, or remote-process injection was found in the reviewed `src` tree. The application performs native UI handling, resource measurement, wordlist generation, file output and checkpoint recovery.

Direct DLL imports are COMCTL32.dll, comdlg32.dll, GDI32.dll, KERNEL32.dll, msvcrt.dll and USER32.dll. No network DLL is directly imported; that fact alone would not exclude dynamically loaded networking, so the source was also reviewed.

Some sensitive-looking imports originate from the static MinGW runtime rather than explicit application behavior. Symbol references in the verified `libwinpthread.a` identify GetThreadContext/SetThreadContext/SuspendThread/ResumeThread/IsDebuggerPresent in its thread implementation, OpenProcess in scheduling support, and SetSystemTime in clock support. Their presence is not by itself evidence of malware and does not prove what caused a particular engine's detection.

The application was built without a packer or obfuscator. Static C++/thread runtime linking and lack of signing may affect reputation/heuristic assessment, but neither has been established as the cause here. The supplied label is a Trojan finding, so the confirmed report must not be reclassified as a harmless wordlist-tool warning.

## Antivirus scan limitation

Installed ClamAV1.4.3 locally from normal checksum-verified Debian packages for an independent local scan. Freshclam could not retrieve its official signature database: the configured network proxy denied the official database host (`database.clamav.net`, HTTP403CONNECT). No valid signature database was available, so a meaningful ClamAV scan was NOT completed. There is no clean-scan result. No executable or source was uploaded to a third-party scanner, and the user's endpoint antivirus was not accessible from this environment.

## Remaining investigation and vendor review

The endpoint product, threat name and release filename are now known. Remaining useful metadata are Defender engine/platform/security-intelligence versions, detection timing and any hash already recorded by the endpoint. Do not restore a removed executable merely to compute its hash. If a copy is already safely retained for analysis, its SHA256 can be compared with the published value above.

The following Windows commands query status/history without restoring files, adding exclusions, disabling protection, scanning or uploading anything:

```powershell
Get-MpComputerStatus |
    Select-Object AMProductVersion, AMEngineVersion,
                  AntivirusSignatureVersion, AntivirusSignatureLastUpdated
Get-MpThreatDetection |
    Where-Object { ($_.Resources -join ' ') -match 'PassGen-v0\.1\.8-win64\.exe' } |
    Select-Object InitialDetectionTime, LastThreatStatusChangeTime,
                  ThreatID, ActionSuccess, Resources
```

These commands were not executed in this Linux environment. History may include local paths; inspect it before sharing.

The appropriate external next step is Microsoft's official developer file-review process for the exact original artifact and hash: https://www.microsoft.com/en-us/wdsi/filesubmission . Select the software-developer scenario and report the observed detection without asserting a confirmed false positive. Include the release/source URL, hash, this source-to-binary comparison and the application purpose. Any actual external file submission requires authorization; none has been made from this environment. Native Defender scanning and the user's endpoint were not available here.

Keep the removed artifact blocked while the finding is unresolved. Obtain a Microsoft review verdict and retest against current security intelligence before representing the issue as resolved. An Authenticode signature can establish publisher identity but is not a malware-clearance verdict.

The endpoint warning is confirmed by the screenshot; its cause and validity remain unresolved. This audit does not recommend disabling protection or creating an exclusion, and it does not replace the flagged executable with a changed hash to evade detection.
