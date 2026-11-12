# TLV / Tag / 数据缓冲层 审查

## 概述

审查对象：`src/emvtlv.c`（1758 行）、`src/emvtag.c`（1309 行）、`src/emvbuf.c`（423 行），参考头文件 `src/inc/emv_tag.h`、`emv_type.h`、`emv_utils.h`、`emv.h`、`emvi.h`、`emvdbg.h`、`emverror.h`。

方法：全文逐段静态阅读；对二进制/哈希查找类逻辑用脚本按其源码语义复算（`tag_N`、`tag_emv_reserved`、`g_template*` 升序与查找命中、`g_emv_tag_lib` 升序与命中）；对每条结论跨文件只读核对调用方（如 `emvdol_packet` → `sdol_process` 的缓冲区尺寸与数据来源）。未编译、未运行（IAR 工程，本机无工具链），所有结论均来自代码本身。

严重度汇总：**共 15 条问题：P0 × 1、P1 × 4、P2 × 7、P3 × 3。**

核心判断：TLV **解码**方向（`emvtlv_decode`/`recur_decode`）的长度校验基本严谨，未发现可达的越界；真正的风险集中在 **DOL 打包方向**（不校验输出容量，且卡可控的 SDOL 直接决定展开长度）和 **appdata 哈希缓冲**（覆盖时截断链表、空间只增不减）。其中 P0 一条可由卡侧数据直接触发栈溢出。

---

## 问题清单（P0→P3）

### [P0] `emvdol_packet` 完全不校验 `val_max_len`，卡可控 SDOL 可造成调用方栈缓冲区溢出

- 位置：`src/emvtlv.c:1541`（函数）、`src/emvtlv.c:1554-1555`、`src/emvtlv.c:1584-1632`；调用方 `src/emv_application_selection.c:436`（flag=3 路径亦见 `:389`），缓冲区定义 `src/emv_application_selection.c:366`，容量宏 `src/inc/emvi.h:45`，SDOL 入库路径 `src/emv_application_selection.c:884-886`。

- 证据：

```c
/* emvtlv.c:1554 */
    val_len = 0;
    emv_memset(val, 0x00, val_max_len);          /* 只清空，之后从不与 val_max_len 比较 */

    for (i = 0; i < dollen;)
    {
        ...
        reqlen = dol[i++];                       /* emvdol_packet:1575，长度字节直接当值长度 */
        ...
        if (fmt || pdata == NULL)
        {
            emv_memset(val + val_len, 0x00, reqlen);   /* 1589：越界写 */
        }
        else
        {
            ...
            emv_memcpy(val + val_len, pdata, reallen); /* 1608/1620：越界写 */
        }
        val_len += reqlen;                       /* 1630 */
    }
    return val_len;
```

```c
/* emv_application_selection.c:365-366, 436 */
	uint8_t temp[EMV_CFG_ICC_SEND_MAXLEN]={0};
	uint8_t pack[EMV_CFG_ICC_SEND_MAXLEN]={0};       /* 262 字节（emvi.h:45） */
	...
			pack_len = emvdol_packet(sdol, len, pack, sizeof(pack));
```

```c
/* emv_application_selection.c:884-886：SDOL(9F3F) 来自卡 PPSE 响应的 BF0C，直接入库 */
		if(tlvs[i].tagname == TAG_9F3E_TERMINAL_CATEGORIES_SUPPORTED_LIST || tlvs[i].tagname == TAG_9F3F_SDOL || tlvs[i].tagname == TAG_008B_POI_INFOMATION)
		{
			appdata_save_tlv(tlvs+i, OVERRIDED, TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA);
		}
```

- 影响：
  - `9F3F` 的入库上限是 252 字节（`src/emvtag.c:120`），但 252 字节能表达的 DOL 展开长度没有上限。构造 `SDOL = "9F1F 7F" × 84`（252 字节，全部为**合法** DOL 条目），展开长度 = 84 × 127 = **10668 字节**，写入 262 字节的栈数组 `pack`，栈上溢出约 10KB；即使只有 3 条 `"9F1F 7F"`（9 字节 SDOL）也已溢出 119 字节。
  - 触发条件仅两点：终端 ICS 打开 SPI（`emv_application_selection.c:552` 的 `MISC_Paypass_Support_SPI`/`SPI_ACTIVE_FLAG`），且卡在 BF0C 里带 `9F3F`。溢出内容完全由卡控制（`9F1F` 未入库时走 `memset` 分支，写入的是 0x00，但后续条目可在溢出区写入 `pdata` 拷贝内容与 `reqlen` 数值），属于卡侧可驱动的栈内存破坏 → 崩溃/任意代码执行面。
  - 调用方在 `emv_application_selection.c:396/444` 才检查 `pack_len > 127`，为时已晚。
- 建议：
  1. 在 `emvdol_packet` 循环内、写入前检查 `val_len + reqlen > val_max_len` → 返回错误（同时 `emvdol_packet_emv`/`cdol_packet`/`udol_packet`/`sdol_packet` 同族函数一并修，见 P3-15）；
  2. 对 DOL 本身做合法性校验：`dollen` 必须是偶数/完整条目、长度字节必须 < 0x80（DOL 的 Length 是单字节，见 EMV Book 3 §5.4 与 C-2 Kernel 规格），拒绝 `0x80` 以上；
  3. 对展开总长度设上限（如 `<= EMV_CFG_ICC_SEND_MAXLEN` 减去标签头开销），超限直接判定卡数据非法。

