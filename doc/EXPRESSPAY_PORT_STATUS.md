# Expresspay（AMEX ZT_AMEX_CORE_V4.2.3）内核移植状态

- 目标工程：`E:\Work\ZT\Entrypoint`，分支 `feature/amex-entrypoint`
- 源仓库（只读）：`E:\Work\ZT\Entrypoint\emv_core_amex_japan`（GBK；全程未修改、未提交）
- 权威方案：`doc/AMEX_PORT_PLAN.md`（本文所有"方案 §x"均指该文档）
- 本阶段范围：S1–S4（工程骨架 → 9 个 L2 模块逐行移植 → 参数装载 → 内核状态机/注册），
  **S5 spec 精修已于 2026-11-12 按 Expresspay v4.2 完成**（见 §9）。
- 本文件是本次交付的**唯一状态文档**；方案 §8 的 S1–S6 进度、偏差与联调注意见下。

---

## 1. 交付物与规模

| 文件 | 行数 | 说明 |
|---|---:|---|
| `kernels/expresspay/include/express_priv.h` | 212 | **新增**：内核私有总头（Entrypoint 头 + 兼容垫片 + 全部阶段原型） |
| `kernels/expresspay/include/express_tags.h` | 161 | **修改**：补 6 个缺失标签、覆盖 TDOL(1F03)/9F2A 别名、覆盖 Status Check(1F02) |
| `kernels/expresspay/include/express_compat.h` | 97 | **修改**：`TRUE/FALSE/ENABLE/DISABLE/UNIQUE/OVERRIDED` 改为 `#ifndef` 保护，避免与 ks_type.h/emvi.h 冲突 |
| `kernels/expresspay/inc/express_types.h` | 241 | **修改**：上下文补 `status/card_type/card_types_allowed/aip[2]/tvr[5]/tsi[3]`、ODA 三个 bool、`bis_unable_online`、`req_sel_accounttype/check_id/confirm_*` 结果字段；新增 `EXPRESS_OUTCOME` |
| `kernels/expresspay/inc/express_api.h` | 87 | **修改**：仅新增 `express_get_outcome()` 声明（G5）；`amex_kernel_register`/`amex_kernel_ops`/`express_post_result` 按原声明实现 |
| `kernels/expresspay/inc/express_param.h` | 214 | 未改（尺寸断言全部保留） |
| `kernels/expresspay/src/express_kernel.c` | 443 | init / preprocess / process / exit / post_result / get_data / **注册** / 兼容描述符 |
| `kernels/expresspay/src/express_param.c` | 442 | conv_param（P0/P1 修复）+ 缺省值 + 终端/AID/DRL 装载 |
| `kernels/expresspay/src/express_app_sel.c` | 415 | 最终 SELECT + load_term_app_param + again_preprocess + 交易日期/ATC/UN + TSI/TVR 预建 |
| `kernels/expresspay/src/express_utils.c` | 315 | AIP/TVR/TSI 写穿透、appdata_save2、PDOL 打包、Outcome、DRL、CRC32 |
| `kernels/expresspay/src/express_initiate_application.c` | 470 | GPO / PDOL / 9F6D-9F6E 判定（emv_initiate_application.c 逐行） |
| `kernels/expresspay/src/express_read_application_data.c` | 484 | AFL 读取/校验（emv_read_application_data.c） |
| `kernels/expresspay/src/express_offline_data_authentication.c` | 1287 | SDA/DDA/CDA/CAPK（emv_offline_data_authentication.c） |
| `kernels/expresspay/src/express_processing_restrictions.c` | 551 | 处理限制 + DRL + 9F6D/9F6E 能力位（emv_processing_restrictions.c） |
| `kernels/expresspay/src/express_cardholder_verification.c` | 859 | CVM（emv_cardholder_verification.c） |
| `kernels/expresspay/src/express_terminal_risk_management.c` | 149 | TRM / 例外文件（emv_terminal_risk_management.c） |
| `kernels/expresspay/src/express_terminal_action_analysis.c` | 1209 | TAA / GAC（emv_terminal_action_analysis.c） |
| `kernels/expresspay/src/express_complete.c` | 410 | 1st GAC 收尾 / Outcome / 在线结果（emv_complete.c） |
| `kernels/expresspay/src/express_issuer_script_processing.c` | 176 | 发卡行脚本（emv_issuer_script_processing.c） |

合计 **19 个文件、8222 行**（src 7210 行 / 头文件 1012 行）。

> 说明：`kernels/expresspay/src/*.c` 中的 9 个 L2 文件由美运同名文件**逐行移植**（GBK→UTF-8 按 0x0A 切分，
> 只做符号前缀与共享服务替换），文件头均注明了来源与修改点。

---

## 2. 阶段进度（方案 §8）

