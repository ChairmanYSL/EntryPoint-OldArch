# AMEX/Expresspay 内核移植方案（ZT_AMEX_CORE_V4.2.3 → Entrypoint 解耦架构）

- 任务：task-6（分析 + 方案 + 头文件骨架；**本阶段不写 `kernels/expresspay/src/*.c`**）
- 目标工程：`E:\Work\ZT\Entrypoint`，分支 `feature/amex-entrypoint`
- 源工程（只读）：`E:\Work\ZT\Entrypoint\emv_core_amex_japan`，分支 `feature/entrypoint-port`，版本宏
  `ZT_AMEX_CORE_V4.2.3`（`submodule/emvco/src/emv.c:39`）
- 本文所有 `文件:行号` 均指**美运 V4.2.3 源码**（下称"美运源码"）；Entrypoint 侧引用注明了 `src/` 路径。
  Entrypoint 仍在 Lead 编辑中（`src/**` 行号可能随修复变化），**引用 Entrypoint 时以函数名/接口名为准**，行号仅作定位提示。
  行号已按原始字节（GBK→UTF-8 逐行转换、保留 CR/LF 计数）核对，转换/读取方法见附录 A。

---

## 0. 结论摘要（给 Lead 的 6 条）

1. **不能把美运 `.c` 直接拷进 `kernels/expresspay/src`**：美运内核与 `libentrypoint.a` 有
   **180 个非 static 全局符号同名**（`emv_memcpy`、`appdata_*`、`emvtlv_*`、`emvicc_*`、`EMV_Process`、
   `emvporting_*` 等，完整交集见 §3.4）。可复用代码只有二选一：要么删美运副本、调 Entrypoint 服务；
   要么整库改名。本方案取前者。
2. **入口契约完全兼容**：美运 `EMV_Init/EMV_PreProcess/EMV_Process` 与 Entrypoint 同名同义，
   状态码数值域一致（1..99=中间状态，100..199=结束状态，负数=错误）；仅 4 个状态"名不同值相同"
   （`EMV_REQ_SEEPHONE`→`EMV_SEEPHONE=107`、`EMV_SW_INTERFACE`→`EMV_SWITCH_INTERFACE=106`、
   `EMV_STA_SELECT_NEXT`→`EMV_REQ_SELECT_NEXT=26`、`EMV_REQ_EC_CONFIRM=25` 无对应）见 §2.3。
3. **参数结构可大幅收敛**：美运 `EMV_TERMPARAM` 38B → `EXPRESS_TERM_PARAM` **33B**（15 个字段中仅 6 个被内核读取）；
   `EMV_AIDPARAM` 712B → `EXPRESS_AID_PARAM` **670B**（去掉嵌入 term 副本与 2 个零使用 + 1 个仅写字段，其中 504B 是 DDOL/TDOL
   规格上限，可按需解析后降到 ~166B）；`EMV_CANDIDATE` 331B → 复用 Entrypoint 的 **65B**。见 §5。
4. **参数来源应走"原始 TLV"而非结构体拷贝**：Entrypoint 已把主机下发的终端参数/AID 参数**原样以 TLV 存盘**
   （`src/emvporting.c:670-699`、`:774-823`、`:1094-1141`），内核只需按需取回原始 TLV、用自己的 `conv_param` 解析，
   天然保留美运私有标签（0xDF55/0xDF8307-09/0x1F02/0x1F30/0x9F6D/0x9F6E）。**唯一缺口**：AID 原始 TLV 的公共 API
   未导出（`emvporting_get_aidparam_tlv` 是内部函数），需 Lead 在 `inc/emvapi.h` 加一行声明（§6.2 G1）。
5. **本阶段头文件已落地并自检通过**：`kernels/expresspay/inc/{express_param,express_types,express_api}.h`、
   `kernels/expresspay/include/{express_tags,express_compat}.h`；clang `-std=c99 -Wall -Wextra` 语法零告警，
   结构体尺寸用编译期断言锁定（实测 33/19/670/26/442/148/56 字节）。见 §8。
6. **美运源码存在 3 条 P0（内存破坏类）与 8 条 P1**，必须在移植时一并修复，清单见 §9（含逐条行号）。

---

## 1. 基线事实

### 1.1 美运源码结构（只读）

| 目录/文件 | 行数 | 作用 |
|---|---|---|
| `submodule/emvco/inc/{emv.h,emv_type.h,emv_tag.h,emv_ics.h,emvoutcome.h,emv_utils.h}` | 594/720/409/318/110/197 | 内核公共头（EMV_PORTING、参数结构、状态码、标签、ICS、Outcome） |
| `submodule/emvco/src/emv.c` | 1791 | 内核主文件：初始化/预处理/状态机/Result 接口/工具函数 |
| `submodule/emvco/src/emv_application_selection.c` | 1334 | 候选列表 + 最终选择 + 终端参数装载（**移植时大部分删除**） |
| `submodule/emvco/src/emv_initiate_application.c` | 520 | GPO/PDOL、9F70/9F71/9F6E 判定 MSD/EMV |
| `submodule/emvco/src/emv_read_application_data.c` | 551 | AFL 读取、数据校验 |
| `submodule/emvco/src/emv_offline_data_authentication.c` | 1465 | SDA/DDA/CDA、CAPK 校验、证书黑名单 |
| `submodule/emvco/src/emv_processing_restrictions.c` | 531 | 处理限制 + DRL + TTQ（`express_SetTTQ`） |
| `submodule/emvco/src/emv_cardholder_verification.c` | 841 | CVM 列表分析、离/联机 PIN、Mobile CVM |
| `submodule/emvco/src/emv_terminal_risk_management.c` | 133 | 终端风险管理（含例外文件） |
| `submodule/emvco/src/emv_terminal_action_analysis.c` | 1174 | TAA、GAC、Outcome CVM |
| `submodule/emvco/src/emv_complete.c` | 394 | 1st/2nd GAC 收尾、脚本处理 |
| `submodule/emvco/src/emv_issuer_script_processing.c` | 160 | 发卡行脚本 |
| `submodule/emvco/src/{emvbuf.c,emvtlv.c,emvicc.c,sha1.c,emvoutcome.c}` | 235/639/403/388/223 | **与 Entrypoint 重复，全部删除** |
| `src/emvapi.c` | 805 | 对外 API + 状态推送回调（**删除**） |
| `src/emvporting.c` | 1970 | 参数文件持久化 + conv_param（**删除，改调 Entrypoint**） |
| `src/emvspi.c` | 897 | EMV_PORTING 实现（**删除，改由 Entrypoint/app 提供**） |

### 1.2 构建与门禁（Entrypoint 侧）

- 门禁：`scripts/build.ps1 -Target host|ndk`（`scripts/build.ps1:18-23,64-78`）。当前脚本对
  `kernels/expresspay/src` 目录做**存在性判断**，目录不存在时 `SKIP`（`:71-78`）；
  `CMakeLists.txt:62-75` 同理。**因此本阶段只建头文件不影响门禁**。
- 现状（2026-11-12 本任务收尾时实测）：Lead 的修复已让 host 门禁达到
  `BUILD OK (14 translation units)` + `SYMBOL AUDIT OK`（此前基线为 12 个 TU 编译失败）；
  本任务交付的头文件**未参与该门禁编译**（无 `kernels/expresspay/src`，脚本不加入 expresspay 模块），
  因此加头文件前后门禁行为一致，已复测确认。
- 门禁脚本的两处必改项（Lead 落地，本任务不能改 `scripts/`）：
  1. `scripts/build.ps1:71-78` 的 expresspay 模块 `Includes` 只有 `kernels/expresspay/{inc,include}`，
     没有 Entrypoint 的 `inc/include/src/inc`。内核 `.c` 需要包含 Entrypoint 头时，必须追加这三个目录
     （CMake 侧因 `target_link_libraries(expresspay PUBLIC entrypoint)` 可自动传播，无此问题）。
  2. 符号审计 `scripts/symbol_audit.ps1` 规则要求 `libentrypoint.a` 不含 `amex_/expresspay_` 符号
     （`scripts/build.ps1:14` 注释）→ 内核导出符号请统一 `express_*` / `amex_*` 前缀，符合既有规则。

---

## 2. 内核入口与状态机

### 2.1 三层调用关系（美运原架构）

```
App
 └─ emvapi.c  (emv_initialize / emv_preprocess / emv_process / emv_process2 / emv_set_*)
     ├─ emv.c EMV_Init(EMV_PORTING*)     : 取缓冲区、取终端参数、取应用列表、初始化上下文
     ├─ emv.c EMV_PreProcess(bOnline)    : TTQ 位 + 遍历 AID 参数做非接限额过滤
     └─ emv.c EMV_Process(msginx)        : 状态机（每调用一次推进一个阶段）
         └─ emv_k_interface 等价物：直接 switch 调 emv_* 阶段函数（emv.c:1168-1245）
```

- `EMV_Init`（`emv.c:759-829`）：`alloc_buffer` 至少 `sizeof(EMV_CONTEXT)+2048`（`:776`）；
  `get_termparam` 失败时回填内置默认 `k_emv_aidparam.term`（`:791-797`，默认值见 `emv.c:116-185`）；
  `get_termapps` 返回 <1 或 >128 时回填默认 AID `A0 00 00 00 25 01`（`:806-817`）；
  `config_checksum = crc32(config, 16)`（`:799`，只校验前 16B，见 P2-1）。
