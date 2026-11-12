# ============================================================================
# scripts/verify.ps1 - full verification for the decoupled Entry Point build
#   - host + NDK build gates (entrypoint core + kernel libs) with symbol audit
#   - struct size report (32-bit ABI) to track AID/TermParam minimization
#   - summary of build artifacts
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify.ps1 [-Tag name]
# ============================================================================
param(
    [string]$Tag = 'verify'
)

$ErrorActionPreference = 'Continue'
$Root = Split-Path -Parent $PSScriptRoot
$results = @()

Write-Host "== 1/3 host gate ==" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'build.ps1') -Tag $Tag
$hostOk = ($LASTEXITCODE -eq 0)
$results += "host gate          : $(if ($hostOk) { 'PASS' } else { 'FAIL' })"

Write-Host "== 2/3 ndk gate ==" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot 'build.ps1') -Target ndk -Tag $Tag
$ndkOk = ($LASTEXITCODE -eq 0)
$results += "ndk gate           : $(if ($ndkOk) { 'PASS' } else { 'FAIL' })"

Write-Host "== 3/3 struct sizes (32-bit ABI) ==" -ForegroundColor Cyan
$tmp = Join-Path $env:TEMP ("ep_verify_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
@'
#include "sdk.h"
#include "emvi.h"
#define S(x) char size_of_##x[sizeof(x)];
S(EMV_TERMPARAM)
S(EMV_AIDPARAM)
S(EMV_TERMAPP)
S(EMV_CANDIDATE)
S(EMV_PREPROCESS_RESULT)
'@ | Set-Content (Join-Path $tmp 'sz.c') -Encoding ascii
$clang = 'D:\Program Files\LLVM\bin\clang.exe'
& $clang -target armv7-none-eabi -ffreestanding -S -I (Join-Path $Root 'inc') -I (Join-Path $Root 'include') -I (Join-Path $Root 'src\inc') (Join-Path $tmp 'sz.c') -o (Join-Path $tmp 'sz.s') 2>$null
if (Test-Path (Join-Path $tmp 'sz.s')) {
    Select-String -Path (Join-Path $tmp 'sz.s') -Pattern '\.size\s+size_of_' | ForEach-Object {
        $line = $_.Line.Trim()
        $name = ($line -split '\s+')[1] -replace '^size_of_', ''
        $size = ($line -split ',')[1].Trim()
        $results += ("struct {0,-24}: {1} bytes" -f $name, $size)
        Write-Host ("  {0,-24} {1} bytes" -f $name, $size)
    }
}
Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "== artifact summary ==" -ForegroundColor Cyan
foreach ($t in @('host', 'ndk')) {
    $libDir = Join-Path $Root ("build_" + $t + "_" + $Tag + "\lib")
    if (Test-Path $libDir) {
        Get-ChildItem $libDir -File -Filter *.a | ForEach-Object {
            Write-Host ("  [{0}] {1} = {2:N0} bytes" -f $t, $_.Name, $_.Length)
        }
    }
}

Write-Host ""
Write-Host "== verify summary ==" -ForegroundColor Cyan
$results | ForEach-Object { Write-Host "  $_" }
if ($hostOk -and $ndkOk) { Write-Host "VERIFY OK" -ForegroundColor Green; exit 0 }
Write-Host "VERIFY FAILED" -ForegroundColor Red
exit 1