| 步骤 | 状态 | 说明 |
|---|---|---|
| S1 | ✅ 完成 | `kernels/expresspay/src` 建立，`build.ps1 -Tag amex` 自动产出 `libexpresspay.a`（host/NDK 均 13 个 TU） |
| S2 | ✅ 完成（编译+链接验证） | 9 个 L2 文件全部改名为 `express_*`，与 `libentrypoint.a` 的 180 个同名符号清零（审计 `duplicates entrypoint symbol` 通过） |
| S3 | ✅ 完成（缺 G2 DRL） | `express_param.c`：conv_param 带长度校验 + 终端/AID 原始 TLV 回读（`emvporting_get_termparam_tlv`/`emvporting_get_aidparam_tlv`） |
| S4 | ✅ 完成（静态链路） | `express_kernel.c`：init/preprocess/process/exit/post_result/`amex_kernel_register()`；状态机与方案 §2.2 一致 |
| S5 | ✅ 完成 | symbol_audit 双向通过；**2026-11-12 按 Expresspay v4.2 完成规范精修**（MSD 删除、TTQ→9F6D/9F6E、离线 PIN 禁止、2nd GAC/脚本/DDX 显式不支持、内核告警清零），逐项证据见 §9 |
| S6 | ⏳ 未做 | 与 Entrypoint 联调（G5/G6/G9 + 一笔真实 AMEX 非接交易）需 Lead 先落地 Entrypoint 侧改动 |

---

## 3. 编译/链接/符号门禁结果（2026-11-12 实测）

| 命令 | 结果 |
|---|---|
| `scripts\build.ps1 -Tag amex`（host） | `BUILD OK (27 translation units)` + `SYMBOL AUDIT OK` |
| `scripts\build.ps1 -Target ndk -Tag amex` | `BUILD OK (27 translation units)` + `SYMBOL AUDIT OK`（libexpresspay.a 84,310 B） |
| `scripts\build.ps1`（默认，无 Tag） | `BUILD OK (27 TU)` + `SYMBOL AUDIT OK`（libentrypoint.a 193,318 B + libexpresspay.a） |
| `scripts\build.ps1 -Target ndk`（默认） | `BUILD OK (27 TU)` + `SYMBOL AUDIT OK` |

审计口径（`scripts/symbol_audit.ps1` 当前版本）：

- `[expresspay] defined=125 undefined=64 external=33`，`external` 全部是 **Entrypoint 已导出符号** 或 **平台 SDK `API_*`**；
- 无 `malloc/free`、无 `eTRACE/eTRACE_TRACE` 类未解析符号、无 `express_/amex_` 外部引用、
  未重复定义 `EMV_/emv_/appdata_/outcome_/emvtlv_/emvicc_/emvporting_` 命名空间符号。

额外做了**真链接验证**（临时目录，不入库）：用 `llvm-nm` 求两库外部依赖后生成 22 个空桩
（`API_fopen/fread/...`、`ics_option`、`CreateEMVDiscretionaryData` 等），
`clang test.c libentrypoint.a libexpresspay.a` **链接成功并可执行**（exit=0）。
证明内核库在目标链接顺序下没有未解析符号。

> 告警状态（2026-11-12 精修后）：`kernels/expresspay/**` 编译告警 **0 条**（host 与 NDK 均无
> `warning:` 输出）。原美运原码的 `-Wunused-but-set-variable` / 逗号表达式 / `char[]`→`uint8_t*`
> 已逐条处理；其中逗号表达式与未初始化指针是**真实缺陷**，修逻辑而不只是消警（见 §9.5）。

---

## 4. 与方案/头文件的偏差（已落地并同步本文件）

