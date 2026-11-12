# ============================================================================
# scripts/build.ps1 — ZT-ENTRYPOINT-CORE 编译门禁
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File scripts/build.ps1              # host(gcc/clang) 构建+符号审计
#   powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Target ndk  # NDK r23b armeabi-v7a 构建
#   powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -WerrorAll   # 全部警告升级为错误（严格门禁）
#
# 产物：
#   build_<target>/obj/<module>/*.o
#   build_<target>/lib/lib<module>.a
#
# 审计规则：
#   - libentrypoint.a 不得出现任何非接内核符号（paypass_/contact_/amex_/expresspay_/qpboc_/visa_/jcb_/pure_/discover_）
#   - libentrypoint.a 与 libexpresspay.a 不得出现 eTRACE/eTRACE_HEX 等未定义业务符号
#   - 未定义符号只允许是平台 SDK API_* 与 libc 基础符号
# ============================================================================
param(
    [ValidateSet('host', 'ndk')]
    [string]$Target = 'host',
    [switch]$WerrorAll,
    [switch]$NoAudit,
    [string]$Tag = ''
)

$ErrorActionPreference = 'Continue'
$Root = Split-Path -Parent $PSScriptRoot

# ---- 工具链 ----------------------------------------------------------------
$NDKBin = 'E:\Android\ndk\android-ndk-r23b\toolchains\llvm\prebuilt\windows-x86_64\bin'
if ($Target -eq 'host') {
    $Clang = 'D:\Program Files\LLVM\bin\clang.exe'
    $Ar = 'D:\Program Files\LLVM\bin\llvm-ar.exe'
    $Nm = 'D:\Program Files\LLVM\bin\llvm-nm.exe'
    $TargetFlags = @()
}
else {
    if (-not (Test-Path $NDKBin)) { throw "NDK not found: $NDKBin" }
    $Clang = Join-Path $NDKBin 'armv7a-linux-androideabi16-clang.cmd'
    $Ar = Join-Path $NDKBin 'llvm-ar.exe'
    $Nm = Join-Path $NDKBin 'llvm-nm.exe'
    $TargetFlags = @('--target=armv7a-linux-androideabi16', '-march=armv7-a', '-mfpu=vfpv3-d16', '-mfloat-abi=softfp', '-fPIC')
}

$Suffix = if ([string]::IsNullOrEmpty($Tag)) { "" } else { "_" + $Tag }; $OutRoot = Join-Path $Root ("build_" + $Target + $Suffix)
$LibDir = Join-Path $OutRoot 'lib'

# ---- 公共编译选项 ----------------------------------------------------------
$CommonFlags = @(
    '-std=c99', '-Os', '-fno-common',
    '-ffunction-sections', '-fdata-sections',
    '-Wall', '-Wextra',
    '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-unused-function',
    '-Wno-sign-compare', '-Wno-missing-field-initializers', '-Wno-missing-braces',
    # 静态错误门禁（始终开启）
    '-Werror=implicit-function-declaration',
    '-Werror=incompatible-pointer-types',
    '-Werror=int-conversion',
    '-Werror=return-type',
    '-Werror=implicit-int'
)
if ($WerrorAll) { $CommonFlags += '-Werror' }

# ---- 模块定义 --------------------------------------------------------------
$Modules = @(
    @{
        Name     = 'entrypoint'
        SrcDirs  = @((Join-Path $Root 'src'))
        Includes = @((Join-Path $Root 'inc'), (Join-Path $Root 'include'), (Join-Path $Root 'src\inc'))
    }
)
$ExpresspayDir = Join-Path $Root 'kernels\expresspay'
if (Test-Path (Join-Path $ExpresspayDir 'src')) {
    $Modules += @{
        Name     = 'expresspay'
        SrcDirs  = @((Join-Path $ExpresspayDir 'src'))
        Includes = @((Join-Path $ExpresspayDir 'inc'), (Join-Path $ExpresspayDir 'include'), (Join-Path $Root 'inc'), (Join-Path $Root 'include'), (Join-Path $Root 'src\inc'))
    }
}

