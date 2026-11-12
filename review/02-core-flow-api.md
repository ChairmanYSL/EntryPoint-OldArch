# 核心流程/API 状态机/outcome 审查

> 模块 B（审查员 reviewer-b）：`src/emv.c`、`src/emvapi.c`、`src/emvoutcome.c`，参考头文件 `inc/emvapi.h`、`src/inc/emv.h`、`emvi.h`、`emv_type.h`、`emvoutcome.h`、`emvspi.h`。
> 全程只读，未修改任何工程文件；报告为唯一产物。

## 概述

`emv.c` 是本内核的上下文/初始化/状态机入口与工具函数层（`EMV_Init`、`EMV_PreProcess`、`EMV_Process`、`EMV_Result*`、TLV 打包与 BCD/hex 工具），`emvapi.c` 是给 L3 应用的对外 API 薄封装，`emvoutcome.c` 负责生成 EMVCo Entry Point 的 Outcome / UIRequest。三者构成 L3 应用看到的全部接口面。

总体结论：**这 3 个文件当前都处于不可交付状态**。`emv.c` 已不是"有缺陷但能跑"的代码，而是**无法通过编译**（缺字段、缺宏、语法错误）；`emvapi.c` 因依赖的 `emvporting.h` 引用了全工程都不存在的类型 `EMV_TERMPARAM` 而编译失败，并且调用了 6 个工程内无定义的符号；`emvoutcome.c` 编译通过，但 Outcome 的"PresentFlag 置位"与实际写入 TLV 表的数据不一致，且引用了未声明的 `JNISend*`。

静态检查还发现 3 处运行时必崩/内存破坏点：`EMV_PreProcess` 把 `uint8_t` 成员当指针解引用地址 0x1、`EMV_TLV_PacketByTAG` 先写后校验导致越界写、`EMV_GetData` 先解引用后判 NULL。此外 Entry Point 预处理对 TTQ 使用 `&=` 清位而非 `|=` 置位，会破坏 TTQ Byte2 的联机/CVM 指示。

问题统计：**28 条**（P0×5、P1×5、P2×13、P3×5）；另有跨模块契约问题单列在「跨模块契约核对」一节（多数落在 reviewer-a/c/d/e 的文件里，供 Lead 去重/分派，不重复计入本文总数）。

## 问题清单（P0→P3）

### [P0-1] `src/emv.c` 无法编译（多类硬错误），内核库无法构建

- 位置：`src/emv.c:869,870,874,905`；`src/emv.c:1044`；`src/emv.c:1155`；`src/emv.c:1236,1253`；`src/emv.c:1606`
- 证据：

```c
/* emv.c:868-874  —— EMV_CONTEXT 中根本没有 amount_empty / oth_amount_empty 字段 */
eTRACE("ep-info: g_emv_cnxt->oth_amount_empty = %s", g_emv_cnxt->oth_amount_empty?"TRUE":"FALSE");
eTRACE("ep-info: g_emv_cnxt->amount_empty = %s", g_emv_cnxt->amount_empty?"TRUE":"FALSE");
...
if(!g_emv_cnxt->oth_amount_empty)
...
/* emv.c:1044 —— EMV_CFG_MAX_CANDIDATES 定义在 emvdbg.h，但 emv.c 只 include 了 emvi.h（emvi.h 不包含 emvdbg.h），emvdbg.h 也未配置 PreInclude */
candidates_max_size = sizeof(EMV_CANDIDATE) * EMV_CFG_MAX_CANDIDATES;
/* emv.c:1155 —— 语法错误 */
int i,j,;
/* emv.c:1236、1253 —— 多余的右括号 */
if(preprocess.ttq[0] & 0x08))
/* emv.c:1606 —— EMV_CONTEXT 中没有 kernel_checksum 字段 */
return (uint8_t*)&g_emv_cnxt->kernel_checksum;
```

- 影响：`emv.c` 无法生成目标码，内核静态库/固件无法构建。旁证：`build/iar/Debug/Obj/` 下**恰好缺少** `emv.o、emvapi.o、emvspi.o、emvporting.o、emv_application_selection.o` 这 5 个 `.o`（其余 8 个 `.o` 齐全，且这 5 个只留下了预处理产物 `.pbi`），与"这 5 个 TU 编译失败"一致。也就是说该工程最近一次构建是失败的，交付物并非来自这份源码。
- 建议：`emvi.h` 的 `EMV_CONTEXT` 与 `emv.c` 需要对齐（补 `amount_empty/oth_amount_empty/kernel_checksum` 或改用现有字段）；`emv.c` 补 `#include "emvdbg.h"`（`EMV_CFG_MAX_CANDIDATES` 与 eTRACE/TRACE 宏都依赖它）；修正 1155/1236/1253 的语法后重新用 IAR 编译。

### [P0-2] `src/emvapi.c` 编译/链接阻断：`EMV_TERMPARAM` 全工程未定义 + 调用 6 个无定义符号

- 位置：`src/emvapi.c:34` → `src/inc/emvporting.h:165,195`；`src/emvapi.c:228,232,236,241,266,271`；`inc/emvapi.h:335,336`
- 证据：

```c
/* emvporting.h:165/195 引用了全工程任何头文件都没有定义的 EMV_TERMPARAM */
int32_t emvporting_get_termparam(EMV_TERMPARAM* tpar);
int32_t emvparanew_get_termparam(EMV_TERMPARAM* par);
/* emvapi.c:34 又 include 了 emvporting.h => emvapi.c 编译失败 */

/* emvapi.c:228-241：EMV_SetCurrentCode / EMV_GetCurrentCode / EMV_SetCurrentCodeE / EMV_GetCurrentCodeE
   在 src/、inc/、include/ 全部 .c/.h 中都没有定义（grep 全工程仅有此处引用） */
return EMV_SetCurrentCode(current);
return EMV_GetCurrentCode(current);
return EMV_SetCurrentCodeE(current);
return EMV_GetCurrentCodeE(current);

/* emvapi.c:266/271：EMV_SetTransAmountEmpty / EMV_SetOtherAmountEmpty 无定义 */
return EMV_SetTransAmountEmpty(flag);
return EMV_SetOtherAmountEmpty(flag);
```

- 影响：`emvapi.c` 编译失败；即使绕过编译，6 个符号在链接期也是 undefined reference。`inc/emvapi.h:335-336` 向 L3 声明了 `emv_set_trans_amount_empty/emv_set_other_amount_empty`，但没有任何实现、`EMV_CONTEXT` 也没有对应字段（见 P0-1），属于"已发布头文件承诺了不存在的 API"。
- 建议：定义 `EMV_TERMPARAM`（或删除 `get_termparam` 相关原型）；为 Empty 系列与 CurrentCode 系列补齐实现（含 `EMV_CONTEXT` 字段），否则从 `inc/emvapi.h` 删除，避免 L3 链接失败。

### [P0-3] `EMV_PreProcess` 把 `uint8_t` 成员 `cvm_limit_exist` 当指针传给 `emv_bcd2dec`，配置了 CVM 限额时必崩

- 位置：`src/emv.c:1268-1275`（关键行 1270）；对照字段定义 `src/inc/emv_type.h:557-558`
- 证据：

```c
/* emv_type.h:557-558 */
uint8_t cvm_limit_exist;         // 存在标记，0/1
uint8_t cvm_limit[6];            // [DF21] 终端执行CVM所需限额

/* emv.c:1268-1274 */
if(aid_param.cvm_limit_exist)
{
    /* 传的是 cvm_limit_exist（值 1），不是 cvm_limit 数组；emv_bcd2dec 的第一个参数是 const uint8_t* */
    if (g_emv_cnxt->trans_amount >= emv_bcd2dec(aid_param.cvm_limit_exist, sizeof(aid_param.cvm_limit_exist)))
    {
        preprocess.reader_cvm_limit_exceed = 1;
```

- 影响：只要某条 AID 的 `cvm_limit_exist=1`，`emv_bcd2dec` 就会把 `0x00000001` 当数组指针读取 1 字节（`bcdlen=1`）→ 访问地址 0x1，**每次交易预处理必崩**。同时该调用在 C 语言层面就是"整数到指针"的约束违反，IAR 下多半直接报错（掩盖在 P0-1 的编译错误后面）。`cl_trans_limit/cl_floor_limit` 两处写法是对的，可对照证明这里确系笔误。
- 建议：改为 `emv_bcd2dec(aid_param.cvm_limit, sizeof(aid_param.cvm_limit))`。

### [P0-4] `EMV_TLV_PacketByTAG` 先写入后做容量校验，越界写

