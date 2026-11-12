# EMVCo Book B（Entry Point）v2.12 — 实现/测试导向笔记

> 用途：ZT-ENTRYPOINT-CORE 的 **Entry Point 层**（组合选择、Outcome、UI Request、预处理）+ 当前 P0/P1 修复。
> 配套文件：`EXPRESSPAY_V4.2_NOTES.md`（Amex 内核层）。EP 层与内核层的边界见 §8。

## 0. 元信息与可追溯性

| 项 | 值 |
|---|---|
| 主源 PDF | `E:\Documents\EMV-Contactless-Book-B-Entry-Point-Specification-v2.12-1.pdf` |
| 版本 | Book B v2.12（页脚 `Page n / 57`，2026 年版权行） |
| 页数 | 57 页（**PDF 页号 == 打印页码**，已验证） |
| 文本提取 | **成功**。PyMuPDF 1.27.2.3 / MuPDF 1.27.2，96,016 字符；57/57 页有文本层 |
| 原文覆盖 | 正文条款 100% 可检索。原书用大量**纯文本流程图**表示分支，提取后成为散落短行 → 我已翻译为判定树（标 `[流程重建]`） |
| 补充源（必须的） | `EMV-Contactless-Book-A-Architecture-and-General-Rqmts-v2.10.pdf`（Book B 把 Outcome 结构、UI Request、预配置数据全部指向 Book A：**Book B §3.5 p41 原文**"Full details of the Outcomes and parameter settings can be found in Book A, Chapter 6"）。Book A 引用格式：`打印页 X / PDF pY`（Book A 打印页 = PDF 页 − 10） |
| 版本错位提示 | 主源是 Book B **v2.12**，补充源是 Book A **v2.10**（本机无 Book A v2.12）。Book A 的 Outcome/UI 定义在 v2.10→v2.12 间可能微调，**涉及认证的条目建议按 v2.12 Book A 复核** `[推断风险]` |

**标注约定**：`【B pXX §Y】`＝Book B 原文页；`【A 打印pXX §Y】`＝Book A 原文；`[推断]`＝我的实现映射；`[流程重建]`＝由原文流程图短行翻译；`[PDF 无]`＝原文检索不到。

---

## 1. 先看这 5 条结论

1. **EP 有 4 个入口**：Start A（Pre-Processing）、Start B（Protocol Activation）、Start C（Combination Selection）、Start D（Kernel Activation）；每个入口都有独立前置条件，**不是"一个入口从头跑到尾"**（【B p10 Table 3-1】）。
2. **Outcome 是 EP 与内核的唯一接口**：8 种 Outcome、11 个必选参数（Book A Table 6-2）；其中 `Start` 参数决定 EP 是否/从哪重启，`UI Request on Outcome/Restart Present` 决定是否弹消息（【A 打印p39-45 §6.2/Table 6-2】）。
3. **Pre-Processing 的 13 条要求是纯算术/位运算，最容易写对也最容易测**（3.1.1.1–3.1.1.13，【B p13-17】）；其中 `>=` / `>` / `==0` 三种边界各有不同（见 §3）。
4. **组合选择 = {ADF Name, Kernel ID} 与 {AID, Kernel ID} 配对**，优先级取 `87` 低位 4 bit（1 最高、0/F 最低），**同优先级按 PPSE 中出现的先后**（【B p20-21、p32-34、p36】）。
5. **Book B 全文没有 `9F33`、没有 `Terminal Capabilities`、没有 `ICTI`**（检索计数 0）：EP 不直接校验终端能力位，它只消费 **Entry Point Configuration Data**（Book A Table 5-2，9 个可选项）并产出 **Pre-Processing Indicators**（Table 5-3，5 个指示位）。"交叉检查"落在组合配置与内核配置之间，属 Book C-n 范围 → 若任务书要求"TTQ/9F33 交叉检查"，需 Lead 明确该要求来自哪份文档。

---

## 2. 入口与状态（Start A/B/C/D、Try Again、Select Next）

### 2.1 四个入口【B p10 Table 3-1】

| Start | 触发者/场景 | 代表环境 |
|---|---|---|
| **A** | 由 reader 触发，`Autorun = No` | 新交易、**金额可变**的 EMV mode 环境 |
| **B** | ① `Autorun=Yes` 时由 reader 触发；② reader 在收到 **Online Request / End Application（参数 Start=B）** 的响应后触发；③ EP 内部处理错误；④ EP 内部处理 **Try Again** | 新交易、**金额固定**（可预计算预处理）或 mag-stripe 环境 |
| **C** | **仅 EP 内部**用于 **Select Next** | — |
| **D** | reader 收到 **Online Request / Request Online PIN（Start=D）** 的响应后触发 | "present and hold" / 输入 Online PIN 后二次上送 |

