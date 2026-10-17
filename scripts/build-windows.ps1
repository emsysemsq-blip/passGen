$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
New-Item -ItemType Directory -Force dist | Out-Null
# Run from an MSYS2 UCRT64 terminal with mingw-w64-ucrt-x86_64-gcc installed.
$flags = @('-std=c++17','-O3','-Wall','-Wextra','-pthread','-static','-static-libgcc','-static-libstdc++')
& windres --preprocessor=g++ --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED -I src src/passgen.rc -O coff -o dist/passgen-res.o
if ($LASTEXITCODE) { throw 'Windows resource build failed' }
& g++ @flags -municode -mwindows src/engine.cpp src/gpu.cpp src/winmain.cpp dist/passgen-res.o -Wl,--no-insert-timestamp -lcomdlg32 -lcomctl32 -o dist/PassGen.exe
if ($LASTEXITCODE) { throw 'GUI build failed' }
& g++ @flags -municode src/engine.cpp src/gpu.cpp src/cli.cpp -Wl,--no-insert-timestamp -o dist/passgen-cli.exe
if ($LASTEXITCODE) { throw 'CLI build failed' }
& g++ @flags -Isrc src/engine.cpp src/gpu.cpp tests/engine_test.cpp -o dist/passgen-tests.exe
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

& g++ @flags -Isrc src/engine.cpp src/gpu.cpp tests/checkpoint_lock_test.cpp -o dist/passgen-checkpoint-lock-tests.exe
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
& g++ @flags -Isrc src/engine.cpp src/gpu.cpp tests/resource_engine_test.cpp -o dist/passgen-resource-engine-tests.exe
if ($LASTEXITCODE) { throw 'Resource engine test build failed' }
& ./dist/passgen-resource-engine-tests.exe
if ($LASTEXITCODE) { throw 'Resource engine tests failed' }

& g++ @flags -Isrc src/gpu.cpp tests/gpu_test.cpp -o dist/passgen-gpu-tests.exe
if ($LASTEXITCODE) { throw 'GPU test build failed' }
& ./dist/passgen-gpu-tests.exe
if ($LASTEXITCODE) { throw 'GPU tests failed' }
& g++ @flags -Isrc src/engine.cpp src/gpu.cpp tests/gpu_engine_test.cpp -o dist/passgen-gpu-engine-tests.exe
if ($LASTEXITCODE) { throw 'GPU integration build failed' }
& ./dist/passgen-gpu-engine-tests.exe
if ($LASTEXITCODE) { throw 'GPU integration tests failed' }