- 位置：`src/emv.c:1899-1945`（关键 1921-1933）
- 证据：

```c
taglen = tlv_set_tag(pTags[i], pOut+reallen);     /* 先把 tag 写进 pOut+reallen */
if (taglen < 0)
    return EMV_FAIL;
if (reallen + taglen > nOutMaxLen)                /* 写完之后才检查是否越界 */
    return EMV_FAIL;
reallen += taglen;

lenlen = tlv_set_len(ilen, pOut+reallen);         /* 同样先写 length 字节再检查 */
if (lenlen < 0)
    return EMV_FAIL;
if (reallen + lenlen > nOutMaxLen)
    return EMV_FAIL;
```

- 影响：这是对外 API（声明于 `src/inc/emv_utils.h:155`）。当调用方给的 `nOutMaxLen` 不足时，`tlv_set_tag/tlv_set_len` 最多会先越界写 1~4 个字节（`tlv_set_tag` 可写 1~4 字节、`tlv_set_len` 1~4 字节），属于外部可触发的缓冲区越界写；与同文件 `EMV_TLV_PacketByDOL` 把 `nOutMaxLen` 直接传给下层 `emvdol_packet_tlv` 的做法不一致。
- 建议：先做 `if (reallen + 4 > nOutMaxLen) return EMV_FAIL;` 之类的上界预判，再调用 `tlv_set_tag/tlv_set_len`；或改为先写入临时栈缓冲再按剩余空间拷贝。

### [P0-5] `EMV_GetData` 先解引用 `g_emv_cnxt` 再判 NULL，未初始化/退出后调用对外 API 必崩

- 位置：`src/emv.c:1576-1581`；调用方 `src/emvapi.c:114-131,148-158,592-614`
- 证据：

```c
uint8_t* EMV_GetData(uint32_t nTag, int32_t* pLen)
{
    if (g_emv_cnxt->status == EMV_STA_IDLE || g_emv_cnxt == NULL)   /* 先解引用，后判 NULL */
    {
        return NULL;
    }
```

- 影响：`g_emv_cnxt` 在 `EMV_Init` 入口被置 NULL（`emv.c:977`）、`EMV_Exit` 又置 NULL（`emv.c:1960-1965`）。因此以下**对外 API 在未 `emv_initialize` 之前或 `emv_exit` 之后调用都会空指针崩溃**，而不是像其它 API 一样返回 `EMV_UNINITIALIZED/EMV_FAIL`：
  - `emv_get_tag_data()` → `EMV_GetData`（`emvapi.c:119`）；
  - `emv_get_version()` → `EMV_GetData(TAG_KERNEL_VERSION)`（`emvapi.c:153`，头文件未声明任何前置初始化要求，L3 在上电显示版本号时很容易先调它）；
  - `emv_is_need_advice()/emv_is_need_signature()`（`emvapi.c:595,607`）。
- 建议：改成 `if (g_emv_cnxt == NULL || g_emv_cnxt->status == EMV_STA_IDLE) return NULL;`。

### [P1-1] Entry Point 预处理把 TTQ 的"置位"写成"清位"（`&=`），破坏 TTQ Byte2 的联机/CVM 指示

- 位置：`src/emv.c:1200,1279,1284,1291,1301`
- 证据：

```c
/* 3.1.1.3 / 3.1.1.9 / 3.1.1.10 / 3.1.1.11：意图是"置 TTQ Byte2 的对应位为 1"（Online cryptogram required，TTQ b8 = 0x80） */
preprocess.ttq[1] &= 0x80;
/* 3.1.1.12：意图是"置 CVM required（TTQ Byte2 的 0x40）" */
preprocess.ttq[1] &= 0x40;
```

- 影响：按 Entry Point 规范，这几处（共 5 处）都是"把某位置 1"，必须用 `|=`。写成 `&=` 后：例如原 TTQ Byte2 = 0x40（仅 CVM required），执行 `&= 0x80` 后变成 0x00；原值 0xC0 会被削成 0x80、CVM required 位丢失。多步叠加后 **TTQ Byte2 被逐次清空**，下游内核依赖 TTQ 决定 CVM 方式、是否联机、ODA 策略，会得到错误决策（联机 PIN/签名/ODA 被关掉或错误触发），属于规范违规且直接影响交易结果。同文件 `emv_ics.h:238-239` 的 `BN_TTQ_REQ_ONLINEAC/BIN_TTQ_REQ_CVM` 也印证这两个位的语义是"要求位"。
- 建议：这 5 处改成 `preprocess.ttq[1] |= 0x80;` / `|= 0x40;`；并补一条针对 TTQ 组合的单元测试（原 TTQ 各位保持、仅目标位置位）。

### [P1-2] 接触式路径不可用：`card_types_allowed` 只允许 CONTACTLESS；且接触路径下内核函数指针从未赋值

- 位置：`src/emv.c:1170-1171`、`src/emv.c:1326-1337`、`src/emv.c:1401-1408`；关联 `src/emv_selec_kernel.c:22-34`、`src/emv.c:998`
- 证据：

```c
/* emv.c:1170-1171（EMV_PreProcess，全工程唯一写 card_types_allowed 的地方） */
g_emv_cnxt->card_types_allowed = EMV_CARD_UNKNOWN;      /* 0 */
g_emv_cnxt->card_types_allowed |= EMV_CARD_CONTACTLESS; /* 只置 2，contact(1) 永远不允许 */

/* emv.c:1326-1336（EMV_SetCardType） */
if ((eType == EMV_CARD_CONTACT || eType == EMV_CARD_CONTACTLESS)
        && (g_emv_cnxt->card_types_allowed & eType) != 0) { ... return EMV_OK; }
else return EMV_CARDTYPEILL;

/* emv_selec_kernel.c:22-34：contact 分支直接 return EMV_OK，未给 emv_k_interface.emv_process 赋值 */
if(card_type == EMV_CARD_CONTACT) { /* emv_k_interface.emv_process = contact_process; 被注释掉 */ return EMV_OK; }
/* emv.c:1403/1407 */
g_emv_cnxt->status = emv_k_interface.emv_process();   /* contact 路径下这里是 NULL 调用 */
```

- 影响：任何合法调用顺序下，接触式卡 `emv_set_cardtype(1)` 都会因 `card_types_allowed` 没有 CONTACT 位而返回 `EMV_CARDTYPEILL`，接触式交易在 `EMV_Process` 起步处就被拒绝；即使把上面的位补上，接触式分支也没有给 `emv_k_interface.emv_process` 赋值，`EMV_Process` 下一次会**调用空函数指针**（`emv_k_interface` 为全局零初始化，`emv_selec_kernel.c:7`）。两条叠加意味着接触式支持实际不存在，与 `inc/emvapi.h:347`、`src/inc/emv.h:446` 声明的 `EMV_CARD_CONTACT` 能力不符。
- 建议：若产品只做非接，请 Lead 与需求方确认后在 `emvapi.h/emv.h` 明确删除接触式能力并让 `emv_set_cardtype(1)` 返回 `EMV_FAIL`；若要支持接触式，需 `EMV_PreProcess` 或初始化阶段同时置 `EMV_CARD_CONTACT`，并恢复 `emv_selec_kernel.c` 的 `contact_process` 赋值，且在 `EMV_Process` 调用前判空。

### [P1-3] `EMV_TLV_PacketByDOL` 把错误当成功返回：`*pOutLen = -1` 且返回 `EMV_OK`

- 位置：`src/emv.c:1880-1897`
- 证据：

```c
ret = emvdol_packet_tlv(pDOL, nDOLLen, pVal, nValLen, pOut, nOutMaxLen);
if (ret < 0)
{
    ret = EMV_FAIL;                  /* 失败时 ret = -1 */
}
if (pOutLen)
{
    *pOutLen = ret;                  /* 把 -1 写进输出长度 */
    ret = EMV_OK;                    /* 却把返回值改成 EMV_OK */
}
return ret;
```

- 影响：调用方按文档（`emv_utils.h:142`，成功才返回 EMV_OK）判断时会**把打包失败当成功**，并拿到长度 -1。若随后用 `*pOutLen` 做 `emv_memcpy(pOut, ..., len)`，`int32_t -1` 会转成 `uint32_t 0xFFFFFFFF`，形成超大拷贝/崩溃。该 API 只有 `pOutLen==NULL` 时才正确返回失败，属于明显的返回值语义缺陷。
- 建议：失败时直接 `return EMV_FAIL;`，不要写 `*pOutLen = -1`；成功时 `*pOutLen = ret; return EMV_OK;`。

### [P1-4] `EMV_ResultSelCandidates` 不校验 index，越界值会驱动候选数组越界访问