- 新交易从 A 或 B 开始，取决于金额是否"invariable"（对该 reader 配置恒定，可预计算）【B p10】。
- `Restart flag`（reader 保留）用于 3.2.1.1/3.2.1.2 判定是否重新初始化预处理指示器、是否重放 `UI Request on Restart`【A 打印p62 §8.1.1.19-8.1.1.20】。
- **Try Again / Select Next 由 EP 立即处理，不外抛**；其余 6 种为 Final Outcome（【B p11、p41 §3.5】）。

### 2.2 Try Again 语义

- **不重启 Pre-Processing**：从 **Start B（Protocol Activation）** 重启；`Pre-Processing Indicators` 与候选卡状态保留（【B p41 §3.5.1.3】）。
- reader 侧：若 `Restart flag=1` 且保留的 `UI Request on Restart Present=Yes`，则重放该 UI Request（例：`'21'` Present Card Again）；否则发 `'15'`（Present Card，Status=Ready to Read）【B p19 §3.2.1.2】。
- 若 reader 在 Start B 主动激活（不是 Try Again 内部重启）：**清候选列表**、重置预处理指示器（回到固定值或 0）、重新拷 TTQ【B p18 §3.2.1.1】。

### 2.3 Select Next 语义

- 从候选列表移除当前 Combination → 回 **Start C（§3.3.2.6 Step 3）**（【B p41 §3.5.1.4】）。用于"卡拒绝/内核不适合"时换下一个组合（如 Visa+Kernel 3 但 PDOL 无 9F66 的情形，见 §4.6）。

---

## 3. Pre-Processing（Start A）逐条规则 —— 3.1.1.1 ~ 3.1.1.13

> 全部来自【B p13-17 §3.1.1】，逐条给"条件→动作→代码判定"。`D = Entry Point Configuration Data（A 打印p24-25 Table 5-2）`；`I = Pre-Processing Indicators（A 打印p25 Table 5-3）`。
> **Table 5-2 的 9 个可选项**：Status Check Support flag、Zero Amount Allowed flag、Zero Amount for Offline Allowed flag、Reader Contactless Transaction Limit、Reader Contactless Floor Limit、Terminal Floor Limit(9F1B)、Reader CVM Required Limit、Terminal Transaction Qualifiers(TTQ)、Extended Selection Support flag。
> **Table 5-3 的 5 个指示位**：Status Check Requested、Contactless Application Not Allowed、Zero Amount、Reader CVM Required Limit Exceeded、Reader Contactless Floor Limit Exceeded。

| 条款 | 条件 | 动作 | 代码判定（每 Combination 执行一遍） |
|---|---|---|---|
| 3.1.1.1（p13） | 每笔开始 | 把 I 的 5 个指示位全清 0 | `memset(&ind, 0, sizeof(ind))` |
| 3.1.1.2（p14） | 该 Combination 配置了 TTQ | ① TTQ 配置值 → `Copy of TTQ`；② **byte2 b8=0、b7=0**；其它位不动 | `copy_ttq = cfg->ttq; copy_ttq[1] &= 0x3F;`（清 b8/b7） |
| 3.1.1.3（p14） | Status Check Support flag 存在且=1，且 `Amount, Authorised` = **1 个货币单位** | `I.StatusCheckRequested = 1` | `if (flag && amount == 1) ind.status_check = 1;` |
| 3.1.1.4（p15） | `Amount = 0` | ① 若 Zero Amount for Offline Allowed flag 存在且=1 → 跳到 3.1.1.5（即**不置 Zero Amount 位**）；② 否则若 Zero Amount Allowed flag 存在且=0 → `I.ContactlessApplicationNotAllowed = 1`；③ 否则 `I.ZeroAmount = 1` | 三段 if，注意①是"直接去 5"不是"跳过后面的 6-8" |
| 3.1.1.5（p15） | `Reader Contactless Transaction Limit` 存在 **且 `Amount >= Limit`** | `I.ContactlessApplicationNotAllowed = 1` | `>=`，**不是 >** |
| 3.1.1.6（p15） | `Reader Contactless Floor Limit` 存在 **且 `Amount > Limit`** | `I.ReaderContactlessFloorLimitExceeded = 1` | `>` |
| 3.1.1.7（p15） | Reader Floor Limit **不存在**、`9F1B` 存在 **且 `Amount > 9F1B`** | `I.ReaderContactlessFloorLimitExceeded = 1` | 回退链 |
| 3.1.1.8（p16） | `Reader CVM Required Limit` 存在 **且 `Amount >= Limit`** | `I.ReaderCVMRequiredLimitExceeded = 1` | `>=` |
| 3.1.1.9（p16） | 对有 TTQ 的 Combination：`I.ReaderContactlessFloorLimitExceeded == 1` | `Copy.TTQ byte2 b8 = 1`（Online cryptogram required） | |
| 3.1.1.10（p16） | `I.StatusCheckRequested == 1` | `Copy.TTQ byte2 b8 = 1` | |
| 3.1.1.11（p16） | `I.ZeroAmount == 1` | 若 `Copy.TTQ byte1 b4 == 0`（online capable）→ `byte2 b8 = 1`；否则（offline-only）→ `I.ContactlessApplicationNotAllowed = 1` | 零金额 + 离线唯一终端 = 不允许 |
| 3.1.1.12（p16） | `I.ReaderCVMRequiredLimitExceeded == 1` | `Copy.TTQ byte2 b7 = 1`（CVM required） | |
| 3.1.1.13（p17） | **所有** Combination 的 `ContactlessApplicationNotAllowed == 1` | 直接产出 **Try Another Interface** Final Outcome，参数：`UI Request on Outcome Present=Yes`、`Message Identifier='18'`（Please Insert or Swipe Card）、`Status=Processing Error`、`Removal Timeout=Zero`、其余 `N/A`/`No` | 注意消息是 **'18'**（不是 '1C'） |

