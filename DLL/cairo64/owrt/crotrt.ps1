# crotrt.ps1 — build the freestanding runtime shims (owrt) into an archive.
# These are OUR code and compile under the strict project profile.
# Usage: pwsh -File crotrt.ps1 [-Build <dir>]
param(
    [string]$Build = "$PSScriptRoot\build"
)

$ErrorActionPreference = "Stop"
$gcc = "C:\w64devkit\bin\gcc.exe"
$ar  = "C:\w64devkit\bin\ar.exe"
$root = "C:\Users\KARIMABENDA\Documents\OpenWindows-Essentials"

New-Item -ItemType Directory -Force -Path $Build | Out-Null
$obj = Join-Path $Build "obj"
New-Item -ItemType Directory -Force -Path $obj | Out-Null

$cflags = @(
    "-std=c99",
    "-ffreestanding",
    "-nostdlib",
    "-O2",
    "-Wall", "-Wextra", "-Werror", "-Wpedantic",
    "-fno-builtin",
    "-fno-stack-protector",
    "-fno-exceptions",
    "-I", "$root\DLL",
    "-I", "$PSScriptRoot"
)

$sources = @("ow_alloc.c", "ow_str.c", "ow_fmt.c", "ow_math.c", "ow_crt.c")
$fails = @()
foreach ($sf in $sources) {
    $o = Join-Path $obj ($sf + ".o")
    & $gcc @cflags -c (Join-Path $PSScriptRoot $sf) -o $o 2>&1 | Tee-Object -Variable err | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $fails += $sf
        Write-Host "FAIL  $sf"
        $err | Select-Object -First 12 | ForEach-Object { Write-Host "      $_" }
    }
}

# setjmp/longjmp pair (assembler; frame layout per mingw-w64 __JUMP_BUFFER)
& $gcc -c (Join-Path $PSScriptRoot "ow_jmp.s") -o (Join-Path $obj "ow_jmp.o") 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { $fails += "ow_jmp.s" }

# Big-frame stack probe (___chkstk_ms) and software popcount come straight
# from GCC's libgcc rather than the CRT — proven objects, no OS import.
$libgcc = Get-ChildItem "C:\w64devkit\lib\gcc\x86_64-w64-mingw32" -Recurse -Filter libgcc.a |
    Select-Object -First 1
if ($libgcc) {
    Push-Location $obj
    & $ar x $libgcc.FullName _chkstk_ms.o _popcount_tab.o _popcountdi2.o _popcountsi2.o 2>&1 | Out-Null
    Pop-Location
}
Write-Host "owrt compile failures: $($fails.Count) / $($sources.Count)"

$lib = Join-Path $Build "libowrt64.a"
& $ar rcs $lib (Get-ChildItem $obj -Filter *.o).FullName
Write-Host "archive: $lib"