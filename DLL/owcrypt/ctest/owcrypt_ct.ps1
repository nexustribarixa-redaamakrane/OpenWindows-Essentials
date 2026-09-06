# owcrypt_ct.ps1 - Build & run the hosted owcrypt smoke test.
#
# Hosted (CRT) test on the development machine. Compiles the freestanding
# owcrypt.owd implementation together with the smoke harness and a k64
# memory facsimile; the host CRT is used only by the harness/facsimile.
#
# Usage: pwsh -File owcrypt_ct.ps1

$gcc  = "C:\w64devkit\bin\gcc.exe"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$ext  = Join-Path $root "Extensions"
$k64i = Join-Path $root "DLL\kernel64"
$tdir = Join-Path $root "DLL\owcrypt\ctest"

$inc = @("-I$ext", "-I$k64i", "-I", (Join-Path $root "DLL\owrp"))
$obj = Join-Path $tdir "owcrypt_module.o"
$exe = Join-Path $tdir "owcrypt_smoke.exe"

& $gcc -std=c99 -O2 -Wall -Wextra @inc `
    -c (Join-Path $tdir "..\owcrypt.c") -o $obj
if ($LASTEXITCODE -ne 0) { exit 1 }

& $gcc $obj (Join-Path $tdir "owcrypt_smoke.c") @inc -o $exe
if ($LASTEXITCODE -ne 0) { exit 1 }

& $exe
exit $LASTEXITCODE