**另需**（Start B 场景）：3.2.1.1 若由 reader 在 Start B 启动，则指示器重置为"D 中的 fixed value（Table 5-3 预置值）"或 0，并重拷 TTQ、清候选表【B p18】。

**实现注意**
- 规则 3.1.1.2 与 3.1.1.9-12 是"配置静态值 → 本笔临时副本"的两层结构：**Copy of TTQ 是 per-transaction 的**，绝不能写回配置区（Book A Table 5-3 把它列为 `Entry Point Pre-Processing Indicators` 的一部分）。
- 规则 3.1.1.3/3.1.1.11 用 `Amount` 的**货币单位数**（不是最小单位）：`Amount=1` 指 1.00；`Amount=0` 指 0.00。
- 3.1.1.4③ 与 3.1.1.5 可能同时置位（金额 0 且 limit 存在但 limit>0 → 不会同时；若 limit=0 则 3.1.1.5 命中且 3.1.1.11 也会命中 → 以"不允许"为准）。

---

## 4. Combination Selection（组合选择）

### 4.1 概念与数据

| 术语 | 含义 | 出处 |
|---|---|---|
| **Combination（reader）** | {AID, Kernel ID} | 【B p20】 |
| **Combination（candidate list）** | {ADF Name, Kernel ID, API(若有), Extended Selection(若有)} | 【B p34 §3.3.2.5E、p50 Glossary】 |
| **Kernel ID** | reader 内置内核的标识（实现侧常量） | 【B p20】 |
| **Kernel Identifier `9F2A`** | 卡侧声明需要的内核 | 【B p20】 |
| **Kernel Identifier-Terminal `96`** | EP 选出内核后产生，8 字节 | 【B p39 §3.4.1.4】 |
| PPSE | `2PAY.SYS.DDF01`，**必须**用 PPSE（"Usage of the PPSE is mandatory"） | 【B p29 §3.3.2】 |

### 4.2 PPSE FCI 结构（【B p22 Table 3-2】）

`6F` FCI Template ─ `84` DF Name（O）─ `A5` FCI Proprietary Template ─ `BF0C` FCI Issuer Discretionary Data ─ 1..n × `61` Directory Entry：
- `4F` ADF Name（M）
- `50` Application Label（O）
- `87` Application Priority Indicator（C：多 Combination 时应有）
- `9F2A` Kernel Identifier（C：卡显式指定内核时）
- `9F29` Extended Selection（C：卡支持扩展选择时）
- `9F0A` ASRPD（O）
- 另：`BF0C` 下可带 `9F3E` Terminal Categories Supported List（O）、`9F3F` SDOL（O）

> 额外数据（如 `73` 模板）**必须忽略**；FCI 内元素顺序可变（【B p23】）。

### 4.3 Kernel Identifier（`9F2A`）编码与默认值

- Byte1 高 2 位（b8b7）：`00` = 国际内核（低 6 位 = Short Kernel ID）；`01` = RFU（卡）；`10` = 国内内核 EMVCo 格式（Short+Extended，**≥3 字节**）；`11` = 国内内核专有格式（≥3 字节）（【B p26 Table 3-4】）。
- Short Kernel ID：`000000` = 内核与 ADF Name 关联；`000001` = 1st kernel；`000010` = **Kernel 2**；`000011` = **Kernel 3**；`000100` = **Kernel 4**；`000101` = Kernel 5；`000110` = Kernel 6；`000111` = Kernel 7；`001000` = Kernel 8（【B p26 Table 3-4】）。
- **`9F2A` 缺失或值为 `00` → 按 ADF Name 取默认 Requested Kernel ID**（【B p33 Table 3-6】）：

