# 应用选择与 Entry Point 组合/内核选择 审查

审查范围（只读静态审查）：
- `src/emv_application_selection.c`（1569 行，重点）
- `src/emv_combination_Selection.c`（135 行）
- `src/emv_selec_kernel.c`（82 行）

参考契约（只读核对，不作为本工程证据）：`src/inc/emv_type.h`、`src/inc/emvi.h`、`src/inc/emvdbg.h`、`src/inc/emv_tag.h`、`src/emv.c`、`src/emvtlv.c`、`src/emvicc.c`、`src/emvbuf.c`、`src/emvspi.c`。

## 概述

本模块位于 Entry Point 最前端：`emv_build_candidate_list()` 选择 `2PAY.SYS.DDF01`（PPSE），解析 FCI（6F/84/A5/BF0C/61）、按终端 AID 列表做精确/部分匹配生成候选列表，并在 PPSE 返回 SPI 相关数据（9F3E/9F3F/8B）时执行 SPI 预处理（SDOL 组包、SPI 响应解析、二次构建候选），随后 `emv_combination_Selection()` 依据候选的 9F2A/AID 选择内核，`emv_select_kernel()` 完成 PayPass 内核初始化并挂接 `emv_k_interface.emv_process`。

代码结构清晰，并与本工程"仅实现 PayPass(QMSDC) 单内核"的定位大体一致；但卡数据驱动的路径存在多处**缺少长度/容量校验**（最严重的是 SDOL 组包把卡提供的 DOL 长度直接展开到 262 字节栈缓冲，以及候选列表无上限检查）、一处明显的**硬编码覆盖导致内核识别/防护整体失效**，以及大量当前构建不可达的接触式/多内核死代码（其内部亦有真实缺陷）。整体质量中等偏下，SPI 路径是本模块风险最集中的区域。

统计：19 条（P0×1、P1×4、P2×8、P3×6）。

## 问题清单（P0→P3）

### [P0] 卡提供的 SDOL 在 sdol_process 中被无界展开，造成栈缓冲区溢出（卡可控内存破坏）

- 位置：`src/emv_application_selection.c:389`、`src/emv_application_selection.c:436`（调用点）；根因 `src/emvtlv.c:1541-1632`（被调函数不检查 `val_max_len`）
- 证据：

```c
/* emv_application_selection.c:386-394 (flag 2) / 433-442 (flag 3) */
sdol = appdata_find(TAG_9F3F_SDOL, &len);
if(sdol && len > 0)
{
    pack_len = emvdol_packet(sdol, len, pack, sizeof(pack));
    if(pack_len < 0)
    ...
```

```c
/* emvtlv.c:1541-1632：emvdol_packet 只在入口 memset(val,0,val_max_len)，
   之后完全不再使用 val_max_len，逐条 DOL 直接写入 */
int32_t emvdol_packet(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t val_max_len)
{
    ...
    val_len = 0;
    emv_memset(val, 0x00, val_max_len);
    for (i = 0; i < dollen;)
    {
        ...
        reqlen = dol[i++];                 /* 卡数据，最大 255 */
        ...
        if (fmt || pdata == NULL)
            emv_memset(val + val_len, 0x00, reqlen);   /* 无边界检查 */
        ...
        val_len += reqlen;                              /* 无上限 */
    }
    return val_len;
}
```

```c
/* sdol_process 的 262 字节栈缓冲，emvi.h:45 EMV_CFG_ICC_SEND_MAXLEN=262 */
uint8_t temp[EMV_CFG_ICC_SEND_MAXLEN]={0};
uint8_t pack[EMV_CFG_ICC_SEND_MAXLEN]={0};
```

- 影响：`9F3F` 由卡在 PPSE FCI 中提供（`deal_ppse_resp` 884-887 行把卡响应的 9F3E/9F3F/8B 存入 appdata，标签库上限仅 252 字节，未限制"各 DOL 项请求长度之和"）。一张恶意/异常卡可让 SDOL 含数十项、每项要求 255 字节，`emv_memset/emv_memcpy` 会向 `pack[262]` 及其后续栈帧写入上万字节，直接破坏返回地址/相邻局部变量，属于可由卡触发的栈溢出（P0）。触发条件：ICS 打开 PayPass SPI（`MISC_Paypass_Support_SPI`/`MISC_Paypass_SPI_ACTIVE_FLAG`，`spi_process` 552 行）且卡在 PPSE 返回 9F3F。
- 建议：在 `emvdol_packet` 内每次写入前检查 `val_len + reqlen <= val_max_len`，溢出返回负值；`sdol_process` 在调用后仍应校验 `0 <= pack_len <= sizeof(pack) - 3`。该根因在 `emvtlv.c`，请 Lead 归入 TLV/数据层模块一并修复（跨模块）。

### [P1] 候选列表无容量上限，候选数可超过 EMV_CFG_MAX_CANDIDATES(12) 造成越界写

- 位置：`src/emv_application_selection.c:116-119`（`candidate_add` 无边界）、`:293`、`:806`、`:1014`（三处调用）、`:112`（`candidate_clr` 只清 12 项）；容量定义 `src/inc/emvdbg.h:20`；缓冲区分配 `src/emv.c:1043-1050`
- 证据：