- 位置：`src/emv.c:1417-1427`；消费方 `src/emvapi.c:301-309`、`src/emv_combination_Selection.c:37-118`
- 证据：

```c
/* emv.c:1421-1425：只校验状态，不校验 eResult 范围 */
if (g_emv_cnxt->status != EMV_REQ_SEL_CANDIDATES && g_emv_cnxt->status != EMV_REQ_SEL_CANDIDATES_AGAIN)
    return rERR(EMV_SELCANDIDATENOTREQ);
g_emv_cnxt->req_sel_candidates_result = eResult;      /* 头文件要求 0..count-1，这里原样存 */
...
/* emvapi.c:303 / emv_combination_Selection.c:39 直接用该值索引 candidates[] */
*len = g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len;
```

- 影响：`candidates` 是按 `EMV_CFG_MAX_CANDIDATES=12` 固定分配的数组（`emv.c:1044`，位于全局缓冲末尾）。L3 传入 `-1`、`count`、或任意大值都会越界读：负下标/大下标可能读到上下文或缓冲区外的数据，随后 `emv_get_kernel_id` 还会按越界读到的 `kernel_id_len`（uint8，最多 255）再拷贝到调用方缓冲；`emv_combination_Selection` 则用越界读到的 AID/kernel_id 去选内核，可能被错误路由到别的内核。头文件只承诺 `EMV_OK/EMV_UNINITIALIZED/EMV_SELCANDIDATENOTREQ`，没有"越界"语义。
- 建议：在 `EMV_ResultSelCandidates` 内校验 `0 <= eResult < g_emv_cnxt->candidates_num`，否则返回 `EMV_SELCANDIDATENOTREQ`（或新增错误码）；`emvapi.c:303` 的裸索引也应改为经同一校验函数取值。

### [P1-5] `EMV_ResultOnline` 把 71/72 脚本模板改名成 0xDF8100/0xDF8101… 后存表：一个不在标签库、一个与 DS Summary 2 冲突

- 位置：`src/emv.c:1527-1559`（关键 1530-1531、1549-1556）；标签库 `src/emvtag.c:16-17,221`；标签定义 `src/inc/emv_tag.h:910-911`
- 证据：

```c
uint32_t tag1 = TAG_ISSUER_SCRIPT_71_BASE;   /* 0xDF8100 */
uint32_t tag2 = TAG_ISSUER_SCRIPT_72_BASE;   /* 0xDF8200 */
...
for (i = 1; i <= tlvs[0].childnum; i++)
{
    if (tlvs[i].tagname == TAG_0071_ISSUER_SCRIPT_TEMPLATE_1)
        tlvs[i].tagname = tag1++;
    else if (tlvs[i].tagname == TAG_0072_ISSUER_SCRIPT_TEMPLATE_2)
        tlvs[i].tagname = tag2++;
    appdata_save_tlv(tlvs + i, OVERRIDED, 0xFFFFFFFF);   /* 返回值未检查 */
}
```

- 影响：`appdata_save_tlv` 会先查标签数据库的更新条件与长度范围（`emvbuf.c:377-393`）：
  - 0xDF8100 **不在** `emvtag.c` 的标签表中 → 返回 `TAG_NOT_IN_DATABASE`，第一个 71 模板被静默丢弃；
  - 第二个 71 模板被改成 0xDF8101，而 0xDF8101 已被 `TAG_DF8101_DS_SUMMARY2`（长度 8~16）占用 → 要么长度不符被拒，要么把 DS Summary 2 的值覆盖成脚本模板，破坏数据认证摘要。
  结果是"在线响应中的发卡行脚本模板看似解析、实际落库失败/写错位置"。是否影响后续脚本处理取决于 PayPass 内核是否从这些 tag 取脚本（外部库不在本工程内，需确认），但 `EMV_ResultOnline` 无法完成它名字承诺的事情。
- 建议：71/72 模板保留真实 tag（0x71/0x72），或改用不在标签库中、且不与 DS Summary 冲突的专属 tag 并在 `emvtag.c` 注册；检查 `appdata_save_tlv` 返回值并上报失败。

### [P2-1] `EMV_Process` 在候选列表/账户类型交互状态会被反复走一遍"内核选择+初始化"

- 位置：`src/emv.c:1389-1408`（case 2/3/4/6）；关联 `src/emv_combination_Selection.c:23-133`、`src/emv_selec_kernel.c:37-63`
- 证据：

```c
case EMV_STA_CANDIDATES_BUILT:
case EMV_REQ_SEL_CANDIDATES:
case EMV_REQ_SEL_CANDIDATES_AGAIN:
case EMV_REQ_SEL_ACCOUNTTYPE:
    ret = emv_combination_Selection();          /* 每次都重新选内核 */
    ...
    g_emv_cnxt->status = emv_k_interface.emv_process();
```

- 影响：`emv_combination_Selection` → `emv_select_kernel` → 对非接 Mastercard 每次都会执行 `paypass_init()` + `paypass_preprocess()`（`emv_selec_kernel.c:45-60`）。而 3/4/6 是"内核请求 L3 回答、L3 调 `EMV_Result*` 后再次 `EMV_Process`"的状态，`EMV_ResultSelCandidates` 等并不改变 `status`，于是每回答一次就重新 `paypass_init/preprocess` 一次。若 PayPass 内核的 init/preprocess 非幂等（通常会复位到应用选择前），会出现应用选择被反复重启、账户类型选择丢失等状态机错误。当前 Entry Point 自己建候选列表（`emv_build_candidate_list` 返回 2），3/4 是否会出现取决于外部 PayPass 内核；6 则可能由内核返回。**需 Lead/内核方确认 `paypass_init/preprocess` 是否幂等**，若不幂等则为 P1。
- 建议：只在 `EMV_STA_CANDIDATES_BUILT` 分支做内核选择/初始化，3/4/6 分支只调用 `emv_k_interface.emv_process()`；或给 `emv_select_kernel` 加"已初始化"状态。

### [P2-2] `EMV_ProcessExit/EMV_initialize` 复位不完整，上一笔状态会污染下一笔

- 位置：`src/emv.c:933-953`（`EMV_initialize`）、`src/emv.c:1948-1958`（`EMV_ProcessExit`）
- 证据：

```c
static void EMV_initialize(int32_t flag)      /* flag 未使用 */
{
    if (g_emv_cnxt->tlvbuffer) emv_memset(g_emv_cnxt->tlvbuffer, 0, g_emv_cnxt->tlvbuffer_size);
    g_emv_cnxt->status = EMV_STA_IDLE;
    g_emv_cnxt->trans_type = _EMV_TRANSTYPE_UNKNOWN;
    g_emv_cnxt->trans_amount = 0;
    g_emv_cnxt->trans_amount_other = 0;
    g_emv_cnxt->ec_onlinpin_falg = 0;
    EMV_InitOutCome(); EMV_InitUIRequest(); EMV_InitErrorIndication();
    emvicc_set_stop_flag(false);
}
```

- 影响：`amount_absent / oth_amount_absent / force_online / card_type / card_types_allowed / online_result / preprocess_result_num / req_*_result` 都不复位。典型事故：上一笔是"金额未知"交易（`amount_absent=true`），下一笔正常消费若 L3 只调用 `emv_set_trans_amount()` 而没有显式 `emv_set_trans_amount_absent(false)`，`load_trans_amount`（`emv.c:903-918`）不会写 9F02/9F03，交易按错误金额/无金额进行；`card_type` 残留也会让下一笔跳过 `emv_set_cardtype` 仍被 `EMV_Process` 接受。`EMV_Exit` 也不清 `g_emvapi` 回调，重新 `emv_initialize` 才会覆盖。
- 建议：复位时清零所有与"本次交易"相关的字段（amount_absent/oth_amount_absent/force_online/card_type/preprocess_result_num/online_result/req_*），或改为每次 `EMV_Process` 首笔用独立 `EMV_TransactionReset()`。

### [P2-3] `emv_get_version` 在 `EMV_STA_IDLE` 下取不到内核版本，且向 `%s` 传 NULL 仍返回 `EMV_OK`

- 位置：`src/emvapi.c:148-158`；`src/emv.c:1578-1595`
- 证据：

```c
int32_t emv_get_version(uint8_t* ver)
{
    uint8_t* ptmp; int32_t ilen;
    ptmp = EMV_GetData(TAG_KERNEL_VERSION, &ilen);
    API_sprintf((char*)ver, "%s & %s", ptmp, EMVAPI_VERSION);   /* ptmp 可能为 NULL */
    return EMV_OK;
}
/* EMV_GetData：未 EMV_Process 前 status==EMV_STA_IDLE 直接 return NULL */
```

