# cairo64_ct.ps1 - Build & run the hosted cairo64 smoke test.
#
# Hosted (CRT) test on the development machine: links the genuine cairo and
# pixman vendor archives against the host libc, so the renderer can actually
# run and render pixels. Not part of the freestanding module build.
#
# Usage: pwsh -File cairo64_ct.ps1

$gcc = "C:\w64devkit\bin\gcc.exe"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$src   = Join-Path $root "vendor\cairo"
$pix   = Join-Path $root "vendor\pixman"
$ext   = Join-Path $root "Extensions"
$tdir  = Join-Path $root "DLL\cairo64\ctest"

$cairo_a = Join-Path $src "ow-build\build\libcairo64_vendor.a"
$pix_a   = Join-Path $pix "ow-build\build\libpixman_vendor.a"

foreach ($f in @($cairo_a, $pix_a)) {
    if (-not (Test-Path -LiteralPath $f)) {
        Write-Error "Missing vendor archive: $f (run ccairo.ps1/cpixman.ps1 first)"
        exit 1
    }
}

$obj = Join-Path $tdir "cairo64_smoke.o"
$exe = Join-Path $tdir "cairo64_smoke.exe"

& $gcc -std=c99 -O2 -Wall -Wextra "-DCAIRO_WIN32_STATIC_BUILD" `
    "-I$ext" `
    -c (Join-Path $tdir "cairo64_smoke.c") -o $obj
if ($LASTEXITCODE -ne 0) { exit 1 }

& $gcc $obj $cairo_a $pix_a -o $exe
if ($LASTEXITCODE -ne 0) { exit 1 }

& $exe
exit $LASTEXITCODE