```c
/* emv_application_selection.c:116-119 */
static void candidate_add(EMV_CANDIDATE* candidate)
{
    emv_memcpy(&g_emv_cnxt->candidates[g_emv_cnxt->candidates_num++], candidate, sizeof(EMV_CANDIDATE));
}

/* emvdbg.h:20 */
#define EMV_CFG_MAX_CANDIDATES			12					//maxium of candidate list

/* emv.c:1043-1050 */
g_emv_cnxt->candidates = (EMV_CANDIDATE*)current_ptr;
candidates_max_size = sizeof(EMV_CANDIDATE) * EMV_CFG_MAX_CANDIDATES;
...
```

- 影响：`tlvs[40]` 单次解码最多容纳 40 个 TLV 对象，`6F/84/A5/BF0C` 占 4 个，每个 61 模板最小只需 `61+4F` 2 个对象，因此**一张卡最多可在一次 PPSE（或 SPI）响应中提供 18 个匹配候选**；卡甚至可以把同一 AID 重复写 13 次，在终端只配置 1 个 AID 时也能触发。第 13 个候选起即写入 `candidates + 12*sizeof(EMV_CANDIDATE)` 之后：普通配置下该区域是 `weak_alloc_buffer()` 静态全局缓冲（`src/emvspi.c:895-904`）中未被使用的尾部，不一定立即崩溃；但已越过 `EMV_CFG_MAX_CANDIDATES` 的声明容量，且 `EMV_GetData(TAG_CANDIDATE_LIST)`（`src/emv.c:1615-1624`）会把 `candidates_num * sizeof(EMV_CANDIDATE)` 交给上层，**上层若按 12 个候选分配接收缓冲即是堆越界写（跨模块）**。若终端 AID 配置较多（termapps_num 较大，静态缓冲尾部余量变小），越界可写出 14KB 全局缓冲。
- 建议：`candidate_add` 增加 `if (g_emv_cnxt->candidates_num >= EMV_CFG_MAX_CANDIDATES) return;`（或返回错误并终止候选构建）；SPI 响应路径更应在 `deal_spi_resp` 内对候选数做显式上限检查。

### [P1] sdol_process flag=3 追加 SPI 数据时容量判断错误：1 字节栈越界写 + 长度字节加法溢出

- 位置：`src/emv_application_selection.c:480-500`（判断与写入）、`:444-476`（长度编码）
- 证据：

```c
/* emv_application_selection.c:480-500 */
if((sizeof(pack) - pack_len) > (poi_info_value_len+3))
{
    i = pack_len;
    emv_memcpy(pack+i, "\x00\x01\x02", 3);   /* 实际总共追加 5 字节 */
    i += 3;
    emv_memcpy(pack+i, term_category, 2);
    i += 2;
    ...
    if(two_byte_len_flag)
    {
        pack[2] += 5;      /* 长度字节直接相加，无溢出检查 */
    }
    else
    {
        pack[1] += 5;
    }
```

- 影响：flag=3 分支中 `poi_info_value_len` 恒为 0（`spi_process` 591 行置 0 后不再赋值），条件实际是 `262 - pack_len > 3`，即允许 `pack_len == 258`；此时先在 258 处写 3 字节、再在 261 处写 2 字节，最后一字节写到 `pack[262]`（数组下标 0..261），**1 字节栈越界写**。可触发组合：卡 SDOL 打包长度 n=255（走两字节长度头，`pack_len=3+255=258`）。同时 `pack[2]+=5` 把 0xFF 变成 0x04（uint8 回绕），发出的 APDU 长度字节与数据长度不符，卡会返回错误状态。n=251..255 都会出现长度字节回绕。
- 建议：容量判断改为 `sizeof(pack) - pack_len >= 5`；长度字节先计算 `n+5` 再判断 `<= 255`（超过则改用 0x82 两字节长度格式或返回错误）。

### [P1] 9F29 扩展选择处理错误/缺失：SPI 路径用 TLV 下标索引 termapps，主 PPSE 路径完全未处理

- 位置：`src/emv_application_selection.c:224-234`（SPI 路径，索引错误）、`:835-1024`（`deal_ppse_resp`，无 9F29 处理）
- 证据：

```c
/* deal_spi_resp：i 是 tlvs[] 的下标（61 模板位置），j 才是 termapps 下标 */
for (i = pos_fci_issuer + 1; i <= tlvs[pos_fci_issuer].childnum + pos_fci_issuer; i++)
{
    if (tlvs[i].tagname == TAG_0061_APPLICATION_TEMPLATE)
    {
        ...
        for (j = 0; j < g_emv_cnxt->termapps_num; j++)
        {
            ...
            if ((pos_extselect = emvtlv_get(i, TAG_9F29_EXTENED_SELECTION, tlvs, SEARCH_ONLY_SON)))
            {
                if(g_emv_cnxt->termapps[i].rsv)      /* 应为 termapps[j].rsv */
                {
                    if(candidate.aid_len + tlvs[pos_extselect].valuelen <= 16)
                    {
                        emv_memcpy(candidate.aid + candidate.aid_len, tlvs[pos_extselect].pvalue, tlvs[pos_extselect].valuelen);
                        candidate.aid_len += tlvs[pos_extselect].valuelen;
                    }
                }
            }
```