### [P1] `emvdol_packet_tlv` 先写后查：输出缓冲越界写、输入越界读，4 字节长度条目可令 `reqlen` 为负

- 位置：`src/emvtlv.c:1725`（函数），关键点 `:1746/:1753/:1756`（写 T/L 无前置检查）、`:1769`（长度移位）、`:1774`、`:1779`（memcpy 先于检查）、`:1784`；调用链 `src/emv.c:1880-1897` → 公共 API `src/inc/emv_utils.h:129-142`。

- 证据：

```c
/* emvtlv.c:1756-1787 */
        tlv[tlvlen++] = dol[i];                    /* 写长度字节，无 tlvlmt 前置检查 */
        reqlen = dol[i++] & 0x7F;
        if (lenlen_exceed_one(dol[i - 1]))
        {
            if (reqlen > 4) { return -1; }
            j = reqlen;
            reqlen = 0;
            while (j > 0)
            {
                tlv[tlvlen++] = dol[i];
                reqlen |= dol[i++] << (--j * 8);   /* 0x84 xx xx xx xx 时 reqlen 可为负 */
            }
        }
        if (i > dollen || tlvlen > tlvlmt) { return -1; }   /* 1774：检查在写之后 */
        emv_memcpy(tlv + tlvlen, val + k, reqlen);          /* 1779：既不查 tlvlen+reqlen<=tlvlmt，也不查 k+reqlen<=vallen */
        k += reqlen;
        tlvlen += reqlen;
        if (k > vallen || tlvlen > tlvlmt) { return -1; }   /* 1784：检查在 memcpy 之后 */
```

- 影响：
  - `nOutMaxLen` 是公共 API 的显式契约（`emv_utils.h:136-142`），但每个 DOL 条目可向 `pOut` 越界写 `reqlen`（正常单字节长度最大 127）字节，同时从 `pVal` 越界读同样长度；T/L 头最多再越界 5~6 字节。
  - 若 DOL 条目使用 4 字节长度（`0x84`），`reqlen` 按 `int32_t` 移位得到可正可负（如 `0x84 FF FF FF FF` → `-1`），`emv_memcpy` 的长度被转成巨大 `size_t` → 立即崩溃/大面积破坏。
  - 该 API 的典型用途是"用卡下发的 CDOL/PDOL 打包数据"，因此 DOL 内容不可信。
- 建议：所有写入前先做 `tlvlen + reqlen <= tlvlmt` 与 `k + reqlen <= vallen` 检查并返回错误；长度字节只允许单字节（<0x80），或严格解析后限制 `reqlen <= 255` 并拒绝负数；`EMV_TLV_PacketByDOL` 在 `ret<0` 时不应再覆盖 `*pOutLen` 为负值后返回 `EMV_OK`（`src/emv.c:1884-1894`）。

### [P1] `appdata_save` 覆盖更大值时截断哈希链，同一桶内的其它 tag 静默丢失

- 位置：`src/emvbuf.c:226-259`（查找 existing 并保留 `pprev`）、`:264-291`（把旧节点置非活跃）、`:326-336`（新节点插入）。

- 证据：

```c
/* emvbuf.c:257-259 查找过程中 pprev 指向"持有 existing 的那个链接" */
		pprev = &ptmp->next;
		ptmp = ptmp->next;
	...
/* emvbuf.c:290 */
        existing->is_active = 0;
	...
/* emvbuf.c:327-336 */
	bufheader *new_node = (bufheader*)g_currpos;
	new_node->tagname = tag;
	new_node->valuelen = valuelen;
	new_node->next = NULL;
	new_node->is_active = 1;
	// 添加到链表
	*pprev = new_node;      /* 直接替换指向 existing 的链接，existing 及其后继整段链被丢弃 */
```

- 影响：当同一个 tag 以**更长的值**再次保存时（`valuelen > existing->valuelen`，`emvbuf.c:275-291`），新节点顶替 `existing` 的位置且 `next = NULL`，于是该哈希桶中排在 `existing` 之后的所有节点（其它 tag）从链上消失：`appdata_find` 再也找不到它们，而 `existing` 及其后继既不释放也不回收（`is_active` 只在不可达的压缩代码里被使用，见下一条）。这会造成"某个已读到的卡数据莫名变空"，进而影响 TVR/CVM/风险判断与后续 GAC 数据，属静默的功能性数据损坏。触发依赖"同 tag 用更长值重存"与"桶内碰撞"（表长 271，一次交易存几十~上百个 tag，碰撞概率不低），例如错误指示 `TAG_DF8115`（`emv_application_selection.c:344`）、`TAG_FF8106`（`emv_application_selection.c:352`）在不同分支以不同长度重存时就可能走到该路径。
- 建议：`override` 分支应把旧节点从链上**摘除并修复前后链接**（`*pprev = existing->next`），或直接在原节点尾部扩展；同时把空出的空间计入可回收列表（或先修复压缩逻辑，见下条）。`new_node->next` 需接上原 `existing->next`（若选择原地替换语义）。

