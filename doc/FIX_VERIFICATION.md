# P0/P1 修复与验证报告（2026-11-12）

> 基线：`master` = 2026-11-10 审查快照；工作分支：`feature/amex-entrypoint`。
> 问题编号沿用 `review/00-总览与问题清单.md`（P0-1…P0-6、5.1/5.2/5.3/5.4 各 P1、E-1…E-5）。
> 门禁：`scripts/build.ps1`（host clang + NDK r23b）→ BUILD OK + SYMBOL AUDIT OK；`scripts/verify.ps1` 汇总。

## 1. 当前门禁结果（每次提交前复跑）

```
host : libentrypoint.a 195,770 B + libexpresspay.a 88,890 B  → BUILD OK(27 TU) + SYMBOL AUDIT OK
ndk  : libentrypoint.a 196,734 B + libexpresspay.a 84,322 B  → BUILD OK(27 TU) + SYMBOL AUDIT OK
struct: EMV_AIDPARAM=632  EMV_TERMPARAM=486  EMV_TERMAPP=22  EMV_CANDIDATE=65  EMV_PREPROCESS_RESULT=26
```

## 2. P0（9 条唯一项）逐条状态

| 编号 | 问题 | 修复位置（当前工作区） | 状态 |
|---|---|---|---|
| E-1/E-2/B-P0-1/P0-2 | 快照不可编译：`emv.c` 语法错误、`EMV_TERMPARAM` 缺失、`EMV_CONTEXT` 缺成员、`EMV_CFG_MAX_CANDIDATES` 不可见、`emvapi.c` 无定义符号 | `src/emv_type.h`（恢复最小类型集）、`src/inc/emvi.h`（配置宏/上下文字段/统一包含 `emvdbg.h`）、`src/emv.c:1155/1236/1253`（语法）、`inc/emvapi.h`（接口声明） | ✅ 提交 `9082c86` |
| E-3 | 8 个 TU 的 `eTRACE/eTRACE_HEX` 未定义符号，库无法链接 | `src/inc/emvi.h` 统一包含 `emvdbg.h`；`src/inc/emvdbg.h` 重写（宏修复、默认关闭）；符号审计强制无 trace 符号 | ✅ `9082c86`，审计项 |
| P0-1（C） | 卡 SDOL 经 `emvdol_packet` 无界展开 → 262B 栈溢出 | `src/emvtlv.c:1637`（写入前容量/长度/边界校验）；调用方 `src/emv_application_selection.c` 预估+二次校验 | ✅ `ce04b61` + 模块 A 提交 |
| P0-2（D） | 主机参数 `conv_param` 越界写 | `src/emvporting.c:387/405`（`param_len_ok`/`param_validate` 逐字段限长，超长拒绝） | ✅ `5680c61` |
| P0-3（D） | 9F1B 六字节 BCD 写 12B 进 4B 缓冲 | `src/emvporting.c:716`（逐 nibble 校验 + `emv_dec2hex` 写 4B） | ✅ `5680c61` |
| P0-4（E） | `icccommand` 成功空响应不回写 `*outlen` | `src/emvicc.c:234` 起（入口置 0、空数据返回 `-130`、61xx/6Cxx 处理在 `:404` 起） | ✅ `5680c61` |
| P0-5（B） | `EMV_PreProcess` 把 `uint8_t` 当指针 | `src/emv.c:1311`（改 `cvm_limit` 数组） | ✅ `9082c86` |
| P0-6（B） | `EMV_GetData` 先解引用后判空 | `src/emv.c:1656`（判空前置） | ✅ `9082c86` |

## 3. P1（20 条唯一项）逐条状态