- `EMV_PreProcess`（`emv.c:896-978`）：**不是** Entry Point Book B 3.1.1 的完整预处理，只做两件事：
  TTQ 位决定卡类型（`:928-970`），以及遍历 AID 参数 TLV 的 9F/DF20 判断"是否至少有一个 AID 的限额够用"
  （`:934-975`，全不够则返回 `EMV_AMOUNTOVER`）。
- `EMV_Process`（`emv.c:1135-1257`）：见 §2.2。返回值为新状态。

### 2.2 美运状态机（`emv.c:1168-1245` 的 switch）

| 当前状态 | 动作函数 | 行号 | 结果状态典型值 |
|---|---|---|---|
| `EMV_STA_IDLE` | `emv_build_candidate_list` | 1170-1173 | `EMV_STA_CANDIDATES_BUILT` |
| `EMV_STA_CANDIDATES_BUILT`/`EMV_STA_SELECT_NEXT` | `emv_app_sel_init`（最终 SELECT + 装载参数） | 1175-1179 | `EMV_STA_APP_SELECTED` |
| `EMV_STA_APP_SELECTED` | `emv_initiate_application`（GPO） | 1181-1184 | `EMV_STA_APP_INITIALIZED` |
| `EMV_STA_APP_INITIALIZED` | `emv_read_application_data` | 1186-1189 | `EMV_STA_READ_APP_DATA_COMPLETED` |
| `EMV_STA_READ_APP_DATA_COMPLETED` | 仅置状态（卡片信息确认占位） | 1191-1193 | `EMV_REQ_CARDINFO_CONFIRM`(24) |
| `EMV_REQ_CARDINFO_CONFIRM` | `emv_offline_data_auth` | 1195-1197 | `EMV_STA_DATA_AUTH_COMPLETED` |
| `EMV_STA_DATA_AUTH_COMPLETED` | `emv_processing_restrictions` | 1199-1201 | `EMV_STA_PROCESS_RESTRICT_COMPLETED` |
| `EMV_REQ_ONLINE_PIN` / `EMV_STA_PROCESS_RESTRICT_COMPLETED` / `EMV_REQ_CHECK_ID` / `EMV_REQ_OFFLINE_PIN[_LAST]` / `EMV_REQ_CONFIRM_BYPASS_PIN` | `emv_cardholder_verify` | 1203-1217 | `EMV_STA_CARDHOLDER_VERIFY_COMPLETED` 或对应 REQ_* |
| `EMV_STA_CARDHOLDER_VERIFY_COMPLETED` / `EMV_REQ_CONFIRM_FORCE_ONLINE` | `emv_terminal_risk_manage` | 1219-1223 | `EMV_STA_RISK_MANAGEMENT_COMPLETED` |
| `EMV_STA_RISK_MANAGEMENT_COMPLETED` | `emv_action_analyse_temp` | 1225-1229 | `EMV_STA_ACTION_ANALYSE_COMPLETED` / `EMV_REQ_GO_ONLINE` / … |
| `EMV_STA_ACTION_ANALYSE_COMPLETED` | `emv_pre_complete`（1st GAC 前处理） | 1231-1233 | `EMV_REQ_GO_ONLINE`/`EMV_REQ_SEEPHONE`/完成态 |
| `EMV_REQ_GO_ONLINE`/`EMV_REQ_ISSUER_REFERRAL` | `emv_compelte_temp`（2nd GAC/收尾） | 1235-1239 | `EMV_ACCEPTED_ONLINE`/`EMV_DENIALED_*` 等 |

补充契约：

- 状态机**可重入推进**：`emvapi.c` 的 `emv_process()`（`:295-404`）在每次调用里只调用一次 `EMV_Process`，
  再用 `switch(ret)` 把"中间状态"翻译成 `STATUS_CONTINUE` 回调、把"结束状态"翻译成 `STATUS_COMPLETION`
  （`emvapi.c:304-398`）→ 移植后这一层由 Entrypoint 的 `EMV_Process` 直接返回相同数值，回调层可丢。
- **前置条件**：`EMV_Process` 要求 `status ∈ [EMV_STA_IDLE, EMV_COMPLETED)`（`emv.c:1143-1153`），
  且 `card_type != EMV_CARD_UNKNOWN`（`:1162-1165`）；`EMV_REQ_SEEPHONE` + `gExpressmobileagain==1`
  时会把状态重置回 `EMV_STA_IDLE` 重来（`:1145-1148`）——这是 Expresspay "see phone again" 的重入路径，
  移植时必须保留（对应 Entrypoint 的 `EMV_SEEPHONE`）。
- `EMV_ProcessExit`→`EMV_initialize(1)`（清状态+清 Outcome，`emv.c:1767-1777` / `:516-560`）；
  `EMV_Exit` 只置空指针（`:1779-1784`）。

### 2.3 `EMV_*` 状态码对照（数值域一致，4 处不同名）

| 美运 | 值 | Entrypoint（`src/inc/emv_type.h`） | 处理 |
|---|---|---|---|
| `EMV_REQ_SEEPHONE`（`emv_type.h:276`） | 107 | `EMV_SEEPHONE`（`:309`） | 内核对外改用 Entrypoint 名；Entrypoint 调度处 `switch` 需加别名或直接比数值 |
| `EMV_SW_INTERFACE`（`:275`） | 106 | `EMV_SWITCH_INTERFACE`（`:308`） | 同上 |
| `EMV_STA_SELECT_NEXT`（`:265`） | 26 | `EMV_REQ_SELECT_NEXT`（`:274`） | 同上 |
| `EMV_REQ_EC_CONFIRM`（`:264`） | 25 | **无** | 内核内部消化（见 §2.5 第 4 条），不向 Entrypoint 透传 |
| 其余 `EMV_STA_*`/`EMV_REQ_*`/`EMV_ACCEPTED_*`/`EMV_DENIALED_*`/`EMV_*ERR` | — | 同名同值 | 直接透传 |

> 结论：**内核返回给 Entrypoint 的状态码不需要翻译表**，只需把 3 个"同名不同值"的常量改成 Entrypoint 名，
> 并保证不产生 25/26 的歧义（26 统一按 `EMV_REQ_SELECT_NEXT` 语义处理）。

### 2.4 需要 App 交互的 `REQ_*` 状态 → 回调 → 返回值

| 状态 | 美运交互回调（`emv.h:79-174`） | 应用回填（`emvapi.c`） | 移植后 |
|---|---|---|---|
| `EMV_REQ_SEL_CANDIDATES[_AGAIN]` | `sel_candidate_list` | `emv_set_candidate_list_result`（`:529`） | Entrypoint 应用选择层已承担 |
| `EMV_REQ_SEL_ACCOUNTTYPE` | `sel_account_type` | `emv_set_account_type_selected`（`:534`） | Entrypoint `EMV_ResultSelAccountType` |
| `EMV_REQ_CHECK_ID` | `check_id` | `emv_set_check_id_result`（`:539`） | Entrypoint `EMV_ResultCheckID` |
| `EMV_REQ_ONLINE_PIN` | `pin_entry(0)` | `emv_set_online_pin_entered`（`:544`） | Entrypoint `EMV_ResultOnlinePIN` |
| `EMV_REQ_OFFLINE_PIN[_LAST]` | `pin_entry(1/2)` | `emv_set_offline_pin_entered`（`:549`） | Entrypoint `EMV_ResultOfflinePIN` |
| `EMV_REQ_CONFIRM_BYPASS_PIN` | `confirm_bypass_pin` | `emv_set_pin_bypass_confirmed`（`:554`） | Entrypoint `EMV_ResultBypassPIN` |
| `EMV_REQ_CONFIRM_FORCE_ONLINE` | `confirm_force_online` | `emv_set_force_online`（`:559`） | Entrypoint `EMV_ResultForceOnline` |
| `EMV_REQ_GO_ONLINE` | `go_online` | `emv_set_online_result`（`:564`） | Entrypoint `EMV_ResultOnline` |
| `EMV_REQ_ISSUER_REFERRAL` | `issuer_referral` | `emv_set_issref_result`（`:569`） | Entrypoint `EMV_ResultIssuerReferral` |
| 提示类（无回填） | `message` | —（`emvspi.c:101-113` 只记 message index） | Entrypoint `message` + Outcome/UIRequest |

移植后这些回调**全部由 Entrypoint `EMV_PORTING` 统一提供**；内核只通过
`express_post_result(kind, result, data, len)`（`kernels/expresspay/inc/express_api.h`）接收结果，
不再直接挂在 porting 表上。

### 2.5 与 Entrypoint 入口/状态机的映射（移植后目标流程）