### [P1] appdata 空间"只申请不回收"：压缩逻辑不可达，且跨交易不清 `g_currpos`

- 位置：`src/emvbuf.c:73-92`（`try_compact_memory` 提前 return）、`:94-146`（无法到达的压缩实现）、`:293-302`（`try_compact_memory` 的唯一用途）、`src/emv.c:935-938`（`EMV_initialize` 只 memset 缓冲区，不重置 `g_currpos`/哈希表）、`src/emv.c:1948-1957`（`EMV_ProcessExit` → `EMV_initialize(1)`）。

- 证据：

```c
/* emvbuf.c:84-92：空间不足时直接失败，压缩代码成为死代码 */
    buffer_end = g_emv_cnxt->tlvbuffer + g_emv_cnxt->tlvbuffer_size;

    // 首先检查是否有足够空间（不需要整理）
    if ((uint8_t*)g_currpos + required <= buffer_end)
    {
        return 1;
    }

	return 0;          /* ← 始终在这里返回，下面 94-146 行不可达 */

    // 计算哈希表头大小
    size_t header_size = HASHTABLE_LEN * sizeof(bufheader*);
```

```c
/* emv.c:935-938：ProcessExit 路径清空数据区，但 emvbuf 的静态 g_currpos 无人复位 */
    if (g_emv_cnxt->tlvbuffer)
    {
        emv_memset(g_emv_cnxt->tlvbuffer, 0, g_emv_cnxt->tlvbuffer_size);
    }
```

- 影响：
  - 分配器是纯 bump 分配（`g_currpos += required`，`emvbuf.c:353`），旧节点（被覆盖/被替换的）永不回收，压缩又不生效：一旦 10KB 缓冲区（`src/emv.c:114` `TLVBUFFER_SIZE (10*1024)`，扣除 271×4 字节头后约 9.1KB）用尽，`appdata_save` 永久返回 `BUF_BUFOVER`。而大量调用方**不检查返回值**（如 `emv_application_selection.c:344/352`、`emv.c:1555`），结果是后续交易里 tag 静默缺失。
  - `EMV_ProcessExit()`（公共 API `emv_process_exit()`，`src/emvapi.c:616-623`）只 memset 缓冲区，`g_hashtable`/`g_currpos` 没复位：哈希表被清零成"空"是对的，但 `g_currpos` 仍停留在上一笔交易结束处，上一笔交易占用的空间永久泄漏。若应用只在启动时 `emv_initialize()` 一次、每笔交易后 `emv_process_exit()`（SDK 常见用法），则每笔交易都净消耗一份数据区，若干笔后进入上述 `BUF_BUFOVER` 死状态。
  - 顺带：死代码 94-146 本身也危险——`:113-116` 在搬移中途 `return 0`（部分表项已改指针、部分未改），若将来把 `return 0` 去掉而启用它，会造成链指针指向旧位置/互相覆盖；`new_pos` 与 `current` 的搬移顺序、以及 `EMV_TLVOBJ->pvalue`/调用方持有的 `appdata_find` 返回指针都会失效（本层没有任何"指针失效"通知机制）。
- 建议：
  - 短期：让 `try_compact_memory` 真正可用（或在 `BUF_BUFOVER` 时返回明确错误并由上层中止交易），并在 `appdata_init` 之外提供 `appdata_reset()`，在 `EMV_initialize` 清缓冲区的同时把 `g_hashtable`/`g_currpos` 一并复位；
  - 中期：改成可回收的空闲链表或按 tag 定长槽位，避免 bump 分配 + 覆盖即泄漏；
  - 若启用压缩，必须同时失效所有缓存的 `appdata_find` 返回值（当前 `sdol_process`、`add_tag_*` 等都持有裸指针）。

### [P1] 在线脚本数据（重命名为 0xDF8100/0xDF8200 的 71/72 模板）必然被 `appdata_save_tlv` 拒绝，静默丢弃

- 位置：`src/emv.c:1549-1556`（重命名后调用 `appdata_save_tlv`）、`src/emvbuf.c:377-385`（要求 tag 必须在 tag 库中且含 RA 条件）、`src/inc/emv_tag.h:910-911`（两个合成 tag 定义，全工程无其它引用）、`src/emvtag.c:16-17`（0x71/0x72 条目只有 K 条件）、`src/emvtag.c:5-354`（tag 库中没有 0xDF8100/0xDF8200 条目）。

- 证据：

```c
/* emv.c:1549-1556：在线响应里的 71/72 模板被改名成 0xDF8100/0xDF8200 后存储 */
                for (i = 1; i <= tlvs[0].childnum; i++)
                {
                    if (tlvs[i].tagname == TAG_0071_ISSUER_SCRIPT_TEMPLATE_1)
                        tlvs[i].tagname = tag1++;        /* 0xDF8100 */
                    else if (tlvs[i].tagname == TAG_0072_ISSUER_SCRIPT_TEMPLATE_2)
                        tlvs[i].tagname = tag2++;        /* 0xDF8200 */
                    appdata_save_tlv(tlvs + i, OVERRIDED, 0xFFFFFFFF);   /* 返回值被忽略 */
                }
```

```c
/* emvbuf.c:377-385：不在 tag 库里就直接拒绝 */
	res = emv_taglib_read_UpdateCondition(tlv->tagname, EMVTAGTYPE_PAYPASS, 1, EMVTAGUPDATECONDITION_RA, &item);
	if(res)
	{
		return TAG_NOT_IN_DATABASE;
	}
```

