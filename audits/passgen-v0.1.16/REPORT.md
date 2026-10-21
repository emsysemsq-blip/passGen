# PassGen v0.1.16 Defender investigation

Status: unresolved. No false-positive determination or malware clearance is claimed.

The user's Windows Security screenshot identifies `Trojan:Win32/Ulthar.A!ml`, severity Severe, status Active, for `PassGen-v0.1.16-win64.exe`, with a download origin matching this repository's release branch. The screenshot does not supply the endpoint file hash, Defender engine/security-intelligence versions or a behavioral trace. The generic threat description is not proof that command execution was observed in this application. The `!ml` suffix does not prove a false positive.

## Evidence collected

- Local release executable: 3,372,597 bytes, SHA256 `7724e900fce61dfb74773efa52d3aad074246c660f1d7fcb8065f047eedee808`; matches the published release checksum and `dist/PassGen.exe`.
- A fresh build from the release application source with the same documented MinGW flags and existing resource object is byte-identical to the release executable. See `rebuild-comparison.json`. This does not independently validate the compiler or establish absence of malicious behavior.
- Reviewed the application source for command/shell execution, network activity, downloads and remote-process injection. No corresponding application path was found. The program implements local UI, resource measurement, candidate generation, output/checkpoint handling and optional OpenCL computation.
- Direct imported DLLs: COMCTL32.dll, comdlg32.dll, GDI32.dll, KERNEL32.dll, msvcrt.dll, USER32.dll. No direct network DLL import was found; this alone cannot rule out dynamically loaded behavior.
- Optional Windows OpenCL loading uses an absolute System32 path and `LOAD_LIBRARY_SEARCH_SYSTEM32`; it does not download a driver. Resource API lookup uses Windows module exports.
- The PE has no Authenticode signature. It was built with static MinGW C++/thread runtimes and without a packer. These observations do not establish why Microsoft flagged it. The historical v0.1.8 audit attributes several sensitive-looking thread/context imports to the runtime; that historical attribution is not a fresh toolchain-integrity audit for this release.
- Microsoft Defender and native Windows are unavailable here. The local ClamAV directory contains no usable signature database, so no independent antivirus scan result is claimed. The native tests previously passed, but functional tests are not malware scans.

## Next step

Keep the file blocked. Submit the original flagged artifact for Microsoft's software-developer review at https://www.microsoft.com/en-us/wdsi/filesubmission with the threat label, release/source links, SHA256 and this report. No file was submitted to an external scanner during this investigation. A Microsoft verdict and a retest with current Defender intelligence are needed before treating the finding as resolved. Code signing establishes publisher identity; it does not guarantee a clean verdict.

Useful read-only endpoint metadata (no restoration, execution or protection changes):

```powershell
Get-MpComputerStatus | Select-Object AMProductVersion, AMEngineVersion, AntivirusSignatureVersion, AntivirusSignatureLastUpdated
Get-MpThreatDetection | Where-Object { ($_.Resources -join ' ') -match 'PassGen-v0\.1\.16-win64\.exe' } | Select-Object InitialDetectionTime, LastThreatStatusChangeTime, ThreatID, ActionSuccess, Resources
```

Do not restore the file merely to calculate its hash. Review local paths before sharing diagnostic output. No changed executable is offered as a way to bypass this detection.