- 影响：`EMV_Init` 后到第一次 `EMV_Process` 之前（L3 显示版本号的典型时机）`ptmp==NULL`，`%s` 收到 NULL 指针：IAR DLIB 可能打印 `(null)`，也可能直接崩（取决于库实现），且函数仍返回 `EMV_OK`，L3 无法区分"拿到版本"与"没拿到"。同时 `ver` 无长度参数，`API_sprintf` 写多少由调用方保证。
- 建议：`EMV_Init` 完成后把 `EMV_VERSION` 作为普通 appdata 存起来（或让 `EMV_GetData` 对 `TAG_KERNEL_VERSION` 在 IDLE 下也返回版本），并对 `ptmp` 判空、失败返回 `EMV_FAIL`；`emv_get_version(ver,len)` 增加长度参数。

### [P2-4] `emvoutcome.c` 用 `sizeof(指针)` 去限制语言偏好长度，多语言终端只拷到 4 字节

- 位置：`src/emvoutcome.c:158-160`、`272-274`、`385-387`
- 证据：

```c
unsigned char lang_pre[8]={0};
unsigned char *lang_pre_ptr = appdata_find(TAG_5F2D_LANGUAGE_PREFERENCE, &lang_pre_len);
...
if(lang_pre_len > sizeof(lang_pre_ptr))     /* 应为 sizeof(lang_pre) = 8；指针 = 4 */
    lang_pre_len = sizeof(lang_pre_ptr);
emv_memcpy(lang_pre, lang_pre_ptr, lang_pre_len);
```

- 影响：32 位 ARM 上 `sizeof(指针)=4`，`LanguagePreference`（5F2D，Entry Point 规定 2~8 字节）超过 4 字节时被截断，后续 `EMV_SetUIRequest` 只发送前 4 字节 → 多语言终端 UI 语言选择错误。同文件 `msg_read_card_ok`（`emvoutcome.c:463`）用的是 `sizeof(lang_pre)`，是正确的，可对照证明这是笔误。
- 建议：三处改为 `sizeof(lang_pre)`。

### [P2-5] Outcome 的 PresentFlag 置位与实际 TLV 表内容不一致（Data Record / Discretionary Data）

- 位置：`src/emvoutcome.c:423-451`（`outcome_end_application_startB`）、`362-421`（`outcome_seephone`）、`304-334`（`outcome_try_again`）；对照 `src/emvoutcome.c:10-13,34-38`
- 证据：

```c
/* outcome_end_application_startB：填了 discretionary 缓冲，但既没更新 len 也没写 FF8106；
   随后把 data_record_pre=1、disc_data_pre=1 置进 PresentFlag */
emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
appdata_save(TAG_DF8115_ERROR_INDICATION, (void *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
emv_memcpy(g_discretionary_data, (void *)&g_err_indicator, sizeof(g_err_indicator));
EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_B, ..., 0, 1, 1, 1, 0, ...);
/*                                            ui_on_restart=1, data_record_pre=1, disc_data_pre=1 */

/* outcome_try_again：memset 了缓冲，但同样没有 g_discretionary_data_len = i、也没有 appdata_save(FF8106) */
emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
emv_memcpy(g_discretionary_data+i, "\xDF\x81\x15\x06", 4); i += 4;
emv_memcpy(g_discretionary_data+i, (void *)&g_err_indicator, sizeof(g_err_indicator));
EMV_SetOutCome(..., 0, 0, 0, 1, 0, ...);        /* disc_data_pre=1，但 FF8106 未更新 */
```

- 影响：`EMV_SetOutCome` 第 8 个参数 `disc_data_pre` 置 Presentation Information 的 bit5、第 7 个参数 `data_record_pre` 置 bit6（与 Book B 的 Presentation Information 位序一致）。置位后 Entry Point 规范要求对应 TLV（Discretionary Data / Data Record）确实存在：但 `g_data_record` 在整个工程里**没有任何写入者**（只有 `emvoutcome.c:34-38` 清零），`TAG_FF8105_DATA_RECORD` 也从未 `appdata_save`；startB/seephone/try_again 又没保存 FF8106。于是 Outcome 声称"带 Data Record/Discretionary Data"，实际 TLV 表里没有或还是旧值，L3 按规范读取会拿到空/过期数据。`outcome_try_again_without_restart`（`emvoutcome.c:223-234`）是正确写法（更新 len + save），可对照证明其余几处漏了。
- 建议：抽一个 `outcome_fill_discretionary()` 统一完成"清零+组 TLV+更新 len+appdata_save(FF8106)"；Data Record 要么实现填充 `g_data_record`/保存 FF8105，要么把 `data_record_pre` 置 0。

### [P2-6] `EMV_SetData` 的入参校验被注释，负长度/NULL 会进 TLV 表；且与 `EMV_GetData` 的 IDLE 语义不对称

- 位置：`src/emv.c:1634-1655`；`src/emvapi.c:133-146`；`src/emv.c:1578`
- 证据：

```c
int32_t EMV_SetData(uint32_t tag, uint8_t *value, int32_t valuelen)
{
    if (g_emv_cnxt == NULL) return EMV_UNINITIALIZED;
//    if (g_emv_cnxt->status < EMV_STA_APP_SELECTED) return rERR(EMV_NOT_STA_APP_SELECTED);
//    if (value == NULL || valuelen < 0) { return rERR(EMV_VALUENULL_LENERR); }   // 校验被注释
    if (appdata_save(tag, value, valuelen, OVERRIDED)) return EMV_FAIL;
```

- 影响：对外 API `emv_set_tag_data(tag, value, length)` 传 `length<0` 时，`appdata_save` 只在 `value==NULL && valuelen!=0` 时拒绝（`emvbuf.c:177-180`），会以 `valuelen=-1` 建一条记录；之后任何按 `int` 长度使用该 tag 的读取方都可能把它当超大长度处理。`value==NULL && valuelen==0` 会建"空值"记录，与"删除 tag"的语义也无法区分。另外 `EMV_SetData` 未限制内核状态（注释掉了状态检查），与 `EMV_GetData` 在 `EMV_STA_IDLE` 一律返回 NULL 的语义不对称：L3 可以在交易开始前 set 却读不回来。
- 建议：恢复 `value==NULL || valuelen<0` 校验；明确 `EMV_SetData` 允许/禁止的状态并与 `EMV_GetData` 一致；`valuelen==0` 定义成"删除"还是"空值"需在头文件写明。

### [P2-7] `emv_process2` 未覆盖 25~59 等大量中间状态，一律回调 `STATUS_ERROR 255` 并丢失错误码

- 位置：`src/emvapi.c:427-545`（default 525-527）；状态定义 `src/inc/emv_type.h:275-314`
- 证据：

```c
switch (ret)
{
    case EMV_STA_CANDIDATES_BUILT: ... case EMV_REQ_CARDINFO_CONFIRM: break;
    default:
        throws_status(STATUS_ERROR, 255);   /* 所有未列出的状态/错误都归到这里 */
        break;
}
...
if(g_emvapi_messageinx != -1) { *msginx = g_emvapi_messageinx; g_emvapi_messageinx = -1; break; }
else { *msginx = -1; }
...
return ret;                                  /* 同时又把原始 ret 返回给调用方 */
```

- 影响：`EMV_Status` 中 26~59（`EMV_REQ_SELECT_NEXT`、`EMV_REQ_SET_BEFOREGPO`、`EMV_STA_DATAEXCHANGE_COMPLETED`、`EMV_REQ_ISSUER_UPDATE`、`EMV_STA_RRP_COMPLETED`、`EMV_STA_BEEP_OK` …）以及 106~117（`EMV_SWITCH_INTERFACE`/`EMV_ENDAPPLICATION`/`EMV_USE_ANOTHER_CARD`…）都不会被识别，凡是 PayPass 内核返回这些状态，L3 收到的是 `STATUS_ERROR/255`；同时函数又返回真实状态码，形成"回调说失败、返回码说继续"的自相矛盾。负错误码也全部被压成 255，L3 无法区分 `EMV_CARDTYPEILL` 与 DA 失败。
- 建议：按 `emv_type.h` 的 `EMV_Status` 补全 switch（至少把 RRP/DataExchange/IssuerUpdate/2nd Tap 等 PayPass 状态映射到 `STATUS_CONTINUE` 对应 info），错误分支把 `ret` 归一化到 `emv_error_info` 后再回调，并保持"回调信息与返回值一致"。

### [P2-8] `emv_set_termparam/emv_add_aidparam/...` 改参数后不会重新初始化内核缓存，接口承诺"改完即生效"名不副实