- 影响：`0xDF8100`/`0xDF8200` 在 `g_emv_tag_lib` 中无任何条目（这两个常量只在 `emv_tag.h:910-911` 定义、在 `emv.c` 使用），`emv_taglib_read_UpdateCondition` 必然返回 1（未收录），`appdata_save_tlv` 必然返回 `TAG_NOT_IN_DATABASE`；调用方又不检查返回值，因此**在线响应中的发卡行脚本永远不入库**，后续无法通过 `EMV_GetData(0xDF8100…)` 取回（`emv.c:1631` 就是 `appdata_find`）。依据 EMV Book 3 §10.2 / Book 4 在线处理要求，终端须保存并处理脚本；此处功能链路在入口层被静默掐断。注：若产品确认脚本由上层另行解析 `pResp` 处理，则本条的危害降为"死接口"，但仍应删除或补全以免误导。
- 建议：给 `0xDF8100`/`0xDF8200` 及其多脚本序号（0xDF8100~0xDF810F 与 DF8101/DF8102 已被 DS Summary 占用，需重新选段）补 tag 库条目（K|RA、maxLen 255），或在线脚本改用 `appdata_save()` 直接存储；并检查 `EMV_ResultOnline` 的返回/错误传播。

### [P2] `emv_taglib_read_UpdateCondition` 在"tag 未收录"时读 `g_emv_tag_lib[-1]`

- 位置：`src/emvtag.c:665-681`，触发点 `:679`。

- 证据：

```c
/* emvtag.c:665-680 */
	if(item != NULL)
	{
        emv_memset(item, 0, sizeof(EMV_TAG_ITEM));
        if (readRes == 0 && pos >= 0)
        {
            item->Tag     = g_emv_tag_lib[pos].Tag;
            ...
        }
		else
		{
            item->Tag     = g_emv_tag_lib[pos].Tag;   /* pos 恒为 -1：越界读数组前 4 字节 */
		}
	}
```

- 影响：任何"查一个 tag 库中不存在的 tag"的调用（`pos` 初值 -1，未命中时不更新）都会读 `g_emv_tag_lib[-1]`，属未定义行为（读到的是相邻只读数据），写入 `item->Tag` 的垃圾值目前未被使用（调用方以 `readRes != 0` 判失败），但这是恒定发生的越界读，成品化/换编译器/加内存保护后可能直接异常，且掩盖真实"未找到"语义。
- 建议：`else` 分支删除（item 已在 `:667` 清零），或在 `pos < 0` 时直接跳过赋值。

### [P2] `emvtlv_decode` 不校验 `tlvs_max`/`nTLVs`：`nTLVs=0` 越界写，`nTLVs=1` 时 SINGLE 检查越界读

- 位置：`src/emvtlv.c:380-407`，对照 `src/emvtlv.c:332-339`（paypass 变体有 `tlvs_max <= 1` 校验）；公共入口 `src/emv.c:1838-1849`。

- 证据：

```c
/* emvtlv.c:380-407 */
int32_t emvtlv_decode(uint8_t *data, int32_t data_len, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags)
{
	if (data == NULL || tlvs == NULL)                       /* 未检查 tlvs_max */
	{
		return rERR(TLV_INPUTDATAERR);
	}
	emv_memset(tlvs, 0x00, sizeof(EMV_TLVOBJ) * tlvs_max);
	tlvs[0].pvalue = data;                                  /* tlvs_max==0 时越界写 */
	tlvs[0].valuelen = data_len;
	...
	if (flags & SINGLE_TLVOBJ)
	{
		if ((tlvs[1].pvalue + tlvs[1].valuelen) != (data + data_len))   /* tlvs_max==1 时越界读 */
```

- 影响：`EMV_TLV_Decode` 是公开 API（`emv_utils.h:113`），`nTLVs` 由集成方传入。传 0 时第 391-392 行直接写到调用方零容量缓冲之外；传 1 且带 `SINGLE_TLVOBJ` 时读 `tlvs[1]`（结构体 20 字节）之外的垃圾指针并做指针加法/比较。同一库里的 `emvtlv_decode_paypass` 明确拒绝 `tlvs_max <= 1`，两处不一致也说明这里是遗漏。
- 建议：`emvtlv_decode` 开头加 `tlvs_max <= 1` 时按 flags 分别校验（无 SINGLE 时允许 `>=1`，有 SINGLE 时 `>=2`），`nTLVs` 非法返回 `TLV_INPUTDATAERR`；公共 API `EMV_TLV_Decode` 同步做参数检查。

### [P2] `emvtlv_decode_itemlist` 长度解析错误 + 堆溢出（当前无调用，属潜在炸弹）

- 位置：`src/emvtlv.c:411-510`（`j > tlvs_max` 判断 `:434`；`data[i+1]` 无边界 `:442`；`i += t_len` 后读 `data[i]` 无边界 `:460`；长格式长度计算 `:470-487`；值拷贝 `:496`；`malloc` 未判空 `:423/:494`）。

- 证据：