| # | 偏差 | 原因/影响 |
|---|---|---|
| D-1 | 新增 `include/express_priv.h` | 9 个 L2 文件需要统一入口；同时集中放"不可直接沿用美运头"的垫片（状态别名、AIP/TTQ/TVR/TSI 宏、`SDK_EXPRESS_DRL` 别名、阶段原型） |
| D-2 | `EXPRESS_CONTEXT` 增加 `status/card_type/card_types_allowed/aip[2]/tvr[5]/tsi[3]`、`da_sda/dda/cda_performed`、`bis_unable_online`、`req_sel_accounttype/check_id/confirm_bypass_pin/confirm_force_online_result` | 逐行移植需要；`aip/tvr/tsi` 用**小数组副本**替代美运裸指针，同时修 P1-6（悬垂指针）。方案 §5.3 里"合并为 `oda_type`"未采纳：保留三个 bool 可减少改写面，`oda_type` 字段暂留未用 |
| D-3 | `express_types.h` 的 `unable_online` 更名为 `bis_unable_online` | 与美运源码字段名 1:1，降低移植期改名错误 |
| D-4 | `express_tags.h` 覆盖 `TAG_DEFAULT_TDOL` = **0x1F03**（Entrypoint 为 PayPass 的 0xDF8102） | 方案 §5.2/P2-8；内核用自己的 `EXPRESS_AID_PARAM` 解析 AMEX 参数文件 |
| D-5 | `express_tags.h` 覆盖 `TAG_STATUS_CHECK_SUPPORT_INDICATOR` = **0x1F02**（Entrypoint 当前值是 0xDFC108） | 美运/规范语义是 1F02。**集成注意**：Entrypoint 的 `emvporting.c` 用 0xDFC108 解析同一参数文件；若主机参数文件只下发 0xDFC108，内核的 `status_check_sup` 会保持缺省 1。见 §7-② |
| D-6 | `express_tags.h` 新增 `TAG_9F2A_KERNEL_IDETIFIER` → `TAG_9F2A_KERNEL_ID` 别名 | 美运拼写与 Entrypoint 不同名 |
| D-7 | 内核上下文**静态分配**（`static EXPRESS_CONTEXT g_express_context`） | 架构约束"禁止堆分配"；Entrypoint 的 `alloc_buffer` 只服务 Entrypoint 自身。内核不再调用 `EMVPORT_AllocBuffer` |
| D-8 | 方案 §6.3 的"丰富版 `EXPRESS_KERNEL_OPS`（含 post_result）"与 Entrypoint 实际 `EMV_KERNEL_OPS{id,name,init,preprocess,process}` 不一致 | 注册走实际 `EMV_KERNEL_OPS`（`amex_kernel_register()`）；`express_api.h` 声明的 `amex_kernel_ops()` 仍返回兼容描述符，两者并存不冲突 |
| D-9 | `express_param_get_aid_count()` 保留但未在流程中调用（3KB 栈数组） | 预处理不再遍历 AID（方案 D3），该 API 仅备用；后续可改为调用方传缓冲 |
| D-10 | 美运 `appdata_save2` 在 Entrypoint 未导出（只有 `appdata_save/_save_or/_save_and/_save_tlv/_save_singlebyte/_save_raw`） | 内核用 `express_appdata_save2()` 薄封装（同语义） |
| D-11 | 美运 `emvpdol_packet` 已删除，改用 Entrypoint 的 `emvdol_packet`（同源等价，且有 DOL 越界校验） | `express_pdol_packet()` 仅做转发 |
| D-12 | 美运 `outcome_*` 全部改名为 `express_outcome_*` | `libentrypoint.a` 已定义 `outcome_request_another_payment/outcome_try_again` 等同名符号，内核改名避免重复定义；同时保存 `EXPRESS_OUTCOME`（G5） |
| D-13 | `express_read_application_data.c` 的 `auth_data[1536]` 改为 `static` | host 门禁下栈帧 >4KB 会引入 `__chkstk` 外部符号（审计未放行）；单线程内核中用静态 scratch 等价 |

---

## 5. 美运 P0/P1 修复落地对照（方案 §9）

| 编号 | 位置 | 本阶段处理 |
|---|---|---|
| P0-1 候选索引越界 | 美运 `emv.c:1259-1269` / `app_sel_init` | **由 Entrypoint 承接**：`EMV_ResultSelCandidates()` 已做 `eResult ∈ [0, candidates_num)` 校验；内核 `express_app_sel_init()` 只经 `EMV_GetSelectedCandidate()`（内部再校验一次），不再直接索引候选数组 |
| P0-2 9F4E 超长写 | `express_param.c` `TAG_9F4E_MERCHANT_NAME` | 先判 `valuelen <= sizeof(merchant_name)`（20），超长丢弃 |
| P0-3 9F06 超长写 | `express_param.c` `TAG_9F06_AID_TERMINAL` | 先判 `valuelen <= sizeof(aid)`（16） |
| P1-1 conv_param 定长拷贝越界 | `express_param.c` 全部 `emv_memcpy` | 统一走 `express_copy_fixed()`（截断+补 0）；DDOL/TDOL/9F1D/9F4E 均带长度上限 |
| P1-2 9F40 只拷 4 字节 | `express_param.c` | 改 5 字节（终端参数与 AID 参数两处） |
| P1-3 termapps 解析残留/无上限 | — | 内核不再解析 termapps（Entrypoint 参数服务负责），N/A |
| P1-4 AID-list 回退 memcmp 取反 | — | AID 列表回退已删除（Entrypoint 应用选择层），N/A |
| P1-5 9F2A 只写 1 字节 | `express_app_sel.c` | 写候选的 `kernel_id[0..len)`（缺省 8 字节 `04 00..`） |
| P1-6 aip/tvr/tsi 悬垂指针 | `express_utils.c` + `express_types.h` | 改为上下文小数组副本；TVR/TSI **写穿透**（同时 `appdata_save_or` 与副本），AIP 在 GPO 解析后 `express_aip_refresh()` |
| P1-7 `API_Rng` 返回值丢弃 | `express_app_sel.c` | 检查 `get_random()` 返回值（失败时 UN 保持 0 并 trace；不中断交易） |
| P1-8 时间获取失败仍写字段 | `express_app_sel.c` | 先判 `get_trans_date_time()`，失败直接结束交易；9F41 失败时用 0 并 trace |
| P1-9 强制联机不经确认 | `express_kernel.c` `express_post_result()` | 只存 `req_confirm_force_online_result`，由 Entrypoint 的 `EMV_ResultForceOnline()` 真实等待用户选择（Entrypoint 侧行为） |
| P1-10 PreProcess 无迭代上限 | `express_kernel.c` | 方案 D3：内核删除"遍历全部 AID 限额"逻辑，由 Entrypoint 预处理承担，循环上限问题随之消失 |
| P1-11 GetData 保留 tag 段 default 落空 | — | 内核 `express_get_data()` 只服务自己的 tag；Entrypoint `EMV_GetData` 不受影响 |
| 额外 P1（新发现） | `express_terminal_action_analysis.c` `express_term_analy_trans_res()` | ① `term_type` 指针与 0x23/0x26/... 做整数比较（恒不等）→ 先解引用；② `TVR/TACDenial/TACOnline/TACDefault` 可能为 NULL → 全部判空，TVR 缺失用全 0 副本 |
| 额外 P1（新发现） | `express_terminal_action_analysis.c` `express_check_tvr()` | `term_type` 与 `tac_denail` 判空 |
| 额外 P1（新发现） | `express_complete.c` `express_pre_complete()` | `9F27/9F70/9F6E` 判空；`TAG_GAC_REQUEST_TYPE` 缺失按"未请求 CDA"处理 |
| 额外 P1（新发现） | `express_processing_restrictions.c` `express_is_a_atm()` | `9F35/9F40` 判空 |
| 额外 P1（新发现） | `express_read_application_data.c` `express_create_un()` | `5F25/9F37` 判空 |
| 额外 P0（新发现） | `express_app_sel.c` | 美运 `emvbuf.c:42-43` 在 `appdata_init()` 内预建 TSI/TVR；Entrypoint 的 `appdata_init()` **不再预建** → 不补建则 TRM/TAA 的 `appdata_find(TVR)` 返回 NULL 后解引用崩溃。移植版在 `express_app_sel_init()` 里补建 TVR(5B)/TSI(3B)。TSI 长度同时由美运的 2B 修正为 3B（EMV Book 3） |
| P2 顺手 | `express_types.h` | 删 `ClessReaderCapab/EnhancedClessReaderCapab`（改 per-AID）、删 checksum 字段；`pay_mode/ec_onlinpin_falg` 等死字段保留在结构体（未参与逻辑） |

