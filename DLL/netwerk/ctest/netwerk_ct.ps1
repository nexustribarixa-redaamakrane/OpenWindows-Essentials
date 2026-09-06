# netwerk_ct.ps1 - Build & run the hosted netwerk smoke test.
#
# Hosted (CRT) test on the development machine. Compiles the freestanding
# netwerk.owd + owcrypt.owd implementations together with the smoke
# harness and a k64 memory facsimile; the host CRT is used only by the
# harness/facsimile.
#
# Usage: pwsh -File netwerk_ct.ps1

$gcc  = "C:\w64devkit\bin\gcc.exe"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$ext  = Join-Path $root "Extensions"
$k64i = Join-Path $root "DLL\kernel64"
$tdir = Join-Path $root "DLL\netwerk\ctest"

$inc = @("-I$ext", "-I$k64i")

$netobj = Join-Path $tdir "netwerk_module.o"
$owcobj = Join-Path $tdir "owcrypt_module.o"
$exe    = Join-Path $tdir "netwerk_smoke.exe"

& $gcc -std=c99 -O2 -Wall -Wextra @inc `
    -c (Join-Path $tdir "..\netwerk.c") -o $netobj
if ($LASTEXITCODE -ne 0) { exit 1 }

& $gcc -std=c99 -O2 -Wall -Wextra -Wno-pedantic @inc `
    -c (Join-Path $root "DLL\owcrypt\owcrypt.c") -o $owcobj
if ($LASTEXITCODE -ne 0) { exit 1 }

& $gcc $netobj $owcobj (Join-Path $tdir "netwerk_smoke.c") @inc -o $exe
if ($LASTEXITCODE -ne 0) { exit 1 }

& $exe
exit $LASTEXITCODE