```c
/* emvtlv.c:434-449：越界写：j 可以等于 tlvs_max（数组下标上限是 tlvs_max-1） */
			if(j > tlvs_max || i >= data_len)
			{
				break;
			}
			...
				if(data[i+1] & 0x80)     /* i+1 可能 == data_len，读越界 */

/* emvtlv.c:470-487：长格式长度算错（2 字节长度只取高字节，3 字节长度位序也错） */
			if(rest_byte > 1)
			{
				for(k = 1; k < rest_byte; k++)
				{
					tlvs[j].valuelen |= data[i+k] << (8 * (len_byte - k));   /* 漏掉最后 1 字节，且移位量整体偏移 */
				}
			}

/* emvtlv.c:494-497：value 长度未与剩余 data_len 比对就 memcpy */
				tlvs[j].pvalue = (uint8_t *)malloc(tlvs[j].valuelen);
				emv_memset(tlvs[j].pvalue, 0, tlvs[j].valuelen);
				emv_memcpy(tlvs[j].pvalue, data+i, tlvs[j].valuelen);
```

- 影响：`tlvs[tlvs_max]` 越界写一个 `EMV_TLVOBJ`（堆/栈破坏）；`0x82` 长格式长度只用了第一个长度字节（=`len_byte=3` 时 `data[i+1] << 16`，丢 `data[i+2]` 且整体位移错），得到的 `valuelen` 与真实长度不符 → 读取越界；`malloc` 失败后立即 `memset(NULL)` 崩溃。目前全工程无调用（`GetItemFromList` 也无人调用），故定为 P2；一旦接到 `STRING_TLVOBJ` 语义的调用方（`emvtag.c:827` 就是按该语义保留的接口）即为 P0 级堆破坏。
- 建议：删除该函数或按 `emvtlv_decode` 的安全写法重写；`j >= tlvs_max`；每个 `data[i]` 访问前判 `i < data_len`；长格式长度用标准循环 `valuelen = (valuelen << 8) | data[i++]`；`malloc` 判空；拷贝前判 `i + valuelen <= data_len`。

### [P2] PayPass tag 列表读写一族（`add_tlv_*/add_tag_*/remove_tag_*/data2send_contains_key`）多处缺陷

- 位置：`src/emvtlv.c:670-711`、`:713-754`、`:756-796`、`:798-840`、`:842-893`、`:895-946`、`:949-999`、`:1001-1128`、`:1131-1258`、`:1261-1388`、`:1391-1433`。

- 证据（要点）：

```c
/* emvtlv.c:1022-1029 与 1077-1085：长度只处理 1/2 字节编码，len>255 时高位被截断 */
	if(len > 127) { len_byte = 2; } else { len_byte = 1; }
	...
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;      /* len=300 -> 写入 0x2C，生成非法 TLV */
			}

/* emvtlv.c:1036-1045：tlv_get_tag/tlv_get_len 没有"剩余长度"参数，offset 贴尾时越界读最多 4 字节 */
            int32_t parsed_tag_len = tlv_get_tag(data_need + offset, &parsed_tag);
			...
            int32_t value_len_bytes = tlv_get_len(data_need + offset, &value_len);

/* emvtlv.c:756-796：列表不存在（data_need==NULL）时整段 save 被跳过，第一次添加静默失效 */
	data_need = appdata_find(TAG_DF8113_TAGS_TO_READ_YET, &cur_len);
	if(cur_len > sizeof(buf)) { return EMV_ERR; }
	...
	if(data_need)
	{
		if(cur_len > 0) { ... appdata_save(...); }
		else { appdata_save(...); }
	}
	return EMV_OK;

/* emvtlv.c:975-995：data2send_contains_key 的 T/L 跳过逻辑残缺，空 if 后仍按旧的 i 继续解析 */
		if((data_need[i] & 0x1F) == 0x1F)
		{
			if((data_need[i+1] & 0x80) == 0x80) { i += 3; } else { i += 2; }
		}
		else { i += 1; }

		if(data_need[i] & 0x80)
		{

		}
```

- 影响：`add_tlv_*` 生成的 TLV 在 `len > 255` 时长度字节被截断 → 下发的 data-to-send/脚本数据长度错误（卡侧行为不可预期）；`tlv_get_tag/tlv_get_len` 在值块末尾越界读（appdata 值区按 4 字节对齐分配，通常只读越 1~3 字节，位于缓冲区尾部时越界到缓冲区外）；`add_tag_*` 在目标列表尚未建立时"第一次添加不生效"；`data2send_contains_key` 的扫描字节对齐错误、且从不跳过 L/V，逻辑上永远找不到正确的 tag。当前这些函数**在工程内无任何调用点**（仅 `emvi.h:405-416` 声明），故整体定 P2；它们是 SDK 内部接口，一旦被上层启用即成为功能错误 + 越界读来源。
- 建议：统一用"剩余长度"参数化的安全 TLV 解析；长度编码交回 `tlv_set_len()`（它支持 0x81/0x82/0x83）；列表不存在时自动创建空节点（`Initialize()`）再添加；删除 `data2send_contains_key` 或按标准 TLV 遍历重写。

### [P2] `GetTagFromList` 使用未初始化的循环下标 `i`

- 位置：`src/emvtag.c:831-874`（声明 `:834`，使用 `:847`，越界读 `:856`）。

- 证据：

