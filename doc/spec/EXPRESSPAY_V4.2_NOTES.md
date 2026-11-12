# Expresspay Terminal Technical Specification v4.2 — 实现/测试导向笔记

> 用途：ZT-ENTRYPOINT-CORE（feature/amex-entrypoint）AMEX 内核移植 + 当前 P0/P1 修复的规范依据。
> 只写可指导实现与测试的条目；规范流水账不抄。

## 0. 元信息与可追溯性

| 项 | 值 |
|---|---|
| 主源 PDF | `E:\Work\SVN Project\emv_doc\卡组织规范\Expresspay\V4.2\Expresspay Terminal Technical Specification v4.2.pdf` |
| 版本/日期 | Version 4.2, October 2022（PDF 元数据 creationDate 2022-10-13，作者 Chester Schendel） |
| 页数 | 115 页（PDF 页号 == 规范打印页号，已验证 p19→"19"、p100→"100"） |
| 文本提取 | **成功**。PyMuPDF 1.27.2.3 / MuPDF 1.27.2，`get_text("text")`，共 253,569 字符 |
| 原文覆盖 | 115/115 页均有文本层；**正文条款 100% 可检索**。含合并单元格/空单元格的位表（Table 21-7/21-9 等）纯文本会丢空列，位表由我按原表行列重建，重建处标 `[表重建]`。DET（§21.6, p102-112）跨页大表仅抽样核对，未逐格重建 |
| 辅助源（仅交叉核对，不替代） | `Expresspay-KnowledgeBase-v4.1.md`（标 `[KB4.1]`，二手）、v4.1 PDF（标 `[v4.1]`） |

**标注约定**

- `【P pXX §Y】`＝该结论出自 PDF 原文，页码为 PDF 页，§ 为规范章节号。原文页均可在本 PDF 检索到。
- `[推断]`＝我的实现/映射推断，规范未明说，需 Lead 或认证方确认。
- `[PDF 无]`＝该内容在本 PDF 中**全文检索不到**（不是"我没找到"，是 `Select-String` 计数为 0）。
- `[KB4.1]`＝只来自 v4.1 知识库，v4.2 已失效或未复核。

---

## 1. 先看这 6 条结论（与任务书预设不同的地方）

1. **"Amex TTQ 9F66 位定义"在规范上不存在。** 本 PDF 全文 `9F66`/`TTQ`/`Terminal Transaction Qualifiers` 出现次数 **0**（§21.6 DET 亦无此 tag）。Amex 终端能力用 **9F6D（Contactless Reader Capabilities，1 字节）+ 9F6E（Enhanced Contactless Reader Capabilities，4 字节）** 表达，并在 PDOL 只要 9F35 时以 **Terminal Type–Modified = 9F35 OR 9F6D** 回给卡片（【P p33-34 §4.3.2；P p89-91 §21.3.2】）。详见 §3.6。
2. **9F40 在 Expresspay v4.2 中同样为 0 次**；`fDDA`、`TSI`、`Issuer Script(71/72)`、`2nd GENERATE AC` 也是 0 次/明示不支持。**异常文件（exception file）** 只在 §9.1/§21.4.3 作为"可选配置"出现，无处理算法。
3. **v4.2 相对 v4.1 删掉了两块**：Mag-stripe Mode 相关内容、Dynamic Reader Limits（DRL）。正文里多处保留 `[Section removed]` 并写明原因（【P p43 §7.2.2 "DRL functionality is no longer supported"；P p10 版本历史】）。`[KB4.1]` 里以 Magstripe/DRL 为前提的条目对 v4.2 一律不可用。
4. **Entry Point 与 Expresspay 内核是二选一关系**：本规范 §2.7 明确"围绕 EMVCo Entry Point 设计终端者必须按 **Book C-4** 编码，而不是本规范"（【P p22 §2.7】）。本工程是 EntryPoint 内核工程，因此 **EP 层规则看 `EP_BOOKB_V2.12_NOTES.md`，Amex 卡交互层规则看本文件**，两者不要互相引用错。
5. **离线 PIN 被禁止**：任何 Expresspay 终端在 Expresspay 交易中 **must not** 处理 Offline PIN（【P p23 §2.10.1】）；Mobile 场景的"Plaintext Offline PIN"被重定义为 Mobile CVM（【P p49 §8.2.3.2】）。
6. **本规范没有证码/测试用例**。证码在 Amex L2 认证文档里（见 §7 末尾列出的本机文件），本文件 §7 是由条款反推的测试点，全部标 `[推断]`。

---

## 2. 内核流程 × 数据依赖 × 代码动作

### 2.0 总流程图解（【P p19 §2.1 Figure 2-1】）

```
Pre-kernel processing / Card activation
  → Application selection (§3)
  → Initiate Application Processing (§4)         [GPO]
  → Read Application Data (§5)
  → Offline Data Authentication (§6)              [SDA / CDA，无 DDA/fDDA]
  → Processing Restrictions (§7)
  → Cardholder Verification (§8)
  → Terminal Risk Management (§9)
  → 1st Terminal Action Analysis (§10)            [GENERATE AC 第 1 次且唯一一次]
  → 1st Card Action Analysis (§11)
        ├─ TC  → Transaction Completion (§17)：离线批准
        ├─ AAC → §17.2.1（可 Try Another Interface，否则拒绝）
        └─ ARQC→ Online Processing (§12) → 按 ARC(8A) 走 §17
  → Transaction Completion (§17)
```

三种交易形态（【P p21-22 §2.4/§2.5】）：**Offline**（只到 §10/§11）、**Partial Online（immediate）**（§3–§12，实时上送）、**Delayed Authorization**（授权延后，mPOS-C/CSP 不支持）。三种形态都必须使能 ODA。

### 2.1 逐阶段依赖表