| 阶段 | 责任方 | 说明 |
|---|---|---|
| 卡类型/内核识别、PPSE/PSE、候选列表、组合选择、内核选择 | **Entrypoint**（`src/emv_application_selection.c`、`emv_combination_Selection.c`、`emv_selec_kernel.c`） | 美运 `emv_build_candidate_list/select_*` 全部删除（与 Entrypoint 同名且实现重复） |
| `EMV_PreProcess`（Book B 3.1.1） | **Entrypoint**（`src/emv.c:1188-1313`） | 美运 `EMV_PreProcess` 的限额过滤并入 Entrypoint 的 `preprocess_result`；内核 `preprocess` 钩子只负责"选中 AID 的参数装载 + 最终 SELECT" |
| 最终 SELECT + AID/标签/9F2A 保存 + 交易日期/ATC/UN 写入 | **内核**（`express_app_sel_init`，裁剪自 `emv_application_selection.c:1259-1327`） | Entrypoint 传入已选候选（见 §6.3 注册接口 `post_select`） |
| GPO→read data→ODA→处理限制→CVM→TRM→TAA→完成 | **内核** | 原 9 个 L2 模块，全部保留（改名 `express_*`） |
| Outcome / UIRequest / 错误指示 / 下次应用 | **Entrypoint**（`src/emvoutcome.c`） | 内核不再调用 `EMV_SetOutCome`（`emvoutcome.c:34`），改为返回状态；Expresspay 特有 Outcome（0xA0/0xB0/0xC0/0xD0，`emvoutcome.h:11-14`）经 `express_get_outcome()` 附加给 Entrypoint（§6.2 G5） |
| 参数持久化（term/aid/capk/黑名单/DRL/流水） | **Entrypoint**（`src/emvporting.c`） | 内核只调 `emv_get_termparam`/`emvporting_get_aidparam_tlv`/`emvporting_get_drl` 等 |

---

## 3. 模块映射表（逐文件/逐模块）

处置图例：**R**=直接复用 Entrypoint 实现；**A**=改造复用（改签名/补边界）；**N**=内核新写/保留改写；**D**=删除。

### 3.1 内核逻辑模块

| 美运源文件 | 关键函数（行号） | 处置 | 说明 |
|---|---|---|---|
| `emv.c` | `EMV_Init`(759) `EMV_PreProcess`(896) `EMV_Process`(1135) `EMV_Result*`(1259-1410) `EMV_GetData`(1412) `EMV_SetData`(1471) `EMV_ProcessExit`(1767) | **N** | 改名 `express_*`；`EMV_Init` 改为 `express_init`（不接收 porting，Entrypoint 已注入）；`EMV_Result*` 改为 `express_post_result` |
| `emv.c` | `emv_memcpy/memset/memcmp/strlen`(244-298)、`emv_bcd2str/str2bcd/dec2hex/hex2dec/dec2bcd/bcd2dec`(330-451)、`emv_date_compare`(467) | **R** | Entrypoint `src/emvi.h:278-293` 同名同义 |
| `emv.c` | `emv_crc32`(218)、`k_emv_aidparam`(116-185)、`g_express_preset`(196) | **N** | 保留为内核私有（`express_crc32`、`k_express_aidparam`、`express_preset`） |
| `emv_application_selection.c` | `emv_build_candidate_list`(1132)、`select_use_ppse/pse/aid_list`(650-1130)、`select_ddf`(130)、`parse_select_resp`(884)、`deal_p*se_resp` | **D** | 与 Entrypoint `src/emv_application_selection.c` 重复（含同名全局符号） |
| `emv_application_selection.c` | `emv_app_sel_init`(1259-1327) | **A** | 删除候选/PPSE 逻辑；保留最终 SELECT、`load_term_app_param`、AID/50/9F12/87/9F2A、日期/ATC/UN |
| `emv_application_selection.c` | `load_term_app_param`(723-859)、`again_preprocess`(664-721) | **A** | 参数来源改为 `express_param_load_aid()`；`again_preprocess` 与 Entrypoint 3.1.1 重叠，保留但仅用于计算 `CANAI/CVMRLEF/TermEFLEF`（见 §5.4 决策 D2） |
| `emv_initiate_application.c` | `emv_initiate_application`(306)、`pdol_process`/`pre_pdol_process`、`deal_gpo_resp`(239) | **N** | ICC 调用改 Entrypoint `emvicc_get_processing_options`（`src/emvi.h:359`） |
| `emv_read_application_data.c` | `emv_read_application_data`(337)、`express_CheckReadAFL`(166)、`check_data_validation`(274) | **N** | ICC 改 `emvicc_read_record`（`:383`） |
| `emv_offline_data_authentication.c` | `emv_offline_data_auth`(1395) 等 | **N** | 密码学改 Entrypoint porting `recover`/`hash`（`src/emv.h:216-227`）；`sha1.c` 删除 |
| `emv_processing_restrictions.c` | `emv_processing_restrictions`(408)、`ExpressGetDynamicReaderLimit`(89)、`again_drl_preprocess`(199)、`express_SetTTQ`(237) | **N** | DRL 来源改为 Entrypoint `emvporting_get_drl` + 内核 `EXPRESS_DRL_PARAMS`（格式差异见 G2） |
| `emv_cardholder_verification.c` | `emv_cardholder_verify`(825)、`express_AnalyCvmlist*`(133-460)、`express_PerformCVM`(473) | **N** | PIN 走 Entrypoint `EMV_ResultOfflinePIN/OnlinePIN` |
| `emv_terminal_risk_management.c` | `emv_terminal_risk_manage`(110) | **N** | 黑名单调用改 Entrypoint porting `fnd_blackcard/fnd_blackcert`（`src/emv.h:321-332`） |
| `emv_terminal_action_analysis.c` | `emv_action_analyse_temp`(803)、`Express_TermAnalyTransRes`(268) | **N** | GAC 改 `emvicc_generate_ac` |
| `emv_complete.c` | `emv_pre_complete`(110)、`emv_compelte_temp`(189) | **N** | 2nd GAC + 脚本；脚本改用 `script_process`（Entrypoint 也有同名，需二选一） |
| `emv_issuer_script_processing.c` | `script_process`(…) | **A** | 与 Entrypoint `script_process` 同名，二选一：建议用 Entrypoint 版并删美运副本 |
| `emvoutcome.c` | `EMV_SetOutCome`(34)、`EMV_SetUIRequest`(88)、`outcome_*`(139,165,…) | **D** | Entrypoint `src/emvoutcome.c` 统一；Expresspay 扩展状态值另存（G5） |

### 3.2 共享服务模块（全部删除 → 调 Entrypoint）

| 美运 | Entrypoint 对应 | 证据（Entrypoint） |
|---|---|---|
| `emvbuf.c`（appdata_save/save2/save_or/save_and/find/init） | `src/emvbuf.c` | `src/emvi.h:467-475` |
| `emvtlv.c`（emvtlv_decode/get/find_dup、tlv_get/set_tag/len、emvdol_packet*） | `src/emvtlv.c` | `src/emvi.h:443-458` |
| `emvicc.c`（icccommand、select/read_record/gpo/gac/get_data/verify…） | `src/emvicc.c` | `src/emvi.h:299-412` |
| `sha1.c` | Entrypoint porting `hash`（app 实现） | `src/emv.h:227` |
| `emvporting.c`（参数文件/conv_param/CAPK/黑名单/流水/DRL） | `src/emvporting.c` | `src/emvporting.h` + `inc/emvapi.h` 的 `emv_*param` 系列 |
| `emvspi.c`（EMV_PORTING 实现） | `src/emvspi.c` + app 注入 | `src/emv.h:38-349` |
| `emvapi.c`（对外 API/回调推送） | `src/emvapi.c` | `inc/emvapi.h` |

### 3.3 数据流映射（内核读什么、从哪来）

| 内核动作 | 美运实现 | 移植后 |
|---|---|---|
| 取终端参数 | `emvporting_get_termparam()` 读文件 + `conv_param`（`emvporting.c:616-653`） | `emv_get_termparam(buf,&len)`（原始 TLV，Entrypoint `inc/emvapi.h` 已有）→ 内核 `express_param_load_term()` |
| 取 AID 参数 | `emvporting_get_aidparam`（`emvporting.c:779-848`，先全局后 AID 覆盖） | `emvporting_get_aidparam_tlv(inx,…)`（内部函数，**需导出**，G1）→ `express_param_load_aid()` |
| 取应用列表 | `emvporting_get_termapps`（`emvporting.c:898-977`） | Entrypoint `EMVPORT_GetTermApps`（`src/emv.h:275`，其 `EMV_TERMAPP` 含 `kernel_id`） |
| 取 CAPK | `emvporting_get_capkparam`（`:1272`） | Entrypoint porting `get_capkparam`（`src/emv.h:296`） |
| 黑名单 | `emvporting_fnd_cardblack/certblack`（`:1378/:1483`） | Entrypoint porting `fnd_blackcard/fnd_blackcert`（`src/emv.h:321/332`） |
| 流水 | `emvporting_sav_translog/get_translog_amount`（`:1525/:1560`） | Entrypoint porting `sav_translog/get_translog_amount`（`src/emv.h:303/310`） |
| DRL | `emvporting_get_drl`（`:1865`） | Entrypoint `emvporting_get_drl`（`src/emvporting.c:2178+`，格式需核对 G2） |
| ICC 收发 | `emvicc_*` → `icc_rw` | Entrypoint `emvicc_*` → porting `icc_rw`（`src/emv.h:203`） |
| 随机数/时间/ATC | `emvspi.c:627/649/680` | porting `get_random/get_trans_date_time/get_trans_sequence_counter`（`src/emv.h:237/247/258`） |

### 3.4 符号冲突清单（移植的硬约束）

用"取所有 `.c` 顶层函数定义（去 static）求交集"的方法测得：**180 个同名非 static 符号**。按类别：