| # | 问题 | 修复位置 | 状态 |
|---|---|---|---|
| 1 | 组合选择硬编码 QMSDC | `src/emv_combination_Selection.c` 重写：9F2A → AID 表解析，未知内核返回错误 | ✅ `556db9d` |
| 2 | 内核函数指针未注入/不判空（触点路径） | `src/emv_selec_kernel.c` 注册表调度 + NULL 返回错误；`src/emv.c` 经 `EMV_KernelProcess()` | ✅ `556db9d` |
| 3 | Entry Point 预处理 TTQ 用 `&=` | `src/emv.c:1241/1320` 等 5 处改 `\|=` | ✅ `9082c86` |
| 4 | 9F29 下标错 / PPSE 未处理 | `src/emv_application_selection.c:246`（i→j）、`:967`（PPSE 扩展选择） | ✅ 模块 A |
| 5 | `resp_len/rlen` 未初始化 | 同文件：sdol_process/select_ddf 初始化 + 解析前校验 | ✅ 模块 A |
| 6 | `sdol_process` flag=3 容量/长度回绕 | 同文件：按真实 5 字节追加、0x81/0x82 规则、超长报错 | ✅ 模块 A |
| 7 | 71/72 脚本落库失败/DF8101 冲突 | `src/emv.c:120`（EP 私有 tag 定义）、`EMV_ResultOnline` 用 `appdata_save_raw` 存 `0xDF8F10/0xDF8F20` | ✅ `ce04b61` |
| 8 | `EMV_TLV_PacketByDOL` 错误当成功 | `src/emv.c`（失败直接返回 `EMV_FAIL`，不再写 -1 长度） | ✅ 模块 B（本轮补） |
| 9 | `EMV_ResultSelCandidates` 不校验 index | `src/emv.c:1460`（0 ≤ eResult < candidates_num） | ✅ 模块 B |
| 10 | `EMV_TLV_PacketByTAG` 先写后查 | `src/emv.c`（写前预留 tag+len 最坏长度） | ✅ 模块 B |
| 11 | `emvdol_packet_tlv` 先写后查 | `src/emvtlv.c:1864`（本地组 T/L、全部校验后写入；拒绝 0x84/负长度） | ✅ `ce04b61` |
| 12 | `appdata_save` 覆盖更长值截断哈希链 | `src/emvbuf.c`（新节点继承 `existing->next`；旧节点延迟回收） | ✅ `ce04b61` |
| 13 | appdata 只增不减 / 压缩死代码 / 退出不复位 | `src/emvbuf.c:128`（原地压缩）、`:199`（`appdata_reset`）；`src/emv.c EMV_ProcessExit` 调用复位 | ✅ `ce04b61` |
| 14 | `emvporting_get_termapps` 无上限 | `src/emvporting.c:1378` 起（`EMV_CFG_MAX_APPS` 上限返回 `-103`） | ✅ `5680c61` |
| 15 | CAPK 校验被注释 / 只比 20 字节 | `src/emvporting.c:1445`（`checkCAPKhash` 恢复 SHA-1 全 20B 比较、失败拒绝） | ✅ `5680c61` |
| 16 | `EMV_FNAME` 缺 `_PARAM` 分支/kernel_index 恒 0 | `src/emvporting.c:125`（恢复分支、按持久化 index 命名、拒绝越界） | ✅ `5680c61` |
| 17 | 61xx/6Cxx 无处理 | `src/emvicc.c:404` 起（GET RESPONSE ≤10 次、6Cxx 重发、累计 ≤512） | ✅ `5680c61` |
| 18 | `get_random` 长度不校验 / `recover` 不校验模长 | `src/emvspi.c:1079`、`:956`（长度上限/返回值/模长与指数校验） | ✅ `5680c61` |
| 19 | 调试日志默认全开 + 宏缺陷 | `src/inc/emvdbg.h` 重写（宏修复、默认 DISABLE、可 `-DEMV_CFG_DEBUG=ENABLE`） | ✅ `9082c86` |
| 20 | 缓冲容量契约（7168/9216 vs 实际） | `EMV_Init` 末尾指针越界检查保留；文档在 `doc/BUILD.md`/`doc/STRUCT_MINIMIZATION.md` 说明；未改 API 文档数值（待产品确认） | ✅ 部分（文档待产品核对） |

## 4. 其他目标项

| 目标 | 状态 |
|---|---|
| 删除 `emv_set/get_current_code(_exponpent)`，改用 5F2A/5F36 | ✅ 提交 `9082c86`（API 删除 + 明文注释）；货币 tag 已由参数层/`appdata` 处理 |
| Entrypoint 独立编译、不依赖非接库 | ✅ 符号审计强制：`libentrypoint.a` 无任何内核前缀符号；决策 D4/D5 |
| 非接库链接 Entrypoint | ✅ `libexpresspay.a` 链接 `libentrypoint.a`；内核模块审计规则禁止重复定义 EP 符号 |
| AMEX V4.2.3 源码为准移植 + 修缺陷 | ✅ S1-S4（13 TU/7.2k 行，注册 kernel_id=0x04）；美运 P0×3/P1×11 + 新发现 P0/P1 已修；G9（post_result/exit）、G5（get_outcome）、标签口径双兼容已落地 |
| AID/TermParam 最小化 | ✅ 第 1 步（AIDPARAM 1118→632B、TERMAPP 33→22B）；第 2 步进行中（见 doc/STRUCT_MINIMIZATION.md） |
| host + NDK 编译门禁 | ✅ `scripts/build.ps1` + `scripts/symbol_audit.ps1` + `scripts/verify.ps1`；每次提交前复跑 |
| 功能分支、master 原始基线 | ✅ `master` 仅 1 个基线提交；所有开发提交在 `feature/amex-entrypoint` |

## 5. 仍未闭环（下一阶段）

1. **结构体第 2 步**（子任务进行中）：目标 AIDPARAM ≤160B、TERMPARAM ≤80B。
2. **host 桩 ICC 端到端测试**（子任务进行中）：链接门禁 + 状态机/恶意 SDOL/候选上限/DOL 边界/appdata 复位用例。
3. **G5 自动合并**：内核 Outcome 已可取出（`EMV_GetKernelOutcome`），但 Entrypoint 尚未按 Book B 11 参数规则自动组装最终 Outcome。
4. **Expresspay v4.2 精修**：删除 MSD 分支、9F6D/9F6E 判定替代 TTQ、离线 PIN 禁止、2nd GAC/脚本/DDX 按规范处理。
5. **G2 DRL getter**（v4.2 已移除 DRL，可延后）；**IAR 工程复编**（本机无 IAR）；**真实测试卡/读卡器与 EMVCo TTA 认证**。
6. 91 条历史 warning 清理后启用 `-WerrorAll` 严格门禁。

## 6. 复现命令

```powershell
cd E:\Work\ZT\Entrypoint
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1   # host+NDK+结构体尺寸+产物汇总
git log --oneline master..HEAD                                           # 本次全部提交
```