- 影响：`termapps[i].rsv` 的 `i` 是 `tlvs[]` 下标（典型值 5~40），而 `termapps` 数组只有 `termapps_num` 项（通常远小于此），属于**越界读**（读出 tlvbuffer 内容），随卡提供的 TLV 数量/顺序变化：可能把不该扩展的 AID 追加 9F29 数据（最终 SELECT 用错误的 ADF 名 → 交易失败），也可能在终端确实配置支持扩展选择时漏加（卡要求扩展选择 → 选卡失败）。同类字段用错在 `deal_pse_resp:750` 也存在（见 P2 死代码项）。另外主路径 `deal_ppse_resp` 只取 9F2A，**完全没有 9F29 处理**（全文件 grep 仅 224 行一处），如果卡在 PPSE FCI 的 61 模板中返回 9F29，本实现不会构造扩展 ADF 名，与同源参考实现的 PPSE 分支（仅作语义参考，不作证据）不一致。
- 建议：`termapps[i]` → `termapps[j]`；在 `deal_ppse_resp` 的 61 模板解析处补上 9F29 的追加逻辑（同一套 `aid_len + valuelen <= 16` 校验），并明确 9F29 的合法来源（PPSE / SPI 响应）后统一处理。

### [P1] resp_len / rlen 未初始化即参与 TLV 解析，卡返回 9000 无数据时按垃圾长度解析

- 位置：`src/emv_application_selection.c:368`（声明）、`:520`、`:533`（使用）；`:1270`（声明）、`:1275`、`:1298`（使用）
- 证据：

```c
/* sdol_process */
int32_t pack_len=0,ret,resp_len;            /* resp_len 未初始化 */
...
ret = emvicc_spi(pack, pack_len, resp, &resp_len);
...
ret = parse_spi_resp(resp, resp_len, ret);  /* 若 emvicc_spi 未写 resp_len 则用栈垃圾 */
```

```c
/* select_ddf */
int32_t ret,rlen;                           /* rlen 未初始化 */
uint8_t resp[EMV_CFG_ICC_RECV_MAXLEN];
ret = emvicc_select(fn, fnlen, false, resp, &rlen);
...
ret = parse_ppse_resp(resp, rlen, ret);
```

```c
/* emvicc.c:350-359：只有 rlen>0 才回写 *outlen */
if (rlen > 0)
{
    if (output != NULL)  emv_memcpy(output, recv, rlen);
    if (outlen != NULL)  *outlen = rlen;
}
return sw;
```

- 影响：卡对 SELECT/SPI 命令只返回 `9000` 而无响应数据（`rlen==0`）时，`icccommand` 不回写 `*outlen`，`resp_len`/`rlen` 保持未初始化的栈值，随后 `emvtlv_decode(resp, resp_len, ...)` 按垃圾长度解析 512 字节栈缓冲（`resp` 本身也未初始化）——可能越界读栈/堆并崩溃，或解析出随机的 FCI 结构。`parse_select_resp` 有 `resp_len<0` 检查（1128 行），说明作者意识到长度合法性，但两个调用点都没做。
- 建议：声明时初始化为 0（`int32_t resp_len = 0, rlen = 0;`），并在 `0x9000 == sw && rlen <= 0` 时按"FCI 缺失"错误处理（S52.6/S54.6 分支）。

### [P2] contactless_type 被无条件覆盖为 QMSDC，内核识别结果与 UNKNOWN 防护整体失效；非 QMSDC 分支无实现

- 位置：`src/emv_combination_Selection.c:123`（覆盖）、`:35-119`（被覆盖的识别逻辑）、`:125-128`（失效的 UNKNOWN 防护）、`:130`；`src/emv_selec_kernel.c:37-75`、`:60`
- 证据：

```c
/* emv_combination_Selection.c:118-130 */
		}
	}

	tmTRACE("entrypoint-info: flag9");

	g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QMSDC;   /* 无条件覆盖 */

	if(g_emv_cnxt->contactless_type == EMV_CONTACTLESSTYPE_UNKNOWN)  /* 永不成立 */
	{
		return EMV_ERR;
	}

	ret = emv_select_kernel(g_emv_cnxt->card_type, g_emv_cnxt->contactless_type);
```

```c
/* emv_selec_kernel.c:37-75：只有 QMSDC 分支有实现，其余为空的 break（返回 EMV_OK 但不设置 emv_process） */
switch(emv_core)
{
    case EMV_CONTACTLESSTYPE_QPBOC: break;
    case EMV_CONTACTLESSTYPE_QVSDC: break;
    case EMV_CONTACTLESSTYPE_QMSDC:
        ret = paypass_init(); ...
        emv_k_interface.emv_process = paypass_process;
        break;
    ...
    default: ret = EMV_ERR; break;
}
```