| Matching AID | 默认 Requested Kernel ID |
|---|---|
| **American Express** | **`00000100`b（Kernel 4）** |
| Discover | `00000110`b |
| JCB | `00000101`b |
| Mastercard | `00000010`b |
| UnionPay | `00000111`b |
| Visa | `00000011`b |
| 其它 | `00000000`b |

- 长度 0 → 取默认；b8b7=`00`/`01` → Requested = byte1；b8b7=`10`/`11` → 需 ≥3 字节且 Short≠`000000`，否则跳过该目录项（【B p33-34】）。

### 4.4 候选列表构建（Step 1/1a/2，【B p30-34】）

```
Step 1: SELECT PPSE('2PAY.SYS.DDF01')
  - SW=9000 且（终端 Terminal Category ∈ 9F3E 或 收到 9F3F）→ Step 1a
  - SW=9000 其它情况 → Step 2
  - 其它 SW → 候选列表为空 → Step 3（→ End Application）
Step 1a: SEND POI INFORMATION（CLA=80 INS=1A P1=00 P2=00, Data='83'Lc{SDOL 值 + POI Info}，Le=00）
  - SW=9000 → 用该响应 FCI 做 Step 2；否则候选空 → Step 3
Step 2: 对每个 Directory Entry（A→E）
  A. ADF Name 缺失或格式非法 → 跳过
  B. ADF Name 与某 reader AID **full match（等长等值）或 partial match（ADF Name 以 AID 开头）** → 记 matching AID；否则跳过
  C. 求 Requested Kernel ID（§4.3 规则）
  D. 该 Combination 支持 Requested Kernel ID?（Requested=0 视为支持；否则需等于 reader 的 Kernel ID）否则跳过
  E. 加入候选：{ADF Name, AID, Kernel ID, API(若), Extended Selection(若)}
Step 3: 候选非空 → §3.3.3 Final；候选空 → End Application（'1C'，Status=Ready To Read，Removal Timeout=0）
```

- **`9F3E`/`9F3F` 格式错 → 丢弃该数据对象并继续**（【B p28 §3.3.1a】）。
- SPI 命令细节【B p55-57 Annex C】：`CLA='80' INS='1A' P1='00' P2='00' Lc=数据长 Le='00'`；数据 = `83` + Lc + (SDOL 请求值) + (POI Info ID `0001` 终端类别，若该类在 9F3E 列表中)；响应 = 同 PPSE FCI 结构，但**不含 9F3E/9F3F**（若带必须忽略）。
- **SDOL 允许请求的终端数据白名单**：`9F02` Amount Authorised、`8B` POI Information、`9F1A` Terminal Country Code、`5F2A` Transaction Currency Code（【B p47 Table A-1】）。其它 tag 终端"可以但非必须"支持 → 实现上建议：白名单外一律按"数据不存在/长度 0"处理（EMV DOL 缺值规则）`[推断]`。

### 4.5 最终组合选择（【B p36 §3.3.3】）

1. 候选只有 1 个 → 选它。
2. 多个 → `87` 中 b4-b1 为优先级：**1 最高、F 最低；0 与"无 87"等价于最低（=F）**；`87` 缺失时 b4-b1 视为 `0000`（【B p23 §3.3.1.1】）。若唯一最高 → 选它；若并列最高 → 取 **PPSE 中出现顺序最靠前**的那个（必要时重放 Step 2 以还原顺序，【B p36 脚注14】）。
3. Extended Selection（`9F29`）：若选中 Combination 含 `9F29` 且该 Combination 的 **Extended Selection Support flag** 存在且=1 → 把 `9F29` 值**追加到 ADF Name 之后**作为 SELECT 数据域（长度约束：`len(9F29) + len(ADF Name) <= 16`，【B p48 Table A-1】）。
4. 发 `SELECT(AID[+Extended Selection])`。
5. SW≠9000 或响应格式错：
   - 若存在 Issuer Authentication Data / Issuer Script → End Application（'1C'，Ready To Read）；
   - 否则 **移除该 Combination → 回 Start C**（【B p37 §3.3.3.5】）。