$script:Failures = @()
$script:Results = @()

function Compile-One {
    param([string]$File, [string]$ModuleName, [string[]]$Includes)
    $objDir = Join-Path $OutRoot "obj\$ModuleName"
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    $obj = Join-Path $objDir (([System.IO.Path]::GetFileNameWithoutExtension($File)) + '.o')
    $args_ = @()
    $args_ += $CommonFlags
    $args_ += $TargetFlags
    foreach ($i in $Includes) { $args_ += @('-I', $i) }
    $args_ += @('-c', $File, '-o', $obj)
    $out = & $Clang @args_ 2>&1
    if ($LASTEXITCODE -ne 0) {
        $errs = $out | Where-Object { $_ -match 'error:' }
        $script:Failures += "FAIL [$ModuleName] $([System.IO.Path]::GetFileName($File))($($errs.Count) errors)"
        Write-Host ("FAIL [{0}] {1}" -f $ModuleName, $File) -ForegroundColor Red
        $errs | Select-Object -First 8 | ForEach-Object { Write-Host ("      " + $_) -ForegroundColor DarkRed }
        return $null
    }
    $warns = $out | Where-Object { $_ -match 'warning:' }
    if ($warns.Count -gt 0) {
        Write-Host ("WARN [{0}] {1} ({2})" -f $ModuleName, [System.IO.Path]::GetFileName($File), $warns.Count) -ForegroundColor Yellow
        $warns | Select-Object -First 3 | ForEach-Object { Write-Host ("      " + $_) -ForegroundColor DarkYellow }
    }
    $script:Results += "PASS [$ModuleName] $([System.IO.Path]::GetFileName($File))"
    return $obj
}

Write-Host "== ZT-ENTRYPOINT-CORE build gate ==" -ForegroundColor Cyan
Write-Host "   target : $Target"
Write-Host "   clang  : $Clang"
Write-Host "   strict : $WerrorAll"
Write-Host ""

foreach ($m in $Modules) {
    $objs = @()
    foreach ($d in $m.SrcDirs) {
        if (-not (Test-Path $d)) { continue }
        Get-ChildItem $d -File -Filter *.c | Sort-Object Name | ForEach-Object {
            $o = Compile-One -File $_.FullName -ModuleName $m.Name -Includes $m.Includes
            if ($o) { $objs += $o }
        }
    }
    if ($objs.Count -eq 0) {
        Write-Host "SKIP [$($m.Name)] no source" -ForegroundColor DarkGray
        continue
    }
    New-Item -ItemType Directory -Force -Path $LibDir | Out-Null
    $lib = Join-Path $LibDir ("lib{0}.a" -f $m.Name)
    if (Test-Path $lib) { Remove-Item $lib -Force }
    & $Ar rcs $lib @objs | Out-Null
    if ($LASTEXITCODE -ne 0) { $script:Failures += "ARCHIVE FAIL: $($m.Name)"; continue }
    $size = (Get-Item $lib).Length
    Write-Host ("ARCH [{0}] {1} ({2:N0} bytes, {3} objs)" -f $m.Name, $lib, $size, $objs.Count) -ForegroundColor Green
}

Write-Host ""
if ($Failures.Count -gt 0) {
    Write-Host "BUILD FAILED: $($Failures.Count) compile failure(s)" -ForegroundColor Red
    $Failures | ForEach-Object { Write-Host ("  - " + $_) -ForegroundColor Red }
    exit 1
}
Write-Host "BUILD OK ($($Results.Count) translation units)" -ForegroundColor Green
if (-not $NoAudit) {
    & (Join-Path $PSScriptRoot 'symbol_audit.ps1') -Target $Target -Tag $Tag
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