- 影响：1) 35-119 行依据 9F2A kernel_id 或 AID 识别 VISA/AMEX/JCB/DISV/PBOC/PURE 的逻辑全部成为死代码，`EMV_CONTACTLESSTYPE_UNKNOWN` 保护不可达；2) 若终端 AID 配置中含非 PayPass 应用（AID 列表由外部参数文件配置，本工程不限制），该应用会以 QMSDC 内核处理，或由 PayPass 内核拒绝，行为不可预期；3) 一旦有人去掉这行硬编码，`emv_select_kernel` 对非 QMSDC 类型返回 `EMV_OK` 却不给 `emv_k_interface.emv_process` 赋值，`emv.c:1403` 会调用上一次交易遗留/为 NULL 的函数指针（崩溃或跑错内核）。
- 建议：若产品确定只支持 PayPass：删除 35-119 行的识别逻辑和 UNKNOWN 判断，并在 `emv_select_kernel` 用 `default: ret = EMV_ERR` 显式拒绝非 QMSDC，避免"能识别但不使用"的误导；若需多内核，删除 123 行覆盖并在各 `break` 分支设置对应的 `emv_process`。

### [P2] deal_ppse_resp 候选构建循环上界多 1，越界读 tlvs[40]（栈数组）

- 位置：`src/emv_application_selection.c:906`（另 `:841` 定义 `EMV_TLVOBJ tlvs[40]`）
- 证据：

```c
/* emv_application_selection.c:906 */
for (i = pos_fci_issuer + 1; i <= tlvs[pos_fci_issuer].childnum + pos_fci_issuer + 1; i++)
```

- 影响：`childnum` 是该节点所有后代的个数，合法后代下标为 `pos+1 .. pos+childnum`，上界应为 `<= pos + childnum`。卡可构造 40 个 TLV 对象刚好填满 `tlvs[40]`（如 BF0C 下 35 个直接子元素），此时候选循环读到 `tlvs[40]`（数组越界 1 项）；若越界处的 `tagname` 恰好等于 `0x61` 且 `valuelen>0`，会继续对越界对象调用 `emvtlv_get`/`i += tlvs[i].childnum`，读取量取决于栈上垃圾值，存在崩溃风险。同函数 877 行的同类循环使用 `<`，是正确的，可对照。
- 建议：改为 `i <= tlvs[pos_fci_issuer].childnum + pos_fci_issuer`。

### [P2] 同一卡应用匹配多个终端 AID 时被重复加入候选；去重逻辑整段被注释

- 位置：`src/emv_application_selection.c:1014-1015`（`continue` 而非 `break`）、`:1497-1523`（去重代码被注释）
- 证据：

```c
/* deal_ppse_resp：内层 for (j...) 中命中后 candidate_add，然后 continue 继续匹配下一个终端 AID */
                    candidate_add(&candidate);
                    continue;
```

```c
/* emv_build_candidate_list：去重逻辑整段注释 */
//	 for (i = 0; i < g_emv_cnxt->candidates_num; i++)
//	 {
//	 	j = i + 1;
...
```

- 影响：`app_cmp` 支持部分匹配（`asi==0` 时只要卡 AID 以终端 AID 为前缀即匹配），一份常见的终端配置（同时配 `A0000000041010`、`A0000000043060` 及部分 AID `A000000004`）会让同一卡应用生成多条同 AID 同 kernel_id 的候选：候选列表重复、`candidates_num` 虚高（叠加 P1 容量问题更快越界）、上层展示/选择出现重复项，违反候选列表去重要求。`deal_pse_resp:809` 用的是 `break`，可对照。
- 建议：命中后改为 `break`（每个 61 模板最多产生一个候选），并恢复/重写去重逻辑（按 aid_len+aid+kernel_id 去重）。

### [P2] spi_process 解析 POI Information(8B) 时缺少边界检查，越界读 appdata

- 位置：`src/emv_application_selection.c:586-604`（`:588`、`:595`、`:602-603`）
- 证据：

```c
i = 0;
while (i < poi_info_len)
{
    if(!emv_memcmp(poi_info+i, "\x00\x01", 2) && (poi_info[i+2] == 0x02))  /* i+2 未判界 */
    {
        ...
        i += 3;
        emv_memcpy(term_category, poi_info+i, 2);   /* i+1 未判界 */
        i += 2;
        ...
    }
    else
    {
        i += 2;
        poi_info_value_len = poi_info[i++];         /* i 未判界 */
        i += poi_info_value_len;
        continue;
    }
```

- 影响：8B 同样来自卡响应（`deal_ppse_resp:884-887` 保存），若卡给出的 8B 值在最后不足 3 字节（如仅 `00 01 02`），`poi_info[i+2]`/`poi_info[i+1]` 会读出该值之后 appdata 缓冲区内容（相邻哈希表头），使终端用错误/垃圾的 Terminal Category 参与 SPI 组包（`sdol_process` flag 1/3）；读取范围有限（在 10KB appdata 缓冲内），一般不会崩溃，但属于卡可控的数据越界读。
- 建议：循环内先判 `i + 3 <= poi_info_len`（ID 2 字节 + L 1 字节），解析 L 后判 `i + L <= poi_info_len`，越界即按格式错误返回/终止 SPI。

### [P2] 候选选择流程失效：构建后强制 index=0，从不返回 EMV_REQ_SEL_CANDIDATES，EMV_ResultSelCandidates 恒失败

- 位置：`src/emv_application_selection.c:1552-1554`（强制 0）；`src/emv.c:1389-1391`、`:1417-1427`
- 证据：

```c
/* emv_build_candidate_list 结尾 */
	g_emv_cnxt->req_sel_candidates_result = 0;

    return rNXT(EMV_STA_CANDIDATES_BUILT);
```