---

## 6. 未实现 / TODO(port) 清单

按影响排序；代码中以 `TODO(port)` 或本节编号标注。

1. **G2 DRL 读取**：Entrypoint 只有 `emv_add_drl/emv_clr_drl`（`src/emvporting.c:2402-2470`，`libentrypoint.a` 无 getter），
   内核 `express_param_load_drl()` 当前返回"无 DRL"（`ifenable=0`），限额回退到 AID 级 `DF19/DF20/DF21`。
   **Expresspay v4.2 已删除 DRL（P p43 §7.2.2；P p10 版本历史），按 v4.2 不需要实现**；
   若将来回退支持 v4.1 才需要 Lead 增加 `emv_get_drl()`。遗留 DRL 代码当前**不参与**任何判定路径。
2. **G5 Outcome 合并**：内核已维护 `EXPRESS_OUTCOME`，并在 `express_api.h` 增加
   `int32_t express_get_outcome(EXPRESS_OUTCOME *out)`（实现见 `express_kernel.c`）；
   Entrypoint 的 Outcome 组装尚未合并 0xA0/0xB0/0xC0/0xD0 等 Expresspay 扩展值，需 Lead 在 `src/emvoutcome.c` 接。
3. **G6 已选候选通道**：本移植用 Entrypoint 已导出的 `EMV_GetSelectedCandidate()`（`src/emv.c:1617`），
   未新增 `post_select` 回调；待 Lead 决定是否按方案 §6.3 增加 `post_select`（当前不需要）。
4. **G9 结果回传**：`express_post_result()` 已实现（kind 与 `EMV_Result*` 一一对应），但
   `EMV_KERNEL_OPS` 无 `post_result` 字段，**Entrypoint 目前不会调用它**。联调前需要：
   - 方案 A（推荐）：`src/emv.c` 的 `EMV_Result*` 末尾调用当前内核 `post_result`；
   - 方案 B：App 先直接调用 `express_post_result()`（`express_api.h` 已声明）。
5. **终端预设（`EXPRESS_TERM_PRESET`）**：上下文支持，但未导出 `express_set_term_type/caps/addcaps/clesscaps/enclesscaps`
   （美运 `EMV_SetTerm*`）。当前 `preset` 全 0 → 始终用 per-AID 参数。TODO：按 App 需求补 5 个 setter。
6. **spec v4.2 精修**：✅ **已完成（2026-11-12）**。MSD 删除、TTQ→9F6D/9F6E、离线 PIN 禁止、
   2nd GENERATE AC / 发卡行脚本 71/72 / Data Exchange(DDX) 显式不支持、内核告警清零，
   逐项证据与规范页码见 **§9**。仅剩"行为变更需 Lead 确认/联调"的契约项，见 §9.7。
