# Entry Point 解耦架构说明

> 仓库：`E:\Work\ZT\Entrypoint`（旧 Kingsee 单体核心改造版）
> 分支：`feature/amex-entrypoint`（`master` 保留 2026-11-10 原始审查基线）
> 本文描述改造后的目标架构与已落地的部分；剩余工作见文末。

## 1. 目标

1. **Entry Point 核心独立编译**：`libentrypoint.a` 不引用任何非接内核（PayPass/Visa/AMEX/…）符号。
2. **内核链接 Entry Point**：每个非接内核编译为独立静态库（`libexpresspay.a` 等），链接 `libentrypoint.a`，通过注册表把 `init / preprocess / process` 交给 Entry Point 调度。
3. **编译期排除静态错误**：host（clang）与 NDK r23b（armeabi-v7a）双目标门禁 + 符号审计，任何未定义/越界依赖在内核构建阶段暴露。
4. **无堆、无平台绑定**：内核与 EP 核心不使用 `malloc/free`；所有 I/O 通过注入的 `EMV_PORTING` 函数表（卡通信/加密/哈希/随机数/时间/文件/日志/黑名单/参数）。

## 2. 目录结构

```
Entrypoint/
├── src/                  Entry Point 核心 + 共享服务（编译为 libentrypoint.a）
│   ├── emv.c             EP 状态机、上下文/缓冲布局、TLV API、工具函数
│   ├── emvapi.c          对外 API 薄封装（emv_* 旧 API，保留兼容）
│   ├── emv_application_selection.c  PPSE/PSE/SPI 选卡、候选列表
│   ├── emv_combination_Selection.c  组合选择：9F2A / AID → 内核 ID（无内核符号）
│   ├── emv_selec_kernel.c           内核选择 + init/preprocess 调度（注册表）
│   ├── emv_kernel.c      ★ 内核注册表（EMV_KERNEL_OPS / EMV_RegisterKernel）
│   ├── emvoutcome.c      EMVCo Entry Point Outcome / UIRequest
│   ├── emvtlv.c          BER-TLV 编解码、DOL 打包（含边界校验）
│   ├── emvtag.c          标签库 / 标签匹配
│   ├── emvbuf.c          哈希表式 appdata 存储（静态池，无堆）
│   ├── emvicc.c          ICC APDU 命令构造/收发（61xx/6Cxx 处理）
│   ├── emvporting.c      终端/AID/CAPK/黑名单/DRL/交易日志持久化
│   ├── emvspi.c          EMV_PORTING 的默认实现（平台/应用可覆盖）
│   └── emvsdk.c          版本信息
├── src/inc/              EP 私有头文件（emvi.h/emv.h/emv_type.h/emv_kernel.h …）
├── inc/emvapi.h          对外 API 头文件（emv_* 层）
├── include/              平台 SDK 头文件（ks_*.h / emvsdk.h / sdk.h）
├── kernels/
│   └── expresspay/       AMEX/Expresspay 子内核（链接 libentrypoint.a）
│       ├── inc/          express_api.h / express_types.h / express_param.h
│       ├── include/      express_tags.h / express_compat.h
│       └── src/          改名后的 L2 阶段 + 参数 + 内核入口（见 doc/AMEX_PORT_PLAN.md）
├── scripts/
│   ├── build.ps1         编译门禁（host / ndk，-Tag 隔离输出目录）
│   └── symbol_audit.ps1  静态库符号审计（解耦硬约束）
├── doc/
│   ├── ARCHITECTURE.md   本文
│   ├── BUILD.md          构建与门禁说明
│   ├── AMEX_PORT_PLAN.md AMEX 移植方案（V4.2.3 → 本架构）
│   ├── EXPRESSPAY_PORT_STATUS.md  AMEX 移植进度/TODO
│   └── spec/             Expresspay v4.2 / EP Book B v2.12 规范笔记（PDF 原文可追溯）
├── review/               2026-11-10 审查报告（P0/P1 来源）
└── emv_core_amex_japan/  美运内核源仓库（独立 git，feature/entrypoint-port 分支，只读）
```

## 3. 分层与依赖

```
应用（Android App / 测试工具）
   │ ① 注入 EMV_PORTING（icc_rw / hash / recover / rng / time / 文件 / 日志 …）
   │ ② EMV_RegisterKernel(&expresspay_ops)     ← 内核库提供
   │ ③ EMV_Init(&porting) → EMV_SetCardType/SetTransAmount/… → EMV_Process()
   ▼
libentrypoint.a ──（不依赖任何内核符号）──  EP 核心 + 共享服务
   ▲
   │ 链接（libexpresspay.a 引用 entrypoint 导出符号）
libexpresspay.a ── 内核状态机（init/preprocess/process），只通过注册表暴露给 EP
```

- **EP 核心职责**：候选列表/组合选择/内核选择、预处理、Outcome/UIRequest、参数与共享数据服务（TLV/tag/appdata/ICC/持久化）。
- **内核职责**：自身交易状态机（GPO/读应用数据/ODA/处理限制/CVM/风险管理/终止动作分析/联机/脚本/完成），使用 EP 共享服务与注入的 porting 接口。

## 4. 内核注册契约（src/inc/emv_kernel.h）