| 类别 | 数量 | 示例 |
|---|---|---|
| appdata/缓冲区 | 6 | `appdata_save/find/init/save_or/save_and/save_iccdata` |
| TLV/DOL | 12 | `emvtlv_decode/get/find_dup`、`tlv_get_tag/set_len/set_tag`、`emvdol_packet*` |
| ICC | 10 | `emvicc_apdu/select/read_record/generate_ac/get_processing_options/...` |
| porting 持久化 | 40 | `emvporting_*`（含 `conv_param`、`init_param`、`EMV_FNAME`） |
| SPI/porting 实现 | 30 | `message/sel_candidate_list/pin_entry/go_online/icc_rw/recover/hash/get_random/...` |
| 对外 API/状态机 | 60 | `EMV_Init/EMV_Process/EMV_PreProcess/EMV_Result*/emv_initialize/emv_set_*` |
| 工具函数 | 12 | `emv_memcpy/memset/memcmp/strlen/bcd2dec/...`、`EMV_GetData/SetData/TLV_*` |
| outcome | 6 | `EMV_SetOutCome/SetUIRequest/InitOutCome/InitUIRequest/outcome_*` |

→ 因此 **`kernels/expresspay/src` 只允许放"改名后的 9 个 L2 阶段 + 内核上下文 + conv_param"**；
每新增一个文件，都要用 `scripts/symbol_audit.ps1` 与 `llvm-nm` 校验没有 `emv_`/`EMV_` 前缀的导出符号
（`scripts/build.ps1:14` 的审计规则已经把 `amex_/expresspay_` 列为内核前缀）。

---

## 4. 目标文件与链接关系

```
kernels/expresspay/
├── inc/                     # 内核对外（供 Entrypoint/app 调用）
│   ├── express_api.h        # 入口 + 注册描述符 + 结果回传        ← 已交付
│   ├── express_types.h      # EXPRESS_CONTEXT / 阶段原型 / 常量    ← 已交付
│   └── express_param.h      # 参数最小集 + DRL + 预设（含尺寸断言）← 已交付
├── include/                 # 内核内部/与 Entrypoint 兼容
│   ├── express_tags.h       # 美运私有标签（含 DF55 等厂商值）     ← 已交付
│   └── express_compat.h     # u8/u32、覆盖标志、未移植调用红名单   ← 已交付
└── src/                     # 本阶段【禁止创建】
    ├── express_kernel.c     # 上下文/init/preprocess/process/exit/注册
    ├── express_app_sel.c    # 最终 SELECT + load_term_app_param（裁剪）
    ├── express_initiate_application.c / express_read_application_data.c
    ├── express_offline_data_auth.c / express_processing_restrictions.c
    ├── express_cardholder_verification.c / express_terminal_risk_management.c
    ├── express_terminal_action_analysis.c / express_complete.c
    ├── express_issuer_script.c
    └── express_param.c      # conv_param / 参数装载 / DRL
```

- 链接：`libentrypoint.a`（核心，不含内核符号） + `libexpresspay.a`（内核，**只依赖** Entrypoint 与 libc）
  → 最终可执行由 App 链接两者；内核符号全部 `express_*`/`amex_*`，Entrypoint 符号全部 `emv_*`/`EMV_*`，
  通过 `scripts/symbol_audit.ps1` 双向审计。
- 门禁：`scripts/build.ps1 -Target host` 与 `-Target ndk`。`src/` 出现后，host 门禁会自动把
  `kernels/expresspay/src/*.c` 编成 `libexpresspay.a`（`scripts/build.ps1:71-78,116-136`）；
  **本阶段无 src 目录 → 门禁行为与现在完全一致（SKIP expresspay）**，已实测确认。
- 编译选项（`scripts/build.ps1:48-60`）：`-std=c99 -Wall -Wextra -Werror=implicit-function-declaration/
  incompatible-pointer-types/int-conversion/return-type/implicit-int`。内核代码必须满足；
  已有头文件已按该门禁自检（§8）。

---

## 5. 结构体最小集分析（重点）

### 5.1 `EMV_TERMPARAM`（美运 `emv_type.h:510-531`，38 字节，15 字段）

"读取点"=内核代码真正读该字段；"写入点"=conv_param/默认值赋值。只写不读的字段全部可删。

| # | 字段 | 读取点（文件:行） | 来源 TLV | 处置 | 理由 |
|---|---|---|---|---|---|
| 1 | `ics[7]` | `emvi.h:359`（`ics_option` 宏）→ `emv_application_selection.c:599,1019`；`emv_offline_data_authentication.c:109,116,144,529,748,1164,1412`；`emv_terminal_action_analysis.c:79` | ICS 配置（非卡内 TLV，终端配置） | **保留** | 选择确认、CAPK 校验、默认 DDOL、CDA 行为 |
| 2 | `term_type` | `emvi.h:261-265`（`TT_ATTENDED/TT_ONLINE_ONLY/...`）→ `emv_initiate_application.c:86,91` 等 | `9F35` | **保留** | 决定在线/离线能力、PDOL 值 |
| 3 | `term_caps[3]` | 仅 `emvi.h:360` `tc_option` 宏；实际调用点已注释（`emv.c:630,647`、`emv_cardholder_verification.c:588`） | `9F33` | **保留（降级为兜底）** | 需要注入 appdata 9F33；内核不再读 |
| 4 | `term_add_caps[5]` | `emv_application_selection.c:90,92,94`（1F30/ICTI 判定）；`emvi.h:361` `atc_option`（无调用） | `9F40` | **保留** | 扩展选择 / 终端类型判定 |
| 5 | `term_trans_qualifiers[4]` | `emv.c:525`、`emv.c:922` → `g_emv_cnxt->ttq`（后续 `ttq_get` 大量使用） | `9F66` | **保留（改名 `ttq`）** | Expresspay TTQ 起点 |
| 6 | `status_check_support_indicator` | **无读取**（`emv_application_selection.c:795` 已注释） | `1F02` | **保留到 EXPRESS_TERM_PARAM，语义为"注入 appdata 用"** | per-AID 的 `status_check_sup` 才是消费者 |
| 7 | `ec_support_indicator` | 只在 `emv_application_selection.c:816` 经 `param.term.*` 读 | `9F7A` | **保留（改为全局字段）** | 注入 9F7A |
| 8 | `ec_trans_limit_exist` | `emv_application_selection.c:817` | `9F7B` 存在性 | **保留（全局）** | 注入 9F7B |
| 9 | `ec_trans_limit[6]` | `emv_application_selection.c:818` | `9F7B` | **保留（全局）** | 注入 9F7B |
| 10 | `sm_support_indicator` | `emv_application_selection.c:819` | `DF69` | **保留（全局）** | 注入 DF69 |
| 11 | `ClessReaderCapab` | **无读取**（`emv.c:124/132` 仅默认常量） | `9F6D` | **删（改 per-AID）** | 真正消费者是 `EMV_AIDPARAM.cl_capability`（`emv_application_selection.c:787`） |
| 12 | `EnhancedClessReaderCapab[4]` | **无读取**（`emv.c:122/133` 仅默认常量） | `9F6E` | **删（改 per-AID）** | 真正消费者是 `EMV_AIDPARAM.enhanced_cl_capability`（`:778`） |
| 13 | `zero_amt_allow` | **无读取**（`conv_param` 写于 `emvporting.c:381`） | `DF8307` | **保留到全局（暂留，见 P2-3）** | 当前版本零金额逻辑在 Entrypoint 侧 |
| 14 | `is_support_excetionfile` | `emv_terminal_risk_management.c:119` | `DF8308` | **保留** | 例外文件开关 |
| 15 | `is_support_revoke_capk` | `emv_offline_data_authentication.c:263` | `DF8309` | **保留** | CAPK 撤销校验开关 |

**精简结果**（`express_param.h:79-105`）：`EXPRESS_TERM_PARAM` = `ics[7]+term_type+term_caps[3]+
term_add_caps[5]+ttq[4]+status_check+ec_support+ec_exist+ec_limit[6]+sm_support+zero_amt_allow+
exception_file+revoke_capk` = **33 字节**（原 38，删除第 11/12 两个 4+1 字节字段后反而多出 3 个
"从 per-AID 提升为全局"的指示器，净减 5 字节；字段语义更清晰）。

### 5.2 `EMV_AIDPARAM`（美运 `emv_type.h:536-598`，712 字节，49 字段）

消费点只有两处（其余 L2 模块全部经 appdata 读标签）：
`emv_application_selection.c:664-721`（`again_preprocess`）与 `:723-859`（`load_term_app_param`）。