| # | 阶段（条款/页） | 终端侧输入 | 卡数据（TLV tag） | 关键判定（可直接写成 if） | 代码动作 |
|---|---|---|---|---|---|
| 1 | Pre-kernel / App Selection【p25-31 §3】 | Amount, Authorized `9F02`、UN `9F37` **必须已就绪**（p26 §3.2.1）；AID 级 Reader Limits；mPOS 联网能力 | PPSE FCI：`6F`/`84`/`A5`/`BF0C`；`61` 目录项：`4F` ADF Name、`87` API、`50` Label、`9F2A` Kernel Id、`9F0A` ASRPD | ① `9F02 >= ReaderContactlessTransactionLimit` 且限额已定义 → **不得发起交易**（p27 §3.2.2，`>=`）；② PPSE 返回 `6A82` → 按 List of AIDs 用 partial AID `A00000002501` 选（p30）；③ PPSE 缺 `6F/A5/BF0C` 或格式错 → **终止**（p30）；④ 目录项 AID 与终端 AID 全/部分匹配即入候选（partial match 必须支持），`87` 缺省按 0，无效目录项忽略（p30）；⑤ 候选空 → §17.2.4 请另一笔（p30） | 建候选表（ADF Name/AID/API）；按 API 升序（1 最高，0/F 同最低）取最高；同优先级按候选表顺序试（p31 §3.4.2） |
| 2 | Protocol Activation【p27-28 §3.3】 | 是否"Try Again 重启中"、是否已"提前拔卡" | — | ① 场激活后无卡 → 消息 `'15' Present Card`；② 冲突 → `'19'`（p27-28）；③ 提前拔卡 → 保持金额/交易类型、**生成新 UN**、消息 `'21'`、从 §3.3 重启（p28）；④ 卡未被移走超时 → `'10'`（p28） | 复位读卡状态；新 UN 只在规定的两处生成（提前拔卡、Try Again） |
| 3 | IAP / Pre-PDOL【p32-35 §4.3.1】 | 9F6D、9F6E（配置值）、Reader CVM Required Limit、Terminal Type `9F35`、能否联网 | — | ① 进场先把 **9F6D byte1 bit4 = 0**、**9F6E byte3 = '00'**（p33）；② CVM 限额未定义 → **视为最大值**；`9F02 >= CVM 限额` → 置 9F6D b1b4=1 且 9F6E b3b7=1；③ 离线唯一终端（Terminal Type `x3`/`x6`）或当前无法联网 → 9F6E b3b8=1；④ 纯在线终端当前无法联网 → 直接 §17.2.6.2 拒绝（p33） | 在构造 GPO 之前完成上述位运算；结构体里 9F6D/9F6E 用"每笔重算的临时副本"，不能污染配置区 |
| 4 | PDOL 处理【p33-34 §4.3.2】 | 9F6D/9F6E/9F35 等 | PDOL（来自 SELECT ADF 的 FCI `9F38`） | ① 有 PDOL：按 9F38 顺序填值，**不得过滤卡片请求的任何数据**（p33-34）；② PDOL 含 `9F35` 且 **不含** `9F6E` → 回 **Terminal Type–Modified = 9F35 OR 9F6D**（例：`22`|`C8`=`EA`）；③ PDOL 同时含 `9F35` 与 `9F6E` → 回**未修改**的 9F35；④ PDOL 不含 `9F35` 但含 `9F6E` → 回 9F6E；⑤ 无 PDOL → 命令数据 `8300`；⑥ GPO 响应格式错/缺强制数据 → 终止（p34） | 9F35 原值只用于报文，Modified 值**仅本笔临时用**（p34 原文明确） |
| 5 | GPO 响应判定【p34-35 §4.3.3/§4.3.4】 | — | AIP `82`、AFL `94`、可选 9F71 | ① AIP b2b8=1 → EMV Mode 双方支持；=0 → 不支持：若 9F6E b1b7(Magstripe)=0 且终端支持 AEIPS(9F6E b1b8=1) 且非 Mobile(AIP b2b7=b2b6=0) → §17.2.2 Try Another Interface，否则 §17.2.4（p35）；② AIP b2b7=1 → 按 Mobile 处理，否则 Card（p35）；③ 9F71 存在且 byte3=`03`(Blocked) → **TVR b3b6=1**（p35）；④ Card 交易必须**忽略** 9F71（p35） | `is_mobile = AIP.b2b7`；`9F71` 只在 is_mobile 时留存 |
| 6 | Read Application Data【p36-37 §5】 | — | AFL 指向记录；强制项见 Table 21-13：`5A` PAN、`5F24` 失效期、`8C` CDOL1（p93） | ① 只读 AFL 指定记录（p37）；② 任一 Table 21-13 项缺失 → **终止交易**（与交易模式无关，p37）；③ 非法数据值**不得**导致终端死锁/不可用（p37） | 读记录结果入 TLV 库；缺强制项走"终止"而**不是**降级 |
| 7 | Offline Data Authentication【p38-41 §6.2】 | 启用开关；≥6 组 Amex CAPK（p40 §6.2.1） | AIP b1（b7 SDA / b1 CDA）；`8F` CAPK Index、`90` Issuer PK Cert、`92/93`、`9F32/9F46/9F47/9F48`、`93` SSAD | ① 终端必须支持 **SDA 和 CDA**；启停可配置（p39-40）；② CDA 优先，卡无 CDA 有 SDA 则 SDA；③ **不执行 ODA 的两种情形**：终端未启用，或本笔将离线拒绝 → 此时 **TVR b1b8=1**（p40）；④ SDA 执行 → TVR b1b2=1(SDA Selected)，失败 → b1b7=1；⑤ CDA **只用 mode 2**（EMV 4.3 Book2 Annex D4），本规范无 2nd TAA（p40 §6.2.3）；CDA 失败 → TVR b1b3=1，且按 EMV4.3iv §6.3.2 处理：TC/ARQC 仍各自继续（p41、p66-67） | 无 DDA 分支、无 fDDA 分支（[PDF 无]） |
| 8 | Processing Restrictions【p42-44 §7.2】 | 终端 AID 级 Application Version Number=`0001`（p99）；Terminal Country Code `9F1A`；是否 ATM | `9F08` 应用版本、`9F07` AUC、`5F28` Issuer Country Code、`5F25` 生效日、`5F24` 失效日、`9F70` 卡接口能力 | ① 版本不同 → TVR b2b8=1（p43）；② AUC 违规（domestic/international cash/goods/services、ATM 位 b1b2 / 非 ATM 位 b1b1）→ TVR b2b5=1（p43-44）；③ 未到生效日 → TVR b2b6=1；已过失效日 → TVR b2b7=1（p44）；④ Delayed Auth：若 9F70 b2b8=1 则查 b2b7/b2b6，违规同样置 b2b5（p44）；⑤ 无 9F70 时按"支持 contact EMV"处理（p43 §7.2.1） | 每项独立置位，不短路 |
| 9 | Cardholder Verification【p45-57 §8】 | Reader CVM Required Limit、9F6E b2 能力位、终端 Online PIN/签名能力 | AIP b1b5(CVM 支持)、`8E` CVM List、`9F71` | 见 §5.2 专章 | 见 §5.2 |
| 10 | Terminal Risk Management【p58-59 §9.2】 | Reader Contactless Floor Limit；否则 Terminal Floor Limit `9F1B`；可选异常文件 | PAN（比对异常文件） | ① **无条件执行**，不看 AIP 的 TRM 位（p59）；② **禁止**随机交易选择与 velocity checking（p59）；③ Reader Floor Limit 存在：`=0` 或 `9F02 > floor` → **TVR b4b8=1**；④ 无 Reader Floor 但有 `9F1B`：`9F02 > 9F1B` → b4b8=1；⑤ 两者都无 → **跳过该检查**（p59） | 注意 `>=`/`>` 差异：Transaction Limit 用 `>=`，Floor Limit 用 `>`，且 floor=0 强制置位 |
| 11 | 1st Terminal Action Analysis【p60-64 §10.3.1】 | TAC-Denial/Online/Default（每终端配置类型一套） | `9F0E` IAC-Denial、`9F0F` IAC-Online、`9F0D` IAC-Default；`9F35` 终端类型 | 按终端类型选检查序列（Table 10-2, p62）：Offline Only→Denial；Online Only→Denial(+无法联网→AAC)；Offline+Online→Denial→Online→(无法联网时)Default；Delayed Auth→Denial→Online。任一位命中即按表置 cryptogram 类型 | 命中判定：`(TVR & (IAC_x | TAC_x)) != 0`（依 EMV 4.3iii §10.7 语义；规范只写"任一对应位置位"） |
| 12 | GENERATE AC【p63-64 §10.3.2】 | — | `8C` CDOL1（Cryptogram Version '01'，p94 Table 21-15：`9F02/9F03/9F1A/95/5F2A/9A/9C/9F37`） | ① 全交易**只有这一次** GENERATE AC（无 2nd GEN AC，p64 §10.3.2.1-10.3.2.2）；② 请求 TC/ARQC/AAC 由 #11 决定；③ 请求 CDA 时按 EMV4.3iii §6.5.5 置参考控制参数 | CID 期望值记录，供 #13 校验"卡是否越权改判" |
| 13 | 1st Card Action Analysis【p65-69 §11.2】 | — | 响应 `77`（Format 2）/`80`（Format 1）+ SW；`9F27` CID、`9F26` AC、`9F36` ATC、`9F10` IAD、CDA 签名 | ① 仅 **Mobile** 且 SW=`6984` → 走 §11.2.1；非 Mobile 的 `6984` 按**非法状态字**处理（p66）；② 请求 CDA 时响应必须是 Format 2 且含 CDA 数据，否则按格式错处理（p66）；③ **TC 只允许在请求过 TC 时出现，否则拒绝交易**；**ARQC 只允许在请求过 TC/ARQC 时出现，否则拒绝**（p68）；④ CDA 失败 → TVR b1b3=1，TC/ARQC 继续各自流程（p67）；⑤ CID 非 TC/ARQC/AAC → 终止或 Try Another Interface（p67） | CID 与"请求类型"必须交叉校验；这是可测试的硬判定 |
| 14 | Online Processing【p70-72 §12.2】 | 联网能力；ARC 映射表 | `8A` ARC、`91` Issuer Auth Data（**不支持**） | 按 Table 21-25（p99）解释 `8A`：`00/08/10/11`=批准；`12`=可换接口否则拒绝；`13`=可请求 Online PIN 否则换接口；**其它值=拒绝**。授权请求数据须安全保存至最终结果再清除（p71） | 只映射 4 类，不要"未知 ARC=批准"的兜底；Online PIN 走 §17.2.5 |
| 15 | Completion【p77-81 §17】 | — | — | 见 §6.1 | — |