```c
/* emv.c:1417-1427：只有状态为 EMV_REQ_SEL_CANDIDATES(_AGAIN) 才允许设置，而全工程无任何地方把状态置为该值 */
int32_t EMV_ResultSelCandidates(int32_t eResult)
{
    ...
    if (g_emv_cnxt->status != EMV_REQ_SEL_CANDIDATES && g_emv_cnxt->status != EMV_REQ_SEL_CANDIDATES_AGAIN)
        return rERR(EMV_SELCANDIDATENOTREQ);
    g_emv_cnxt->req_sel_candidates_result = eResult;
```

- 影响：存在多个候选时，Entry Point 直接选最高优先级（index 0），`emv_process2` 中 `EMV_REQ_SEL_CANDIDATES → EMV_CANDIDATE_LIST` 的消息分支（`src/emvapi.c:441-444`）永远不会走到，`emv_set_candidate_list_result()`（`emvapi.c:547-549`）永远返回 `EMV_SELCANDIDATENOTREQ`。若产品要求"多候选由应用/持卡人选择"，该功能实际缺失；若确定"接触式 PayPass 自动选最高优先级"，应删除这条失效 API 及状态分支，避免调用方误用。
- 建议：按产品行为二选一：实现"构建完成后若有多个候选则置 `EMV_REQ_SEL_CANDIDATES` 返回应用选择"，或删除/明确标注该 API 不支持。

### [P2] emv_combination_Selection 用未校验的候选索引访问候选数组

- 位置：`src/emv_combination_Selection.c:37-41`（读取）；根因 `src/emv.c:1424`（不校验范围）
- 证据：

```c
/* emv_combination_Selection.c:37-42 */
eTRACE("...#%d", g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len);
if(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len != 0)
{
    ...
    if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_MASTERCARD, ...))
```

```c
/* emv.c:1421-1426：不检查 eResult 是否在 0..candidates_num-1 */
g_emv_cnxt->req_sel_candidates_result = eResult;
```

- 影响：`req_sel_candidates_result` 是外部可写值（`EMV_ResultSelCandidates`/`emv_set_candidate_list_result`，以及内核可能返回 `EMV_REQ_SEL_CANDIDATES` 之后由应用设置）。传负值或 ≥`candidates_num`（尤其 ≥12 时）会越界读候选数组（读出缓冲区外的栈/静态区数据），进而按垃圾 kernel_id 选择内核；负值会读到 `g_emv_cnxt` 之前的内存。属健壮性/接口校验缺陷（跨模块：校验应加在 `emv.c:1417`）。
- 建议：`EMV_ResultSelCandidates` 中增加 `if (eResult < 0 || eResult >= g_emv_cnxt->candidates_num) return rERR(...)`；本文件使用前也可再判一次。

### [P2] PSE/接触式选择路径整体不可达（当前构建），若启用则存在死循环等多个真实缺陷

- 位置：`src/emv_application_selection.c:1431-1436`（`select_use_pse` 无调用者）、`:1322`（`select_use_aid_list` 无调用者）；内部缺陷 `:666`、`:703`、`:717`、`:750`、`:779`
- 证据：

```c
/* 唯一入口 emv_build_candidate_list 只调用 PPSE（1444 行），select_use_pse / select_use_aid_list 为 static 且全工程无调用者 */
static int32_t select_use_ppse(void) { candidate_clr(); return select_ddf((uint8_t*)DF_PPSE, emv_strlen(DF_PPSE), NO_9F11); }
static int32_t select_use_pse(void)  { candidate_clr(); return select_ddf((uint8_t*)DF_PSE,  emv_strlen(DF_PSE),  NO_9F11); }
```

```c
/* deal_pse_resp：recno 未初始化且 while(1) 中从不递增 */
uint8_t sfi, recno;
...
while (1)
{
    sw = emvicc_read_record(sfi, recno, resp, &resp_len);
    if(0x9000 == sw) { ... }
    else if(0x6A83 == sw) { break; }
    else { return rERR(AS_RDRECERR); }
}
```

```c
/* 717 行把 primitive 的 SFI(88) 位置当父节点，其 childnum 为 -1 → 循环体永一次都不执行 */
for (i = pos_sfi + 1; i <= tlvs[pos_sfi].childnum + pos_sfi; i++)
...
/* 750 行：i 是 TLV 下标，j 才是 termapps 下标 */
if (g_emv_cnxt->termapps[i].enable == 0)
...
/* 779 行：judge_icti(NO_9F11) 恒为 0 → 9F12 优先名分支恒为假 */
if ((pos_perf_name = emvtlv_get(i, TAG_9F12_APPLICATION_PREFERRED_NAME, tlvs, SEARCH_ONLY_SON)) && judge_icti(NO_9F11))
```

- 影响：当前构建中接触式被 `EMV_PreProcess` 禁用（`src/emv.c:1170-1171` 只置 `EMV_CARD_CONTACTLESS`，`EMV_SetCardType` 1327-1328 行据此拒绝 `EMV_CARD_CONTACT`），因此上述代码不可达，不是现网故障；但一旦恢复接触式支持：`deal_pse_resp` 会因 `recno` 未初始化（且不递增）对同一垃圾记录号无限循环（卡返回 9000 时终端挂死）；即使修好 recno，717 行的错误父节点使 AEF 记录中的 61 模板永远解析不到，PSE 目录构建必然为空；750 行越界读 termapps；9F12 被忽略。这组缺陷是明确的"地雷"，应随接触式功能恢复一并修复。
- 建议：要么删除不可达的 PSE/接触式代码（本工程当前定位），要么按上述 5 处逐一修复并补充"卡返回 9000 的循环必须以 6A83/记录号上限退出"的保护。

