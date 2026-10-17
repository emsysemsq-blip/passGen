# Antivirus investigation: PassGen v0.1.8

Date: 2026-10-17. Status: investigation incomplete; no antivirus clearance is claimed.

The reported warning cannot yet be attributed to a specific detection. The antivirus product, exact threat name, and triggering stage (download, extraction, execution, or generation) have not been supplied. The user entered version0.1.18; the published release examined here is0.1.8, so the precise file also needs confirmation.

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

The application was built without a packer or obfuscator. Static C++/thread runtime linking and lack of signing may affect reputation/heuristic assessment, but neither has been established as the cause here. A wordlist generator may also receive a tool-category classification; that cannot be assessed without the exact detection name.

## Antivirus scan limitation

Installed ClamAV1.4.3 locally from normal checksum-verified Debian packages for an independent local scan. Freshclam could not retrieve its official signature database: the configured network proxy denied the official database host (`database.clamav.net`, HTTP403CONNECT). No valid signature database was available, so a meaningful ClamAV scan was NOT completed. There is no clean-scan result. No executable or source was uploaded to a third-party scanner, and the user's endpoint antivirus was not accessible from this environment.

## Next evidence needed

Confirm the exact downloaded filename and compare its SHA256 above. On Windows the read-only commands below show identity and signing status:

```powershell
Get-FileHash -LiteralPath '.\PassGen-v0.1.8-win64.exe' -Algorithm SHA256
Get-AuthenticodeSignature -LiteralPath '.\PassGen-v0.1.8-win64.exe' |
    Select-Object Status, StatusMessage, SignerCertificate
```

Provide the antivirus product and exact threat label. If the local hash differs, the file has not yet been matched to this investigated artifact. If the label is a malware finding, retain the vendor report for analysis; if it is a publisher/reputation or tool-category warning, address that specific classification through the vendor's review process. Any external file submission requires the user's authorization.

The warning has not been reproduced or resolved. This audit does not recommend disabling protection or creating an exclusion, and it does not replace the flagged executable with a changed hash to evade detection.