### 2.2 命令支持矩阵（【P p23-24 §2.10.2 Table 2-3】）

| 命令 | 必须支持 |
|---|---|
| SELECT / GET PROCESSING OPTIONS / READ RECORD / GENERATE AC | **Mandatory** |
| EXTERNAL AUTHENTICATE、GET CHALLENGE、GET DATA、INTERNAL AUTHENTICATE、VERIFY | **Not Supported** |

> 实现含义：不需要实现 GET DATA 路径（Expresspay v4.2 用不到），`VERIFY` 不存在 → 离线 PIN 不可能成立，与 §2.10.1 一致。

---

## 3. 终端能力字段位定义（9F6D / 9F6E / 9F35-Modified / 9F33）

### 3.1 9F6D Contactless Reader Capabilities（1 字节，终端强制配置）【P p89 Table 21-7；P p89-90】

`[表重建]`：原表只有 b8/b7/b4 有定义，b6-b5、b3-b1 保留且必须置 0。

| b8 | b7 | b4 | 含义 |
|---|---|---|---|
| 0 | 0 | 0 | Deprecated（Expresspay 1.0） |
| 0 | 0 | 1 | Not Available for Use |
| 0 | 1 | 0 | Deprecated（≥3.0 Magstripe - CVM Not Required） |
| 0 | 1 | 1 | Deprecated（≥3.0 Magstripe - CVM Required） |
| 1 | 0 | 0 | Deprecated（Expresspay 2.0 - EMV and Magstripe） |
| 1 | 0 | 1 | Not Available for Use |
| 1 | 1 | 0 | **Expresspay ≥3.0 EMV and Magstripe - CVM Not Required** ✔v4.2 合法值 |
| 1 | 1 | 1 | **Expresspay ≥3.0 EMV and Magstripe - CVM Required**（仅 pre-PDOL 临时置位） |

- 本规范终端的 9F6D 只能是 **`11XX0XXX`（CVM 不需要）** 或 pre-PDOL 临时 **`11XX1XXX`（CVM 需要）**（【P p90】）。b8b7=11 是"EMV and Magstripe"的**历史编码**，v4.2 不再有 Magstripe 但必须保持该值（p90 原文）。
- bit4 是**每笔动态**位：进场清 0，`9F02 >= CVM limit` 时置 1（p33 §4.3.1）。

### 3.2 Terminal Type–Modified（= 9F35 OR 9F6D）【P p90 Table 21-8；P p33-34】

- 只在 **PDOL 请求 9F35 且不请求 9F6E** 时使用；值仅本笔有效，**授权/请款报文必须用未修改的 9F35**（p34 原文，两处强调）。
- 例：`9F35='22'`，`9F6D='C8'` → `'EA'`（p34）。
- 9F35 低 3 位编码（p89 Table 21-6）：`001` 出纳/在线唯一、`010` 出纳/离线+在线、`011` 出纳/离线唯一、`100` 无人/在线唯一、`101` 无人/离线+在线、`110` 无人/离线唯一；mPOS-C/CSP 固定 `XX10X001`（Merchant, Attended-Online only）。**离线唯一 = 低 3 位 `011` 或 `110`**（p33 §4.3.1 用 `x3`/`x6` 表述）。

### 3.3 9F6E Enhanced Contactless Reader Capabilities（4 字节，终端强制配置）【P p90-91 Table 21-9】

`[表重建]`：`X` = 收单方可配置；数字 = 强制值。

**Byte 1（leftmost）**

| bit | 值 | 含义 |
|---|---|---|
| b8 | X | 1 = 支持 AEIPS contact mode |
| b7 | 0 | 0 = 不支持 Expresspay Magstripe Mode |
| b6 | 0 | 0 = 不支持 Expresspay EMV full online mode（仅为兼容保留，**设置了也必须忽略**，p91 原文） |
| b5 | 1 | 1 = 支持 Expresspay EMV partial online mode |
| b4 | 1 | 1 = 支持 Expresspay Mobile |
| b3 | X | 1 = 拒绝后允许 Try Another Interface（**若 b8=1 则本位必须为 1**，p91 Note 1） |
| b2-b1 | 00 | RFU |