### [P2] app_cmp 未校验终端 AID 的最小长度，配置异常时退化为过宽匹配

- 位置：`src/emv_application_selection.c:77-89`；上游 `src/emvporting.c:1194-1201`（只限制 AID ≤16，未限制 ≥5）
- 证据：

```c
/* app_cmp */
if (caid_len >= 5 && caid_len <= 16)
{
    if(g_emv_cnxt->trans_type == termapp->trans_type || termapp->trans_type == 0xFF)
    {
        if ((caid_len == termapp->aid_len) || (caid_len > termapp->aid_len && termapp->asi == 0))
        {
            return emv_memcmp(caid, termapp->aid, termapp->aid_len);
        }
    }
}
```

- 影响：只校验卡侧 AID 长度 5..16，未校验 `termapp->aid_len`。若参数文件中某条 AID 长度 <5（AID 字段超 16 字节时 `enable` 保持 0、`aid_len` 为 0，见 `emvporting.c:1192-1201`），则 `aid_len==0` 时 `emv_memcmp(...,0)` 恒为 0，该条配置会**匹配任意卡片 AID**；`aid_len` 为 1~4 时变成极宽的前缀匹配。属于错误配置下的静默放行，可能导致未授权应用被选中。
- 建议：`app_cmp` 增加 `if (termapp->aid_len < 5 || termapp->aid_len > 16) return -1;`；`emvporting` 加载时对 AID 长度非法项直接丢弃而不是保留空项。

### [P3] 87（Application Priority Indicator）未校验长度，零长度时读 1 字节越界

- 位置：`src/emv_application_selection.c:936-938`（PPSE 路径，可达）、`:209-211`（SPI 路径）
- 证据：

```c
if ((pos_api = emvtlv_get(i, TAG_0087_API, tlvs, SEARCH_ONLY_SON)))
{
    candidate.priority = *(tlvs[pos_api].pvalue);   /* 未检查 valuelen==1 */
}
```

- 影响：卡返回长度为 0 的 87 时，`pvalue` 指向数据末尾，读取 1 字节越界（仍在 512 字节 resp 缓冲内，不会崩溃），`priority` 为相邻字节内容，影响候选优先级排序。同文件 `deal_pse_resp:796-799` 有 `valuelen != 1` 检查，说明这是遗漏。
- 建议：加 `if (tlvs[pos_api].valuelen != 1) { /* 按 FCI 格式错误处理或忽略 */ }`。

### [P3] sdol_process 对 SDOL 打包结果 >255 字节时的长度编码截断

- 位置：`src/emv_application_selection.c:396-403`（flag 2）、`:444-451`（flag 3）
- 证据：

```c
if(pack_len > 127)
{
    two_byte_len_flag = true;
    if(sizeof(pack) - pack_len >= 3)
    {
        temp[i++] = 0x83;
        temp[i++] = 0x81;
        temp[i++] = (uint8_t)pack_len;   /* pack_len 可达 262，截断成 0..6 */
    }
```

- 影响：`emvdol_packet` 返回值可到 259，`(uint8_t)pack_len` 截断后 TLV 长度字节与实际数据长度不符，发出的 SPI APDU 格式非法（卡侧 6700/6A81）。触发需 SDOL 打包长度 >255（卡可构造）。
- 建议：`pack_len > 255` 时使用 0x82 两字节长度编码（或限制 SDOL 打包长度 ≤255 并对超长返回错误）。

### [P3] select_ddf 的 PPSE 判断用 DF_PSE 长度，且 icti 形参未使用

- 位置：`src/emv_application_selection.c:1278`、`:1268`、`:1428`
- 证据：

```c
int32_t select_ddf(uint8_t *fn, int32_t fnlen, uint8_t icti)   /* icti 未使用（NO_9F11 传入后被丢弃） */
...
if(!emv_memcmp(fn, DF_PPSE, emv_strlen(DF_PSE)))               /* 用 DF_PSE 的长度比较 DF_PPSE */
```

- 影响：`"1PAY.SYS.DDF01"` 与 `"2PAY.SYS.DDF01"` 长度同为 14，当前无功能影响；但长度常量写错使维护者在修改 DF 名格式时易引入越界/漏比较。`icti` 参数说明原先设计允许调用方指定 ICTI（用于 9F12 选择），现在被 `judge_icti(NO_9F11)` 写死（见 P2 死代码项），属接口与实现脱节。
- 建议：改为 `emv_strlen(DF_PPSE)`；删除未使用的 `icti` 形参或在 `deal_ppse_resp` 中使用它。

### [P3] candidate.rsv[0] 语义不一致且无消费者；candidate_del 未被使用

- 位置：`src/emv_application_selection.c:292`、`:805`、`:1008`、`:1257`、`:121-132`
- 证据：