| 字段 | 读取点 | 来源 TLV | 处置 |
|---|---|---|---|
| `term`（嵌入的 38B `EMV_TERMPARAM`） | 仅 4 个字段：`term.ec_support_indicator`(816)、`term.ec_trans_limit_exist`(817)、`term.ec_trans_limit`(818)、`term.sm_support_indicator`(819) | 9F7A/9F7B/DF69 | **删除嵌入整体**，4 字段提升到 `EXPRESS_TERM_PARAM` |
| `aid[16]+aid_len` | `:736` 日志、`:730` 取参 | 9F06 | 保留 |
| `app_sel_indicator` | 仅 `emvporting.c:404` 写；per-AID 语义由 `EMV_TERMAPP.asi` 承担（`emvporting.c:956-963`） | DF01 | 保留（兼容用，标注可删） |
| `app_ver[2]` | `:821` → 9F09 | 9F09 | 保留 |
| `target_percent` / `max_target_percent` / `threshold_value[4]` | `:822/:823/:824` → DF17/DF16/DF15 | DF15-17 | 保留 |
| `default_ddol_len` / `default_ddol[252]` | `emv_offline_data_authentication.c:748`（经 ics 开关）→ `:826-827` → DF55 | **DF55**（注意不是 DF14） | 保留（可二次优化，见下） |
| `default_tdol_len` / `default_tdol[252]` | `emv_terminal_action_analysis.c:79` → `:828-829` → 1F03 | 1F03 | 保留（可二次优化） |
| `term_id[8]` / `term_ifd[8]` / `term_country_code[2]` | `:831/:832/:833` | 9F1C/9F1E/9F1A | 保留 |
| `acquirer_id[6]` / `merchant_category_code[2]` / `merchant_id[15]` / `merchant_name_len+[20]` | `:848/:849/:850/:851` | 9F01/9F15/9F16/9F4E | 保留（`merchant_name` 缓冲区仅 20B，是 P0-2 的根因） |
| `trans_currency_code[2]+exponent`、`trans_ref_currency_code[2]+exponent` | `:836/:837/:838/:839` | 5F2A/5F36/9F3C/9F3D | 保留 |
| `term_action_code_{default,online,denial}_exist+[5]` | `:840-845` | DF11/DF12/DF13 | 保留 |
| `term_risk_mana_data_len+[8]` | `:846-847` | 9F1D | 保留 |
| `online_pin_support_indicator` | `:853` | DF18 | 保留 |
| `extend_sel_sup` | `:854` | 1F30 | 保留 |
| `status_check_sup` | `:796` | 1F02 | 保留 |
| `cl_trans_limit(_exist)`、`cl_floor_limit(_exist)`、`cvm_limit(_exist)`、`term_floor_limit[4]` | `again_preprocess` `:670-714`（→ CANAI/CVMRLEF/TermEFLEF）+ `:794-813` | DF20/DF19/DF21/9F1B | 保留 |
| `enhanced_cl_capability[4]` / `cl_capability` | `:778` / `:787`（`g_express_preset` 优先） | 9F6E / 9F6D | 保留 |
| `term_type` / `term_cap[3]` / `term_add_cap[5]` | `:750/:759/:769`（J03 特殊需求） | 9F35/9F33/9F40 | 保留 |
| **`ttq[4]`** | **零使用**（`:856-857` 已注释；注释明确"express does not need" 9F66 入 appdata） | 9F66 | **删** |
| **`zero_amt_allow`** | **零使用** | DF8307 | **删** |

**精简结果**（`express_param.h:117-207`）：`EXPRESS_AID_PARAM` = **670 字节**（原 712，−42）。
其中 **504 字节是 `default_ddol[252]+default_tdol[252]`**（EMV Book 3 规格上限）。
二次优化建议（不阻塞本阶段）：DDOL/TDOL 只在 ODA/TAA 两个点使用，可改为"解析时直接
`appdata_save(TAG_DF55/1F03,…)`"，结构体不再持有数组 → `EXPRESS_AID_PARAM` 降到 **166 字节**。
是否采纳由 Lead 定（涉及 conv_param 拆两段）。

### 5.3 `EXPRESS_*`（预设 / DRL）与 `EMV_CONTEXT` 私有字段

| 结构 | 源定义 | 字段使用证据 | 精简后 |
|---|---|---|---|
| `EMV_TERM_PRESET`→`EXPRESS_TERM_PRESET` | `emvi.h:220-232` | 写：`emv.c:1037-1132`（8 处 `exist_*`）；读：`emv_application_selection.c:744,746,753,755,763,765,772,774,781,783`（部分行有重复） | **19 字节**，全部保留（`ClessReaderCapab` 无 exist 标志，`emv_application_selection.c:781` 直接判非 0，保持原语义） |
| `SDK_EXPRESS_DRL`→`EXPRESS_DRL` | `emvi.h:105-118` | 写/读：`emv_processing_restrictions.c:96-137,151-194`（默认 DRL）、`:199-235`（`again_drl_preprocess`）、`:301-330`（按 AppProgID 选表） | **26 字节**，全保留；`EXPRESS_DRL_PARAMS` = 442 字节（16 项 + default） |
| `EMV_CONTEXT`→`EXPRESS_CONTEXT` | `emvi.h:141-218` | 见 §5.3.1 | **148 字节**（原结构 = 约 190B + 指向共享缓冲区的指针/计数，换算后见下） |

`EMV_CONTEXT` 字段处置（只列非平凡项；完整读写点见 `express_types.h:88-160` 注释）：

| 原字段 | 读写点 | 处置 |
|---|---|---|
| `emvbuffer_size/tlvbuffer/tlvbuffer_size/termapps/termapps_num/candidates/candidates_num/req_sel_candidates_result` | `emv.c:786,802-817,1235-1236`；`emv_application_selection.c:1245,1265` | **删** → Entrypoint 共享缓冲区/候选列表管理 |
| `config`(38B) | `emv.c:791-799,525,922` | → `EXPRESS_TERM_PARAM term`(33B) |
| `kernel_checksum/config_checksum` | `emv.c:788,799,1435-1447` | **删**（Entrypoint 统一 `EMV_GetData` 内核版本/校验和） |
| `ttq[4]` | `emvi.h:363-364` + 多处 | 保留（运行期改写副本） |
| `enhancecap[4]` | emvi.h:164；**零读写** | 删（P2-4） |
| `CANAI/CVMRLEF/TermCFLEF/TermEFLEF` | 见 `express_types.h:120-131` 注释 | 保留（Expresspay 预处理结果位） |
| `gExpressmobileagain/msdoremv/CVMParameter` | 同上 | 保留 |
| `bis_card_sup_onlinepin/bis_unable_online` | 同上 | 保留 |
| `aip/tvr/tsi`（指针） | `emvbuf.c:44-45` 缓存；`emvi.h:278-288,363-369` | **改为每次 `appdata_find` 取用**（P1-6 悬垂指针） |
| `da_sda_performed/da_dda_performed/da_cda_performed` | `emv_offline_data_authentication.c:1402-1426`；`emv_terminal_action_analysis.c:861,866` | 合并为 `oda_type`（1B） |
| `pay_mode/ec_onlinpin_falg/cv_rule_index/offline_pin_unfinished/offline_pin_tries` | 仅初始化或零使用 | 删（P2-6） |
| `req_*_result/online_result` | 见 §2.4 | 保留（由 `post_result` 写入） |

### 5.4 复核"美运现有 `express_param.h`"（`E:\Work\ZT\NEW_Arch\Entrypoint\expresspay\inc\express_param.h`）

结论：**不满足 V4.2.3 内核需要，不能直接沿用**。逐条：

| 问题 | 证据 |
|---|---|
| `EXPRESS_TERM_PARAM.ttq[3]` **长度错误**（9F66 是 4 字节） | 参考实现 `express_param.h:53` vs 美运 `emv_type.h:515`/`emv_param` 解析 4B（`emvporting.c:355`） |
| `mer_cat_code[2]` 注释标 `9F16`，但 9F16 是 Merchant Identifier（15B），MCC 是 9F15 | 参考 `:48` vs 美运 `emv_type.h:575-576`、`emvporting.c:476-481` |
| `ics[7]` 注释写"9F4E 相关"，9F4E 是商户名 | 参考 `:54` |
| **缺** 9F6D/9F6E（Expresspay 核心终端能力） | 美运 `emvporting.c:373-378,536-541`；消费 `emv_application_selection.c:772-788`、`emv_initiate_application.c:249-296` |
| **缺** 9F7A/9F7B/DF69/1F02/1F30/DF8307 等注入型指示器 | 美运 `emv_application_selection.c:795-819,853-854` |
| **缺** TAC（DF11-13）、商户/收单方/终端号（9F01/9F15/9F16/9F4E/9F1C/9F1E/9F1A）、DF14/1F03 DOL、DF15-17 目标率、9F1D、5F2A/5F36/9F3C/9F3D | 美运 `emv_type.h:536-598`、`emvporting.c:395-565` |
| `EXPRESS_AID_PARAM.cl_cvm_limit[6]` 为**额外字段**（V4.2.3 只有 `cvm_limit`） | 参考 `:76-77` vs 美运 `emv_type.h:584-585` |
| `reader_cl_trans_limit_cdcvm` 是 MasterCard PayPass 的 DF8125，不属于 Expresspay 4.2.3 | 参考 `:84-85` vs 美运无此字段 |
| `kernel_id` 为 1 字节（应为 8 字节 9F2A；单字节只够放首字节） | 参考 `:88` vs 美运 `emv_type.h:643` 的 `rsv` 用法 + Entrypoint `EMV_CANDIDATE.kernel_id[8]`（`src/inc/emv_type.h:685`） |
| 缺 DRL（`EXPRESS_DRL` 有，但 `EXPRESS_AID_PARAM` 未与之建立 AppProgID→DRL 的关联逻辑说明） | 美运 `emv_processing_restrictions.c:301-330` |

**本任务交付的 `kernels/expresspay/inc/express_param.h` 已按上表修正**，并加了编译期尺寸断言。

### 5.5 决策点（需 Lead 裁决）

- **D1（推荐 A）**：内核参数结构独立（`EXPRESS_*`，本方案），Entrypoint 的 `EMV_TERMPARAM/EMV_AIDPARAM`
  只保留**预处理必需子集**。两者通过"同一份原始 TLV 存储 + 各自解析"解耦，互不改头文件。
  - 备选 B：内核复用 Entrypoint 的 `EMV_AIDPARAM`（`src/inc/emv_type.h:591-657`，当前 1118B）→ 需把美运
    46 个字段全塞进去并保留私有标签，头文件膨胀且 PayPass/AMEX 相互牵连，**不推荐**。
