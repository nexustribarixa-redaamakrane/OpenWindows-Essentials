# owrt_self.ps1 — compile & run the owrt shim self-test (hosted probe).
#
# The probe links the owrt MEMBER OBJECTS directly, never the archive:
# the archive's malloc/free shadow the CRT allocator at link time (the
# CRT startup calls malloc, which would route into the k64-backed
# allocator and recurse through the stub). Linking the exercised units
# in isolation keeps the probe hermetic.
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))   # repo root
$gcc  = 'C:\w64devkit\bin\gcc.exe'
$ct   = $PSScriptRoot
$obj  = Join-Path (Join-Path $root 'DLL\cairo64\owrt') 'build\obj'
$tmp  = Join-Path $env:TEMP 'opencode'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

$out = Join-Path $tmp 'owrt_self.exe'
$src = Join-Path $ct 'owrt_self.c'

$units = @(
    'ow_jmp.o',
    'ow_math.c.o',
    'ow_fmt.c.o',
    'ow_str.c.o',
    'ow_crt.c.o'
) | ForEach-Object { Join-Path $obj $_ }

& $gcc -O2 -o $out $src $units
if ($LASTEXITCODE -ne 0) { throw 'owrt self-test compile failed' }

& $out
if ($LASTEXITCODE -ne 0) { throw "owrt self-test FAILED (exit=$LASTEXITCODE)" }
Write-Output 'PASS: owrt self-test (hosted)'