7. **S6 联调**：尚无桩 ICC 的全流程自动测试（从 `EMV_STA_CANDIDATES_BUILT` 走到 `EMV_ACCEPTED_ONLINE`）。
   建议 Lead 在 G5/G6/G9 落地后用模拟卡 APDU 序列做一次端到端（GPO→READ RECORD→GENERATE AC→ARQC→ARC=00）。
8. `express_param_get_aid_count()` 栈占用 3KB、未被调用；如无需求可删。
9. ~~美运原码遗留告警（逗号表达式、unused-but-set）未清理~~ → ✅ **已清零（2026-11-12）**：
   `kernels/expresspay/**` host/NDK 编译均无 `warning:`；逗号表达式/未初始化指针属真实缺陷，已修逻辑（见 §9.5）。

---

## 7. 给 Lead 的集成注意（必须看）

1. **注册**：App/Entrypoint 启动时调用一次 `amex_kernel_register()`（`kernels/expresspay/inc/express_api.h`），
   内部 `EMV_RegisterKernel(&ops)`，kernel_id=`EMV_KERNEL_ID_AMEX(0x04)`，ops={init,preprocess,process}。
   注册后 Entrypoint 的 `emv_select_kernel()` 会先 `init()` 再 `preprocess(force_online)`，随后每次 `EMV_Process`
   走 `EMV_KernelProcess()`。**内核内部状态从 `EMV_STA_IDLE` 起步**：在 Entrypoint 状态
   `EMV_STA_CANDIDATES_BUILT/REQ_SEL_*` 时第一次进 `express_process()` 会执行
   "最终 SELECT + 参数装载"并返回 `EMV_STA_APP_SELECTED`（与方案 §2.5 一致）。
2. **同一份原始参数的标签差异**（重要）：
   - TDOL：内核按 **0x1F03** 解析，Entrypoint 的 `EMV_AIDPARAM.default_tdol` 按 **0xDF8102**（PayPass）解析；
   - Status Check：内核按 **0x1F02**，Entrypoint `emvporting.c` 按 **0xDFC108**。
   若主机参数文件要同时服务两套解析，需**两个标签都下发**（或由 Lead 统一标签口径后同步改 `express_tags.h` 与 Entrypoint）。
   其余标签（DF55/DF19/DF20/DF21/DF11-13/9F6D/9F6E/9F70/9F71…）两侧一致，无需重复下发。
3. **TSI/TVR 预建**：Entrypoint 的 `appdata_init()` 不再预建 TSI/TVR；内核在最终 SELECT 后自行预建
   （`express_app_sel_init()`）。Entrypoint 若在别处假设 TSI/TVR 一定存在，请注意时序（内核预建发生在
   `EMV_STA_APP_SELECTED` 之前）。
4. **`appdata_init()` 的调用**：内核在 `express_app_sel_init()` 里调用 `appdata_init()`（美运语义），
   会清空 Entrypoint 在 `EMV_STA_IDLE` 写入的 `9C/9F02/9F03/81/9F04`，随后内核立即重建。
   **Entrypoint 不要在 `APP_SELECTED` 之前写任何内核需要保留的 appdata**。
5. **无堆分配**：内核上下文静态分配；`auth_data` 为函数内静态 scratch（单线程）。NDK 目标无 `__chkstk` 问题。
6. **符号审计**：`libexpresspay.a` 定义 125 个符号（全部 `express_*`/`amex_*`/静态局部），
   外部依赖 33 个（Entrypoint 导出或 `API_*`）。不要在这两个库里再引入裸 `malloc/eTRACE`。
7. **未提交**：本次未执行任何 `git add/commit/checkout/reset`；工作区内的
   `M kernels/expresspay/{include/express_tags.h, src/express_app_sel.c, express_complete.c,
   express_initiate_application.c, express_read_application_data.c, express_terminal_action_analysis.c}`
   是 Lead 的中间提交 `6550f3b` 之后本阶段补的 P1 修复，请由 Lead 决定是否合入提交。

---

## 8. 复现命令

```powershell
# 内核（host / NDK）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Tag amex
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Target ndk -Tag amex

# 默认双库（libentrypoint.a + libexpresspay.a）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Target ndk
```

产物：`build_host_amex/lib/libexpresspay.a`、`build_ndk_amex/lib/libexpresspay.a`（默认 Tag 为空时在
`build_host/lib/`、`build_ndk/lib/`）。

---

## 9. Expresspay v4.2 规范精修（2026-11-12）

> 依据：`doc/spec/EXPRESSPAY_V4.2_NOTES.md`。下文的 `P pXX` 指该笔记所引原始 PDF 页码，
> `[PDF 无]` 表示原文全文检索 0 次。
> 基线：host/NDK `BUILD OK + SYMBOL AUDIT OK`；`scripts/test_host.ps1 -Tag spec` 85 断言 / 0 失败。

### 9.1 删除 MSD / Mag-stripe（v4.2 已移除）