- 位置：`src/emvapi.c:628-699`
- 证据：

```c
int32_t emv_clr_termparam(void)
{
    if (emvporting_clr_termparam()) return EMV_FAIL;
//    EMV_Init(&g_emv_porting);        /* 重新加载被注释掉 */
    return EMV_OK;
}
int32_t emv_set_termparam(uint8_t* par, int32_t len)
{
    if (emvporting_set_termparam(par, len)) return EMV_FAIL;
//    EMV_Init(&g_emv_porting);        /* 同上 */
    return EMV_OK;
}
```

- 影响：`g_emv_cnxt->termapps` 与每条 AID 参数是 `EMV_Init` 时从文件读入并缓存在全局缓冲里的（`emv.c:1013-1027`）。`emv_set_termparam/add_aidparam/add_capkparam` 只写文件，不刷新缓存；L3 调用后立刻开交易仍用旧参数，返回 `EMV_OK` 会误导上层。注释掉的 `EMV_Init` 说明作者知道要重载，但直接 `EMV_Init` 会清掉交易状态，不能放在这些 API 里。
- 建议：要么在 API 文档明确"参数修改后须重新 `emv_initialize` 生效"，要么实现"仅重载参数、保留交易上下文"的 `EMV_ReloadParams()`。

### [P2-9] 头文件原型缺口：多个被跨文件调用/对外提供的函数没有声明，只能隐式声明

- 位置：`src/inc/emv.h`（缺 `EMV_SetTransAmountAbsent/EMV_SetOtherAmountAbsent`）；`src/inc/emvoutcome.h`（缺 `EMV_SetOutCome`、`EMV_InitErrorIndication`、`EMV_InitDataRecord`、`EMV_InitDiscData`、`outcome_try_again_without_restart`、`outcome_try_again_end_application`、`outcome_end_application`、`outcome_end_application_startB`、`outcome_seephone`、`msg_read_card_ok`、`outcome_request_another_payment`）；`inc/emvapi.h`（缺 `emv_set_card_type`、`emv_get_kernel_id`、`emv_set_current_code`、`emv_get_current_code`、`emv_set_current_exponpent`、`emv_get_current_exponpent`、`emv_clr_drl`、`emv_add_drl`）
- 证据：

```c
/* emvapi.c:254-262 调用 emv.h 里没有原型的函数 */
int32_t emv_set_trans_amount_absent(bool flag) { return EMV_SetTransAmountAbsent(flag); }
int32_t emv_set_other_amount_absent(bool flag) { return EMV_SetOtherAmountAbsent(flag); }
/* emv.c:950 调用 emvoutcome.h 没有声明的函数 */
EMV_InitErrorIndication();
/* emv_application_selection.c:1341/1346/1360/1376 调用 emvoutcome.h 没有声明的函数 */
return outcome_request_another_payment();
```

- 影响：C99 下隐式声明是约束违反，IAR 会告警（部分配置下报错），且隐式声明默认返回 `int`，与实际返回类型不一致时会静默截断指针/64 位值（本工程返回 `int/int32_t`，暂时不致命）。更实际的是：这些函数无法被 L3 正常引用（没有声明），接口面不完整、维护时容易重复定义。
- 建议：把上述原型补进对应头文件；`emv_get_kernel_id` 之类若为内部函数应加 `static` 或移入 `emvi.h`。

### [P2-10] `emv_set_card_type` 无 return（未定义行为）；`emv_get_kernel_id` 无 NULL/边界校验

- 位置：`src/emvapi.c:210-214`、`src/emvapi.c:301-309`
- 证据：

```c
int32_t emv_set_card_type(int32_t cardtype)      /* 声明返回 int32_t，但函数体没有 return */
{
	if(g_emvapi.init.emv_cardtype != NULL)
		g_emvapi.init.emv_cardtype(cardtype);
}

int32_t emv_get_kernel_id(uint8_t * val, int32_t *len)
{
	*len = g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len;
	if(...kernel_id_len)
		emv_memcpy(val, ...kernel_id, ...kernel_id_len);
	return EMV_OK;
}
```

- 影响：`emv_set_card_type` 若被调用，返回值是栈上不确定值（UB），调用方可能误判失败/成功；`emv_get_kernel_id` 既没有声明（见 P2-9），也不判 `g_emv_cnxt/val/len` 是否为空、不做候选下标校验（叠加 P1-4），是典型的"内部测试残留接口被留在库里"。
- 建议：`emv_set_card_type` 补 `return EMV_OK;`（或按回调结果返回），并决定它是否属于对外接口；`emv_get_kernel_id` 增加 NULL/下标校验后并入 `emvi.h`，或删除。

### [P2-11] `EMV_GetTransLog/EMV_GetLoadLog` 用 256 字节栈缓冲接收卡片记录，长度由卡片控制

- 位置：`src/emv.c:1657-1733`（`buf[256]` 见 1660）、`src/emv.c:1735-1811`（1738）；下层 `src/emvicc.c:350-359`
- 证据：

```c
int32_t EMV_GetTransLog(int32_t nRecNo, uint8_t* pOut, int32_t* pOutLen)
{
    uint8_t  buf[256];
    ...
    if ((ret = emvicc_read_record(*ptmp, (uint8_t)nRecNo, buf, &len)) != 0x9000) ...
    if (pOut) emv_memcpy(pOut, buf, len);        /* len 由卡片返回决定，未与 sizeof(buf)/调用方缓冲比较 */
/* emvicc.c:350-359 */
if (rlen > 0) { if (output != NULL) emv_memcpy(output, recv, rlen); if (outlen != NULL) *outlen = rlen; }
```

- 影响：`emvicc` 层的接收缓冲是 `EMV_CFG_ICC_RECV_MAXLEN=512`（`emvi.h:46`），拷贝到调用方时不做容量校验。`EMV_GetTransLog` 传入的 `buf` 只有 256 字节，若卡片对 READ RECORD 返回超过 256 字节（T=1 单帧上限 + SW1SW2 可能接近/超过 256），就会**栈溢出**，且这是卡片数据可控的路径（PBOC 日志记录读取）。`pOut` 是 L3 提供的缓冲，同样没有容量参数，`*pOutLen` 只能事后告知。
- 建议：`emvicc_read_record` 增加输出容量参数并在 `rlen > capacity` 时返回错误；`EMV_GetTransLog` 用 `sizeof(buf)` 显式裁剪或改为动态/分块读取；对外 API 增加 `pOut` 容量参数（`emv_utils.h:211` 目前没有）。

### [P2-12] 对外读取 API 没有输出容量参数（`emv_get_tag_data` / `emv_get_candidate_list` 等）

- 位置：`src/emvapi.c:114-131`、`src/emvapi.c:160-175`；声明 `inc/emvapi.h:243,273`
- 证据：

```c
int32_t emv_get_tag_data(uint32_t tag, uint8_t* value, int32_t* length)
{
    ptmp = EMV_GetData(tag, &ilen);
    if (ptmp != NULL && ilen >= 0)
    {
        if (value) API_memcpy(value, ptmp, ilen);   /* 不知道 value 有多大 */
        if (length) *length = ilen;
        return EMV_OK;
    }
...
int32_t emv_get_candidate_list(void* list)          /* 不知道 list 能放几个 EMV_CANDIDATE */
{
    ptmp = EMV_GetData(TAG_CANDIDATE_LIST, &ilen);
    ...
    API_memcpy(list, ptmp, ilen);
```

- 影响：标签值可能来自卡片（如 9F4F 日志格式最长 255、BF0C 子元素、9F10 等），候选列表长度也由卡决定。L3 必须自己知道每个 tag 的理论最大长度并分配足够缓冲，否则 `emv_get_tag_data` 会把卡片可控长度直接 memcpy 进 L3 缓冲 → 越界写。这类"输出缓冲无容量"的 API 在支付终端里属于系统性风险（同类还有 `emv_get_version`、`emv_get_transwater`、`emv_get_termparam`）。
- 建议：给所有输出型 API 增加 `inout` 长度参数（传入容量、返回实际长度），并做 `ilen > capacity` 截断/报错。

### [P2-13] `EMV_Init` 在终端应用列表获取失败/数量越界后不中止，继续用非法数量布局并初始化

- 位置：`src/emv.c:1015-1027`、`src/emv.c:1029-1064`
- 证据：