**Byte 2（CVM 能力）**

| bit | 值 | 含义 |
|---|---|---|
| b8 | X | 1 = 支持 Mobile CVM |
| b7 | X | 1 = 支持 Online PIN |
| b6 | X | 1 = 支持 Signature |
| b5 | X | 1 = 支持 Plaintext Offline PIN（v4.2 下**不得**使用，p23/p49） |
| b4-b1 | 0000 | RFU |

**Byte 3（每笔动态）**

| bit | 值 | 含义 | 置位时机 |
|---|---|---|---|
| b8 | X | 1 = 终端仅离线 | 离线唯一终端或当前无法联网（p33） |
| b7 | X | 1 = CVM Required | `9F02 >= Reader CVM Required Limit`（p33）；进场先清 0 |
| b6-b1 | 000000 | RFU | — |

**Byte 4**

| bit | 值 | 含义 |
|---|---|---|
| b8 | X | 1 = 终端豁免 No CVM 检查（Terminal Exempt from No CVM Checks） |
| b7 | X | 1 = Delayed Authorization 终端 |
| b6 | X | 1 = Transit 终端 |
| b5-b4 | 00 | RFU |
| b3-b1 | XXX | Expresspay Kernel Version：`001`=3.0、`010`=3.1、`011`=**4.x**、`1XX`=其它 RFU |

- mPOS-C/CSP 固定值（p91）：B1=`00011000`、B2=`1XX00000`、B3=`0X000000`、B4=`00000011`。
- **优先级**：9F6E 的 byte2 CVM 能力 + byte3 b7（CVM Required）**优先于** 9F33 中的同类设置（【P p100 §21.4.3】原文 "shall take priority over similar settings in Terminal Capabilities '9F33'"）。

### 3.4 9F33 Terminal Capabilities（3 字节）

- Expresspay v4.2 只把 9F33 当作"终端可配置项"引用，**本身不定义位**：§21.4.3 Table 21-26 列出 9F33/9F35/9F6D/9F6E 必须可独立配置且不得互相矛盾（【P p100】）；§21.6 DET 中 9F33 的 Presence=**M**、格式 b、长度 **3**、Values 列写 **"Defined in [EMV 4.3iv] Annex A2"**（【P p111 DET 中 9F33 条目；DET 起始 p102】）。
- 实现含义：**9F33 的位含义必须去 EMV 4.3 Book 4 Annex A2 查**（本机有 `EMV_v4.3_Book_4_Other_Interfaces...pdf` / `EMV_v4.4_Book_4`）。本规范只约束"可配置 + 与 9F6E 不冲突"。
- ✅ 可测点：同一笔交易里 9F33 的 CVM 位与 9F6E byte2 **冲突时，以 9F6E 为准**（p100）。

### 3.5 9F40 Additional Terminal Capabilities

**`[PDF 无]`** 全文检索 `9F40` 计数 = 0。DET 里没有该 tag，也没有任何条款引用。
→ 实现/测试结论：**Amex 链路不需要 9F40**；如果现有代码从其他内核（qPBOC/PayWave）继承了 9F40 配置，在 Amex 分支上应视为"不参与判定"，不要用它影响 TVR/Outcome。

### 3.6 TTQ / 9F66 —— 需要 Lead 决策的一条

**事实（全部来自原文，不是推断）**

1. 本 PDF：`9F66` = 0 次，`TTQ` = 0 次，`Terminal Transaction Qualifiers` = 0 次。DET（p102-112）无该 tag。
2. EMVCo **Book B v2.12 §3.1 p13 脚注 2** 原文：*"This is a payment system specific tag used by **Kernels 3, 6 and 7**."*（9F66）
3. **Book B v2.12 p23**：目录项 Kernel Identifier 缺失时按 ADF Name 推默认内核 —— *"For an **American Express** ADF Name, it will use **Kernel 4**"*；**Table 3-6 p33**：American Express AID 的默认 Requested Kernel ID = `00000100b`（= Kernel 4）。
4. 因此 **Amex = Kernel 4**，而 9F66 是 Kernel 3/6/7 的 tag → **不存在"Amex 版本的 TTQ 9F66 位定义"**。

**结论与建议**

- 任务书要求的"TTQ(9F66) Amex 逐位"在现有两份规范里**没有可引用来源**；Book C-4（Amex Kernel 4 规范）**不在本机 emv_doc 目录中**（已递归检索 `Kernel|C-4|Expresspay|Amex|Contactless` 确认无 C-4）。
- 如果需求文档/代码里出现"Amex TTQ"，最可能是把 **通用 EMVCo TTQ（Book A Table 5-4）** 或 **其他内核的 TTQ 配置** 误当成 Amex 要求。**建议 Lead 让需求方给出 Book C-4 原文页**；在此之前不要把 TTQ 引入 Amex 判定路径。
- 若 EP 层确实要服务"配置了 TTQ 的 Combination"（例如同一读卡器上并存 Kernel 3），可用的**通用 TTQ 位表**（EMVCo Book A v2.10 Table 5-4，打印页 26 / PDF p36）如下，**仅供 EP 层 Copy of TTQ 使用，不是 Amex 定义**：

| 字节 | bit | 含义（Book A Table 5-4 原文） |
|---|---|---|
| 1 | b8 | 1b=支持 Mag-stripe mode |
| 1 | b7 | RFU(0) |
| 1 | b6 | 1b=支持 EMV mode |
| 1 | b5 | 1b=支持 EMV contact chip |
| 1 | b4 | 1b=Offline-only reader / 0b=Online capable reader |
| 1 | b3 | 1b=支持 Online PIN |
| 1 | b2 | 1b=支持 Signature |
| 1 | b1 | 1b=支持 Online Authorization 的 ODA |
| 2 | b8 | 1b=Online cryptogram required |
| 2 | b7 | 1b=CVM required |
| 2 | b6 | 1b=(Contact Chip) Offline PIN supported |
| 2 | b5-b1 | RFU |
| 3 | b8 | 1b=支持 Issuer Update Processing |
| 3 | b7 | 1b=支持 Consumer Device CVM |
| 3 | b6-b1 | RFU |
| 4 | b8-b1 | RFU |

- **EP 层对 Copy of TTQ 的强制改写**（Book B v2.12，逐条可测）：
  - 3.1.1.2（p14）：把配置 TTQ 拷入 Copy of TTQ 后，**byte2 b8、b7 清 0**（online cryptogram 不需要 / CVM 不需要）。
  - 3.1.1.9（p16）：Reader Contactless Floor Limit Exceeded → **byte2 b8=1**。
  - 3.1.1.10（p16）：Status Check Requested（金额 = 1 个货币单位且支持状态检查）→ **byte2 b8=1**。
  - 3.1.1.11（p16）：Zero Amount → 若 TTQ b1b4=0（online capable）则 byte2 b8=1；若 b1b4=1（offline-only）则置 `Contactless Application Not Allowed`。
  - 3.1.1.12（p16）：Reader CVM Required Limit Exceeded → **byte2 b7=1**。

