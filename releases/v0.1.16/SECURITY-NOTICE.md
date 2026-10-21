# PassGen 0.1.16

## Unresolved Microsoft Defender detection

The user supplied a Windows Security screenshot reporting `Trojan:Win32/Ulthar.A!ml`, Severe, Active, for this release executable. Do not run or restore the flagged file while the finding remains unresolved. Do not disable protection or add an exclusion.

An independent rebuild using the existing source and toolchain is byte-identical to the published executable (SHA256 `7724e900fce61dfb74773efa52d3aad074246c660f1d7fcb8065f047eedee808`). This is source/binary consistency evidence, not an antivirus clearance. The endpoint copy's hash and Defender versions are not known. No Microsoft review verdict has been obtained. See `audits/passgen-v0.1.16/REPORT.md` in the repository.

Built from the accompanying C++17 source with MinGW-w64 for Windows x64.
The executable is unsigned, requests ordinary user privileges, and is not packed.
OpenCL is optional and loaded from the system driver; the application does not download drivers.

This build has not been scanned with Microsoft Defender in this environment.
Native core regressions passed; Windows binaries compiled successfully. GUI runtime testing was blocked by the cloud display restriction. See VALIDATION.md in the source archive for the exact validation scope.