- **D2（推荐保留）**：`again_preprocess`（`emv_application_selection.c:664-721`）与 Entrypoint 3.1.1 预处理
  有重叠但**语义不同**（前者计算 `CANAI/CVMRLEF/TermEFLEF` 供 `express_SetTTQ` 使用）。建议：
  Entrypoint 负责 `preprocess_result`（候选级过滤），内核 `preprocess` 钩子再调用一次 `again_preprocess`
  设置 Expresspay 私有标志。重复计算代价可接受（仅比较 4 个限额），换取不修改 Entrypoint 语义。
- **D3（推荐删）**：美运 `EMV_PreProcess` 的"全 AID 限额过滤返回 `EMV_AMOUNTOVER`"（`emv.c:934-975`）
  由 Entrypoint `EMV_PreProcess` 的 `preprocess_result[i].contactless_app_not_allowed` 取代；
  内核不重复实现，避免两套"金额超限"判定不一致。

---

## 6. 必需 Entrypoint 服务清单（函数签名级）

### 6.1 已有、可直接复用（无需改动）

| 服务 | 声明位置 | 内核用途 |
|---|---|---|
| `EMV_PORTING.icc_rw/recover/hash/get_random/get_trans_date_time/get_trans_sequence_counter` | `src/emv.h:203-258` | ICC/密码学/随机/时间/ATC |
| `EMV_PORTING.get_termapps/get_aidparam/get_capkparam` | `src/emv.h:275-296` | 应用列表/CAPK |
| `EMV_PORTING.get_translog_amount/sav_translog/fnd_blackcard/fnd_blackcert` | `src/emv.h:303-332` | 流水/黑名单 |
| `EMV_PORTING.message/pin_entry/go_online/confirm_*` 等 | `src/emv.h:70-178` | 交互 |
| `emvicc_*`（select/read_record/gpo/gac/get_data/verify/…） | `src/emvi.h:299-412` | 全部 ISO 7816 命令 |
| `appdata_*`、`emvtlv_*`、`emvdol_*`、`tlv_*` | `src/emvi.h:443-475` | 数据/TLV/DOL |
| `EMV_GetData/EMV_SetData/EMV_Result*/EMV_ProcessExit` | `src/emv.h:430-618` | 数据与结果 |
| `emv_get_termparam`（原始 TLV 回读） | `inc/emvapi.h` | 终端参数装载 |
| `emvporting_get_drl` / `emv_add_drl` / `emv_clr_drl` | `src/emvporting.c:2166-2200`、`inc/emvapi.h` | DRL |
| `emv_get_aidparam`（结构体版） | `inc/emvapi.h` | 若采用 D1-B 备选 |

### 6.2 缺口清单（需 Lead/Entrypoint 补）

| ID | 缺口 | 建议 | 优先级 |
|---|---|---|---|
| **G1** | **AID 原始 TLV 回读未导出**：`emvporting_get_aidparam_tlv(int32_t inx, uint8_t* par, int32_t* rlen)` 已在 `src/emvporting.c:1094` 实现，但 `inc/emvapi.h` 没有对应 `emv_get_aidparam_tlv` 声明 | `inc/emvapi.h` 增加 `int32_t emv_get_aidparam_tlv(int32_t inx, uint8_t* par, int32_t* rlen);` + `emvapi.c` 薄封装 | **P0（阻塞内核参数装载）** |
| **G2** | DRL 存储格式不一致：Entrypoint `BCTC_DRL`（`src/inc/emv_type.h:649-656`）只有 `Flag/Index/CLROFFLimit/CLRTLimit/CVMLimit`；美运 `SDK_EXPRESS_DRL`（`emvi.h:105-118`）有 `ifenable/AppProgID/Support*`（15 字段） | 方案 1（推荐）：Entrypoint 保持 BCTC_DRL 存储，内核 `express_param_load_drl()` 做字段映射（`CVMLimit` 语义等价）；方案 2：`inc/emv_type.h` 增加 `EMV_DRL_EXPRESS` | P1 |
| **G3** | Entrypoint 的 `EMV_AIDPARAM`（当前 1118B，`src/inc/emv_type.h:591-657`）与美运 `EMV_AIDPARAM`(712B) 字段集不同；若两套共存需明确"谁是权威"（D1） | 采纳 D1-A：Entrypoint 版仅服务预处理；美运字段全部进内核私有 `EXPRESS_AID_PARAM` | P1（设计决策） |
| **G4** | 预处理 3.1.1 需要 9F6E/9F6D、9F66（TTQ）等 Expresspay 终端能力；Entrypoint `EMV_PREPROCESS_RESULT` 无对应位 | Entrypoint 预处理继续只算"是否允许该 AID"；Expresspay 的 TTQ 最终值由内核 `express_SetTTQ` 计算（现状即如此，美运 `emv_processing_restrictions.c:237-345`），无需改 Entrypoint | P2（无需动作） |
| **G5** | Expresspay 扩展 Outcome 值 0xA0(通信错误重起)/0xB0(CDCVM 重起)/0xC0(两次呈卡)/0xD0(呈卡保持)（美运 `emvoutcome.h:11-14`）在 Entrypoint Outcome 枚举中不存在（`src/inc/emvoutcome.h:4-11`） | 内核提供 `int32_t express_get_outcome(EXPRESS_OUTCOME *out)`（内部结构，含 Status/Start/OnlineRespData/CVM/PresentFlag/InterfacePrefer/FieldoffReq/RemovalTimeout），Entrypoint 在组装最终 Outcome 时若内核是 AMEX 则合并 | P1 |
| **G6** | 内核需要"最后选中候选（aid/termapp 索引/kernel_id）"的传递通道；当前 `emv_kernel_interface`（`src/inc/emv.h:621-634`）只有阶段函数指针，无参数 | 注册接口增加 `int32_t (*post_select)(const EMV_CANDIDATE *selected, int32_t termapp_index)`；或 Entrypoint 暴露 `const EMV_CANDIDATE *entrypoint_get_selected_candidate(void)` | **P0（阻塞最终 SELECT）** |
| **G7** | Entrypoint 侧 `emv_combination_Selection.c:123` 无条件 `g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QMSDC;`（调试残留），会把 AMEX 强制改成 PayPass | 删除该行（Lead 的 `src/` 范围） | **P0（集成阻塞，Entrypoint 缺陷）** |
| **G8** | `src/emv_selec_kernel.c:64-71` 的 `EMV_CONTACTLESSTYPE_AMEX` 分支为空 | 在该分支注册 AMEX 内核（`amex_kernel_register()`），并把 `emv_k_interface` 换成内核注册表 | **P0（集成阻塞）** |
| **G9** | `EMV_Result*`（`src/emv.c:1447-1604`）只写 Entrypoint 上下文，不通知当前内核 | 增加 `kernel->post_result(kind,result,data,len)` 调用（对非 AMEX 内核为 NULL） | P1 |
| **G10** | `scripts/build.ps1:71-78` expresspay `Includes` 缺 Entrypoint 头路径（§1.2） | 追加 `inc/include/src/inc` | P1（`src/` 出现后立即需要） |
| **G11** | 美运 `EMV_CANDIDATE` 331B（含 `ASRP_Data[255]`、IIN/9F0C/5F55/5F56/9F7D，`emv_type.h:614-643`）在 Entrypoint 的 `EMV_CANDIDATE`(65B) 中无存放位置 | Entrypoint 候选结构按需扩展，或内核在 appdata 暂存 9F0A；当前记为缺口，不阻塞主流程 | P2 |
| **G12** | 例外文件（Exception File）**无缺口**：美运 `emv_terminal_risk_management.c:119-142` 用 `EMVPORT_FndBlackCard(pan,panlen,psn)` 实现，Entrypoint 已有 `fnd_blackcard`（`src/emv.h:321`，参数同义） | 内核只保留 `is_support_excetionfile` 开关，调用改 `EMVPORT_FndBlackCard` | 无需动作 |
| **G13** | Data Exchange **无缺口**：V4.2.3 源码中无 Data Exchange/DDX 相关实现（`grep -i data_exchange` 命中 0） | 若后续升级到支持 DDX 的 Expresspay 版本，再按 Entrypoint 现有 DDX 能力评估 | 无需动作 |

### 6.3 建议的注册接口（Lead 落地 `inc/entrypoint_kernel.h`）

```c
typedef struct entrypoint_kernel_ops
{
    uint8_t  kernel_id[8];                 /* 9F2A，AMEX = {0x04,0,0,0,0,0,0,0} */
    uint8_t  kernel_id_len;                /* AMEX = 8 */
    uint8_t  contactless_type;             /* EMV_CONTACTLESSTYPE_AMEX = 4 */
    uint8_t  reserved[5];

    int32_t (*init)(void);
    int32_t (*preprocess)(uint8_t b_online);
    int32_t (*process)(void);
    void    (*exit)(void);

    void    (*post_select)(const EMV_CANDIDATE *selected, int32_t termapp_index); /* G6 */
    void    (*post_result)(int32_t kind, int32_t result, const uint8_t *data, int32_t len); /* G9 */
    int32_t (*get_outcome)(void *out);     /* G5，AMEX 私有 Outcome 扩展 */
} ENTRYPOINT_KERNEL_OPS;

int32_t entrypoint_kernel_register(const ENTRYPOINT_KERNEL_OPS *ops);   /* app 启动时调用 */
const ENTRYPOINT_KERNEL_OPS *entrypoint_kernel_find(const uint8_t *kernel_id, uint8_t len);
```