### 3.7 9F6D/9F6E ↔ TTQ 语义对照（`[推断]`，供熟悉 TTQ 的同事迁移阅读，不作为规范依据）

| 语义（TTQ 视角） | Expresspay v4.2 对应位 |
|---|---|
| TTQ b1b4 offline-only reader | 9F6E b3b8 = 1 |
| TTQ b1b3 支持 Online PIN | 9F6E b2b7 = 1 |
| TTQ b1b2 支持 Signature | 9F6E b2b6 = 1 |
| TTQ b1b6 支持 EMV mode | 9F6D b8b7=11 + 9F6E b1b5=1（partial online） |
| TTQ b1b5 支持 contact chip（=AEIPS） | 9F6E b1b8 = 1 |
| TTQ b1b1 ODA for online auth | 无直接位；终端配置 ODA 开关（p39-40） |
| TTQ b2b8 online cryptogram required | 无终端静态位；由 floor limit / ARC / 卡决定 |
| TTQ b2b7 CVM required | 9F6E b3b7（每笔动态） |
| TTQ b3b7 Consumer Device CVM | 9F6E b2b8 Mobile CVM |

---

## 4. 金额与限额（最容易出 P0 的一片）

### 4.1 三个限额的定义与配置层级【P p22 §2.6；P p26 §3.2.1；P p100 §21.4.2】

| 限额 | 规范定义（原文要点） | 配置层级 | 长度/格式 |
|---|---|---|---|
| **Reader Contactless Transaction Limit** | "the amount limit allowed for contactless transactions" | **AID 级**（每个 AID 一套 "AID Reader Limits"） | n 12 / 6 字节 BCD（Book B Table A-1 p47；Expresspay DET 未列，属 Acquirer Risk Data） |
| **Reader Contactless Floor Limit** | "the amount limit at which online authorization is requested" | AID 级 | 同上 |
| **Reader CVM Required Limit** | "the amount limit at which cardholder verification is requested" | AID 级 | 同上 |
| （备选）Terminal Floor Limit `9F1B` | 仅当 Reader Contactless Floor Limit 未配置时使用 | 终端/应用级 EMV tag | 6 字节 |

> Amex 侧限额由 American Express 定义、可由市场进一步细化（【P p26 §3.2.1】）；**Expresspay v4.2 不再有 DRL**（p43 §7.2.2）。

### 4.2 判定边界表（严格按原文符号，**别统一成 >= 或 >**）

| 判定 | 比较符 | 结果动作 | 原文位置 |
|---|---|---|---|
| 金额 vs Transaction Limit | **`>=`**（"greater than or equal to"） | 该 AID **不得**发起 Expresspay 交易（EP 层：置 Contactless Application Not Allowed） | 【P p27 §3.2.2】；Book B 3.1.1.5 p15 |
| 金额 vs Floor Limit | **`>`**（"greater than"/"exceeds"） | **TVR b4b8 = 1**（Transaction Exceeds Floor Limit）；EP 层：Reader Contactless Floor Limit Exceeded | 【P p59 §9.2】；Book B 3.1.1.6 p15 |
| Reader Floor Limit **= 0** | 等于 0 即触发 | **TVR b4b8 = 1**（无条件） | 【P p59 §9.2】 |
| 金额 vs CVM Limit | **`>=`**（"at or above"/"equal to or exceeds"） | 需要 CVM：9F6D b1b4=1、9F6E b3b7=1 → §8.2.2 | 【P p33 §4.3.1】、【P p46 §8.2.1】；Book B 3.1.1.8 p16 |
| 金额 = 1 个货币单位 | 等于 1 | EP 层若支持 Status Check 且 flag=1 → 置 Status Check Requested | Book B 3.1.1.3 p14 |

### 4.3 缺失/边界值的默认行为（任务书点名要的）

| 情形 | 规范规定（原文） | 代码动作 |
|---|---|---|
| **Reader CVM Required Limit 缺失** | "**must be assumed to be the maximum value**"（p33 §4.3.1、p46 §8.2.1 各一次） | 初始化为"最大可表示金额"（n12 → `999999999999`），则 `9F02 >= limit` 永不成立 → **不需要 CVM** |
| **Amount, Authorized 未知** | "**must be assumed to be zero**"（p46 §8.2.1） | 进 CVM 判定前 `amount = (known ? amount : 0)` |
| **Amount = 0** | Expresspay 本体不拦；EP 层有专门规则（见 EP 笔记 3.1.1.4/3.1.1.11）：Zero Amount 且 offline-only reader → 不允许；online capable → 强制 online cryptogram | EP 层处理，内核不要重复判 |
| **Reader Contactless Floor Limit 缺失** | 回退 Terminal Floor Limit `9F1B`（p59） | `if(!reader_floor.present && terminal_floor_9F1B.present) check(9F1B)` |
| **Reader Floor 与 9F1B 都缺失** | "the above check is **not performed**"（p59） | 不置 TVR b4b8；**不要**默认成 0 或拒绝 |
| **Reader Contactless Transaction Limit 缺失** | 「**if defined** and amount >= limit」→ **未定义即不拦**（p27 §3.2.2） | 限额不存在 → 放行（注意与 CVM limit 的"缺失=最大"是同一个效果，但语义来源不同） |
| **Terminal Type 触发离线唯一** | `9F35` 低 3 位 = `011` 或 `110`（p33 用 `x3`/`x6`） | pre-PDOL 置 9F6E b3b8=1 |

### 4.4 可直接落地的伪代码（与现有内核 API 无关，仅表达判定顺序）

```c
// CVM 需求判定（§4.3.1 / §8.2.1）
uint64 limit = reader_cvm_limit.present ? reader_cvm_limit.value : 999999999999ull; // n12 最大值
uint64 amt   = amount_known ? amount_authorized : 0;
if (amt >= limit) {                          // 注意 >=
    ef6d.byte1 |= 0x08;                      // 9F6D b1b4 CVM Required
    ef6e.byte3 |= 0x40;                      // 9F6E b3b7 CVM Required（b8=0x80 是 offline-only！）
    cvm_required = 1;
}

// Floor limit 判定（§9.2）—— 三种分支，互斥
if (reader_floor.present) {
    if (reader_floor.value == 0 || amount_authorized > reader_floor.value)  // 注意 floor==0 强制
        tvr.byte4 |= 0x80;
} else if (terminal_floor_9F1B.present) {
    if (amount_authorized > terminal_floor_9F1B.value) tvr.byte4 |= 0x80;
}   // 两者皆无：什么也不做

// Transaction limit 判定（§3.2.2）—— 在 Pre-processing 阶段，早于任何卡交互
if (reader_txn_limit.present && amount_authorized >= reader_txn_limit.value)
    return NOT_ALLOWED;                      // 不得发起 Expresspay 交易
```