```c
/* deal_spi_resp：rsv[0] 存 TLV 下标 i */
candidate.rsv[0] = i;
/* deal_ppse_resp / deal_pse_resp：rsv[0] 存 termapps 下标 j */
candidate.rsv[0] = j;
/* parse_select_resp：rsv[0] 固定 0 */
candidate.rsv[0] = 0;
```

- 影响：字段含义在不同路径不同（`emv_type.h:595` 注释说明 rsv[0] 本应表示"终端应用列表索引"，供加载 AID 参数用）。当前本工程内无读取方（`rsv[` 只在上述 4 处写入），故不产生现网故障；一旦内核/后续代码按注释语义读取，SPI 路径会取到错误的应用参数索引。
- 建议：统一为 `j`（终端应用下标）并补上消费者，或在结构注释中标注"未使用"；删除未被引用的 `candidate_del()`。

### [P3] deal_spi_resp 忽略 9F0A(ASRPD)；parse_select_resp 的 BF0C 子元素查重循环上界漏 1（当前不可达）

- 位置：`src/emv_application_selection.c:236-239`、`:1184`
- 证据：

```c
if ((pos_temp = emvtlv_get(i, TAG_9F0A_ASRPD, tlvs, SEARCH_ONLY_SON)))
{
    //TODO: add process of 9F0A
}
```

```c
/* 1184：应为 i <= tlvs[pos1].childnum + pos1，当前漏掉最后一个直接子元素，查重不完整 */
for (i = pos1 + 1; i < tlvs[pos1].childnum + pos1; i ++)
```

- 影响：9F0A 未落库/未参与后续处理（PayPass 若需要 ASRPD 会在内核侧缺失）；查重漏检最后一对重复标签，属格式校验不完整。
- 建议：实现 9F0A 的保存/使用（按需）；修正 1184 行条件。

### [P3] 死代码与未使用变量

- 位置：`src/emv_combination_Selection.c:25-31`、`src/emv_selec_kernel.c:15-20`、`src/emv_application_selection.c:1322`、`:1431`
- 证据：

```c
/* emv_combination_Selection.c:25-31：err/len/choice/buf/again/account_type/ptr/pIAD 中多数未使用，buf 占 512 字节栈 */
int32_t ret, len;
int32_t choice;
uint8_t  buf[EMV_CFG_ICC_RECV_MAXLEN];
int32_t again = false;
...
uint8_t *ptr;
uint8_t pIAD[128] = {0};
```

```c
/* emv_selec_kernel.c:15-20：EMV_ANDROID_SO 分支为空 switch，当前 emv.h 中该宏为 0，不参与编译 */
#if EMV_ANDROID_SO
	switch(emv_core)
	{
	}
```

- 影响：可维护性/可读性问题，并浪费栈空间（嵌入式终端栈通常紧张）。`select_use_aid_list`、`parse_select_resp`、`deal_pse_resp`、`parse_pse_resp`、`candidate_del` 均为不可达代码，掩盖了模块真实支持范围。
- 建议：清理未使用变量与不可达函数（或按产品规划补齐接触式支持）；`#if EMV_ANDROID_SO` 空分支应加 `#error`/注释说明该配置未实现。

## 已核对无明显问题的项

- FCI 顶层结构：`6F → 84 / A5 → BF0C` 的必需性检查齐全（`deal_ppse_resp` 844-875、`deal_pse_resp` 668-693、`deal_spi_resp` 143-174 行），`BF0C.childnum <= 0` 也被拒绝。
- `emvtlv_get()` 返回约定核对：`0` 表示未找到（`emvtlv.c:513-549`），根节点 `tlvs[0]` 是伪对象、子项从 1 开始（`emvtlv.c:380-408`），因此本文件中 `if (!(pos = emvtlv_get(...)))` 的失败判断是正确的，不构成"6F 在首位被误判缺失"的问题。
- 候选结构写入边界：`candidate.aid`(16)、`candidate.lable`(16)、`candidate.preferred_name`(16)、`candidate.kernel_id`(8) 的所有拷贝点均有长度限制（`201-206`、`767-772`、`928-933`、`781-786`、`1215-1220`、`1235-1240`、`216-221`、`943-952`），未发现候选结构内部溢出（容量问题见 P1-2，属数组层面）。
- AID 匹配规则：卡 AID 长度被限定 5..16；终端 `asi==1` 时要求完全匹配、`asi==0` 时允许前缀匹配，`trans_type` 支持 `0xFF` 通配（`app_cmp` 77-89 + `emvporting.c:1212-1220`），与配置模型一致。
- 9F2A Kernel Identifier 归一化：对 00/40/02/42/08/48 做了映射，并校验与终端配置 kernel_id 一致（`243-290`、`958-1007`），拷贝长度均有上限。
- 候选排序与优先级归一化：`priority &= 0x0F`、0 视为最低优先级 15、插入排序升序（`1481-1539`），逻辑正确（重复项问题见 P2）。
- 空候选列表处理：`candidates_num == 0` 时写 DF8115/FF8106 并返回 `outcome_other_card`（`1454-1476`），与 S52.14/S52.18/S52.19 的意图一致。
- PPSE/SELECT 状态字处理：`select_ddf` 区分 L1 错误（<0：置 L1 error + 重新进出/超时）与 SW（`parse_ppse_resp` 的 S52.7/S52.9 分支），错误指示器与 discretionary data 组包长度在 1200 字节缓冲内（`parse_ppse_resp:1054-1118`）。
- SPI 响应状态字处理：`parse_spi_resp` 对 9000/非 9000 分别走 S54.6/S54.7，非 9000 时写入 DF8115/DF8106 并 `outcome_other_card`（`314-358`），错误指示器字段赋值完整。
- SPI 标签来源与模板校验：`9F3E/9F3F/8B` 通过 `appdata_save_tlv(..., TAG_BF0C...)` 保存，标签库确认三者属于 BF0C 允许模板（`emvtag.c:468-479`、`appdata_save_tlv:363-413`），长度上限（252/64）由标签库约束。
- `candidate_clr` 与 `candidate_add` 的配对：各构建入口（`select_use_ppse`/`select_use_aid_list`）均先清空候选，未发现跨交易残留（仅容量问题见 P1-2）。
- `spi_process` 的 SPI 开关与分支选择：`!tc_sup_list_exist && !sdol_exist` → 跳过 SPI；其余按 sdol/类别匹配组合调用 flag 1/2/3（`552-651`），分支覆盖完整（flag 3 的容量缺陷见 P1-3）。