| 文件:行 | 改动 |
|---|---|
| `inc/express_types.h:47-50` | 删除 `EXPRESS_MSD_OR_EMV` 与 `msdoremv` 字段；内核只有 EMV 模式 |
| `include/express_priv.h:63-70,112-127` | 删除 `EMV_MODE/MSD_MODE`、`AIP_MSD_SUPPORT()`、`RD_MSD_ATC_LENERR`、`ttq_get/set` |
| `src/express_initiate_application.c:263-314` | GPO 响应只判 `AIP b2b8`（EMV mode）；`b2b8=0` 时按 p35 §4.3.3 走 Try Another Interface / Request Another Payment；`9F39` 恒为 `07`（非接 EMV） |
| `src/express_read_application_data.c:471-473` | 删除 MSD 强制数据（`57/5F25/5F20`）、`GET DATA(9F36 ATC)` 与 `express_create_un()`（改写 `5F25`/`9F37`） |
| `src/express_processing_restrictions.c:379-381` | 删除 `ExpresspayMSDmandatoryTag`、`express_check_data_miss_msd()`、`express_process_restrict()` |
| `src/express_terminal_action_analysis.c` | 删除整段被注释的 MSD/2nd GAC 旧实现，避免误导 |
| `src/express_app_sel.c:19` | 注释明确 9F66 不注入 appdata；9F6D/9F6E 注入终端能力 |

规范依据：v4.2 已移除 Mag-stripe Mode（`P p10` 版本历史、`P p20 §2.2`、`P p43 §7.2.2`
"DRL functionality is no longer supported"；`P p23-24 Table 2-3` 无 Magstripe 命令）。

### 9.2 TTQ(9F66) → 9F6D / 9F6E / 9F35|9F6D

v4.2 全文 `9F66`/`TTQ` 出现 **0 次**（`P p102-112 DET` 也无该 tag）；终端能力必须用
9F6D（1B）+ 9F6E（4B）表达，PDOL 只要 9F35 时回 `Terminal Type–Modified = 9F35 OR 9F6D`
（`P p33-34 §4.3.2`、`P p89-91 Table 21-7/21-8/21-9`）。

| 文件:行 | 改动 |
|---|---|
| `inc/express_param.h:70-110,232` | 终端参数去掉 `ttq[4]`，改为 `cl_capability`(9F6D) + `enhanced_cl_capability[4]`(9F6E)；`sizeof` 33→34 |
| `src/express_param.c:73-82,113-117` | 缺省值：9F6D=`C0`（`11XX0XXX`，CVM Not Required，b4 由 pre-PDOL 动态置位）；9F6E=`9C E0 00 03`（B1 b8/b5/b4/b3、B2 b8/b7/b6、B3=00、B4=03） |
| `src/express_param.c:172-182` | 新增 9F6D/9F6E 解析；主机若仍下发 9F66 → **记录并忽略**（`TAG_9F66` 不再进入任何判定） |
| `src/express_kernel.c:158-171` | preprocess 能力门禁：9F6D b8b7 必须 `11b`、9F6E b1b7 必须 0（无 Magstripe）、b1b5 必须 1（partial online），否则 `EMV_QPBOCUNSUPPORTED` |
| `src/express_initiate_application.c:125-172` | PDOL 含 9F35 且不含 9F6E 时临时写 `9F35|9F6D` 组包，**组包后立即恢复原始 9F35**；授权/请款报文读到的仍是原值（`P p34` 明确 Modified 值仅本笔使用） |
| `src/express_initiate_application.c:291-310` | 卡侧 AIP b2b8=0（无 EMV mode）→ 按 p35 分流，不再有 MSD 分支 |
| `src/express_processing_restrictions.c:260-270` | `express_set_ttq()` 更名 `express_set_cvm_required()`：CVMRLEF 时置 9F6D b1b4=0x08、9F6E b3b7=0x40（**b8=0x80 是 offline-only，不能写反**） |

位定义核对（`P p90-91 Table 21-9`）：9F6E Byte3 b8=`0x80`=Terminal offline only；
b7=`0x40`=CVM Required；Byte1 b7=`0x40`=支持 Magstripe（v4.2 必须 0）。
9F33 的 CVM 位与 9F6E 冲突时以 9F6E 为准（`P p100 §21.4.3`）。

### 9.3 离线 PIN 禁止 + Online PIN 联机

规范：Expresspay 终端在 Expresspay 交易中 **must not** 处理 Offline PIN（`P p23 §2.10.1`）；
命令表无 VERIFY（`P p23-24 Table 2-3`）；`Table 21-16 p94-95` 中离线 PIN 方法码不受支持。

