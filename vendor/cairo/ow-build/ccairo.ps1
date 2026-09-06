# ccairo.ps1 - Build the genuine cairo subtree (vendor/cairo) into an archive.
#
# The vendored cairo sources are compiled verbatim (never edited) into
# libcairo64_vendor.a. Generated config headers live in this ow-build dir and
# are injected via -iquote/-I. Compiled over the genuine pixman subtree.
# Freestanding C99; image/recording/user-font surface set only.
#
# Usage: pwsh -File ccairo.ps1
param(
    [string]$Build = "$PSScriptRoot\build"
)

$ErrorActionPreference = "Stop"
$gcc = "C:\w64devkit\bin\gcc.exe"
$ar  = "C:\w64devkit\bin\ar.exe"
$root = "C:\Users\KARIMABENDA\Documents\OpenWindows-Essentials"
$cairo   = Join-Path $root "vendor\cairo"
$pixman  = Join-Path $root "vendor\pixman"
$cairosrc = Join-Path $cairo "src"
$pixsrc   = Join-Path $pixman "pixman"
$pixowb   = Join-Path $pixman "ow-build"
$owb = Join-Path $cairo "ow-build"

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
    "-Wno-int-conversion",
    "-fno-stack-protector",
    "-fno-exceptions",
    "-fno-builtin",
    "-DCAIRO_WIN32_STATIC_BUILD",
    "-DCAIRO_COMPILATION",
    "-iquote", $owb,
    "-I", $cairosrc,
    "-I", $pixsrc,
    "-I", $pixowb
)

# Genuine source list: parsed from the subtree's own src/meson.build so it
# always matches the vendored tree (cairo_sources, meson caiero build).
$srcList = @()
$inList = $false
foreach ($line in (Get-Content (Join-Path $cairosrc "meson.build"))) {
    if ($line -match '^cairo_sources = \[') { $inList = $true; continue }
    if ($inList -and $line -match '^\]') { break }
    if ($inList -and $line -match "'([^']+\.c)'") { $srcList += $Matches[1] }
}
if ($srcList.Count -eq 0) {
    Write-Error "Could not parse cairo_sources from src/meson.build"
    exit 1
}

$fails = @()
foreach ($sf in $srcList) {
    $o = Join-Path $obj ($sf + ".o")
    & $gcc @cflags -c (Join-Path $cairosrc $sf) -o $o 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $fails += $sf
        Write-Host "FAIL  $sf"
        & $gcc @cflags -c (Join-Path $cairosrc $sf) -o $o 2>&1 | Select-Object -First 8 | ForEach-Object { Write-Host "      $_" }
    }
}
Write-Host "cairo compile failures: $($fails.Count) / $($srcList.Count)"

$lib = Join-Path $Build "libcairo64_vendor.a"
if ($fails.Count -eq 0) {
    & $ar rcs $lib (Get-ChildItem $obj -Filter *.o).FullName
    Write-Host "archive: $lib"
} else {
    Write-Error "cairo build failed; no archive produced."
    exit 1
}