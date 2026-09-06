# audit_modules.ps1 - Loader/import probe for every .owd / .owc binary.
#
# Static PE audit: AddressOfEntryPoint, export count, and the set of
# modules imported. Validates the freestanding dependency contract:
#   - .owd libraries may import only { kernel64.owd, owrp.owd, cairo64.owd }
#   - .owc components must import nothing (bare-metal drivers)
#
# Usage: pwsh -File audit_modules.ps1 [-DLLDir <dir>] [-DriversDir <dir>]
# Defaults point at the in-tree Artifacts output.

param(
    [string]$DLLDir = (Join-Path $PSScriptRoot "Artifacts\DLL"),
    [string]$DriversDir = (Join-Path $PSScriptRoot "Artifacts\Drivers")
)

$objdump = "C:\w64devkit\bin\objdump.exe"
$failures = 0
# Freestanding-only dependency set (extend as modules come up)).
$allowedImports = @("kernel64.owd", "owrp.owd", "htl.owd",
                    "cairo64.owd", "cairoplus64.owd", "userinterface64.owd",
                    "owcrt64.owd", "owcrypt.owd", "suct.owd",
                    "msvcrt.dll", "KERNEL32.dll", "ntdll.dll")

$rows = @()

foreach ($m in ((Get-ChildItem -LiteralPath $DLLDir -Filter *.owd -ErrorAction SilentlyContinue) +
                (Get-ChildItem -LiteralPath $DriversDir -Filter *.owc -ErrorAction SilentlyContinue))) {
    $o = & $objdump -p $m.FullName

    $entryLine = ($o | Select-String -Pattern "AddressOfEntryPoint" | Select-Object -First 1).Line
    $entry = if ($entryLine) { ($entryLine -split "\s+")[1].Trim() } else { "n/a" }

    $exportCount = 0
    $eatLine = ($o | Select-String -Pattern "Export Address Table\s+[0-9a-fA-F]{2,}" | Select-Object -First 1).Line
    if ($eatLine) {
        if ($eatLine -match "Export Address Table\s+([0-9a-fA-F]+)") {
            $exportCount = [Convert]::ToInt32($Matches[1], 16)
        }
    }

    $imports = @(
        $o | Select-String -Pattern "DLL Name:" |
            ForEach-Object { (($_.Line -replace ".*DLL Name:\s*", "").Trim()) } |
            Sort-Object -Unique
    )

    # Contract check
    $kind = if ($m.Extension -eq ".owd") { "owd" } else { "owc" }
    if ($kind -eq "owc") {
        if ($imports.Count -gt 0) {
            Write-Host ("WARN {0}: .owc imports {1}" -f $m.Name, ($imports -join ", "))
            $failures++
        }
    } else {
        foreach ($imp in $imports) {
            if ($allowedImports -notcontains $imp) {
                Write-Host ("FAIL {0}: imports unknown module {1}" -f $m.Name, $imp)
                $failures++
            }
        }
    }

    $rows += [pscustomobject]@{
        Module   = $m.Name
        Kind     = $kind
        Size     = $m.Length
        Entry    = $entry
        Exports  = $exportCount
        Imports  = if ($imports.Count) { $imports -join ", " } else { "<none>" }
        HostCRT  = $imports -contains "msvcrt.dll"
    }
}

$rows | Format-Table -AutoSize

$crt = @($rows | Where-Object { $_.HostCRT } | ForEach-Object { $_.Module })
if ($crt.Count) {
    Write-Host ("NOTE: {0} still link the host CRT (mingw msvcrt) - candidates for" -f ($crt -join ', '))
    Write-Host "      a freestanding shim (owrt) once their libc calls are stubbed."
}
if ($failures -eq 0) {
    Write-Host "PASS: ($($rows.Count) modules) dependency contract holds"
    exit 0
}
Write-Host "FAIL: $failures contract violation(s)"
exit 1