6. **Visa 专项（3.3.3.6, p38）**：若选中 AID 指示 Visa 且内核为 Kernel 3，且 FCI 的 PDOL **缺失或不含 `9F66`** → 移除该 Combination，回 Start C。**这条是"TTQ 只在 Kernel 3 场景被检查"的直接证据**，与 Amex(Kernel 4) 无关。
7. 通信错误（Transmission/Protocol/Timeout）→ 回 **Start B**（【B p38 §3.3.3.7】）。

### 4.6 关于任务书里的 "ICTI"

**`[PDF 无]`**：Book B v2.12 全文检索 `ICTI` = 0（Book A v2.10 亦为 0）。若需求文档有 ICTI，请 Lead 提供来源文档；本笔记无法给出规范条款。

---

## 5. Kernel Activation（Start D）

- EP 激活选中 Combination 的内核，并把下列数据交给内核（【B p39 §3.4.1.1-3.4.1.3】）：
  1. 该 Combination 的 **Entry Point Pre-Processing Indicators**（Book A Table 5-3）；
  2. SELECT(AID) 的 **FCI** 与 **SW1SW2**（Start D 重启时不需要）。
- 生成 **Kernel Identifier-Terminal `96`（8 字节）**（【B p39-40 §3.4.1.4/Table 3-7】）：Byte1-3 = 选中 Combination 的 Kernel ID（无 Extended 时 Byte2/3=`00 00`）；Byte4 b8=1 表示 reader 支持 Kernel 8，b7=1 表示本笔 Combination 配了 Kernel 8；Byte5-8 = `00`。
- 之后所有命令/响应由内核处理，EP 退出直到内核返回 Outcome。

---

## 6. Outcome 结构与取值（EP ↔ 内核 ↔ reader）

### 6.1 8 种 Outcome【A 打印p34-38 §6.1 Table 6-1】

| Outcome | 性质 | 谁来产生 | 谁来处理 |
|---|---|---|---|
| Select Next | 即时 | 内核 | EP（→ Start C） |
| Try Again | 即时 | 内核 | EP（→ Start B） |
| Approved | Final | 内核 | EP 处理参数 → reader |
| Declined | Final | 内核 | EP → reader |
| Online Request | Final（可带 Start） | 内核 | reader/terminal 去联机，然后按 Start 重启 EP |
| Request Online PIN | Final（可带 Start） | 内核 | reader 收 PIN，然后按 Start 重启 EP |
| Try Another Interface | Final | 内核 **或** EP（异常时自己产生） | reader |
| End Application | Final | 内核 **或** EP | reader |

### 6.2 11 个必选参数（【A 打印p39-44 §6.2 Table 6-2】）—— 这是 EP 层数据结构的"字段清单"

| 参数 | 取值 | 语义/实现要点 |
|---|---|---|
| **Start** | `B` / `C` / `D` / `N/A` | B=从 Protocol Activation 重启（"two presentments"、End Application 确认拔卡后重启、Try Again）；C=从 Combination Selection（仅 Select Next）；D=从 Kernel Activation（"present and hold"、Online PIN 二次上送，**不拔卡、场保持**）；N/A=不重启 |
| **Online Response Data** | `EMV Data` / `Any` / `N/A` | 仅当联机响应含 `91`/`71`/`72` 时才重启内核（EMV Data）；`Any`=无条件重启 |
| **CVM** | `Online PIN` / `Confirmation Code Verified` / `Obtain Signature` / `No CVM` / `N/A` | 内核的 CVM 诉求/结论；`N/A`=内核无偏好，由 POS 受理规则决定 |
| **UI Request on Outcome Present** | Yes/No | Yes → 随 Outcome 携带并立即显示的 UI Request（§7） |
| **UI Request on Restart Present** | Yes/No | Yes → 该 UI Request 在 EP 重启（Start B）时重放（与上一条成对使用，如"联机前提示 + 联机后指示"） |
| **Data Record Present** | Yes/No | 是否有可用于授权/请款的数据记录（内核提供，格式见各 Book C-n） |
| **Discretionary Data Present** | Yes/No | 是否有 discretionary data（如 balance、持卡人名）；**用法不在本规范范围** |
| **Alternate Interface Preference** | `Contact Chip` / `Mag-stripe` / `N/A` | Try Another Interface 时内核的偏好（是否采纳由支付系统规则决定） |
| **Receipt** | Yes / N/A | 是否要打印/电子提供凭条；注意：CVM=`Obtain Signature` 时终端可无条件打签名凭条 |
| **Field Off Request** | hold time（100ms 单位，含 0） / N/A | 非 N/A 即"关闭场"；hold time 用于延迟下一次场变更 |
| **Removal Timeout** | time（**100ms 单位，默认 0=不启动**） / 值 | 仅 Online Request 用：超时后提示拔卡；联机结果回来/无法联机时停止计时，并把超时结果留给重启后的内核；超时动作=发 `'17'` + Status=Card Read Successfully（【A 打印p63 §8.1.1.21】） |

