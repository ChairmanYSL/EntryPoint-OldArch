# AID / TermParam 结构体最小化（第 2 步已完成）

> 目标：在功能不变的前提下删除 Entrypoint 侧未使用/重复字段，缩小内核栈占用与静态库体积。
> 测量方式：`clang -target armv7-none-eabi`（32 位 ABI）生成 `.size`，与 `scripts/verify.ps1` 第 3 步一致。
> 两步均已完成：**第 1 步**去重复内嵌（`EMV_AIDPARAM.term`）；**第 2 步**参数回读改为原始 TLV 后按"读取点"精简字段。

## 1. 尺寸总表

| 结构体 | 原始 | 第 1 步后 | 第 2 步后（现在） | 说明 |
|---|---|---|---|---|
| `EMV_AIDPARAM` | 1118 B | 632 B | **55 B** | 只剩 `EMV_PreProcess()` 消费的字段 + AID 标识 |
| `EMV_TERMPARAM` | 486 B | 486 B | **23 B** | Entrypoint 内**零读取**，只保留参数文件解析可达的 6 个字段 |
| `EMV_TERMAPP` | 33 B | 22 B | 22 B | 第 1 步已定型（本次未动） |
| `EMV_CANDIDATE` | 65 B | 65 B | 65 B | 保持（EP 候选列表 ×12） |
| `EMV_PREPROCESS_RESULT` | 26 B | 26 B | 26 B | 保持（EP 预处理 ×AID 数） |

合计每 AID 的预处理栈帧从 1118 B（原始）→ 632 B → **55 B**；`emvporting_get_aidparam()` 里原本用于承接
终端级 tag 的 `EMV_TERMPARAM` scratch（486 B）本次直接删除，不再占用该函数的栈。

## 2. 第 2 步的依据与做法

### 2.1 前提（已成立）

内核（`kernels/expresspay`）取参数走**原始 TLV 回读**：`emvporting_get_termparam_tlv()` /
`emvporting_get_aidparam_tlv()` 把参数文件里保存的原始 TLV 字节直接返回，不经过
`EMV_AIDPARAM` / `EMV_TERMPARAM`。因此这两个结构体只服务 Entrypoint 自身，字段精简不影响内核。

### 2.2 数据驱动统计

对两个结构体的每个字段用 `Select-String` 在 `src/**.c` 中统计 `->field` / `.field` 的读写点（排除注释）：

```
$field = 'cl_trans_limit'
Select-String -Path 'src\*.c','src\inc\*.h' -Pattern "(->|\.)$field\b" |
    Where-Object { $_.Line -notmatch '^\s*(//|/\*|\*)' }
```

结论：`EMV_TERMPARAM` 的 33 个字段在 Entrypoint 内**没有任何读取点**；`EMV_AIDPARAM` 的 54 个字段中只有
13 个被 `src/emv.c` 的 `EMV_PreProcess()` 读取，其余 36 个（只写或无引用）删除，另保留 5 个 AID 标识字段
（`aid/aid_len/app_sel_indicator/app_ver/kernel_id`）供 `EMVPORT_GetAppParam()` 契约使用。保留判据：

1. `src/emv.c` 读取的字段；
2. 对外 API（`emv_get_termparam()/emv_get_aidparam()` 及 `_tlv` 序列化）语义需要的字段；
3. 参数解析写入后会被 Entrypoint 自身消费的字段。

### 2.3 保留字段与消费者

`EMV_TERMPARAM`（6 字段，23 B）：

| 字段 | tag | 消费者 |
|---|---|---|
| `ics[7]` | 1F01 | `init_param()` 默认值 + `conv_param()` 回读（结构体 SPI 输出） |
| `term_type` | 9F35 | 结构体 SPI 输出（AIDPARAM 不再重复保存） |
| `status_check_support_indicator` | 1F02 / DFC108 | 结构体 SPI 输出（双 tag 长度校验保留） |
| `cl_trans_limit[6]` | DF20 | 终端级默认值输出（per-AID 值写入 `EMV_AIDPARAM`） |
| `term_caps[3]` | 9F33 | 结构体 SPI 输出（原写 `EMV_AIDPARAM`，但无读取点，移入此处） |
| `term_add_caps[5]` | 9F40 | 同上 |

`EMV_AIDPARAM`（18 字段，55 B）：

