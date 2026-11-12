# 构建与门禁（BUILD）

## 1. 前置条件（本机已验证）

| 工具 | 路径/版本 | 用途 |
|---|---|---|
| clang | `D:\Program Files\LLVM\bin\clang.exe` | host 编译门禁 |
| NDK r23b | `E:\Android\ndk\android-ndk-r23b`（clang 12.0.8） | armeabi-v7a 目标编译门禁 |
| PowerShell | 5.1 / 7 | 运行构建脚本 |
| git | 2.40 | 分支/提交 |

无需 IAR（IAR 工程文件仍保留，但本机无法验证；见 §6）。

## 2. 命令

```powershell
cd E:\Work\ZT\Entrypoint

# 默认：host 构建 + 符号审计（libentrypoint.a + kernels/* 若存在）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1

# NDK r23b armeabi-v7a 构建 + 符号审计
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Target ndk

# 并行开发用独立输出目录（build_<target>_<tag>）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Tag c
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Target ndk -Tag c

# 只审计已有产物
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\symbol_audit.ps1 -Target host
```

产物：
```
build_host/obj/entrypoint/*.o
build_host/lib/libentrypoint.a
build_host/lib/libexpresspay.a        （kernels/expresspay/src 存在时）
build_ndk/…                            （NDK 目标，同结构）
```

## 3. 编译选项（scripts/build.ps1）

- `-std=c99 -Os -ffunction-sections -fdata-sections -fno-common`
- `-Wall -Wextra`（常见噪音关闭），**静态错误一律为 error**：
  `-Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -Werror=return-type -Werror=implicit-int`
- `-WerrorAll`：全部警告升级为 error。**2026-11-12 起全库 0 warning，`verify.ps1` 默认以此严格模式构建（host + NDK）**。
- 宏：默认 `EMV_CFG_DEBUG=DISABLE`（日志关闭）；需要调试时 `-DEMV_CFG_DEBUG=ENABLE`（需在 build.ps1 增加编译定义）。

## 4. 符号审计规则（scripts/symbol_audit.ps1）

对每个静态库计算"全库未定义外部符号"（库内互引不算）：

1. **libentrypoint.a**
   - 不得定义任何非接内核前缀符号：`paypass_ / contact_ / amex_ / expresspay_ / qpboc_ / visa_ / jcb_ / pure_ / discover_ / disv_ / express_`
   - 不得残留调试宏未解析符号：`eTRACE / eTRACE_HEX / TRACE / ...`
   - 外部依赖只允许：`API_*`（平台 SDK）、libc 基础符号、`ics_option`、`CreateEMVDiscretionaryData`
2. **内核库（如 libexpresspay.a）**
   - 不得与 libentrypoint.a 导出符号重名
   - 不得定义 `EMV_/emv_/appdata_/candidate_/outcome_/emvtlv_/emvicc_/emvporting_` 命名空间符号（这些归 EP）
   - 允许引用 libentrypoint.a 已定义符号 + 上述白名单
3. 编译器内部符号（`@feat.00` 等）与段符号自动忽略。

审计失败即构建失败（`build.ps1` 末尾自动调用）。

## 5. 新增内核库步骤

1. 在 `kernels/<name>/` 建 `inc/ include/ src/`；`src/*.c` 会被 `build.ps1` 自动收集。
2. 内核实现 `EMV_KERNEL_OPS`（kernel_id/init/preprocess/process）并通过 `EMV_RegisterKernel()` 注册；**不要**定义 EP 命名空间符号，全部加内核前缀。
3. 头文件包含路径：`kernels/<name>/inc`、`kernels/<name>/include`、`inc`、`include`、`src/inc`（build.ps1 已加）。
4. 运行双目标门禁：`scripts\build.ps1` 与 `scripts\build.ps1 -Target ndk`，必须 BUILD OK + SYMBOL AUDIT OK。

## 6. IAR 工程（未在本机验证）

- `build/iar/entrypoint_kernel.ewp` 为原始 IAR 工程（ARM，Debug，输出静态库）。
- 新增 `src/emv_kernel.c`、`src/inc/emv_kernel.h`、`src/inc/emv_port_sdk.h` 已不在 .ewp 文件列表内，需在有 IAR 的环境补入；内核库建议在 IAR 中新建独立工程链接 entrypoint 库。
- IAR 编译器版本：7.40.3（见 `build/iar/Debug/Obj/*.pbi.cout`）；编译选项 `-std=c99`（IAR 对应 C99 模式）。

## 7. 常见问题

| 症状 | 原因/处理 |
|---|---|
| `unknown type name 'EMV_TERMPARAM'` | 头文件未包含 `emv_type.h`/`emvi.h`；检查包含顺序（先 sdk.h → emvi.h） |
| `call to undeclared function 'eTRACE'` | 该 .c 未包含 `emvi.h`（emvi.h 已统一包含 emvdbg.h） |
| 符号审计报 `malloc` | 内核/EP 禁止堆分配；改用静态缓冲或调用方缓冲 |
| 符号审计报 kernel symbol（entrypoint 库） | EP 核心错误引用了具体内核，应改为注册表调用 |
| 门禁输出目录冲突 | 并行开发时给每个任务 `-Tag <名字>` |