```c
g_emv_cnxt->termapps_num = EMVPORT_GetTermApps(g_emv_cnxt->termapps);
eTRACE("g_emv_cnxt->termapps_num = %d\n", g_emv_cnxt->termapps_num);
if(g_emv_cnxt->termapps_num < 1 || g_emv_cnxt->termapps_num > EMV_CFG_MAX_APPS)
{
    ret = rERR(EMV_GETTERMAPPSERR);      /* 只记错误码，不 return */
}
uint32_t termapps_size = sizeof(EMV_TERMAPP) * g_emv_cnxt->termapps_num;  /* 用非法数量算布局 */
current_ptr += ((termapps_size + 3) & ~3);
...
EMV_initialize(0);      /* 仍继续初始化：清零 tlvbuffer(10KB) */
appdata_init();
return ret;             /* 最后才把错误返回；此时 g_emv_cnxt 已指向不一致布局 */
```

- 影响：`EMVPORT_GetTermApps` 返回 `-1/-3`（文件不存在/读失败）时 `termapps_num` 为负，`termapps_size` 为负 → `current_ptr` **回退**，`tlvbuffer` 指到上下文/termapps 内部，随后 `EMV_initialize` 按 `tlvbuffer_size=10240` 的 `memset` 会覆盖 `EMV_CONTEXT` 自身；返回 `EMV_GETTERMAPPSERR` 时调用方如果只记日志继续跑（很常见），`g_emv_cnxt` 已是损坏布局。数量 >128 时也不会中止，只是布局更大、可能靠 candidates 末尾检查兜底。
- 建议：检测到非法数量立即 `return rERR(EMV_GETTERMAPPSERR)`（此时 g_emv_cnxt 可保持 NULL）；并在所有指针切分完成后做一次"总大小 ≤ len"的整体预算检查，再调用 `EMV_initialize/appdata_init`。

### [P3-1] 死代码与"返回未计算值"的接口

- 位置：`src/emv.c:203-227`（`emv_crc32` 无调用点）；`src/emv.c:955-968`（`static emv_save_translog` 无调用点）；`src/emv.c:812-854`（`emv_bcd_add` 仅出现在注释里）；`src/emv.c:483-515`（`emv_date_compare` 工程内无调用点）；`src/emv.c:1340-1346`（`EMV_SetRRPTimeDelay` 未声明，且 `paypass_set_rrp_delay` 工程内无定义）
- 影响：编译器会给出未使用告警；`TAG_KERNEL_CHECKSUM/TAG_CONFIG_CHECKSUM` 依赖的 `kernel_checksum/config_checksum` 从未计算（`emv_crc32` 未被调用），即使补上字段也恒为 0，DET/校验功能形同虚设。`EMV_SetRRPTimeDelay` 依赖的符号不在本工程，若被 L3 使用会链接失败。
- 建议：删除死代码或补上调用与校验逻辑；`kernel_checksum/config_checksum` 明确由谁计算、何时计算。

### [P3-2] 数值工具边界：移位越界、负长度视为"相等"、6 字节 BCD 金额溢出

- 位置：`src/emv.c:389-405`（`emv_hex2dec` 的 `hex[i] << (8*(hexlen-1-i))`）；`src/emv.c:229-247`（`emv_memcmp` 的 `++count; while(--count)`）；`src/emv.c:420-437`（`emv_bcd2dec` 返回 `uint32_t`）
- 影响：`emv_hex2dec` 在 `hexlen>4` 时移位 ≥32 位（未定义行为），当前调用点均为 4 字节，暂无实际触发；`emv_memcmp` 若被传入 `count` 为负（`uint32_t` 收到 0xFFFFFFFF 会 `++count` 变 0，循环不执行并返回 0="相等"），属于潜在"长度污染即认证通过"的隐患；`emv_bcd2dec` 对 6 字节限额（DF19/DF20/DF21，最大 999999999999）返回 `uint32` 会回绕，导致 `emv.c:1227,1243,1270` 的限额比较在配置大额时失真。
- 建议：`emv_hex2dec` 增加 `hexlen>4` 保护；`emv_memcmp` 用 `int32_t` 安全循环并拒绝负数；金额限额比较改用 6 字节 BCD 比较或 `uint64_t`。

### [P3-3] `emv_date_compare` 世纪硬编码 0x20，1999 年过期的卡会被判为 2099 年有效

- 位置：`src/emv.c:494-507`
- 证据：

```c
if (len < 4) {
    //if (*date >= 0x50) { temp[0] = 0x19; } else
    { temp[0] = 0x20; }      /* 一律按 20xx 处理 */
    temp[1] = *date;         /* YY */
```

- 影响：5F24=0x991231 的卡（1999-12-31 过期）会被拼成 2099-12-31，比较结果"未过期"；对当前流通卡影响很小，但作为对外导出的日期比较工具（接触/非接内核都会调用）不合规范窗口化要求。当前工程内无调用点，风险取决于外部 PayPass 内核是否调用。
- 建议：按 EMV 常用窗口（YY<50→20xx，否则 19xx）或直接与当前日期同格式比较，并去掉对"20"的硬编码。

### [P3-4] `EMV_initialize` 的 `flag` 未使用、`preprocess_result_num` 无消费者且不复位；`EMV_SetUIRequest` 的 value/currency 参数被忽略

- 位置：`src/emv.c:933`（`EMV_initialize(int32_t flag)` 内未引用 flag）、`src/emv.c:1305`（`preprocess_result_num++`，全工程无读取点、复位时未清零）；`src/emvoutcome.c:109-135`（`value_qualifier/value/currency_code` 参数全部被注释掉，`EMV_UIREQ_DATA` 也没有对应字段）
- 影响：死参数/死字段增加维护误导；Entry Point 规范里 UIRequest 的 Value Qualifier/Value/Currency Code（如余额显示）在本实现缺失，L3 若依赖这些字段显示金额/余额将拿不到数据。另外 `OUTCOME_START_NA=0xF0` 被用作 End Application 的 Start（`emvoutcome.c:178,346`），而 Book B 定义的 Start 取值是 0x00/0x10/0x20/0x30，0xF0 是否为 L3 约定的"NA"需确认（依据：Book B Outcome 定义，PDF 未逐字提取）。
- 建议：删除未用参数/字段或补齐实现；`OUTCOME_START_NA` 的取值与 L3 对齐后写进头文件注释。

### [P3-5] 输出参数未判 NULL

- 位置：`src/emvapi.c:530-539`（`*msginx` 直接写）；`src/emvapi.c:303`（`*len` 直接写）；`src/emvapi.c:170`（`API_memcpy(list, ...)` 未判 `list`）
- 影响：头文件把参数标为 `[OUT]`，但库内其它 API 普遍做了 NULL 保护（如 `EMV_GetData` 的 `pLen`），这里不一致；L3 传 NULL 会崩。
- 建议：`if (msginx)`、`if (len)`、`if (list)` 后再写。

## 跨模块契约核对（供 Lead 汇总）

> 本节只做"声明 ↔ 实现 ↔ 调用"的比对；标 (a)/(c)/(d)/(e) 的条目落在其他审查员文件里，供 Lead 去重/分派，不计入本文 28 条。

### 1. `inc/emvapi.h` ↔ `src/emvapi.c` ↔ `src/emv.c` 三方一致性与语义差异

| 头文件声明（inc/emvapi.h） | emvapi.c 实现 | 内核实现 | 核对结果 |
|---|---|---|---|
| `emv_initialize(EMV_INIT_PARAM*)` (291) | 拷贝回调后 `EMV_Init(&g_emv_porting)` (181-207) | `EMV_Init`（emv.c:972） | 基本一致；但 `par->pCardEventOccured` 全工程无调用点，`par->emv_cardtype` 只能通过未声明的 `emv_set_card_type` 触发（见 2 条） |
| `emv_get_version(uint8_t*)` (264) | `API_sprintf("%s & %s", ptmp, ...)` (148-158) | `EMV_GetData(TAG_KERNEL_VERSION)` 在 IDLE 返回 NULL | **不一致/不安全**：无长度参数、NULL `%s`、恒返回 OK（P2-3） |
| `emv_get_tag_data(tag, value, length)` (243) | 无容量拷贝 (114-131) | `EMV_GetData` | 语义一致但无输出容量（P2-12） |
| `emv_set_tag_data` (255) | `EMV_SetData` (133-146) | 校验被注释（emv.c:1634） | **入参校验缺失**（P2-6） |
| `emv_set_trans_amount_empty/other_amount_empty` (335-336) | 调 `EMV_SetTransAmountEmpty/OtherAmountEmpty` (266,271) | **无实现**，`EMV_CONTEXT` 无字段 | **声明/实现/字段三方全缺**（P0-2） |
| `emv_set_candidate_list_result(index)` (379) | 透传 `EMV_ResultSelCandidates` (547-550) | 不校验 index（emv.c:1421） | **语义不一致**：头文件 0..count-1，实现不校验（P1-4） |
| `emv_process()` (357) / `emv_process2(int32_t*)` (368) | 直接透传 / 大 switch (313-545) | `EMV_Process` 可返回 1~59、100~117（emv_type.h:251-314） | `emv_process2` 只覆盖部分状态（P2-7） |
| `emv_get_kernel_index/set_kernel_index` (719,728) | 调 `emvporting_*` 后恒 `EMV_OK` (783-795) | porting 返回 void（emvporting.h:158-159） | 一致，非缺陷（修正了初判） |
| 其余 `EMV_Result*` API | 透传 | 均校验 `status==REQ_*` | 一致（见"已核对"） |
| 未声明但已实现：`emv_set_card_type`、`emv_get_kernel_id`、`emv_set/get_current_code*`、`emv_clr_drl/add_drl` | emvapi.c:210,226-242,301,803-820 | 部分符号工程内无定义 | **接口面不一致 + 链接风险**（P0-2、P2-9、P2-10） |