| 文件:行 | 改动 |
|---|---|
| `src/express_cardholder_verification.c:264-266,619-631` | CVM List 中 0x03/0x04/0x05（plaintext/enciphered offline PIN）一律返回 `CV_NOT_SUPPORTED` 并记录；**不会选到离线 PIN** |
| `src/express_cardholder_verification.c:104-114,250-258,604` | 新增 `express_term_is_offline_only()`（读 appdata `9F35` 低 3 位 `011/110`，与 pre-PDOL 同源）；离线唯一/`bis_unable_online` 时不把 Online PIN 放进可用集合 |
| `src/express_cardholder_verification.c:606-611` | Online PIN：`CVMParameter=OUTCOME_CVM_ONLINE_PIN`、`bis_online_pin_entered=TRUE`、`tvr_set(ONLINE_PIN_ENTERED)`（TVR b3b3） |
| `src/express_terminal_action_analysis.c:878-882` | Online PIN 交易强制 `AC_TC → AC_ARQC`（必须联机） |
| `src/express_kernel.c:262-269` | 若外部误驱动到 `EMV_REQ_OFFLINE_PIN/_LAST/CONFIRM_BYPASS_PIN` → 显式 `EMV_TERMINATE`，不再走 CVM |
| `src/express_kernel.c:347-358` | `EXPRESS_RESULT_OFFLINE_PIN` / `BYPASS_PIN`：忽略并置 `EMV_OPER_UNSUPPORTED`；删除 `offline_pin[13]` 缓冲与 3 个离线 PIN 计数/未完成字段（`inc/express_types.h`） |
| `inc/express_api.h:57` | `EXPRESS_RESULT_OFFLINE_PIN` 注释明确 v4.2 禁止离线 PIN、内核忽略；常量仅为 ABI 兼容保留 |

### 9.4 2nd GENERATE AC / 发卡行脚本 71/72 / DDX（均不支持）

规范：全交易只有一次 GENERATE AC（`P p64 §10.3.2.1-10.3.2.2`）；§13 Issuer Authentication
不支持（`P p73`）；§14 2nd TAA 不支持（`P p74`）；§15 2nd CAA 不支持（`P p75`）；
§16 原文 **"Issuer Script Processing — Not supported in this specification."**（`P p76`）；
Data Exchange/DDX 在 v4.2 全文 0 次（`[PDF 无]`；EP 层规则见 `EP_BOOKB_V2.12_NOTES.md §8.2`）。

| 文件:行 | 改动 |
|---|---|
| `src/express_issuer_script_processing.c:1-38` | 整文件改为**显式拒绝桩**：`express_script_process()` 记录"not supported"并返回 -1、`*reslen=0`；不再调用 `EMVPORT_IccRW` |
| `src/express_kernel.c:365-368` | 授权结果只记录 `online_result`；**不再**把 71/72 重标记为内核私有 `DF8100/DF8200`（脚本由 Entrypoint 用自己的私有 tag 保存） |
| `src/express_complete.c` | 删除整段被注释的 2nd GAC + 脚本实现；头注释标明 2nd GAC/脚本/DDX 不支持 |
| `src/express_terminal_action_analysis.c` | 删除被注释的 2nd GAC/脚本旧实现 |
| `src/express_complete.c:236-239,299-301` | ARC `01/02` 不再走 Issuer Referral：按 `Table 21-25 p99` 的"其它值→拒绝"处理；`EMV_REQ_ISSUER_REFERRAL` 分支删除 |

### 9.5 告警清零（含真实逻辑缺陷）

`kernels/expresspay/**` 当前 host/NDK **0 warning**。逐项：

| 告警/缺陷 | 位置 | 处理 |
|---|---|---|
| 逗号表达式丢弃左操作数 | `express_offline_data_authentication.c:120-122,230-231,441-442,613-617,650-705` 等 11 处 | 统一改为 `return rERR(DA_ICCDATAMISSING)`（调用方据此置 TVR b1b6=1）或 `return rERR(DA_IAUTHERR)`（DDA 失败专用分支），保留原语义并消除 `-Wunused-value` |
| 未初始化指针用于日志 | `express_offline_data_authentication.c:468`（旧 `AIP` 变量在赋值前被 `eTRACE_HEX` 使用） | 改用已赋值的 `ptmp`；AIP 读取改为局部 `aip` + NULL/长度校验 → `DA_ICCDATAMISSING`（debug 打开时不再读野指针） |
| `nextcvm` set but not used | `express_cardholder_verification.c` | 删除 `express_cvm_processing()` 中的影子变量与两处赋值（`express_analy_cvmlist()` 内部同名变量仍参与返回值） |
| `match_iac/match_tac` set but not used | `express_terminal_action_analysis.c` | 删除（赋值后紧跟 `return AC_AAC`，本就不可能有读取点） |
| `char[3]`→`uint8_t*` | `express_terminal_action_analysis.c:887-891` | `"Z1"/"Y1"` 显式 `(u8 *)` 转换 |
| `ret` set but not used | `express_processing_restrictions.c:280,345-351` | 删除变量，`(void)` 忽略 DRL 预处理返回值 |
| `check_flag` set but not used | `express_initiate_application.c` | 删除（原逻辑只用于注释语义） |
| **空指针解引用** | `express_processing_restrictions.c:94-111` `express_is_a_atm()` | 补 `9F35`/`9F40` 判空（原 P1 清单声称已修，实际未落地） |

### 9.6 仍不支持项（规范明示，不是 TODO）

