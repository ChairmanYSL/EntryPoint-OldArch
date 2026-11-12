# ============================================================================
# scripts/symbol_audit.ps1 - static library symbol audit (decoupling guard)
#
# For a static library, "undefined" means undefined by ALL members (link-time
# external dependency). Checks:
#   1) libentrypoint.a external deps only: platform SDK API_* / libc / app symbols
#   2) libentrypoint.a must not define or reference any contactless kernel symbol
#   3) no unresolved trace macro symbols (eTRACE / eTRACE_HEX / TRACE ...)
# NOTE: ASCII-only file on purpose (works with both powershell.exe and pwsh).
# ============================================================================
param(
    [ValidateSet('host', 'ndk')]
    [string]$Target = 'host'
)

$ErrorActionPreference = 'Continue'
$Root = Split-Path -Parent $PSScriptRoot
$LibDir = Join-Path $Root "build_$Target\lib"

if ($Target -eq 'host') { $Nm = 'D:\Program Files\LLVM\bin\llvm-nm.exe' }
else { $Nm = 'E:\Android\ndk\android-ndk-r23b\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-nm.exe' }

if (-not (Test-Path $LibDir)) { Write-Host "no lib dir: $LibDir" -ForegroundColor Red; exit 1 }

$KernelPrefixes = @('paypass_', 'contact_', 'amex_', 'expresspay_', 'qpboc_', 'visa_', 'jcb_', 'pure_', 'discover_', 'disv_', 'express_')

$AllowedPrefixes = @(
    'API_', '__aeabi_', '__android_', '__stack_chk',
    '_memcpy', '_memset', '_memmove', '_memcmp', '_strlen', '_strcpy', '_snprintf', '_vsnprintf',
    'memcpy', 'memset', 'memmove', 'memcmp', 'strlen', 'strcpy', 'strncpy', 'strcmp', 'strtok',
    'sprintf', 'snprintf', 'vsprintf', 'vsnprintf', 'printf', '__stdio_common_',
    'atoi', 'strtol', 'toupper', 'tolower'
)
$AllowedExact = @(
    '__errno', 'errno',
    'ics_option', 'CreateEMVDiscretionaryData'
)

$violations = @()
$reports = @()

foreach ($lib in Get-ChildItem $LibDir -File -Filter '*.a' | Sort-Object Name) {
    $module = $lib.BaseName -replace '^lib', ''
    $raw = & $Nm $lib.FullName 2>$null
    $defined = @()
    $undef = @()
    foreach ($line in $raw) {
        if ($line -match '^\s*$') { continue }
        if ($line -match '^[^:]+:\s*$') { continue }
        $parts = $line.Trim() -split '\s+'
        if ($parts.Count -lt 2) { continue }
        $type = $parts[0]
        $name = $parts[-1]
        if ($type -eq 'U') { $undef += $name } else { $defined += $name }
    }
    $defined = $defined | Sort-Object -Unique
    $undef = $undef | Sort-Object -Unique
    $external = $undef | Where-Object { $defined -notcontains $_ }

    $reports += ("[{0}] defined={1} undefined={2} external={3}" -f $module, $defined.Count, $undef.Count, $external.Count)

    foreach ($d in $defined) {
        foreach ($p in $KernelPrefixes) {
            if ($d -like "$p*") { $violations += "[$module] defines kernel symbol: $d"; break }
        }
        if ($d -match '^(eTRACE|eTRACE_HEX|TRACE|TRACE_HEX|apduTRACE|htTRACE|tmTRACE)') {
            $violations += "[$module] defines trace macro symbol: $d"
        }
    }

    foreach ($s in $external) {
        $bad = $false
        foreach ($p in $KernelPrefixes) { if ($s -like "$p*") { $bad = $true; break } }
        if ($s -match '^(eTRACE|eTRACE_HEX|TRACE|TRACE_HEX|apduTRACE|apduTRACE_HEX|htTRACE|htTRACE_HEX|tmTRACE|tmTRACE_HEX)$') { $bad = $true }
        if ($bad) { $violations += "[$module] forbidden external symbol: $s"; continue }

        $allowed = $AllowedExact -contains $s
        if (-not $allowed) { foreach ($p in $AllowedPrefixes) { if ($s -like "$p*") { $allowed = $true; break } } }
        if (-not $allowed) { $violations += "[$module] unexpected external symbol: $s" }
    }
}

$reports | ForEach-Object { Write-Host "  $_" -ForegroundColor DarkGray }

if ($violations.Count -gt 0) {
    Write-Host ""
    Write-Host "SYMBOL AUDIT FAILED ($($violations.Count) violation(s))" -ForegroundColor Red
    $violations | Sort-Object -Unique | Select-Object -First 60 | ForEach-Object { Write-Host ("  - " + $_) -ForegroundColor Red }
    exit 1
}
Write-Host "SYMBOL AUDIT OK" -ForegroundColor Green
exit 0
