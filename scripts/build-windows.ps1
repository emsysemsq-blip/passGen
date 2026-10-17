$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
New-Item -ItemType Directory -Force dist | Out-Null
# Run from an MSYS2 UCRT64 terminal with mingw-w64-ucrt-x86_64-gcc installed.
$flags = @('-std=c++17','-O3','-Wall','-Wextra','-pthread','-static','-static-libgcc','-static-libstdc++')
& g++ @flags -municode -mwindows src/engine.cpp src/winmain.cpp -lcomdlg32 -lcomctl32 -o dist/PassGen.exe
if ($LASTEXITCODE) { throw 'GUI build failed' }
& g++ @flags -municode src/engine.cpp src/cli.cpp -o dist/passgen-cli.exe
if ($LASTEXITCODE) { throw 'CLI build failed' }
& g++ @flags -Isrc src/engine.cpp tests/engine_test.cpp -o dist/passgen-tests.exe
if ($LASTEXITCODE) { throw 'Test build failed' }
& ./dist/passgen-tests.exe
if ($LASTEXITCODE) { throw 'Tests failed' }

& g++ @flags -Isrc tests/gui_smoke.cpp -o dist/passgen-gui-smoke.exe
if ($LASTEXITCODE) { throw 'GUI smoke test build failed' }
Push-Location dist
try {
    & ./passgen-gui-smoke.exe
    if ($LASTEXITCODE) { throw 'GUI smoke test failed' }
} finally { Pop-Location }

& g++ @flags -Isrc src/engine.cpp tests/checkpoint_lock_test.cpp -o dist/passgen-checkpoint-lock-tests.exe
if ($LASTEXITCODE) { throw 'Checkpoint lock test build failed' }
& ./dist/passgen-checkpoint-lock-tests.exe
if ($LASTEXITCODE) { throw 'Checkpoint lock tests failed' }
& g++ @flags -Isrc tests/checkpoint_retry_test.cpp -o dist/passgen-checkpoint-retry-tests.exe
if ($LASTEXITCODE) { throw 'Checkpoint retry test build failed' }
& ./dist/passgen-checkpoint-retry-tests.exe
if ($LASTEXITCODE) { throw 'Checkpoint retry tests failed' }

& g++ @flags -Isrc tests/resource_test.cpp -o dist/passgen-resource-tests.exe
if ($LASTEXITCODE) { throw 'Resource test build failed' }
& ./dist/passgen-resource-tests.exe
if ($LASTEXITCODE) { throw 'Resource tests failed' }
& g++ @flags -Isrc src/engine.cpp tests/resource_engine_test.cpp -o dist/passgen-resource-engine-tests.exe
if ($LASTEXITCODE) { throw 'Resource engine test build failed' }
& ./dist/passgen-resource-engine-tests.exe
if ($LASTEXITCODE) { throw 'Resource engine tests failed' }
