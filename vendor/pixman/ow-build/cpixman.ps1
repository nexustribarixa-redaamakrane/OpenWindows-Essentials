# cpixman.ps1 - Build the genuine pixman subtree (vendor/pixman) into an archive.
#
# The vendored pixman sources are compiled verbatim (never edited) into
# libpixman_vendor.a. Generated config headers live in this ow-build dir and
# are injected via -iquote/-I. Freestanding C99; no SIMD, no libc, no OS.
#
# Usage: pwsh -File cpixman.ps1
param(
    [string]$Build = "$PSScriptRoot\build"
)

$ErrorActionPreference = "Stop"
$gcc = "C:\w64devkit\bin\gcc.exe"
$ar  = "C:\w64devkit\bin\ar.exe"
$root = "C:\Users\KARIMABENDA\Documents\OpenWindows-Essentials"
$pixman = Join-Path $root "vendor\pixman"
$src = Join-Path $pixman "pixman"
$owb = Join-Path $pixman "ow-build"

New-Item -ItemType Directory -Force -Path $Build | Out-Null
$obj = Join-Path $Build "obj"
New-Item -ItemType Directory -Force -Path $obj | Out-Null

$cflags = @(
    "-std=c99",
    "-ffreestanding",
    "-nostdlib",
    "-O2",
    "-w",
    "-Wno-incompatible-pointer-types",
    "-fno-stack-protector",
    "-fno-exceptions",
    "-fno-builtin",
    "-DHAVE_CONFIG_H",
    "-DPIXMAN_NO_TLS",
    "-I", $owb,
    "-I", $src
)

# Core sources (meson pixman_files). SIMD/arch-specific members (mmx, sse2,
# ssse3, vmx, power8/9, arm*, mips-dspr2, rvv, loongson) are disabled for the
# OpenWindows target. pixman-region.c is #included by region16/32/64f and so
# is not compiled standalone.
$sources = @(
    "pixman.c",
    "pixman-access.c",
    "pixman-access-accessors.c",
    "pixman-arm.c",
    "pixman-bits-image.c",
    "pixman-combine32.c",
    "pixman-combine-float.c",
    "pixman-conical-gradient.c",
    "pixman-edge.c",
    "pixman-edge-accessors.c",
    "pixman-fast-path.c",
    "pixman-filter.c",
    "pixman-glyph.c",
    "pixman-general.c",
    "pixman-gradient-walker.c",
    "pixman-image.c",
    "pixman-implementation.c",
    "pixman-linear-gradient.c",
    "pixman-matrix.c",
    "pixman-mips.c",
    "pixman-noop.c",
    "pixman-ppc.c",
    "pixman-radial-gradient.c",
    "pixman-region16.c",
    "pixman-region32.c",
    "pixman-region64f.c",
    "pixman-riscv.c",
    "pixman-solid-fill.c",
    "pixman-timer.c",
    "pixman-trap.c",
    "pixman-utils.c",
    "pixman-x86.c"
)

$fails = @()
foreach ($sf in $sources) {
    $o = Join-Path $obj ($sf + ".o")
    & $gcc @cflags -c (Join-Path $src $sf) -o $o 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $fails += $sf
        Write-Host "FAIL  $sf"
        & $gcc @cflags -c (Join-Path $src $sf) -o $o 2>&1 | Select-Object -First 8 | ForEach-Object { Write-Host "      $_" }
    }
}
Write-Host "pixman compile failures: $($fails.Count) / $($sources.Count)"

$lib = Join-Path $Build "libpixman_vendor.a"
if ($fails.Count -eq 0) {
    & $ar rcs $lib (Get-ChildItem $obj -Filter *.o).FullName
    Write-Host "archive: $lib"
} else {
    Write-Error "pixman build failed; no archive produced."
    exit 1
}