### 6.3 EP 的处理规则（【B p41-42 §3.5】）

| 条款 | 规则 |
|---|---|
| 3.5.1.1 | `UI Request on Outcome Present = Yes` → **EP 自己发**该 UI Request |
| 3.5.1.2 | `Field Off Request != N/A` → 关场，并按 hold time 保持 |
| 3.5.1.3 | Outcome = Try Again → 回 Start B |
| 3.5.1.4 | Outcome = Select Next → 移除当前 Combination → 回 Start C |
| 3.5.1.5 | 其它 → 作为 Final Outcome 交给 reader，**并附带**：参数集、内核给的数据、选中的 ADF Name（含 Extended Selection）、Kernel Identifier-Terminal（`96`） |

> 实现含义：EP 只需对 3 个参数动作（UI Request、Field Off、Start），其余参数**原样透传**给 reader/terminal；不要解释 `Data Record`/`Discretionary Data` 内容（那是 Book C-n 的事）。

### 6.4 reader/terminal 侧对 Final Outcome 的处理（【A 打印p45-55 §6.3 Table 6-3/6-4/6-5】）

| 状态 | 处理要点 |
|---|---|
| 第一次 Final Outcome | Approved → 记"批准"、通知持卡人、按需出凭条、收签名/Online PIN、用 Data Record 建请款记录；Declined → 记"拒绝"；Online Request → 联机授权（用 Data Record；CVM=Online PIN 时提交密文 PIN），拿到响应或"无法联机"后按 Start 重启 EP；Try Another Interface → 换 contact chip/mag-stripe（可参考 Alternate Interface Preference，用 ADF Name 尝试选中同一应用）；End Application → 结束 |
| 第二次 Final Outcome（Online 之后） | 内核重启后可能再给 Approved/Declined/Online Request…；处理同上 |
| 第三次 Final Outcome（Online PIN 之后） | 同上，但用同一份交易数据二次上送 |
| 通用 | 每个 Final Outcome 都要处理 `Field Off Request`（关场+hold）、`Removal Timeout`（启表）、`UI Request on Outcome`；`Start != N/A` → 置 Restart flag 并保留 `Start/Online Response Data/UI Request on Restart`（【A 打印p62 §8.1.1.19-20】） |

---

## 7. UI Request（显示/状态/金额）与消息 ID

### 7.1 UI Request 数据字段（【A 打印p53-54 §7.1 Table 7-1】）

| 字段 | 规则（可直接实现的） |
|---|---|
| **Message Identifier** | 二字符十六进制标识，对应 §9.4 标准消息。**未识别 → reader 必须忽略，且不改变当前显示** |
| **Status** | 交易状态（拔卡提示等），用于 LED/蜂鸣。**未识别 → 忽略，不改灯、不发声** |
| **Hold Time** | 整数，单位 **100ms**，默认 0；语义 = **延迟处理"下一条" UI Request 的 Message Identifier**，直到 Hold Time 结束（不是"显示这么久"）；只有需要最短显示时间的消息才应带 Hold Time；"alert 类定时动作（LED 序列/蜂鸣）不会被后续 UI Request 截断" |
| **Language Preference** | 若存在（EMV `5F2D`，ISO 639-1）且 reader/terminal 支持该语言 → **本条及之后所有消息**都用该语言直到交易结束（【A 打印p61 §8.1.1.17】） |
| **Value Qualifier** | 本规范只定义 `Amount` 或 `Balance` |
| **Value** | 与消息一起显示的金额/余额；按 **Currency Code** 表示，**前导零抑制但至少保留 1 位有效数字**（0.09 显示为 `0.09`，不是 `.09`/`09`/`9`；0 显示 `0.00`） |
| **Currency Code** | ISO 4217 数字码 |

**reader 强制要求**（【A 打印p61-62 §8.1.1.15-8.1.1.18】）：呈现消息与状态的语义；Hold Time 内收到的后续 UI Request 不得处理；支持且请求了 Language Preference → 后续全部用该语言；Value Qualifier 存在且币种可识别 → 按要求显示金额/余额。

### 7.2 标准消息 ID 表（EP/内核会用的）【A 打印p82-85 Table 9-5】