```c
/* emvtag.c:834,847 */
	int32_t len,i,tag_len,counter=0;      /* i 未初始化 */
	...
	while (i < len)                        /* 未定义行为：用栈上残留值做上界比较 */
	{
		...
		if((list_ptr[i] & 0x1F) == 0x1F)
		{
			if((list_ptr[i+1] & 0x80) == 0x80)   /* i+1 未判边界 */
```

- 影响：`i` 为栈垃圾值：常见结果是一次都不进循环（返回 0）或读到列表尾之后（`list_ptr[i]` 越界）→ 随机崩溃或返回错误的 tag 列表。函数在 `emv_tag.h:1011` 导出但没有内部调用（当前不可达），故定 P2；属"明显笔误"级 bug。
- 建议：`int32_t len, i = 0, tag_len, counter = 0;`，并补 `i + tag_len <= len` 检查。

### [P2] `tlv_set_tag`/`tlv_set_len` 无目标容量参数，调用方只能"先写后查"（最多越界 8 字节）

- 位置：`src/emvtlv.c:70-114`（`tlv_set_tag`，`emv_memcpy(buf, tmpbuf, taglen)` 于 `:111`）、`:116-152`（`tlv_set_len`）；调用方 `src/emv.c:1921-1932`。

- 证据：

```c
/* emvtlv.c:102-113：形参里没有"目标缓冲剩余长度"，函数无条件写 taglen(1~4) 字节 */
    tmptag = 0;
    tmplen = tlv_get_tag(tmpbuf, &tmptag);
    if (tmplen != taglen || tmptag != tag)
    {
        return -1;
    }
    emv_memcpy(buf, tmpbuf, taglen);
```

```c
/* emv.c:1921-1932：先写，后检查 */
        taglen = tlv_set_tag(pTags[i], pOut+reallen);
        if (taglen < 0) return EMV_FAIL;
        if (reallen + taglen > nOutMaxLen) return EMV_FAIL;    /* 已经写完了 */
        reallen += taglen;
        lenlen = tlv_set_len(ilen, pOut+reallen);
        if (lenlen < 0) return EMV_FAIL;
        if (reallen + lenlen > nOutMaxLen) return EMV_FAIL;    /* 已经写完了 */
```

- 影响：`EMV_TLV_PacketByTAG`（公共 API，`emv_utils.h:144-155`）在 `pOut` 只剩 0~3 字节时仍会写入完整 T（最多 4 字节）与 L（最多 4 字节）→ 最多 8 字节堆/栈越界写。属 API 设计缺陷：编码函数必须知道容量才能自身安全。
- 建议：给两个函数加 `int32_t bufsize` 参数并在写前校验；或调用方在调用前用 `GetTagLength`/长度编码长度预估并先做容量检查（不改签名时的最小修复）。

### [P2] `appdata_save` 对 `valuelen` 的负值/整数溢出无防护

- 位置：`src/emvbuf.c:177-180`（只挡 `value==NULL && valuelen!=0`）、`:293`（`(valuelen + 3) & ~0x03` 有符号溢出）、`:331/:339-351`（写 `valuelen` 与 `emv_memcpy(..., valuelen)`）。

- 证据：

```c
/* emvbuf.c:293 */
	required = sizeof(bufheader) + ((valuelen + 3) & ~0x03);
	...
    new_node->valuelen = valuelen;                 /* 331：允许负数写入节点 */
    if (valuelen > 0) { ... emv_memcpy(value_ptr, value, valuelen); }
```

- 影响：`valuelen = -1` 时 `required` 退化为 `sizeof(bufheader)`，但节点里记录 `valuelen=-1`，此后 `appdata_find` 的 `*pvaluelen = -1` 会传给 `emv_memcpy`/`emv_memset` 等（`size_t` 巨值）；`valuelen` 接近 `INT32_MAX` 时 `valuelen+3` 有符号溢出为负 → `required` 变小但 `emv_memcpy` 用原值复制 → 堆溢出。当前本层调用点长度均来自 512/1024 字节数组或 tag 库上限，暂不可达，定为 P2/防御性。
- 建议：入口处拒绝 `valuelen < 0` 或 `valuelen > tlvbuffer_size - sizeof(bufheader)`；`required` 用 `size_t` 运算并在加法后判上限。

### [P3] tag 库 0xDF8112 重复条目且破坏升序，导致 0xDF811F 的二分查找失效

- 位置：`src/emvtag.c:253`（0xDF8112，maxLen 255）、`:277`（0xDF811F）、`:279`（0xDF8112 第二次出现，maxLen 256，且位置在 0xDF811F 之后）。

- 证据：

```c
/* emvtag.c:277-279 */
	{TAG_DF811F_SECURITY_CAP, EMVTAGUPDATECONDITION_K, 1, 1, EMVTAGFORMAT_B, EMVTAGTYPE_PAYPASS},

	{TAG_DF8112_TAGS_TO_READ, EMVTAGUPDATECONDITION_K|EMVTAGUPDATECONDITION_ACT|EMVTAGUPDATECONDITION_DET, 0, 256, EMVTAGFORMAT_B, EMVTAGTYPE_PAYPASS},
```