补充：`EMV_INIT_PARAM.pEMVProcessCallback` 只在 `throws_status()`（`emvapi.c:91-102`）里使用，而 `throws_status` 只被 `emv_process2` 调用；`emv_process`（更常用的入口）完全不上报内核提示信息，L3 用 `emv_process` 就收不到 `EMV_MSG_*` 消息。若产品 API 规范要求两种入口等价，需要调整。

### 2. `g_emv_porting`（EMV_PORTING）回调表 ↔ 内核实际调用

- `(e) src/emvspi.c:1276-1310` 的 `g_emv_porting` 用指定初始化器，**未初始化 `trace/trace_hex`**（`emv.h:50,60` 的前两个成员），两指针恒为 NULL。经全工程检索，内核只通过 `API_Trace/API_TraceHex` 宏输出（`emvdbg.h`），**没有** `g_emv_port->trace` 的调用点，因此当前不构成故障；但 `inc/emv.h` 把它们定义为移植接口，任何后续新增调用都会空指针崩溃。建议在表中显式初始化或从结构体删除。
- 用户交互回调（`message/sel_candidate_list/sel_account_type/deactivate_card/check_id/pin_entry/confirm_bypass_pin/confirm_force_online/go_online/issuer_referral`）：本工程 `EMVSPI_DO=0`（`emvspi.c:92`），除 `message()` 外全部实现为 `return EMV_OPER_REPORT_APP;`（如 `emvspi.c:718-881`）。这意味着内核必须把 `-7` 翻译成 `EMV_REQ_*` 状态交由 L3，L3 再通过 `EMV_Result*` 回填；契约本身自洽，但与 `inc/emv.h:71-187` 里"回调直接返回 UI 选择结果（0..count-1 等）"的文档描述是两套模式，头文件没有说明本构建走 report-app 模式。**契约文档与实现不一致**，建议在 `emv.h`/`emvspi.h` 注明。
- `message()`（`emvspi.c:691-703`）把 `EMV_MSG_*` 存进 `g_emvapi_messageinx`，由 `emv_process2` 消费并清 -1（`emvapi.c:530-539`）——链路正确；但 `emv_process` 不消费该变量，若 L3 混用两个入口，消息会残留到下一次 `emv_process2` 调用。建议在 `EMV_ProcessExit/EMV_initialize` 里一并复位 `g_emvapi_messageinx`（`emvapi.c:188` 只在 initialize 时复位）。
- `alloc_buffer`：`emvspi.c:895-907` 的弱实现返回 14KB+256 的静态缓冲并整体清零；`EMV_Init` 要求 `len >= sizeof(EMV_CONTEXT)+2048`（`emv.c:994`），随后按 `EMV_TERMAPP/EMV_PREPROCESS_RESULT/TLVBUFFER_SIZE(10KB)/EMV_CANDIDATE*12` 依次切分（`emv.c:1004-1059`）。`emv.c:1015-1027` 对 `GetTermApps` 返回值/数量的校验在写入之后且不中止（见 P2-13），`emvporting.c:1143-1245` 的 AID 追加本身没有 `EMV_CFG_MAX_APPS` 上限，属于跨模块的"先写后查"。
- `icc_rw`：`emvspi.c:920-940` 对未知 `type` 返回 `0x0000`（文档说 0x0000 即错误），会与"正常 SW=0x0000"的返回值语义混淆；内核按 SW 判定（`emvicc.c:345-348`）会把它当失败——行为可接受，但建议返回 `-1` 以区分。
- `get_trans_date_time(date, NULL)`：`emv.c:490` 传 `time=NULL`，实现 `emvspi.c:1069-1091` 有 `if (time != NULL)` 保护，**契约匹配**（见"已核对"）。
- `get_random/recover`：`emvspi.c:1042-1058` 的 `get_random` 用 `uint8_t buf1[256]` 接 `len`，`emvspi.c:952-982` 的 `recover` 按 `pk->rsa.modulus_len` 拷贝到 `RSA_PUBKEY`，两者都未校验长度上限；内核调用点若传 >256 / >248 会栈溢出。属 `(e)` 范围，列入下节。

### 3. 内核接口表 `emv_k_interface` / `paypass_*` / `contact_*`

- `(a) src/emv_selec_kernel.c:7` 的 `emv_k_interface` 是全局零初始化；只有 QMSDC 分支给它 `emv_process` 赋值（`:60`），contact 分支（`:22-34`）赋值语句被注释掉，其余非接分支只 `break`。`emv.c:1403/1407` 无条件调用它 → contact/非 MC 非接路径下是空指针调用。当前 `emv_combination_Selection.c:123` 又把 `contactless_type` 无条件改回 QMSDC，掩盖了非 MC 分支；但 contact 分支仍暴露（叠加 P1-2）。
- `paypass_init/paypass_preprocess/paypass_process` 仅在 `include/paypass_api.h:6-8` 声明，本工程 13 个 `.c` 中**无定义**；`contact_init/contact_process` 同理（`include/contact_api.h:4-6`）。它们应由外部 PayPass/Contact 内核库提供，但本工程既没有库文件列表也没有说明。**若构建时不链接该外部库，`emv_selec_kernel.c` 会链接失败**；若链接，则 P2-1 的"重复 init/preprocess"问题需要内核方确认。
- `EMV_ANDROID_SO=0`（`emv.h:24`）时 `emv_selec_kernel.c` 走 `#else` 分支；`emv_combination_Selection.c:15-21` 的 `KERNEL_ID_*` 常量把 kernel id 写死为 2~7/0x2D，与 `EMV_CONTEXT.candidates[].kernel_id_len` 配合使用，跨模块的 id 编码约定没有集中定义。

### 4. outcome / UIRequest 头文件契约

- `EMV_SetOutCome` 定义在 `emvoutcome.c:59` 但 `emvoutcome.h` 没有声明（`emvoutcome.h:143-147` 只声明了 `EMV_InitOutCome/EMV_InitUIRequest/outcome_other_card/outcome_select_next/outcome_try_again`），而 `outcome_end_application/seephone/startB/try_again_*/msg_read_card_ok/outcome_request_another_payment` 也是"定义了但部分没声明、部分被跨文件调用"。见 P2-9。
- `JNISendOutcome/JNISendUIReq`（`emvoutcome.c:174,186,206,244,287,298,330,354,403,415,445,480`）在工程内既无声明也无定义；只有"应用没设置回调"时才走这条路径。若上层不提供这两个符号，**链接会失败**；`emvoutcome.c` 能生成 `.o` 只是因为 IAR 对隐式函数声明只告警。建议改为向"移植层回调缺失即返回错误"的设计，或把 JNI 适配移到 Android 工程。
- `EMV_OUTCOME_DATA.RemovalTimeout` 是 1 字节（`emvoutcome.h:107`），`EMV_SetOutCome` 最后两个参数 `field_off_time` 用 `OUTCOME_FIELDOFF_NA=0xFF` 表示 NA、`removal_timeout` 用 0x0D 等具体值；Book B 的 Field Off Request 若为多字节/BCD，需要与 L3 对齐（本次按 1 字节核对，未见明显越界）。
- Outcome 与 UIRequest 的**发送顺序**：所有 outcome_* 都是先发 UIRequest（`UI_TYPE_ON_OUTCOME/ON_RESTART`）再发 Outcome，且 PresentFlag 的 bit8/bit7 分别表示"Outcome 带 UI/重启带 UI"，与 Book B 语义一致；但 `outcome_seephone` 的回调类型不一致：`g_send_outcome_cb.pSendUIRequest(..., UI_TYPE_ON_RESTART)`（`:399`）而回落路径 `JNISendUIReq(..., UI_TYPE_ON_MSG)`（`:403`），同一事件两种 type，L3 会按不同分支处理。建议统一。