---

## 5. ODA / CVM / TVR

### 5.1 ODA 适用条件（【P p38-41 §6】）

| 项 | 规定 |
|---|---|
| 终端必须支持的方法 | **SDA + CDA**（"all Terminals must support SDA and CDA"，p39 §6.2） |
| 启用 | 可配置（"must be configurable for deployment"） |
| 执行条件 | 启用 **且** AIP 指示支持 **且** 本笔不是"将离线拒绝" |
| 优先级 | **CDA > SDA**；有 CDA 必须做 CDA；无 CDA 有 SDA 才做 SDA（p40） |
| 不执行时 | **TVR b1b8=1**（Offline data authentication was not performed，p40） |
| SDA | 需 CAPK 存在且有效；执行置 **TVR b1b2=1**；失败置 **b1b7=1**；缺 Table 21-19 强制数据 → 失败+按 EMV4.3iii §7.5 处理（p40） |
| CDA | 只在 **mode 2** 下做（EMV 4.3 Book2 Annex D4）；在 §10 GENERATE AC 请求 CDA、在 §11 校验；失败置 **TVR b1b3=1** 并按 EMV4.3iv §6.3.2 继续（p40-41、p67） |
| DDA / fDDA | **`[PDF 无]`**（DDA 位在 AIP 中固定为 0；全文无 fDDA） |
| CAPK | 至少可存 **6 组** Amex CA 公钥及关联数据（p40 §6.2.1） |
| 性能 | CDA 卡 profile：CAPK 1984 bit / Issuer PK 1976 bit / ICC PK 1152 bit；Terminal Tariff ≤ **100 ms**（p82 §18） |

### 5.2 CVM 走位（把 8.2.1→8.2.5 的流程图翻成判定树）

```
① 是否需要 CVM（§8.2.1, p46）
   amount>=CVM limit ?
     ├─ 否 → §8.2.5 CVM Limit Not Exceeded（按 Mobile/Card 分岔）
     └─ 是 → AIP b1b5(CVM 支持)?
              ├─ 否 → §8.2.4 CVM unable to complete（TVR b3b8=1）
              └─ 是 → §8.2.2 CVM Processing

② §8.2.2（p47）
   无 CVM List 或 List 无规则 → CVM Results(9F34) b1='3F' No CVM Performed；TVR b1b6=1；跳 §9
   否则：不支持的方法剔除 —— 不把 "No CVM required" 放进支持集合（因 9F6E b3b7=1）
        终端/收单侧不支持 Online PIN → 不把 Online PIN 放进支持集合
   → §8.2.3 CVM List Processing

③ §8.2.3（p48-49）：按 EMV4.3iii §10.5 逐条匹配，取最高优先级双方支持方法
   必须可用方法（除 mPOS-C）：Mobile CVM、Online PIN、Signature、No CVM Required
        mPOS-C：Mobile CVM、Signature、No CVM Required
   CVM List 必须一直保留到最终 Outcome（§17.2.6），因为 §17.2.5 可能还要用（p48 原文）

④ 具体方法
   §8.2.3.1 Online PIN（p49）
       → 立刻置 TVR b3b3=1（Online PIN Entered）
       → PIN 在"已提示可拔卡"之后收（§11.2 的 '17' 之后）
       → 显示 '09'（Please Enter Your PIN）
       → PIN 成功 → 走在线授权（Online PIN 交易必须在线）
   §8.2.3.2 Mobile CVM（p49-51）
       → 终端支持=9F6E b2b8；卡侧 CV Rule b1 bits6-1='000001'
       → 9F71 未返回 / byte3='03'(Blocked) → 视为失败，CVM Results 按 EMV4.3iv §6.3.4.5
       → byte1='3F' → 失败；byte1='01' 且 byte3='02' → CVM Results b3='02' Successful → §9
       → 失败且本交易**尚未重启过** → Outcome = **Try Again**（§17.2.3）；已重启过 → 继续 §8.2.3
   签名 → 按 EMV 规则做，完成在线/离线批准后由 §17 收签名（'1A' Approved Please Sign）

⑤ §8.2.4 CVM 无法完成（p52）
   TVR b3b8=1；CVM Results 按 EMV4.3iv §6.3.4.5
   若 [终端支持 AEIPS(9F6E b1b8=1)] 且 [AIP b2b7=0 且 b2b6=0] 且 [9F70 b1b6=1 或 9F70 不存在]
        → Outcome = Try Another Interface（§17.2.2，消息 '1D'）
   否则 → 继续 §9

⑥ §8.2.5 CVM 限额未超（p53-56）
   Mobile：9F71 返回且 byte1='01' 且 byte3='01'(failed) 且未重启 → Try Again；
           否则按"卡支持 CVM?/无 List?/含 No CVM Required 且条件满足?/否则做 §8.2.3"
   Card  ：卡不支持 CVM → CVM Results='No CVM Performed' → §9
           无 List → TVR b1b6=1 + 'No CVM Performed' → §9
           含 'No CVM Required' 且条件码适用 → CVM Results='No CVM Required' → §9
           否则还有双方支持方法 → §8.2.3；没有 → §8.2.4
```

**CVM 方法码（Table 21-16, p94-95）**：`000000` Fail、`000001` **Mobile CVM**（Card 不支持）、`000010` Enciphered PIN online、`011110` Signature、`011111` No CVM required；`000011/000100/000101` 均为 Expresspay 不支持。条件码（byte10）：`00` Always、`01` unattended cash、`02` 非 unattended cash/manual cash/cashback、`03` 终端支持该 CVM、`04` manual cash、`05` cashback、`06/<X`、`07/>X`、`08/<Y`、`09/>Y`。

### 5.3 卡响应/状态字处理（【P p65-69 §11】）

| 卡返回 | 允许前提 | 终端处理 |
|---|---|---|
| SW `6984` | **仅 Expresspay Mobile** | '17' 300ms → 若未重启过：Outcome=Try Again（§17.2.3）；已重启过 → **终止**（视为错误，p68） |
| SW `6984`（非 Mobile） | — | 按**非法状态字**处理（p66） |
| TC | 请求过 TC | 离线批准；CDA 已验证成功（p68 §11.2.2） |
| TC | **未**请求 TC | **拒绝交易**（p68） |
| AAC | 任意 | §17.2.1：可 Try Another Interface，否则 '07' 拒绝（p68） |
| ARQC | 请求过 TC 或 ARQC | Delayed Auth → §17；在线能力 → §12；否则 §17.2.1（p69） |
| ARQC | 未请求 TC/ARQC | **拒绝交易**（p68） |
| Format 1/2 格式错、缺 CDA 数据、CID 非法 | — | 允许换接口则 Try Another Interface，否则 §17.2.4（p67） |

### 5.4 TVR / TSI（Table 21-24, p98）