- 影响：`g_emv_tag_lib` 依赖升序做二分（`emvtag.c:561-663`、`:703-724`）。按源码语义复算：最后一个元素 0xDF8112 使搜索 0xDF811F 时永远命中不到（`emv_taglib_read_UpdateCondition` 与 `emv_taglib_ReadItemWithExactMatch` 对 0xDF811F 均返回"未收录"）。由于 0xDF811F 的库条目只有 K 条件（不含 RA），当前没有可观测的功能后果，故定 P3；但重复条目（255/256 两个 maxLen）本身也说明表被手工改乱，后续再加 tag 会继续破坏二分。
- 建议：删除 `emvtag.c:279` 的重复条目（保留 253 行），或在 277 行后按升序插入；把 0xDF8112 的 maxLen 统一为一个值。建议加一个编译期/启动期断言校验 `g_emv_tag_lib` 严格升序（同时校验 `tag_N`/`g_template*` 列表）。

### [P3] `appdata_save` 拒绝"正好填满缓冲区"的分配；`appdata_save_or/and` 未校验负下标

- 位置：`src/emvbuf.c:314-324`（`is_valid_pointer(new_end)`）、`:148-158`（`is_valid_pointer` 用半开区间 `p < end`）、`:421/:436`（只判 `index >= len`）。

- 证据：

```c
/* emvbuf.c:314-324 */
    if (new_end > buffer_end)
	{
        return BUF_BUFOVER;
    }
    if (!is_valid_pointer(new_pos) || !is_valid_pointer(new_end))   /* new_end==buffer_end 时永远为假 */
	{
        return BUF_BUFOVER;
    }
```

```c
/* emvbuf.c:421 */
	if(ptmp == NULL || len < 0 || index >= len)      /* index 为负时通过，随后 ptmp[index] 越界写 */
	{
		return EMV_FAIL;
	}
	ptmp[index] |= value;
```

- 影响：正好用尽的分配被判失败（浪费 + 提前触发 `BUF_BUFOVER`）；`appdata_save_or/and` 传负下标会写 `bufheader` 之前 16 字节内（`tagname/valuelen/next` 区域），改动链表头 → 后续 `appdata_find` 崩溃。两者当前均无调用方（`appdata_save_or/and` 只在 `emvi.h:461-462` 声明），定 P3。
- 建议：`new_end` 的合法性判断改为 `new_end <= buffer_end`；`appdata_save_or/and` 增加 `index < 0` 检查。

### [P3] 其它未使用函数中的缺陷（记录备查）

- 位置与内容：
  - `src/emvtlv.c:1807-1857` `emvdol_tag_exists()`：多字节 tag 只累计 2 字节（`if (++j == 2) return 0`），3/4 字节 tag（如 `DF 81 13`）一律返回"不存在"，且 `reqlen = dol[i++]` 未按 DOL 语义校验、无 `dollen` 剩余检查。
  - `src/emvtag.c:1366-1396` `OutputTLV()`：`output_size` 全程未用（只判 `<0`），`output[i++] = tlvs[pos].valuelen` 只写 1 字节长度（>127 生成非法 TLV），且直接读 `tlvs[1]`、不校验 `tlv_size`；`emv_dec2hex(tlvs[pos].tagname, tag, tag_len)` 的 `tag[3]` 在 4 字节 tag 时越界 1 字节。
  - `src/emvtag.c:1337-1349` `IsTagPrivateClass()`：固定 `emv_dec2hex(tag_dec, tag_hex, 3)`，1~2 字节私有类 tag（如 `DF60`）被高位补 0 → `tag_hex[0]==0` → 恒判 false。
  - `src/emvtag.c:811-828` `GetItemFromList()`：把 `STRING_TLVOBJ` 传给 `emvtlv_decode`（该函数根本不处理此 flag，应传给被废弃的 `emvtlv_decode_itemlist`），语义落空。
  - `src/emvtag.c:775-807` `emv_ParseExtLenWithMaxLen()`：`ParseData[k+1..]` 未与调用方给出的真实缓冲长度比较（只与 `MaxLen` 比、且 `k+j > MaxLen` 是差一判断），无调用方。
  - `src/emvtlv.c:1541/1635/1859/1954` 的 `emvdol_packet_emv()/cdol_packet()/udol_packet()/sdol_packet()`：与 P0 同构，全部不校验 `val_max_len`（`cdol_packet`/`udol_packet` 还不做 `&0x7F`，长度字节 ≥0x80 时 `reqlen` 可达 255）；`emvdol_packet_emv` 的 `find_emv_reserved_tag()` 返回 0 表示"命中"，而代码在命中时把该字段清零（`:1694-1698`），语义与 `emvdol_packet` 的 `!IsKnown()` 过滤相反，需业务确认。这些函数当前无调用点。
- 影响：静态隐患，接上调用即为功能错误或内存问题。
- 建议：删除无调用代码，或在接入前按各自注释补齐参数校验与边界检查。

---

## 已核对无明显问题的项