- 与现有 `emv_k_interface`（`src/inc/emv.h:621-634`）兼容：保留后者作为内部阶段表，
  `emv_select_kernel()` 内按 `kernel_id` 从注册表取 `ops`，只填 `emv_process` = `ops->process`；
  init/preprocess 在 select 时调用（与 PayPass 现状 `emv_selec_kernel.c:45-60` 同一模式）。
- 内核侧已按此形状落地 `EXPRESS_KERNEL_OPS` + `amex_kernel_ops()` + `amex_kernel_register()`
  （`kernels/expresspay/inc/express_api.h`），字段顺序一致即可零改动切换。

---

## 7. 与其他内核（PayPass / Contact / 其他）互不影响的约束

1. **符号隔离**（硬约束）：`libexpresspay.a` 不得定义任何 `emv_`/`EMV_` 前缀全局符号；
   `libentrypoint.a` 不得引用 `express_`/`amex_` 前缀符号。落地后跑 `scripts/build.ps1`（自动调
   `symbol_audit.ps1`）。本方案据 180 个冲突符号得出的"删除清单"（§3.2）是达标前提。
2. **上下文隔离**：内核只写 `g_express_cnxt`（`express_types.h`），不碰 `g_emv_cnxt`；
   Entrypoint 只读内核返回的状态码，不读内核上下文（跨内核唯一共享是 appdata 与 porting 表）。
3. **注册表不覆盖**：注册按 `kernel_id` 索引（AMEX=0x04，PayPass=0x02，Visa=0x03，JCB=0x05，
   DISV=0x06，PBOC=0x07，SAMA=0x2D，见 `src/emv_combination_Selection.c:15-21`），
   不允许"最后一次注册覆盖全部"。`emv_k_interface` 里的函数指针在每次 `emv_select_kernel` 时重置
   （建议 Entrypoint 在 select 前 `memset`，避免 PayPass 指针残留导致 AMEX 交易调进 PayPass）。
4. **appdata 生命周期**：Entrypoint 在每笔交易 `EMV_Process` 的 `EMV_STA_IDLE` 分支
   （`src/emv.c:1370-1378`）写交易类型/金额；内核只能在 `preprocess` 之后写 appdata，
   不得在 init 阶段写（否则会被下一次 `appdata_init` 清掉）。内核不缓存 appdata 指针（P1-6）。
5. **porting 回调归属**：交互回调只由 Entrypoint 调用，内核不自持回调指针（删除 `emvspi.c` 后自然满足）。
6. **内核索引（kernel_index）**：美运用 `EMVPORT_GetKernelIndex()`（`emvspi.c:826`）区分参数文件名；
   移植后由 Entrypoint 的 `get_kernel_index()`（`src/emv.h:340`）+ 现有参数文件名规则决定，
   内核不得自行拼文件名（删除 `EMV_FNAME`，`emvporting.c:112-165`）。
7. **Outcome 唯一出口**：内核不调用 `EMV_SetOutCome/EMV_SetUIRequest`（美运 `emvoutcome.c:34/88`）；
   所有 Outcome 由 Entrypoint 组装，内核只通过 `get_outcome`/`post_result` 提供补充字段。

---

## 8. 交给下一名工程师的实施顺序（可直接照做）

| 步骤 | 内容 | 验收 |
|---|---|---|
| S1 | 把 `kernels/expresspay/src/*.c` 建起来（空文件 + 仅 include），先让 `scripts/build.ps1 -Target host` 通过（此时 lib 里只有空对象） | `BUILD OK` + `libexpresspay.a` 存在 |
| S2 | 复制并改名 9 个 L2 模块（`emv_initiate_application.c`→`express_initiate_application.c` …），**不改逻辑**，只把 `emv_*`/`EMV_*` 全局符号改前缀、把 `emvtlv_*`/`appdata_*`/`emvicc_*` 调用指向 Entrypoint；补 `express_compat.h` | 编译零错误、零隐式声明；`nm` 无 `emv_` 导出 |
| S3 | 实现 `express_param.c`：`conv_param`（修 §9 的 P0/P1 边界）+ `express_param_load_term/load_aid`（走 G1） | 用美运 `#if 0` 样例帧（`emv.c:116-185`）构造参数 TLV 做单元自测 |
| S4 | 实现 `express_kernel.c`：init/preprocess/process/exit/注册/`post_result`/`post_select` | 状态机可从 `EMV_STA_CANDIDATES_BUILT` 走到 `EMV_ACCEPTED_ONLINE`（用桩 ICC） |
| S5 | 删除/不引入 §3.2 的重复模块；跑 `symbol_audit.ps1` | 双向符号审计通过 |
| S6 | NDK 门禁 + 与 Entrypoint 联调（Lead 完成 G7/G8/G6 后） | 一笔 AMEX 非接交易全流程 |

---

## 9. 美运源码问题清单（移植时一并修复）

> 分级：P0=内存破坏/必然崩溃；P1=功能错误/规范违规/高概率边界错；P2=健壮性/可维护性。
> 行号均为美运 V4.2.3 原始行号。

### P0

| # | 位置 | 证据 | 影响与修法 |
|---|---|---|---|
| P0-1 | `emv.c:1259-1269` + `emv_application_selection.c:1265,1280-1291` | `EMV_ResultSelCandidates` 直接 `g_emv_cnxt->req_sel_candidates_result = eResult;`（`:1266`）无范围校验；随后 `choice = g_emv_cnxt->req_sel_candidates_result;`（`:1265`）用于 `g_emv_cnxt->candidates[choice]`（`:1269`）与 `load_term_app_param(candidates[choice].rsv[0])`（`:1291`）、`candidate_del(choice)`（`:1280`） | App 传入越界索引 → 候选数组越界读 + `candidate_del` 越界写。修：`emv.c:1266` 前加 `if (eResult < 0 || eResult >= candidates_num) return EMV_ERR`；`app_sel_init` 里再挡一次 |
| P0-2 | `emvporting.c:482-485` | `API_memcpy(apar->merchant_name, tlvs[i].pvalue, tlvs[i].valuelen);`，目标 `merchant_name[20]`（`emv_type.h:578`），宿主 TLV 可 >20 | 主机下发超长 9F4E → 参数结构越界写（该结构在 `emvporting_get_aidparam` 的栈上/调用者栈上）。修：先判 `valuelen <= sizeof(merchant_name)` |
| P0-3 | `emvporting.c:399-402` | `apar->aid_len = tlvs[i].valuelen; API_memcpy(apar->aid, tlvs[i].pvalue, tlvs[i].valuelen);`，目标 `aid[16]`（`emv_type.h:539`） | 9F06 长度未校验（`emvporting_add_aidparam` 在 `:695` 校验了，但 `conv_param` 解析**终端参数文件**时同样会进这个分支，`emvporting.c:816`）→ 越界写。修：校验 `valuelen <= 16` |

### P1