| 字段 | tag | 消费者 |
|---|---|---|
| `aid[16]/aid_len` | 9F06 | `EMVPORT_GetAppParam()` 契约（AID 标识） |
| `app_sel_indicator` | DF01 | 同上 |
| `app_ver[2]` | 9F09 | `init_param()` 默认值 + 回读 |
| `kernel_id` | 9F2A | `init_param()` 默认值 0x02 + 回读 |
| `ttq[4]` | 9F66 | `EMV_PreProcess()` 3.1.1.2 |
| `status_check_support` | — | `EMV_PreProcess()` 3.1.1.3 |
| `currency_exp` | 5F36 | `EMV_PreProcess()` 3.1.1.3（状态检查金额 10^exp） |
| `zero_amt_offline_allow` | — | `EMV_PreProcess()` 3.1.1.4 |
| `zero_amt_allow` | — | `EMV_PreProcess()` 3.1.1.4 |
| `cl_trans_limit(_exist)[6]` | DF20 | `EMV_PreProcess()` 3.1.1.5 |
| `cl_floor_limit(_exist)[6]` | DF19 | `EMV_PreProcess()` 3.1.1.6 |
| `cvm_limit(_exist)[6]` | DF21 | `EMV_PreProcess()` 3.1.1.8 |
| `term_floor_limit(_exist)[4]` | 9F1B | `EMV_PreProcess()` 3.1.1.7（4 字节直取 / 6 字节 BCD 换算保留） |

### 2.4 删除字段与理由（TERMPARAM 27 个、AIDPARAM 36 个）

| 组 | 字段 | 理由 / 替代路径 |
|---|---|---|
| DDOL/TDOL（504 B） | `default_ddol(_len)[252]`、`default_tdol(_len)[252]` | Entrypoint 不做 ODA；内核用原始 TLV。`param_len_ok()` 的长度校验保留，不再写结构体 |
| 大块透传 | `merchant_name[128]`、`poi_info[128]`、`proprietary_tag[128]`、`merch_custom_data[20]` | Entrypoint 无读取点；需要时由应用/内核从原始 TLV 取 |
| 商户/收单/终端标识 | `acquirer_id*`、`term_ifd`、`merchant_category_code`、`merchant_id*`、`term_country_code`、`term_id*` | 同上（原为"待 appdata 注入"的 PayPass 遗留，注入代码早已全部注释） |
| 目标百分比/阈值 | `target_percent`、`max_target_percent`、`threshold_value` | 内核风险参数，Entrypoint 不消费 |
| TAC 数组 | `term_action_code_{default,online,denial}(_exist)[5]` | 内核终端行为分析使用，Entrypoint 不消费 |
| 风险数据 | `term_risk_mana_data(_len)[8]` | 同上 |
| RRP | `rrp_min_grace`、`rrp_max_grace`、`rrp_expect_c_apdu`、`rrp_expect_r_apdu`、`rrp_accuracy_threshold` | PayPass 专有，Entrypoint 已移除 `EMV_SetRRPTimeDelay`（见 `src/emv.c`） |
| 非接/移动能力 | `cvmcap_*`、`paypass_kernel_config`、`reader_cl_trans_limit_*`、`msd_cvmcap_*`、`mobile_sup_indicator`、`secure_cap`、`msd_app_ver`、`card_data_input_cap` | 内核私有参数，Entrypoint 不消费 |
| 交易参考货币 | `trans_ref_currency_code/exponent` | 内核不读回，Entrypoint 不消费 |
| 终端能力（AID 侧） | `term_caps`、`term_add_caps`、`term_type` | 移到 `EMV_TERMPARAM`（或本就不该在 AID 记录里） |
| 重复/无引用 | `zero_amt_allow_flag`（0 引用）、`status_check_support_indicator`（AID 侧，改由 `status_check_support` 表达）、`online_pin_support_indicator` | 无读取点 |
| 货币/其他 | `trans_currency_code`、`trans_currency_exponent`（TERMPARAM） | 5F2A/5F36 由 appdata 保存；Entrypoint 无读取点 |

### 2.5 代码侧同步修改

- `src/inc/emv_type.h`：两个结构体改为最小集；新增 C99 编译期尺寸门禁
  `emv_chk_termparam_size[(sizeof(EMV_TERMPARAM) <= 80) ? 1 : -1]`、
  `emv_chk_aidparam_size[(sizeof(EMV_AIDPARAM) <= 160) ? 1 : -1]`（超限即负数组，编译报错）。