```c
typedef struct {
    uint8_t     kernel_id;                     /* EMVCo 9F2A 首字节：0x02 MC / 0x03 Visa / 0x04 AMEX / 0x05 JCB / 0x06 Discover / 0x07 PBOC / 0x2D PURE */
    const char* name;
    int32_t (*init)(void);                     /* 内核初始化（复位自身上下文） */
    int32_t (*preprocess)(bool force_online);  /* EP 预处理交接（TTQ/限额/零金额等） */
    int32_t (*process)(void);                  /* 内核状态机单步；返回 EMV_* 状态码 */
} EMV_KERNEL_OPS;

int32_t EMV_RegisterKernel(const EMV_KERNEL_OPS* ops);   /* 应用在 EMV_Init 前调用 */
const EMV_KERNEL_OPS* EMV_FindKernel(uint8_t kernel_id);
int32_t EMV_KernelProcess(void);                          /* EP 内部调度 */
```

- **选择逻辑**（`emv_combination_Selection.c`）：候选带 [9F2A] 时取首字节作为内核 ID；无 9F2A 时按 AID（RID）表；都不匹配则 `EMV_ERR` → EP 以"Invalid Kernel"结束应用（不再空指针调用）。
- **未注册内核**：`EMV_FindKernel` 返回 NULL → EP 返回错误并结束应用，应用需先注册内核；PayPass/Visa 等内核库按同一契约实现。
- **状态码契约**：内核 `process()` 返回 EP 状态枚举（`EMV_REQ_*`/`EMV_STA_*`/`EMV_COMPLETED`…）；`EMV_KernelProcess()` 遇无内核/未注册返回 `EMV_ERR`。

## 5. 与旧架构的差异（已落地）

| 项 | 旧实现 | 新实现 |
|---|---|---|
| 内核调用 | `emv_k_interface.emv_process = paypass_process`（直接引用内核符号，未赋值时空指针崩溃） | 注册表 `EMV_RegisterKernel`，EP 零内核符号依赖 |
| 组合选择 | 末尾无条件 `contactless_type = QMSDC` | 9F2A/AID 解析，未知内核返回错误 |
| 货币代码 API | `emv_set/get_current_code(_exponpent)` 独立 API | 删除；使用标签 5F2A/5F36 |
| Outcome 平台回调 | 内嵌 `JNISend*` 外部符号 | 仅通过 `EMV_SetOutcome_Callback/EMV_SetUIRequest_Callback` 注入 |
| 内存 | `malloc`（Tag 列表/解码 itemlist） | 静态缓冲，无堆 |
| 调试日志 | 宏默认开启、`eTRACE_HEX` 宏语法错误 | 宏修复；生产默认关闭（可 `-DEMV_CFG_DEBUG=ENABLE` 打开） |
| 构建 | IAR only，快照不可编译 | host clang + NDK r23b 双门禁 + 符号审计（IAR 工程需同步文件列表） |

## 6. 内存模型

- EP 上下文 + 池：应用经 `EMV_PORTING.alloc_buffer()` 提供一块连续缓冲（默认实现 14KB+256），`EMV_Init` 依次切分：`EMV_CONTEXT` | `EMV_TERMAPP×N` | TLV 缓冲(10KB) | `EMV_PREPROCESS_RESULT×N` | `EMV_CANDIDATE×12`；末尾指针越界检查失败返回 `EMV_ALLOCBUFFERERR`。
- appdata 存储：TLV 缓冲内哈希表，`appdata_save/find`；新增 `appdata_reset`（交易间复位）与 `appdata_save_raw`（内核内部固定 tag，跳过标签库校验）。
- 内核上下文：内核库自持（如 `EXPRESS_CONTEXT`），不使用 EP 上下文布局。

## 7. 剩余工作

见 `doc/EXPRESSPAY_PORT_STATUS.md`、`doc/DECISIONS.md` 与仓库提交记录；当前状态：

- **已落地**：S1-S4（Expresspay 子内核 13 TU 编译/链接/注册），G9（EMV_Result* → 内核 `post_result`、`EMV_ProcessExit` → 内核 `exit`，2026-11-12 提交 `9fb6295`）；G6（`EMV_GetSelectedCandidate`）；G1（参数原始 TLV 回读声明）；G8（组合选择/内核选择解耦）。
- **待落地（按优先级）**：
  1. ~~**标签口径统一**~~（已完成 2026-11-12）：Entrypoint 参数层与 Expresspay 内核双向兼容 `0x1F03/0xDF8102`（Default TDOL）与 `0x1F02/0xDFC108`（Status Check Support）。
  2. **G2** DRL getter（`emv_get_drl`）：Expresspay v4.2 已移除 DRL，可继续用 AID 级 DF19/DF20/DF21 回退；如需支持旧配置再补。
  3. **G5**（部分完成）：`EMV_KERNEL_OPS.get_outcome` + `EMV_GetKernelOutcome()` 已落地（2026-11-12），App 可取到 Expresspay 私有 0xA0/0xB0/0xC0/0xD0；**Entrypoint 自动合并进最终 Outcome 仍待做**（需按 Book B 的 11 参数组装规则实现）。
  4. **spec v4.2 精修**：删除 MSD 分支、9F6D/9F6E 判定重写（Amex 无 TTQ）、离线 PIN 强制禁止、2nd GAC/脚本/DDX 按规范处理。
  5. **桩 ICC 端到端**：GPO→READ→GAC→ARQC→ARC=00 全流程联调（当前仅编译/链接/静态审计）。
  6. Entrypoint 侧 `EMV_TERMPARAM/EMV_AIDPARAM` 第 2 步最小化（`doc/STRUCT_MINIMIZATION.md`）。
  7. IAR 工程复编（本机无 IAR）、真实测试卡/读卡器与 EMVCo TTA 认证。