1. **TLV 长度编码解码（`emvtlv.c:294-311`、`recur_decode_paypass:205-222`）**：短格式、`0x81/0x82/0x83` 长格式均正确；`0x80`（不定长/非法）因 `tmplen < 1` 被拒绝；长度字节个数先与 `data_len - data_index` 比较、值长度再与剩余长度比较，两处均在读之前，未发现越界读。
2. **递归解码的深度与容量（`emvtlv.c:244-330`）**：嵌套深度受 `tlvs_max` 约束（每层至少占一个 `EMV_TLVOBJ`，`tlvs_index == tlvs_max` 即返回 `TLV_NOOBJSPACE`），不会无限递归/栈耗尽；`tlvs_index += tlvs[..].childnum` 的回退量与子调用实际占用严格一致，不会越过 `tlvs_max` 写；`childnum` 在 `:328/:239` 精确赋值。
3. **tag 编码（`emvtlv.c:23-42`）**：最长 4 字节，第 5 字节前即返回 -1；与 `tlv_set_tag` 的互逆编码校验（`:102-109`）能拒绝非法 tag（如 `9F`、`1F xx xx xx`）。
4. **`EMV_TLVOBJ.pvalue` 指向与生命周期**：`pvalue = data + offset`（`:226/:315`），**不拷贝**，指向调用方输入缓冲；本工程内部所有 `emvtlv_decode*` 调用方都在同一作用域内立即使用（如 `emv_application_selection.c:143/668/706/844/1139`、`emvbuf.c:461`），并在返回前把需要保留的数据 `appdata_save` 拷贝入库，未发现悬垂使用。注：该别名语义未在 `emv_utils.h:102-113` 文档中写明，建议补充（否则集成方释放/复用 `pData` 后会读到脏数据）。
5. **`emvtlv_get()`（`emvtag.c` 对应实现 `emvtlv.c:513-549`）**：`SEARCH_ONLY_SON` 跳过子树、`SEARCH_ALL_DESC` 平铺扫描两种语义与调用方一致（如 `emv_application_selection.c:1145/1162`）；`childnum<0`（primitive）时提前返回 0，不会越界。
6. **`emvtlv_find_dup()`（`emvtlv.c:551-573`）**：调用方总用 `SEARCH_ALL_DESC` 取**第一个**匹配后再查后续重复（`emv_application_selection.c:1162-1177`），语义正确。
7. **查找表升序**：脚本复算确认 `tag_N`、`tag_emv_reserved`、`tag_CN`、`tag_NOTApply` 以及 `g_template70_list`、`g_template77_list`（二分使用的两张表）严格升序、每个元素都能被二分命中；tag 库仅 0xDF811F 一项受 P3-13 影响。
8. **`tlv_set_len()`（`emvtlv.c:116-152`）**：`0x81/0x82/0x83` 边界、`>0xFFFFFF` 拒绝均正确。
9. **`appdata_save` 同长/更短值覆盖分支（`emvbuf.c:275-288`）**：语义正确（原地更新 `valuelen` 并覆盖）。
10. **`appdata_find()`（`emvbuf.c:474-521`）**：哈希下标 `%271` 与表长一致；未命中时把 `*pvaluelen` 置 0，调用方据此判空是安全的。
11. **`appdata_init()`（`emvbuf.c:34-71`）**：头区/数据区划分、`tlvbuffer_size` 下限检查（`< header+1024` 报错）正确。
12. **`emv_taglib_ReadItemWithExactMatch()`（`emvtag.c:686-773`）**：二分 + 左右各 10 个近邻扫描的边界（`mid==0`、`i<size`、`i<=pos+10`）均正确，未溢出。
13. **`ParseAndStoreCardResponse()`（`emvtag.c:1514-1577`）**：`emvtlv_decode` 失败即返回、按 `tlvs[0].childnum` 遍历下标范围（1..childnum）正确、`emv_taglib_ReadItemWithExactMatch` 内部会 `memset(item)` 故"未收录"时 `item.Update==0` 跳过而非误存。**但该函数全工程无调用点**（仅 `emv_tag.h:1031` 声明），其"未校验 `tlvs[1].tagname` 是否等于 `tmplate`"、"嵌套子对象也按外层 template 校验"两点因此未定级，若将来启用需重审。

---

## 审查局限

- 本次为**纯静态审查**：IAR 工程（`build/iar/entrypoint_kernel.ewp`）无 Makefile/CMake，本机无法编译或运行，所有结论来自阅读与按其语义的手工/脚本复算，未经运行时验证。
- 源码树编码不统一（部分 GBK、部分被错误转成 UTF-8 且中文注释丢字），`read` 到乱码属编码问题，未计入问题清单；引用行号均以工程原文件为准。
- 未覆盖 `emv.c`、`emv_application_selection.c`、`emvspi.c`、`emvicc.c` 等其它模块的完整逻辑（属其他审查员范围），仅在核对本层函数的调用方/缓冲区尺寸时做只读引用（P0-1、P1-2、P1-5、P2-11）。
- 严重度依赖"可达性"判断：本层存在较多**当前无调用点**的函数（P2-8 之后多处已标注），其定级按"一旦接入即为内存破坏"给到 P2 而非 P0；若 Lead 认为 SDK 对外暴露（`emv_utils.h`/`emv_tag.h`）的函数应按"对外 API 必然被使用"对待，P2-8、P2-11、P3-15 可上调一级。
- 规范判定依据 EMV Book 3（TLV/DOL 编码、§5.4、§10.2 持卡人/终端数据对象）与 Mastercard C-2/C-3 Kernel 规格常识；未逐条比对规范 PDF 原文（`E:\Work\SVN Project\emv_doc` 本次未提取）。