| ID | 文本 | 典型场景 |
|---|---|---|
| '03' | Approved | 批准 |
| '07' | Not Authorised | 拒绝 |
| '09' | Please enter your PIN | CVM=Online PIN |
| '0F' | Processing error | 非恢复性错误（配合 '17'/'18' 理解） |
| '10' | Remove card / Please remove card | "卡未移走"事件 |
| '14' | Welcome | reader Idle 状态 |
| '15' | Present card | Ready to Read（金额可同屏） |
| '16' | Processing | reader 处理中（= Processing 状态） |
| '17' | Card read OK. Remove card | 可拔卡（= Card Read Successfully） |
| '18' | Please insert or swipe card | 非接条件不满足（= Processing Error: conditions for use of contactless not satisfied） |
| '19' | Please present one card only | 冲突（= Contactless collision detected） |
| '1A' | Approved, please sign | 批准且需签名 |
| '1B' | Authorising, please wait | 联机中 |
| '1C' | Insert, swipe or try another card | 换卡/换接口 |
| '1D' | Please insert card | 换 contact chip |
| '1E' | （清显示） | 清屏 |
| '20' | See phone for instructions | 需在手机侧操作（= 特殊 Processing Error） |
| '21' | Present card again | ① 联机后要求再次拍卡；② 可重试的错误；通常配 Ready to Read |

> 注意：'14'~'21' **只用于接触式/非接**的分工——'14'~'21' 是非接专用（'0F'/'10' 两种接口都用但条件不同）（【A 打印p82】）。

### 7.3 Book B 中固定的 UI Request 组合（可作回归用例）

| 场景 | 参数 | 出处 |
|---|---|---|
| 全部 Combination 都不允许 | '18' + Status=Processing Error | 【B p17 §3.1.1.13】 |
| Protocol Activation 正常 | '15' + Status=Ready to Read | 【B p19 §3.2.1.2】 |
| 冲突发生 / 冲突解除 | '19' + Status=Processing Error / '19' + Ready to Read | 【B p19 §3.2.1.4/3.2.1.5】 |
| 候选列表为空 | '1C' + Status=Ready To Read | 【B p35 §3.3.2.7】 |
| SELECT 失败且有 issuer 数据 | '1C' + Status=Ready To Read | 【B p37 §3.3.3.5】 |

---

## 8. EP 对终端参数与配置的要求（"交叉检查"到底在哪层）

### 8.1 EP 消费的配置数据只有 Table 5-2 的 9 项

见 §3 开头。**EP 不读 9F33、不读 9F35、不读 TTQ 作为"能力交叉检查"**：
- `[PDF 无]` Book B v2.12 全文 `9F33`=0、`Terminal Capabilities`=0、`ICTI`=0。
- TTQ 是 EP 唯一"半读半写"的终端数据（`Copy of TTQ`），且**只在 Combination 配置了 TTQ 时才处理**（3.1.1.2、3.1.1.9-12）。
- 内核能力/终端能力的匹配（如"卡要求 CVM 但终端无 PIN pad"）由内核在 Book C-n 内处理；EP 只通过 Outcome 的 `CVM` 参数把结果告诉 reader。

### 8.2 数据交换（Data Exchange）

- Book B：`[PDF 无]`。
- Book A（【A 打印p60-61 §8.1.1.12-8.1.1.14】）：同一交易内**只有内核发起 Data Exchange 时**终端才可发送；内核期望响应时终端必须发信息**或空响应**；必须发给正确内核。
- 对 Amex：Expresspay 内核不使用 Data Exchange（`EXPRESSPAY_V4.2_NOTES.md` 已检索确认）→ 本工程 AMEX 分支应无需实现；多内核共存的 EP 曾需支持。

### 8.3 EP 处理的"数据元素通用规则"（易漏）

| 条款 | 规则 | 出处 |
|---|---|---|
| 3.6.1.1 | 卡返回的数据对象若标记为 terminal/reader sourced → **忽略**，当不存在 | 【B p43】 |
| 3.6.1.2 | 卡返回的未识别数据对象 → **忽略**，当不存在 | 【B p43】 |
| 3.6.2.1 | padding 按 EMV Book 1 Annex B 与数据格式规则 | 【B p43】 |
| 3.6.3.1 | **TLV 顺序任意，必须接受**（不要按固定顺序解析） | 【B p43】 |

---

## 9. 强制 / 可选条款清单（按实现落点）