## 跨模块问题 / 需 Lead 裁决

1. **`src/emv.c:1155` 存在语法错误**：`int i,j,;`（文件原始字节为 `09 69 6E 74 20 69 2C 6A 2C 3B 0D`）。若这是真实构建源码，工程无法编译；请 Lead 裁决这是"源码损坏"还是需要单独记录的真实缺陷（不在本模块范围内，但影响全工程审查结论）。
2. **P0 栈溢出根因在被调函数** `src/emvtlv.c:1541-1632`（`emvdol_packet` 完全不使用 `val_max_len`），本模块只是调用方；建议与 TLV/数据层审查员（模块 C）合并定级与修复。
3. **候选数可超过 `EMV_CFG_MAX_CANDIDATES(12)`**，而 `EMV_GetData(TAG_CANDIDATE_LIST)`（`src/emv.c:1615-1624`）按 `candidates_num*sizeof(EMV_CANDIDATE)` 交给上层；若上层（如 Android/JNI 侧）按 12 项分配接收缓冲，则形成上层堆越界写，需要 Lead 与上层接口负责人确认缓冲约定。
4. **`req_sel_candidates_result` 无范围校验**（`src/emv.c:1417-1427`）在多个模块被当作数组下标使用（本模块 `emv_combination_Selection.c:37-41`、`src/emvapi.c:303-306`），建议统一在 API 入口校验。
5. **接触式能力实际被禁用**：`EMV_PreProcess` 只置 `EMV_CARD_CONTACTLESS`（`src/emv.c:1170-1171`），`EMV_SetCardType` 据此拒绝 CONTACT（`1327-1334`）；因此本模块的 PSE/接触式代码当前全部不可达。这决定了 P2-12 的定级（当前非现网故障），若产品规划恢复接触式需重新评级。

## 审查局限

- 本工程为 IAR 工程（`build/iar/entrypoint_kernel.ewp`，仅包含 13 个 .c），无 Makefile/CMake，且依赖外部内核库实现的 `contact_process`/`paypass_process`/`ics_option` 等符号；本机无法编译/链接，全部结论为静态阅读所得，未做运行时验证。
- 源码注释编码损坏（GBK 中文被错误转成 UTF-8，含 U+FFFD），无法依赖注释；同源参考副本（`E:\Work\ZT\contact_core_japan_v4.4a\submodule\emvco\src\`）仅用于理解注释语义与实现意图（例如确认 `termapps[i]` 应为 `termapps[j]`、去重逻辑曾存在），未将其行号/内容作为本工程证据。
- `emv_ics.h` 中 SPI/持卡人确认等开关的默认值由外部 `ics_option()` 提供，本工程内无该函数定义，**无法静态确认 SPI 路径、卡持有人确认路径在目标产品中是否默认开启**；P0/P1 中依赖 SPI 的条目（P0、P1-3、P1-4 的 SPI 部分）只在实际打开 SPI 时可达。
- 未能提取 `E:\Work\SVN Project\emv_doc\...` 下的 EMV/PayPass 规范 PDF 文本，规范一致性判断基于代码注释中的 S52/S54 流程编号、EMV Book B/Book 1 通用知识与同源参考实现；涉及"9F29 应出现在 PPSE 还是 SPI 响应""PPSE 是否必须含 61 模板（`mandatory_exist` 检查在 SPI 之前，`835-896`）"两处未能离线确认，故未把后者计为缺陷，仅在此说明。
- 卡黑名单/证书黑名单逻辑不在本模块实现（位于 `emv.c`/`emvspi.c`/`emvporting.c`），本次未覆盖；`emv_combination_Selection` 的 `EMV_CONTACTLESSTYPE_*` 常量和内核库行为以头文件/外部库约定为准。
- 未覆盖宿主侧（上层应用/JNI）如何使用 `EMV_GetData(TAG_CANDIDATE_LIST)`、候选选择结果等接口，跨模块影响（P1-2、跨模块问题 3/4）需由 Lead 汇总确认。
