# AID / TermParam 结构体最小化（进行中）

> 目标：删除未使用/重复字段，缩小内核栈占用与静态库体积，保持功能不变。
> 测量方式：`clang -target armv7-none-eabi -S` 生成 `.size`（32 位 ABI）。
> 当前分两次推进：**第 1 次已完成（去除重复内嵌），第 2 次待做（参数回读改为原始 TLV，再大幅精简）。**

## 1. 当前尺寸（第 1 次优化后）

| 结构体 | 优化前 | 现在 | 说明 |
|---|---|---|---|
| `EMV_AIDPARAM` | 1118 B | **632 B** | 删除内嵌的 `EMV_TERMPARAM term`（486 B，Entrypoint 内无读取点）；其余 504 B 为 `default_ddol[252]`+`default_tdol[252]`（规格上限） |
| `EMV_TERMPARAM` | 486 B | 486 B | 含 `merchant_name[128]`、`poi_info[128]`、`proprietary_tag[128]` 等大字段，第二次优化目标 ≤ 80 B |
| `EMV_TERMAPP` | 33 B（含重复 `rsv`、`kernel_id[8]`） | **22 B** | 修重复成员 + `kernel_id` 改标量 |
| `EMV_CANDIDATE` | 65 B | 65 B | 保持（EP 候选列表 ×12） |
| `EMV_PREPROCESS_RESULT` | 26 B | 26 B | 保持（EP 预处理 ×AID 数） |

`emvporting_get_aidparam()` 现在用函数内 `EMV_TERMPARAM` 临时缓冲承接 AID 记录里的终端级 tag（不再写入 `EMV_AIDPARAM`）；内核侧参数获取走 `emvporting_get_aidparam_tlv()/get_termparam_tlv()` 原始 TLV 回读。

把 `EMV_AIDPARAM` 加入 `EMV_Init` 缓冲布局是不做的：它只出现在 `EMV_PreProcess` 的栈帧（每次调用 632 B，较原 1118 B 节省 486 B 栈）。

## 2. 第 2 次优化方案（待实施）

1. **参数回读改原始 TLV**：`emvporting_add_aidparam/add_termparam` 在解析成功后，把规整后的 TLV 原始串额外持久化到独立 tag（如 `DF830B`）或参数文件附加区；`emvporting_get_*_tlv` 直接返回该串。这样：
   - 内核（AMEX/PayPass）拿到完整原始参数，不依赖 Entrypoint 结构体字段；
   - Entrypoint 的 `EMV_AIDPARAM` 只保留自用字段（预估 ≤ 80 B）：`ttq[4]`、`status_check_support`、`currency_exp`、`cl_trans_limit(_exist)`、`cl_floor_limit(_exist)`、`cvm_limit(_exist)`、`term_floor_limit(_exist)`、`zero_amt_allow(_flag)`、`zero_amt_offline_allow`、`aid[16]/aid_len/app_sel_indicator`。
   - `EMV_TERMPARAM` 只保留：`ics[7]`、`term_type`、`term_caps[3]`、`term_add_caps[5]`、`status_check_support_indicator`、`cless reader caps`、`ec_*`、`sm_support`、`zero_amt_allow`、`exception/revoke` 标志（预估 ≤ 40 B）。
2. **DDOL/TDOL**：Entrypoint 不做 ODA，参数由内核按原始 TLV 自取 → Entrypoint 侧可删除 `default_ddol/tdol[252]`（−504 B），内核自己的 `EXPRESS_AID_PARAM` 保留（方案 §5：670 B，其中 504 B 为规格上限）。
3. **编译期门禁**：在 `scripts/build.ps1` 加 `-WerrorAll` 前置清理；`_Static_assert(sizeof(EMV_TERMPARAM) <= 64)` 等断言防回涨（C99 无 `_Static_assert`，用 `typedef char chk[(cond)?1:-1]`）。
4. **回归**：host+NDK 门禁 + 参数文件样例（美运 `#if 0` 样例帧）解析/回读一致性测试。

## 3. 美运内核侧（已由 `doc/AMEX_PORT_PLAN.md` §5 定稿）

| 结构体 | 尺寸 | 备注 |
|---|---|---|
| `EXPRESS_TERM_PARAM` | 33 B | 15 字段中仅 6 个被内核读取 |
| `EXPRESS_AID_PARAM` | 670 B | 504 B 为 DDOL/TDOL 规格上限；二次可降到 166 B |
| `EXPRESS_TERM_PRESET` | 19 B | 9F33/9F35/9F40/9F6D/9F6E 预设 |
| `EXPRESS_DRL` / `EXPRESS_DRL_PARAMS` | 26 B / 442 B | 4.2 规范已移除 DRL，保留用于兼容旧配置 |
| `EXPRESS_CONTEXT` | 148 B | 内核交易上下文 |
| `EXPRESS_KERNEL_OPS` | 56 B | 注册表描述符 |

## 4. 验收标准

- host + NDK 双目标 `BUILD OK`、`SYMBOL AUDIT OK`；
- 结构体尺寸断言通过且记录进本文；
- 参数解析/回读样例测试通过（`doc/spec/EXPRESSPAY_V4.2_NOTES.md` 的限额默认值用例）；
- `libentrypoint.a` 体积不回升（当前 193 KB，其中调试宏已关闭）。