| 落点 | 强制（shall） | 可选/条件 | 出处页 |
|---|---|---|---|
| Pre-Processing | 每 Combination 计算 5 个指示器；TTQ copy 与改写；3.1.1.4-3.1.1.13 全量 | StatusCheck flag、Zero Amount flags、Extended Selection flag、TTQ 本身都是"若存在" | B p13-17 |
| Protocol Activation | 场激活+polling；'15'；冲突 '19'；不得用 Higher layer-INF | UI Request 重放（Restart flag=1 且有保留的 Restart UI） | B p18-19 |
| Combination Selection | 必须 SELECT PPSE；候选构建 A-E；最终选择优先级；SELECT AID；3.3.3.5 失败处理 | SPI（9F3E/9F3F 存在时）；ASRPD 使用；Extended Selection | B p29-38 |
| SPI 命令 | 卡请求时必须发；`CLA=80 INS=1A` | 追加 POI Info（除 0001 外需卡用 `8B` 明确请求） | B p55-57 |
| Kernel Activation | 激活内核；传指示器/FCI/SW；生成 `96` | Start D 时无需 FCI/SW | B p39-40 |
| Outcome | 8 种；11 参数；3.5.1.1-3.5.1.5 处理 | Start/Online Response Data/CVM/Alternate Interface/Receipt 的消费由 reader 决定 | B p41-42 + A 打印p39-55 |
| UI Request | 未识别 ID/Status 必须忽略；Hold Time 必须生效；Language Preference 需粘住 | Value Qualifier/Value/Currency 显示 | A 打印p53-54、p61-62 |
| 数据规则 | 忽略 terminal-sourced/未识别对象；任意 TLV 顺序 | — | B p43 |

---

## 10. 可测点（派生，`[推断]`）

| 用例 | 输入 | 期望 |
|---|---|---|
| 组合优先级 | PPSE 3 项，API=2/1/0 | 选 API=1 |
| 同优先后序 | 两个 API=1，PPSE 顺序 A 在 B 前 | 选 A |
| API 缺失 | 无 87 | 视为最低，等同 F |
| Kernel ID 缺省 | 仅 Amex ADF Name，无 9F2A | Requested Kernel = `00000100`（Kernel 4） |
| Partial AID | ADF Name=`A00000002501ABCD`，reader AID=`A00000002501` | 匹配成功 |
| Kernel 不支持 | 卡 9F2A=Kernel 5，reader 无 Kernel 5 | 跳过该目录项；候选空 → End Application '1C' |
| SW≠9000 换下一候选 | 2 个候选，第一个 SELECT 返回 6A82 | 移除第一个，回 Start C 选第二个 |
| Visa+Kernel3 无 9F66 | 选中 Visa/K3 且 PDOL 无 9F66 | 移除该 Combination（3.3.3.6） |
| 金额=限额 | Reader Contactless Transaction Limit=1000，Amount=1000 | 不允许（`>=`），最终 '18' + Processing Error |
| 金额=floor+1 | Floor Limit=1000，Amount=1001 | FloorLimitExceeded=1 → TTQ b2b8=1（若配 TTQ） |
| 金额=1 单位 | Amount=1，支持 StatusCheck | StatusCheckRequested=1 → b2b8=1 |
| 零金额+离线唯一 | Amount=0，Copy.TTQ b1b4=1 | ContactlessApplicationNotAllowed=1 |
| TTQ 复位 | 配置 TTQ=任意，进 Pre-Processing | Copy.TTQ b2b8=0、b2b7=0 |
| CVM 限额边界 | CVM limit=500，Amount=500 | CVMRequiredLimitExceeded=1 → b2b7=1 |
| Extended Selection | 9F29='00'，ADF Name 长 7 | SELECT 数据 = ADF Name+`00`，总长 ≤16 |
| Outcome 重启 | Online Request + Start=D | 内核二次上送且**不拔卡、场不关** |
| Removal Timeout | Online Request + Removal Timeout=20 | 2 秒后提示 '17'（100ms 单位） |
| Field Off Request | 参数=5 | 关场并保持 500ms 再处理后续场变更 |
| UI 未识别 ID | Message Identifier='EF' | 显示保持不变 |
| Hold Time | '20' 带 1s，立刻来 '21' | '21' 延后 1 秒才处理 |
| TLV 乱序 | FCI 里 `A5` 在 `6F` 之外的任意顺序 | 正确解析 |

---

## 11. 未覆盖 / 需外部来源

| 内容 | 原因 |
|---|---|
| Book A v2.12 原文 | 本机只有 v2.10；Outcome/UI 定义以 v2.10 引用，认证前建议取 v2.12 复核 |
| Book C-n（Kernel 1-8） | 内核内部行为、Data Record 格式、Discretionary Data 定义均不在 Book B |
| Book D / L1 非接协议 | Book B 多处引用 `[EMV L1 Contactless]`（polling、冲突、ATTRIB），本笔记不展开 |
| 物理 UI 图形/时序细节 | Book A 第 9 章（LED/蜂鸣/双行显示）属"建议"，非认证要求（【A 打印p53 附近说明】） |
| 证码 | EMVCo Book AB Test Plan（本机：`EMVCo_TTA_Contactless_Book_AB_Test_plan_v210a_211224_CATA.pdf`），本笔记未提取 |