| # | 位置 | 证据 | 影响与修法 |
|---|---|---|---|
| P1-1 | `emvporting.c:318-573`（conv_param 整个函数） | 数十处 `API_memcpy(dst, tlvs[i].pvalue, 固定长度)` 未校验 `tlvs[i].valuelen`：`:343(7)、:349(3)、:352(5)、:355(4)、:359、:363、:367(6)、:370、:374、:377(4)、:381、:385、:389、:407(2)、:410、:416(4)、:433(8)、:436(8)、:439(2)、:443(2)、:446、:449(2)、:452、:456(5)、:460(5)、:464(5)、:474(6)、:477(2)、:480(15)、:534(4)、:537、:540(4)、:551、:555(3)、:559(4)、:563` | 宿主下发短 TLV 时从 TLV 值尾部越界读（可读到宿主缓冲区外，敏感数据泄漏或崩溃）。修：抽 `express_copy_fixed()`，逐项校验长度并补 0 |
| P1-2 | `emvporting.c:558-561` | `case TAG_9F40...: API_memcpy(apar->term_add_cap, tlvs[i].pvalue, 4);` —— 9F40 是 **5 字节**，只拷 4 字节 | 第 5 字节保留旧值/未初始化 → 9F40 注入 appdata 错误，影响 PDOL/CVM。修：改 5 |
| P1-3 | `emvporting.c:917-943` | `emvporting_get_termapps` 里 TLV 解析失败只 `TRACE` 不 `continue`（`:940-943`），随后继续 `EMV_TLV_Find(...,tlvs,...)`（`:947`）使用上一次迭代的残留 tlvs；且循环无 `apps_num < EMV_CFG_MAX_APPS` 上限判断（`:971-972` 直接 `tmp++`） | AID 参数文件损坏/条目 >128 → 用未初始化数据 + 越界写调用者缓冲区（`EMV_Init` 只在 `emv.c:802` 定位起点，没有容量检查）。修：解析失败 `continue`；超上限 `break` |
| P1-4 | `emv_application_selection.c:1239-1248` | `if(emv_memcmp(g_emv_cnxt->candidates[i].aid, "\xA0\x00\x00\x00\x25\x01", 6)) { g_emv_cnxt->req_sel_candidates_result = i; }` —— 美运 `emv_memcmp` 相等返回 0（`emv.c:244-262`），此处未取反 | AID-list 回退路径上会把"非 AMEX AID"选为最终应用 → 后续 SELECT 错卡/错内核。修：改为 `if (!emv_memcmp(...))`（同时补 break） |
| P1-5 | `emv_application_selection.c:1299` | `appdata_save(TAG_9F2A_KERNEL_IDETIFIER, &(g_emv_cnxt->candidates[choice].rsv), 1, UNIQUE);` —— 把 `rsv[0]`（termapp 索引，`:807`/`:294` 赋值）当成 9F2A 内核 ID 存 1 字节 | 9F2A 内容错误（应为 8 字节内核 ID），Entrypoint 的内核选择/Outcome 会读到错误 kernel id。修：存 `candidates[choice].kernel_id/kernel_id_len` |
| P1-6 | `emvbuf.c:42-46` + `emvi.h:278-288,363-369` | `appdata_init()` 把 `g_emv_cnxt->tsi/tvr = appdata_find(...)` 缓存为裸指针；后续 `appdata_save` 会在同一 tlvbuffer 内追加/搬移（Entrypoint 版本还有 `try_compact_memory`，`src/emvbuf.c`） | appdata 压缩/搬移后 `tvr_set/tsi_set` 写悬垂指针。修：不缓存，每次 `appdata_find` |
| P1-7 | `emvspi.c:627-634` | `API_Rng(buf, len);` 返回值被丢弃，函数恒返回 0 | 随机数失败时 9F37 UN 为 0/未初始化，风控/ODA 失效。修：检查 `API_Rng` 返回值 |
| P1-8 | `emvspi.c:649-671` | `int ret = API_TimeGet(&curDateTime);` 失败时仍用未初始化的 `curDateTime` 填充日期时间，且返回 `ret`（调用方 `emv_application_selection.c:1308-1313` 确实检查了 `<0`，但 `g_emv_cnxt->ttq` 等无关） | 时间获取失败时实际写入了随机 BCD 日期（虽被检查，但字段已写）。修：先判 ret 再填 |
| P1-9 | `emvapi.c:359-362` | `case EMV_REQ_CONFIRM_FORCE_ONLINE: EMV_ResultForceOnline(1); //FIXME 不确认直接强制联机` | 强制联机不经用户确认，违反 EMV Book 4 终端行为。修：移植后由 Entrypoint `EMV_ResultForceOnline` 真实等待用户选择 |
| P1-10 | `emv.c:934-975` | `EMV_PreProcess` 里 `while(1)` + `index++` 遍历 AID 参数（`:936-964`），无迭代上限，依赖 `emvporting_get_aidparam_tlv` 读越界后返回错误退出 | 文件异常时无限循环。修：加 `index < EXPRESS_MAX_APPS` |
| P1-11 | `emv.c:1431-1462` | `EMV_GetData` 对内核保留 tag 段（`TAG_KERNEL_BASE`）只处理 3 个 case，`default` 直接落空；且 `pLen` 在 `default` 分支没有置 0（已在上方 `:1414-1417` 置 0，实际安全） | 属于健壮性，非本轮重点；移植后该函数由 Entrypoint 的 `EMV_GetData` 取代（`src/emv.c:1606`） |

### P2（移植时顺手清理）

| # | 位置 | 说明 |
|---|---|---|
| P2-1 | `emv.c:799` | `config_checksum = emv_crc32(0, (u8*)&config, 16)` 只对结构体前 16 字节做校验和，字段改动（如 9F6E）不改变校验和 → 配置篡改检测失效 |
| P2-2 | `emv.c:1145-1148` | `EMV_REQ_SEEPHONE` + `gExpressmobileagain==1` 时直接 `status = EMV_STA_IDLE` 重来，未清 appdata/未复位 CVM 状态 |
| P2-3 | `emvporting.c:380-382` | `zero_amt_allow` 只在 `conv_param` 写入、内核零读取（Entrypoint 侧另有零金额逻辑） |
| P2-4 | `emvi.h:164` | `enhancecap[4]`（9F6E 缓存）零读写 |
| P2-5 | `emvi.h:211` + `emv_processing_restrictions.c:316` | `TermCFLEF` 只有清零没有置位（只有 `TermEFLEF` 在用）→ 死字段 |
| P2-6 | `emvi.h:170,187,177,183,184` | `pay_mode/ec_onlinpin_falg/cv_rule_index/offline_pin_unfinished/offline_pin_tries` 仅初始化或零使用 |
| P2-7 | `emvspi.c:304-335` | `icc_rw` 失败返回 `0x0000`（上层按"超时"处理），输出长度上限硬编码 512（与 `EMV_CFG_ICC_RECV_MAXLEN` 重复定义） |
| P2-8 | `emv_tag.h:455` | Default DDOL 标签被改成 **0xDF55**（注释：与 V2CA0130004V4.1a 案例冲突）→ 移植时保持，但必须在参数文档中标注，避免下一名工程师按 EMVCo 的 0xDF14 改回 |
| P2-9 | `emv.c:116-185` | 内置默认 AID 参数 `k_emv_aidparam` 含硬编码日本 J03 参数（term_type=0x22、MCC=0、AID=A00000002501）；移植后建议保留但置于 `#ifdef EXPRESS_USE_BUILTIN_DEFAULT_PARAM` |
| P2-10 | `emvapi.c:300-400`（`emv_process` 的 `while(flg)`） | `flg` 始终为 0，循环体只执行一次 → 死循环结构；移植后整层删除 |

---

## 10. 风险与待裁决

| 风险 | 说明 | 建议 |
|---|---|---|
| R1 | 本任务收尾时 Entrypoint 核心已能在 host 门禁 `BUILD OK + SYMBOL AUDIT OK`，但 `libexpresspay.a` 尚无 `src`，未做"双库真链接"验证；头文件只做了独立语法/尺寸自检 | 头文件不引入新依赖（已确认：仅 stdint/stdbool + 自定义宏），按 §8 的 S1 起纳入门禁后复验 |
| R2 | Entrypoint 的 `EMV_TERMPARAM`(486B)/`EMV_AIDPARAM`(1118B) 是"PayPass 超集"，且注释（`src/inc/emv_type.h:551-555`）写明"待最小化阶段精简" | 若 Lead 后续按本方案 §5 精简 Entrypoint 版，注意 `EMV_AIDPARAM.term` 被 3 处 PayPass 逻辑引用，需同步改 |
| R3 | `EMV_CFG_MAX_CANDIDATES`/`EMV_CFG_ICC_RECV_MAXLEN` 等宏两边可能不一致（美运 512/128，Entrypoint 自行定义） | 内核只用 Entrypoint 的宏，不重复定义（`express_compat.h` 里已避免） |
| R4 | 美运 `EMV_CANDIDATE` 331B（含 `ASRP_Data[255]`）在 Entrypoint 侧只有 65B，9F0A/IIN/9F7D 等 AMEX 候选扩展信息无处存放 | 若 AMEX 需要 ASRP 过滤（4.2 §…），Entrypoint 候选结构需加字段；当前先记缺口（G11，P2），由 Lead 决定 |
| R5 | 本阶段无 `src/`，`scripts/build.ps1` 的 expresspay 分支不执行 → 头文件"未参与编译" | 已通过独立 clang 自检（见下）；S1 起纳入门禁 |

**附录 A：源码读取方法（GBK + 行号保真）**

```powershell
# 逐行转换：先按 0x0A 切分原始字节，再对每行单独 GBK 解码，保证行号与源文件一致
# （直接 GetString 整文件解码会在畸形双字节序列处吞掉换行符，导致行号漂移 10+ 行）
$gbk = [System.Text.Encoding]::GetEncoding(936)
# 见 本任务实际使用脚本：按 0x0A 切分 → GetString(byte[], offset, count) → 写 UTF-8
```

**附录 B：本次自检记录**

```
$ clang -std=c99 -Wall -Wextra -I kernels/expresspay/inc -I kernels/expresspay/include \
        amex_hdr_check.c -o amex_hdr_check && ./amex_hdr_check
EXPRESS_TERM_PARAM=33
EXPRESS_TERM_PRESET=19
EXPRESS_AID_PARAM=670
EXPRESS_DRL=26
EXPRESS_DRL_PARAMS=442
EXPRESS_CONTEXT=148
EXPRESS_KERNEL_OPS=56
```

（对比：美运原始 `EMV_TERMPARAM=38`、`EMV_AIDPARAM=712`、`EMV_TERMAPP=20`、`EMV_CANDIDATE=331`；
Entrypoint 当前 `EMV_TERMPARAM=486`、`EMV_AIDPARAM=1118`、`EMV_TERMAPP=22`、`EMV_CANDIDATE=65`。）

**附录 C：本任务交付物清单**

| 文件 | 说明 |
|---|---|
| `doc/AMEX_PORT_PLAN.md` | 本文 |
| `kernels/expresspay/inc/express_param.h` | 参数最小集 + DRL + 预设 + 尺寸断言 |
| `kernels/expresspay/inc/express_types.h` | 内核上下文/阶段原型/常量 |
| `kernels/expresspay/inc/express_api.h` | init/preprocess/process/exit + 注册描述符 + post_result |
| `kernels/expresspay/include/express_tags.h` | 美运私有标签（含 0xDF55 等） |
| `kernels/expresspay/include/express_compat.h` | 短类型名/覆盖标志/未移植调用红名单 |
| （未创建）`kernels/expresspay/src/**` | 按任务约束本阶段不建，保证门禁仍只编译 Entrypoint 核心 |
