# ============================================================================
# scripts/test_host.ps1 - Entrypoint + AMEX Expresspay host end-to-end gate
#
# One command does everything:
#   1) builds libentrypoint.a + libexpresspay.a (host, via scripts/build.ps1)
#   2) compiles tests/stub/stub_platform.c + tests/host_e2e.c
#   3) links them into tests/build/host_e2e.exe (this link itself is the
#      "no unresolved platform symbol" gate)
#   4) runs the test and returns its exit code (0 = PASS, 1 = FAIL)
#
# It uses its own output directory (tests/build) and never touches
# build_host/ or build_ndk/ except by invoking the normal build gate.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_host.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_host.ps1 -SkipBuild
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_host.ps1 -Tag amex
# ============================================================================
param(
    [switch]$SkipBuild,
    [string]$Tag = ''
)

$ErrorActionPreference = 'Continue'

$Root   = Split-Path -Parent $PSScriptRoot
$Clang  = 'D:\Program Files\LLVM\bin\clang.exe'
$Suffix = if ([string]::IsNullOrEmpty($Tag)) { '' } else { '_' + $Tag }
$BuildDir = Join-Path $Root ('build_host' + $Suffix)
$LibDir   = Join-Path $BuildDir 'lib'
$TestDir  = Join-Path $Root 'tests'
$OutDir   = Join-Path $TestDir 'build'
$Exe      = Join-Path $OutDir 'host_e2e.exe'

Write-Host "== Entrypoint host E2E gate ==" -ForegroundColor Cyan
Write-Host "   root     : $Root"
Write-Host "   test out : $OutDir"
Write-Host ""

if (-not (Test-Path $Clang)) {
    Write-Host "FAIL: clang not found: $Clang" -ForegroundColor Red
    exit 1
}

# ---- 1) product libraries ---------------------------------------------------
if (-not $SkipBuild) {
    Write-Host "-- building product libraries (scripts/build.ps1 -NoAudit) --" -ForegroundColor DarkGray
    $buildArgs = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', (Join-Path $PSScriptRoot 'build.ps1'),
        '-Target', 'host', '-NoAudit'
    )
    if (-not [string]::IsNullOrEmpty($Tag)) { $buildArgs += @('-Tag', $Tag) }
    & powershell @buildArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "HOST E2E: FAIL (product build failed)" -ForegroundColor Red
        exit 1
    }
}

$LibEp = Join-Path $LibDir 'libentrypoint.a'
$LibEx = Join-Path $LibDir 'libexpresspay.a'
foreach ($lib in @($LibEp, $LibEx)) {
    if (-not (Test-Path $lib)) {
        Write-Host "FAIL: missing library: $lib" -ForegroundColor Red
        Write-Host "      run without -SkipBuild, or build it with scripts\\build.ps1 -Tag $Tag" -ForegroundColor Red
        exit 1
    }
}

# ---- 2) compile tests -------------------------------------------------------
$Inc = @(
    (Join-Path $Root 'include'),
    (Join-Path $Root 'inc'),
    (Join-Path $Root 'src\inc'),
    (Join-Path $Root 'kernels\expresspay\inc'),
    (Join-Path $Root 'kernels\expresspay\include'),
    (Join-Path $TestDir 'stub')
)
$Cflags = @(
    '-std=c99', '-Os', '-fno-common', '-g',
    '-Wall', '-Wextra',
    '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-function',
    '-Wno-sign-compare', '-Wno-missing-field-initializers', '-Wno-missing-braces'
)
$IncArgs = @()
foreach ($i in $Inc) { $IncArgs += @('-I', $i) }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$Objs = @()
foreach ($src in @((Join-Path $TestDir 'stub\stub_platform.c'), (Join-Path $TestDir 'host_e2e.c'))) {
    $obj = Join-Path $OutDir (([System.IO.Path]::GetFileNameWithoutExtension($src)) + '.o')
    $args = @()
    $args += $Cflags
    $args += $IncArgs
    $args += @('-c', $src, '-o', $obj)
    Write-Host "-- compiling $([System.IO.Path]::GetFileName($src))" -ForegroundColor DarkGray
    $out = & $Clang @args 2>&1
    if ($LASTEXITCODE -ne 0) {
        $out | Where-Object { $_ -match 'error:|warning:' } | Select-Object -First 20 |
            ForEach-Object { Write-Host ("   " + $_) -ForegroundColor Red }
        Write-Host "HOST E2E: FAIL (test compile failed: $([System.IO.Path]::GetFileName($src)))" -ForegroundColor Red
        exit 1
    }
    $Objs += $obj
}

# ---- 3) link: objects + libexpresspay.a + libentrypoint.a ------------------
# /WHOLEARCHIVE pulls *every* object out of both static libraries, so this link
# fails if ANY translation unit in either library still has an unresolved
# external symbol.  That makes the link itself the full "no unresolved platform
# symbol" gate (not just the objects reachable from the test).
Write-Host "-- linking (whole-archive unresolved-symbol gate)" -ForegroundColor DarkGray
$LinkArgs = @()
$LinkArgs += $Objs
$LinkArgs += @(
    ("-Wl,/WHOLEARCHIVE:" + $LibEx),
    ("-Wl,/WHOLEARCHIVE:" + $LibEp),
    '-o', $Exe
)
$linkOut = & $Clang @LinkArgs 2>&1
if ($LASTEXITCODE -ne 0) {
    $linkOut | Select-Object -First 30 | ForEach-Object { Write-Host ("   " + $_) -ForegroundColor Red }
    Write-Host "HOST E2E: FAIL (link gate: unresolved symbols or library error)" -ForegroundColor Red
    exit 1
}
Write-Host "   linked: $Exe (whole-archive: libexpresspay.a + libentrypoint.a)" -ForegroundColor Green

# ---- 4) run -----------------------------------------------------------------
Write-Host ""
Write-Host "-- running $Exe --" -ForegroundColor DarkGray
$testOut = & $Exe 2>&1
$testExit = $LASTEXITCODE
$testOut | ForEach-Object { Write-Host $_ }
Write-Host ""

$assertions = '?'
$failures = '?'
$knownFails = '?'
foreach ($line in $testOut) {
    if ($line -match 'assertions:\s*(\d+),\s*failures:\s*(\d+),\s*known-fails.*:\s*(\d+)') {
        $assertions = $Matches[1]
        $failures = $Matches[2]
        $knownFails = $Matches[3]
    }
}

if ($testExit -eq 0) {
    Write-Host ("HOST E2E: PASS (assertions={0}, failures={1}, known-fails={2})" -f $assertions, $failures, $knownFails) -ForegroundColor Green
    exit 0
}

Write-Host ("HOST E2E: FAIL (assertions={0}, failures={1}, known-fails={2})" -f $assertions, $failures, $knownFails) -ForegroundColor Red
exit 1