### 5. 其他模块问题（不重复计入本文，供 Lead 分派）

- `(d) src/emvporting.c:1225`：`tmp->kernel_id = *(tlvs[pos].pvalue);` —— `kernel_id` 是 `uint8_t[8]`，数组不能整体赋值，是硬编译错误（已用 `gcc -fsyntax-only` 复核）；应改为 `kernel_id[0] = ...` 或 memcpy。
- `(d) src/emvporting.c:1143-1245`：`emvporting_get_termapps` 无 `EMV_CFG_MAX_APPS` 上限，`while(1){...apps_num++; tmp++;}` 一直写到文件结束，可能冲出 `emv.c` 划给 termapps 的区域（见第 2 节 alloc_buffer 条目）。
- `(c) src/emvtlv.c:1753-1787`：`emvdol_packet_tlv` 在检查 `tlvlen > tlvlmt` 之前就先写 tag/len，且 `emv_memcpy(tlv+tlvlen, val+k, reqlen)` 的越界检查也在写之后，与 P0-4 是同一类"先写后查"。
- `(e) src/emvspi.c:1042-1058`：`get_random` 的 `uint8_t buf1[256]` 与 `len` 无上限校验；`(e) src/emvspi.c:952-982`：`recover` 按 `modulus_len` 拷贝到固定结构，未校验 ≤248。
- `(e) src/emvicc.c:350-359`：`icccommand` 把 ≤512 字节的 `recv` 直接拷给调用方 `output`，无容量参数（P2-11 的根因）。
- `(a) src/emv_combination_Selection.c:123`：无条件 `g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QMSDC;`，把所有非接卡都路由到 Mastercard 内核（这也是第 3 节所述"非 MC 非接分支不会触发空指针"的原因）。
- `(d) src/inc/emvporting.h:165,195`：`EMV_TERMPARAM` 全工程未定义，是 `emvspi.c/emvporting.c/emvapi.c` 三个 TU 编译失败的共同根因（本文 P0-2 只记 emvapi.c 侧）。
- 工程层面：无 Makefile/CMake，`build/iar/Debug/Obj/` 缺 5 个 `.o`（emv/emvapi/emvspi/emvporting/emv_application_selection），与上述编译错误一一对应；另有 `EMV_CFG_MAX_CANDIDATES` 在 `emv_application_selection.c:112` 同样因未 include `emvdbg.h` 而报未定义（`(a)` 范围，同 P0-1 根因）。

## 已核对无明显问题的项

- `EMV_SetOutCome` 的 5 个置位参数到 PresentationFlag 的映射：`ui_on_outcome→0x80、ui_on_restart→0x40、data_record_pre→0x20、disc_data_pre→0x10、receipt→0x08`，与 Book B 的 Presentation Information 位序（b8→b5、b4）一致（依据规范常识，未逐字比对 PDF）。**位映射本身没问题，问题在"置位后数据没写"（P2-5）。**
- `EMV_Result*` 系列的状态前置校验：`SelCandidates`（两种候选状态）、`SelAccountType`、`CheckID`、`OnlinePIN`、`OfflinePIN`（含 LAST）、`BypassPIN`、`ForceOnline`、`Online`、`IssuerReferral` 都会在 `status` 不匹配时返回对应 `*NOTREQ` 错误，未发现"状态机跳过一步也能回填"的漏洞。
- `EMV_ResultOnlinePIN/OfflinePIN` 的缓冲边界：网上 PIN `nDataLen<=256`（`emv.c:1465`）、离线 PIN `4<=nDataLen<=12` 且 `offline_pin[12+1]` 的 `[nDataLen]='\0'` 正好落在 13 字节内（`emv.c:1487-1490`），未越界；`EMV_OPER_OK` 之外的结果不写 pin（`emv.c:1463,1484`），符合头文件"仅 OK 时数据有效"的约定。
- `EMV_Init` 的缓冲切分：`len >= sizeof(EMV_CONTEXT)+2048` 检查、4 字节对齐、`candidates` 末尾越界检查（`emv.c:994-1059`）都在；`preprocess_result` 的分配长度按 `sizeof(EMV_PREPROCESS_RESULT)` 计算，与写入端 `preprocess_result+i` 一致（`emv.c:1039-1041,1304`）。`termapps_num` 的范围检查存在但**没有立即返回**（见 P2-13）。
- `EMV_Process` 的状态范围守卫（`status<EMV_STA_IDLE || status>=EMV_COMPLETED`）与 `card_type==EMV_CARD_UNKNOWN` 守卫（`emv.c:1357-1373`）正确，非法状态不会进入 `emv_k_interface.emv_process()` 之外的分支。
- `EMV_PreProcess` 的金额为 0/限额比较逻辑（`trans_amount==0` 的 zero_amt 分支、`cl_trans_limit` 用 `>=`、`cl_floor_limit`/`term_floor_limit` 用 `>`、`term_floor_limit` 用二进制 `emv_hex2dec`、`cl_floor_limit` 用 BCD `emv_bcd2dec`）与 Book B 预处理条款的意图一致，未发现比较方向错误；问题只在 `cvm_limit` 传参（P0-3）与 TTQ 置位（P1-1）。
- `outcome_try_again_without_restart`（`emvoutcome.c:214-248`）：清零缓冲、`g_discretionary_data_len=i`、`appdata_save(TAG_FF8106)` 的顺序正确，可作为其余 outcome 函数的模板。
- `msg_read_card_ok`（`emvoutcome.c:453-483`）：语言偏好用 `sizeof(lang_pre)` 裁剪，正确；`appdata_find` 失败时靠短路求值避免使用未初始化 `len`。
- `emv_bcd2hex`（`emv.c:439-467`）对每个 nibble 做 `>9` 校验，非法 BCD 返回 -1，没有越界；`emv_str2bcd`/`emv_bcd2str` 在工程内调用点长度受控（`emvicc.c:160-183`，调用方缓冲由 APDU 上限约束）。
- `api_sprintf` 与 `EMV_GetData` 的 `TAG_KERNEL_VERSION` 正常路径（status 非 IDLE）：返回 `EMV_VERSION` 并写长度，逻辑正确（异常路径见 P2-3）。
- `emvapi.c` 的 `g_emvapi_messageinx` 生命周期：`emv_initialize` 复位 -1、`emv_process2` 消费后置 -1（`emvapi.c:188,530-535`），未发现"消息泄漏到下一笔"的路径——除非 L3 混用 `emv_process`（见第 2 节）。
- `EMV_PORTING.get_trans_date_time` 的调用约定：内核在 `emv_date_compare` 里传 `time=NULL`，弱实现 `emvspi.c:1083` 做了 NULL 判断，契约匹配。

## 审查局限

1. 工程无 Makefile/CMake、本机无 IAR，无法做目标构建。为确认"能否编译"这类事实，额外用 `gcc -std=c99 -fsyntax-only`（只做语法/语义检查，不生成目标码、不修改任何工程文件）对 13 个源文件做了交叉校验，结论仍以源码行号为准。`build/iar/Debug/Obj` 下缺失的 5 个 `.o` 是独立旁证。
2. 源码中文注释已被损坏（UTF-8 文件内混 U+FFFD、部分文件整体 GBK），本报告**不依赖注释**下结论；仅用 UTF-8 镜像（`_review_utf8/`）和同源干净副本帮助理解注释语义，未把镜像路径/行号作为证据。
3. `paypass_init/paypass_preprocess/paypass_process`、`contact_init/contact_process` 以及 `JNISendOutcome/JNISendUIReq` 的实现不在本工程源码内（应由外部内核库/上层提供），其行为无法核对；涉及"内核被反复复位""空函数指针""链接失败"的结论都按"若未提供则……"表述，需 Lead 结合构建链路确认。
4. `E:\Work\SVN Project\emv_doc` 下的规范 PDF 未能在本次会话内提取为文本，Entry Point 相关结论依据 EMV Book B v2.10 / Book 3 的规范常识并在条目中注明；`OUTCOME_START_NA=0xF0`、`TTQ` 具体位的规范条文建议 Lead 用 PDF 做最终核对。
5. 本报告只审查 `emv.c/emvapi.c/emvoutcome.c`；`emvspi.c/emvporting.c/emv_application_selection.c/emv_combination_Selection.c/emv_selec_kernel.c/emvtlv.c/emvicc.c` 中的问题只在与本模块契约相关时记录，权威结论以对应审查员的报告为准。