| 字节 | 位 | 含义 | Expresspay 会置位? |
|---|---|---|---|
| 1 | b8 | ODA 未执行 | 是（§6.2） |
| 1 | b7 | SDA 失败 | 是（§6.2.2） |
| 1 | b6 | 卡数据缺失（ICC Data Missing） | 是（§8.2.2/§8.2.5） |
| 1 | b5 | 卡在终端异常文件 | 可选（异常文件存在时） |
| 1 | b4 | DDA 失败 | 否（无 DDA） |
| 1 | b3 | CDA 失败 | 是 |
| 1 | b2 | SDA Selected | 是 |
| 2 | b8 | 卡/终端应用版本不同 | 是（§7.2.3） |
| 2 | b7 | 应用已过期 | 是 |
| 2 | b6 | 应用未生效 | 是 |
| 2 | b5 | 卡产品不允许该服务（AUC 检查失败） | 是（含 Delayed Auth 补充检查） |
| 2 | b4 | New Card | 规范未要求（[PDF 无] 算法） |
| 3 | b8 | CVM 未成功 | 是（§8.2.4） |
| 3 | b7 | 未识别 CVM | 按 EMV |
| 3 | b6 | Offline PIN Try/Mobile CVM 次数超限 | 是（9F71 byte3='03' → §4.3.4） |
| 3 | b5 | 需 PIN 但 PIN pad 缺失/故障 | 按 EMV |
| 3 | b4 | 需 PIN 但未输入 | 按 EMV |
| 3 | b3 | 已输入 Online PIN | 是（§8.2.3.1） |
| 4 | b8 | 超 floor limit | 是（§9.2） |
| 4 | b7/b6 | Lower/Upper consecutive offline limit | 规范未要求（velocity 被禁，p59） |
| 4 | b5 | 随机选中上送 | **禁止置位**（p59 禁随机选择） |
| 4 | b4 | 商户强制上送 | 可配（§9.1 提到 merchant forced） |
| 5 | b8 | 使用 Default TDOL | 否（无 TDOL） |
| 5 | b7 | Issuer Authentication 失败 | 否（§13 不支持） |
| 5 | b6/b5 | 脚本处理失败（final GENAC 前/后） | 否（§16 不支持） |

**TSI：`[PDF 无]`**（全文 0 次）。原因：Expresspay 只有一次 GENERATE AC、不做发卡行认证与脚本，TSI 无意义。→ 实现上**不要**为 Amex 交易构造/校验 TSI；若上游报文格式要求，用常量（如 `0000`/`E800` 之类由收单侧规定）需向 Amex 确认 `[推断]`。

---

## 6. 交易结果、异常、脚本、Data Exchange

### 6.1 Outcome 与最终结果（【P p77-81 §17】）

| Outcome 来源 | 触发 | 终端动作 / 消息 |
|---|---|---|
| 离线批准（TC） | §11.2.2 | '17' 300ms → §17.2.6.1：无签名 `'03'` Approved；需签名 `'1A'` Approved Please Sign |
| 在线批准（ARC 00/08/10/11） | §12.2.2 | §17.2.6.1 同上 |
| 拒绝 | AAC / ARC 其它值 / 无法联网且 Default 命中 | `'07'`（Not Authorized） |
| Try Another Interface | AAC 且双方支持换接口；或 §11 格式错；或 §17.2.1 条件满足；或 ARC=`12` | `'1D'`（Please Insert Card）→ 转 AEIPS 接触交易（p78-79） |
| Try Again | Mobile CVM 失败（未重启过）/ SW 6984（未重启过） | 消息 `'20'`（See Your Phone for Instructions，1s）→ `'21'`（Present Card Again）→ 从 §3.3 重启；**保留金额/币种/预处理结果，只生成新 UN**；"不检查是否同一张卡"（p79）；**只允许一次** |
| Request Online PIN | ARC=`13` 且双方支持 Online PIN | `'09'` 收 PIN → 带着同一份授权数据再上送（§17.2.5） |
| Request Another Payment | 提前终止（候选空、SW 异常、缺强制数据等） | `'0F'`（Processing Error）300ms，然后 `'1C'`：非接接口唯一终端的文案为 "Try Another Card"，其它终端为 "Insert, Swipe or Try Another Card"（p80） |

- **Delayed Authorization**：ARQC 后直接 §17，不等待授权响应；结果展示后完整交易结束（p69、p78）。
- 交易完成前必须**安全保存**授权请求数据，最终结果达成后**清除**（p71 脚注：具体保存/清除受本地法规约束）。

### 6.2 异常文件 / 撤销

| 项 | 规范状态 |
|---|---|
| Terminal Exception File | **可选**配置（p58 §9.1 "optional exception file"；p100 §21.4.3）；命中 → **TVR b1b5=1**（Table 21-24 p98）。**无命中后的处置算法**（由 IAC/TAC 决定） |
| 撤销 / 冲正（reversal/void） | **`[PDF 无]`**。规范不涉及；属于收单/报文层，需查 Amex 收单规范 `[PDF 无]` |
| 重复交易 / ATC 检查 | **`[PDF 无]`**（v4.2 已删 Magstripe Mode 相关 ATC/AC 检查；`[KB4.1]` §8.3 的 Magstripe 检查不适用） |

### 6.3 发卡行脚本 71/72

- §16 原文：**"Issuer Script Processing — Not supported in this specification."**（p76）
- 连带：§13 Issuer Authentication **不支持**（p73）；§14 **2nd Terminal Action Analysis 不支持**（p74）；§15 **2nd Card Action Analysis 不支持**（p75）。
- → 实现上：**不要实现 71/72 处理、91 认证、第二次 GENERATE AC**；若 EP 层收到 `71/72`，按 Book A 8.1.1.22 的"是否重启内核"判定，但因为 Amex 内核不支持，重启也应无动作 `[推断]`（需 Lead 按 EP 侧实现确认）。

### 6.4 Data Exchange

- Expresspay v4.2 **`[PDF 无]`**（0 次）。
- Data Exchange 是 **EP/Book C 内核层概念**，相关强制条款在 **EMVCo Book A v2.10 §8.1.1.12-8.1.1.14**（打印页 60-61 / PDF p70-71）：只有内核发起 Data Exchange 时终端才可发送；内核期望响应时终端必须发信息或空响应；必须发给正确的内核。
- **Amex 链路结论**：Amex 内核（Kernel 4/Expresspay）不发起 Data Exchange `[推断，基于本规范全文无该机制]`。

---

## 7. 测试点（规范无证码 → 由条款反推，全部 `[推断]`）

> 本规范不含任何证码/测试用例。Amex L2 证码在本机以下文档中（**未逐页提取，仅确认存在**）：
> - `E:\Work\SVN Project\emv_doc\卡组织规范\Expresspay\Expresspay L2 terminal approval process - Admin Document V1.pdf`
> - `E:\Work\SVN Project\emv_doc\卡组织规范\Expresspay\过检名单\Amex Enabled Acquiring Solutions - Contactless Terminals - December 2021 .pdf`
> - `E:\Work\SVN Project\emv_doc\卡组织规范\Emvco\EMVCo_TTA_Contactless_Book_AB_Test_plan_v210a_211224_CATA.pdf`

