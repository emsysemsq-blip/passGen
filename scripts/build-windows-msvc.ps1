# Native Windows 10+ x64 build. Run in an x64 Native Tools Command Prompt for
# Visual Studio 2022, then invoke: powershell -File scripts/build-windows-msvc.ps1
# Requires the Desktop development with C++ workload and a Windows SDK.
# /MT uses the normal static Microsoft C/C++ runtime, so the application does
# not require a separately installed Visual C++ Redistributable.
# This script builds and runs the tests locally; it does not upload any files.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Set-Location (Join-Path $PSScriptRoot '..')

foreach ($tool in @('cl.exe', 'link.exe', 'rc.exe')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Missing $tool. Run this script in the Visual Studio x64 Native Tools environment."
    }
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'An x64 Native Tools environment is required (VSCMD_ARG_TGT_ARCH=x64).'
}
foreach ($resource in @('src/passgen.rc', 'src/passgen.manifest', 'src/version.hpp')) {
    if (-not (Test-Path -LiteralPath $resource)) {
        throw "Missing required release resource: $resource"
    }
}

$objectDirectory = 'dist/obj/msvc'
New-Item -ItemType Directory -Force -Path $objectDirectory | Out-Null
$compileFlags = @(
    '/nologo', '/std:c++17', '/O2', '/EHsc', '/W4', '/utf-8', '/MT',
    '/permissive-', '/DNDEBUG', '/DUNICODE', '/D_UNICODE', '/DNOMINMAX',
    '/DWINVER=0x0A00', '/D_WIN32_WINNT=0x0A00', '/Isrc'
)

function Compile-Source {
    param([string]$Source, [string]$ObjectName)
    $object = Join-Path $objectDirectory ($ObjectName + '.obj')
    & cl.exe @compileFlags /c $Source "/Fo$object" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "C++ compilation failed: $Source" }
    return $object
}

function Build-Program {
    param(
        [string]$Name,
        [string]$Source,
        [bool]$UsesEngine = $true,
        [string]$Subsystem = 'CONSOLE',
        [string[]]$Libraries = @(),
        [bool]$UsesGuiResource = $false
    )
    $objects = @(Compile-Source $Source $Name)
    if ($UsesEngine) { $objects += $engineObject }
    $linkFlags = @('/NOLOGO', '/MACHINE:X64', "/SUBSYSTEM:$Subsystem,10.00", "/OUT:dist/$Name.exe")
    if ($UsesGuiResource) {
        $objects += 'dist/passgen.res'
        # passgen.rc already embeds the explicit asInvoker manifest as ID 1.
        # Preserve that one manifest instead of generating a second copy.
        $linkFlags += '/MANIFEST:NO'
    }
    # Let the standard CRT choose main/wmain/wWinMain; do not override /ENTRY.
    & link.exe @linkFlags @objects @Libraries | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Link failed: $Name" }
}

& rc.exe /nologo /I src /fo dist/passgen.res src/passgen.rc | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Windows version/manifest resource build failed' }
$engineObject = Compile-Source 'src/engine.cpp' 'engine'

Build-Program -Name 'PassGen' -Source 'src/winmain.cpp' -Subsystem 'WINDOWS' `
    -Libraries @('user32.lib', 'gdi32.lib', 'comdlg32.lib', 'comctl32.lib') -UsesGuiResource $true
Build-Program -Name 'passgen-cli' -Source 'src/cli.cpp'
Build-Program -Name 'passgen-tests' -Source 'tests/engine_test.cpp'
Build-Program -Name 'passgen-resource-engine-tests' -Source 'tests/resource_engine_test.cpp'
Build-Program -Name 'passgen-checkpoint-lock-tests' -Source 'tests/checkpoint_lock_test.cpp'
Build-Program -Name 'passgen-checkpoint-retry-tests' -Source 'tests/checkpoint_retry_test.cpp' -UsesEngine $false
Build-Program -Name 'passgen-resource-tests' -Source 'tests/resource_test.cpp' -UsesEngine $false
Build-Program -Name 'passgen-gui-smoke' -Source 'tests/gui_smoke.cpp' -UsesEngine $false -Libraries @('user32.lib')

# The GUI smoke helper launches PassGen.exe relative to its working directory.
# Run the complete native suite in dist, and fail immediately on any nonzero exit.
Push-Location dist
try {
    foreach ($test in @(
        'passgen-tests.exe',
        'passgen-resource-engine-tests.exe',
        'passgen-checkpoint-retry-tests.exe',
        'passgen-resource-tests.exe',
        'passgen-checkpoint-lock-tests.exe',
        'passgen-gui-smoke.exe'
    )) {
        Write-Host "Running $test"
        & (Join-Path '.' $test) | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "Native Windows tests failed: $test" }
    }
} finally {
    Pop-Location
}
Write-Host 'Native MSVC x64 build and test suite completed successfully.'
