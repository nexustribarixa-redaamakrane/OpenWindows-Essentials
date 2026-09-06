# cairo64_ct.ps1 - Build & run the hosted cairoplus64 smoke test.
#
# Hosted (CRT) test on the development machine: links the genuine
# cairoplus64 module implementation against the cairo/pixman vendor
# archives and the host libc, so the renderer can actually run and render
# pixels. The k64 API is a local facsimile. The owrt archive is NEVER
# linked here (its malloc shadows the CRT allocator).
#
# Usage: pwsh -File cairoplus64_ct.ps1

$gcc = "C:\w64devkit\bin\gcc.exe"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$src   = Join-Path $root "vendor\cairo"
$pix   = Join-Path $root "vendor\pixman"
$ext   = Join-Path $root "Extensions"
$k64i  = Join-Path $root "DLL\kernel64"
$tdir  = Join-Path $root "DLL\cairoplus64\ctest"

$cairo_a = Join-Path $src "ow-build\build\libcairo64_vendor.a"
$pix_a   = Join-Path $pix "ow-build\build\libpixman_vendor.a"

foreach ($f in @($cairo_a, $pix_a)) {
    if (-not (Test-Path -LiteralPath $f)) {
        Write-Error "Missing vendor archive: $f (run ccairo.ps1/cpixman.ps1 first)"
        exit 1
    }
}

$inc = @("-I$ext", "-I$k64i")
$obj = Join-Path $tdir "cairoplus64_modules.o"

& $gcc -std=c99 -O2 -Wall -Wextra "-DCAIRO_WIN32_STATIC_BUILD" `
    @inc `
    -c (Join-Path $tdir "..\cairoplus64.c") -o $obj
if ($LASTEXITCODE -ne 0) { exit 1 }

$exe = Join-Path $tdir "cairoplus64_smoke.exe"
& $gcc $obj (Join-Path $tdir "cairoplus64_smoke.c") `
    @inc "-DCAIRO_WIN32_STATIC_BUILD" $cairo_a $pix_a -o $exe
if ($LASTEXITCODE -ne 0) { exit 1 }

& $exe
exit $LASTEXITCODE