- `src/emvporting.c`：
  - `init_param()` 不再预置 DDOL/TDOL 默认值，只保留 `kernel_id=0x02`、`app_ver={0,2}`、`ics` 默认；
  - `conv_param()` 只保留上表字段的赋值；被删字段的 tag 仍走 `param_len_ok()` 长度校验（坏参数依旧拒绝落盘）；
  - `emvporting_get_aidparam()` 删除 `EMV_TERMPARAM` 临时缓冲，改为 `conv_param(NULL, apar, ...)`
    （终端参数文件先填默认，AID 记录再覆盖，语义不变）；同时清掉 `ret` 未使用告警。
- `src/emvspi.c`：`get_termparam()/get_aidparam()` 只委派、未引用被删字段，仅补注释说明"结构体只返回最小集，
  完整参数用 `emv_get_termparam()/emv_get_aidparam()` 的原始 TLV"。
- `src/emv.c`：**无需改动**（`EMV_PreProcess()` 读取的 13 个字段全部保留）。

## 3. 行为变化（API 契约）

| 入口 | 变化 |
|---|---|
| `emv_get_termparam()/emv_get_aidparam()`（`inc/emvapi.h`，原始 TLV） | **不变**：仍原样返回参数文件 TLV。内核 `express_param.c` 依赖此路径 |
| `EMV_SPI.get_aidparam()`（`src/emvspi.c`） | 结构体只填上表 18 个字段；AID 记录里其它 tag 不再映射进结构体（`EMV_PreProcess` 所需字段齐全） |
| `EMV_SPI.get_termparam()`（`src/emvspi.c`，未注册进 `EMV_SPI`，`emv.h` 中已注释） | 只填 6 个字段；完整终端参数请走 TLV API |
| 参数文件写入 `emv_set_termparam()/emv_add_aidparam()` | 不变：仍整串保存原始 TLV，长度校验规则不变 |

已知遗留（本次刻意不动，避免把半接线的功能"打开"）：`ttq`、`status_check_support`、`currency_exp`、
`zero_amt_offline_allow`、`zero_amt_allow` 目前**没有写入方**（`conv_param()` 历史上只写
`status_check_support_indicator`，9F66 TTQ 从未写入 AIDPARAM），因此 `EMV_PreProcess()` 的
3.1.1.2/3.1.1.3/3.1.1.4 分支仍按 0 值运行（改动前即如此）。字段按方案保留，接线是独立的 P1 事项
（需产品确认 TTQ/状态检查金额语义）；该缺口见 `review/04-porting-params.md:670`。

## 4. 验证

```
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1            # host
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Target ndk # NDK r23b
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify.ps1           # 双门禁 + 尺寸报告
```

尺寸探针（与 `scripts/verify.ps1` 第 3 步同法）：

```
clang -target armv7-none-eabi -std=c99 -Os -S -I inc -I include -I src\inc build_probe\size_probe.c -o build_probe\size_probe.s
Select-String build_probe\size_probe.s -Pattern '\.size\s+probe_'
# probe_termparam, 23 / probe_aidparam, 55 / probe_termapp, 22 / probe_candidate, 65
```

结果：host `BUILD OK (27 TU)` + `SYMBOL AUDIT OK`，NDK `BUILD OK (27 TU)` + `SYMBOL AUDIT OK`；
编译期尺寸断言通过。告警：用与门禁相同的 clang 参数单独编译 `src/emvporting.c`，改动前 10 条 → 现在 9 条
（消失的是原 `emvporting_get_aidparam()` 的 `ret` set-but-unused），**未新增**；其余为既有的
`rERR(...)` 结果未使用与"赋值当条件"告警。
参考：本会话实测 host `libentrypoint.a` 195,770 B（改动前）→ 193,074 B（`verify.ps1`）；同期还有并行告警
清理提交，库体积差异仅供参考。

## 5. 后续（不在本步范围）

- `doc/DECISIONS.md` D13 状态需从"第 2 步待做"改为"完成（AIDPARAM 55 B / TERMPARAM 23 B）"。
- `doc/AMEX_PORT_PLAN.md`、`review/**` 中引用的旧尺寸/旧行号属历史记录，未回改。
- `ttq`/`status_check_support` 等 5 个预处理字段的写入方接线（见 §3 遗留）。