| 场景 | 前置 | 期望（可断言） | 依据页 |
|---|---|---|---|
| 交易限额边界 | 定义 limit=1000（最小单位） | 金额=999 放行；**金额=1000 拒绝发起** | p27 |
| CVM 限额缺失 | 不配置 Reader CVM Required Limit，大额 | 不需要 CVM（9F6D b1b4 保持 0） | p33/p46 |
| CVM 限额边界 | limit=500 | 499 → 无 CVM；**500 → 9F6D b1b4=1 且 9F6E b3b7=1** | p33/p46 |
| Floor=0 | Reader Floor Limit=0，金额任意 | 必然 **TVR b4b8=1** | p59 |
| Floor 缺失 | 只配 9F1B=100 | 金额=100 → 不置位（`>`）；101 → 置位 | p59 |
| Floor 全缺 | 两者都不配 | **不置位**，且交易不因该检查被拒 | p59 |
| 缺 CVM List | 9F02≥CVM limit，卡不支持 CVM | CVM Results=`3F`，**TVR b1b6=1**，继续 §9 | p47/p52 |
| 卡越权返回 TC | 终端请求 ARQC | **拒绝** | p68 |
| 卡越权返回 ARQC | 终端请求 AAC | **拒绝** | p68 |
| 非 Mobile 返回 6984 | 塑料卡 | 按非法状态字 → 终止/换接口 | p66 |
| Mobile 6984 一次 | Mobile 卡 | 首次 → Try Again（'20'+'21'），第二次再 6984 → **终止** | p67-68 |
| ARC=12 | 在线响应 8A='12' | Try Another Interface（若支持）否则拒绝 | p99/p71 |
| ARC=13 | 8A='13' 且支持 Online PIN | 请求 PIN '09'，再次上送同一授权数据 | p80 |
| ARC=未知值 | 8A='99' | **必须拒绝**（不是批准） | p99 |
| CDA 失败 | 卡返回 CDA 数据但签名校验失败 | **TVR b1b3=1**，TC/ARQC 仍走各自后续 | p41/p67 |
| 未启用 ODA | 配置关闭 | **TVR b1b8=1** | p40 |
| PPSE 缺 BF0C | 卡返回残缺 FCI | 立即终止 → '0F'+'1C' | p30 |
| 提前拔卡 | 读卡中拔卡 | 金额保持、**UN 更新**、'21'、从 §3.3 重启 | p28 |
| Terminal Type-Modified | 9F35='22'、9F6D='C8' | GPO 中 9F35 字段 = **'EA'**；授权报文 9F35 = '22' | p34 |
| 9F6E 与 9F33 冲突 | 9F33 CVM 位=0 但 9F6E b2=1 | 以 **9F6E** 为准 | p100 |
| 离线 PIN | 任何 | **不发起 VERIFY 命令** | p23/p49 |

---

## 8. 相对 v4.1 的差异（Lead 手上 4.1 知识库需更新处）

| 主题 | v4.1 KB 写法 | v4.2 事实（原文） |
|---|---|---|
| Mag-stripe Mode | 大篇幅（加密图版本 '02'、伪 Track、ATC 校验等） | **已移除**；全文多处 `[Section removed]`；§2.2 明确不支持（p20）；§2.10.2 命令表中无 Magstripe 特有命令 |
| DRL（Dynamic Reader Limits） | 7.2 提及 | **已移除**（p43 §7.2.2 原文 + p10 版本历史） |
| AIP byte1 b7 "SDA supported (Expresspay 2.0 only)" | 有 | 保留但语义为历史兼容（Table 21-11 p92） |
| 9F71 Mobile CVM Results | 有 | 保留；**Card 交易必须忽略**（p35） |
| 9F67 Card Form Factor | 有（Table 21-18 p95-96） | 保留：`000001` 标准 ID-1、`000002` 其它卡、`000003` Mobile、`000004` Wearable、`000005` Other |
| 卡产品 `9F7D` / IIN `42`/`9F0C` / `5F55`/`5F56` | — | v4.2 新增在 PPSE/ADF FCI（p86-87） |

---

## 9. 附录

### 9.1 终端配置项清单（实现必须能配的）

| 层级 | 项 | 页 |
|---|---|---|
| RID `A000000025` | CAPK（≥6 组）；TAC-Denial/Online/Default | p99 |
| 应用 `A00000002501` | AID（partial AID selection **必须 ON**）；Application Version Number=`0001`（预留 3 个槽位）；Acquirer Risk Data：3 个 Reader Limits | p99-100 |
| 终端 | 9F33、9F35、9F6D、9F6E（独立可配、不得互相矛盾）；可选 Terminal Exception File | p100 |
| 其它 | Terminal Country Code `9F1A`、Transaction Currency `5F2A`、Merchant/Attended 等 `9F35` 编码 | p89/p99 |

### 9.2 消息 ID（Expresspay 用到的，含 Hold Time）

| ID | 文本 | Hold | 出处 |
|---|---|---|---|
| '03' | Approved | — | p80 |
| '07' | Not Authorized | — | p81 |
| '09' | Please Enter Your PIN | — | p49/p80 |
| '0F' | Processing Error | 300ms | p80 |
| '10' | Please Remove Card | — | p28 |
| '14' | Welcome（Idle，Pre-processing 前） | — | p26 |
| '15' | Present Card | — | p27 |
| '16' | Processing（卡已选） | — | p33 |
| '17' | Card read OK. Please remove card | **300ms** | p68/p69 |
| '19' | Please Present One Card Only | — | p28 |
| '1A' | Approved Please Sign | — | p80 |
| '1B' | Authorizing, Please Wait | — | p71 |
| '1C' | Insert/Swipe/Try Another Card | — | p80 |
| '1D' | Please Insert Card | — | p79 |
| '20' | See Your Phone for Instructions | **1s** | p79 |
| '21' | Present Card Again | — | p28/p79 |

### 9.3 未覆盖 / 需外部来源

| 内容 | 原因 |
|---|---|
| Book C-4（Amex Kernel 4）原文 | 本机无该 PDF；Amex 内核 TTQ/PDOL 细节、Kernel 4 专有要求无法从 v4.2 得到 |
| Amex L2 证码逐条 | 本规范不含；需提取 `Expresspay L2 terminal approval process` 等文档 |
| EMV 4.3 Book 3/4 条款原文 | 本文件只引用节号（10.5/10.6/10.7/6.3.2 等），本机有对应 PDF 可直接查 |
| TAC 具体值 | 规范写 "TAC values are configurable and defined elsewhere"（p99），必须从 Amex 配置文档取 |