| 项 | 内核状态 | 规范依据 |
|---|---|---|
| MSD / Mag-stripe Mode | 已删除，无分支 | `P p10`、`P p20 §2.2`、`P p43 §7.2.2`、`P p23-24 Table 2-3` |
| TTQ 9F66 | 不使用；主机下发则记录并忽略 | 全文 0 次；`P p89-91 Table 21-7/21-9`；Book B v2.12 §3.1 脚注（9F66 属 Kernel 3/6/7） |
| Offline PIN | 禁止；CVM 方法码 0x03/04/05 一律不支持 | `P p23 §2.10.1`；`P p23-24 Table 2-3`；`P p94-95 Table 21-16` |
| 2nd GENERATE AC / 2nd TAA / 2nd CAA | 不支持；无第二次 GAC | `P p64 §10.3.2.1-2`；`P p74 §14`；`P p75 §15` |
| Issuer Script 71/72 / Issuer Authentication 91 | 不支持；`express_script_process()` 返回 -1 | `P p73 §13`；`P p76 §16` |
| Data Exchange / DDX | 不使用；无代码路径 | `[PDF 无]`；EP 笔记 `§8.2` |
| DRL（Dynamic Reader Limits） | 已删除；遗留 DRL 代码不参与判定 | `P p43 §7.2.2`；`P p10` |
| DDA / fDDA / TSI / 9F40 | 不做/不构造；AIP DDA 位固定 0 | `[PDF 无]`（`P p98 Table 21-24` 仅列出可置位，DDA/fDDA 无算法；TSI 0 次） |
| Terminal Exception File | 可选配置，仅置 TVR b1b5，无处置算法 | `P p58 §9.1`、`P p100 §21.4.3`、`P p98 Table 21-24` |
| Issuer Referral | 无；ARC `01/02` 按其它值拒绝 | `P p99 Table 21-25` |
| Zero Amount / 状态检查 | 由 Entrypoint 预处理判定，内核不重复判 | EP 笔记 3.1.1.3/3.1.1.4/3.1.1.11 |

### 9.7 对 Entrypoint 的契约要求（需要 Lead 决策/落地）

1. **9F6D/9F6E 必须下发**：内核 preprocess 在 AID 选择前就能拿到终端级 9F6D/9F6E；
   未下发时用内核缺省 `C0 / 9C E0 00 03`。若主机只按 AID 级下发，preprocess 的终端能力门禁会使用缺省值，
   真正的 GPO 仍用 AID 级值（`express_app_sel.c` 注入路径）。
2. **"当前无法联网"没有入口**：`bis_unable_online` 没有 Entrypoint 来源，内核只能按 `9F35` 离线唯一判定；
   v4.2 要求"当前无法联网 → 9F6E b3b8=1；纯在线终端无法联网 → 直接拒绝"（`P p33 §4.3.1`）。
   建议 Lead 在 `EMV_PORTING`/preprocess 增加一个"本笔可联机"入参（当前不改 `src/**`，仅登记契约）。
3. **71/72 脚本**：Entrypoint 已用 EP 私有 tag 保存；**不要**调用内核 `express_script_process()`（返回 -1），
   也不要再期望内核重标记 `DF8100/DF8200`。
4. **离线 PIN 结果**：EP/App 不应再投递 `EP_RESULT_KIND_OFFLINE_PIN(5)` / `BYPASS_PIN(6)`；
   内核会忽略并置 `EMV_OPER_UNSUPPORTED`。`EXPRESS_RESULT_OFFLINE_PIN` 常量仅为 ABI 兼容保留。
5. **Online PIN**：EP 需实现 `EMV_REQ_ONLINE_PIN`（显示 `'09'`、收 PIN、TVR b3b3、用 kind=4 回传）；
   内核已强制该笔转 ARQC。
6. **9F35 Modified**：内核只在 GPO 的 PDOL 值中临时返回 `9F35|9F6D`，组包后恢复原值；
   授权/请款/报文侧读 9F35 必须得到未修改值（`P p34`）。
7. **Outcome 合并（G5）**：仍按 §7-2 由 Lead 在 `src/emvoutcome.c` 合并 0xA0/0xB0/0xC0/0xD0。
8. **DRL**：按 v4.2 无需新增 `emv_get_drl()`；仅当回退支持 v4.1 才需要。

### 9.8 门禁与测试结果（本轮最终）

| 命令 | 结果 |
|---|---|
| `scripts\build.ps1 -Tag spec` | `BUILD OK (27 translation units)` + `SYMBOL AUDIT OK`；`[expresspay] defined=125 undefined=64 external=33`；**0 warning** |
| `scripts\build.ps1 -Target ndk -Tag spec` | `BUILD OK (27 translation units)` + `SYMBOL AUDIT OK`；`[expresspay] defined=93 undefined=64 external=33`；**0 warning** |
| `scripts\test_host.ps1 -Tag spec` | `HOST E2E: PASS (assertions=85, failures=0, known-fails=0)` |

本轮未执行 `git add/commit/checkout/reset`；所有改动待 Lead 统一提交。
