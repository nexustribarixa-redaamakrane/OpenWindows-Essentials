# engines_ct.ps1 - Build & run the hosted engines smoke test.
#
# Hosted (CRT) test on the development machine. Compiles the freestanding
# engines.owd implementation together with the smoke harness; the host
# CRT is used only by the harness.
#
# Usage: pwsh -File engines_ct.ps1

$gcc  = "C:\w64devkit\bin\gcc.exe"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$ext  = Join-Path $root "Extensions"
$tdir = Join-Path $root "DLL\engines\ctest"

$inc = @("-I$ext")

$engobj = Join-Path $tdir "engines_module.o"
$exe    = Join-Path $tdir "engines_smoke.exe"

& $gcc -std=c99 -O2 -Wall -Wextra @inc `
    -c (Join-Path $tdir "..\engines.c") -o $engobj
if ($LASTEXITCODE -ne 0) { exit 1 }

& $gcc $engobj (Join-Path $tdir "engines_smoke.c") @inc -o $exe
if ($LASTEXITCODE -ne 0) { exit 1 }

& $exe
exit $LASTEXITCODE