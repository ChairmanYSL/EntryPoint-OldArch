# ICC 接口与 SPI 适配层 审查

## 概述

审查对象：`src/emvicc.c`（567 行）、`src/emvspi.c`（1317 行）。参考头文件（只读）：`src/inc/emvspi.h`、`emvi.h`、`emv.h`、`emv_type.h`、`emverror.h`、`emvdbg.h`、`emv_utils.h`、`include/ks_hw.h`、`ks_crypt.h`、`ks_sdk.h`（GBK 文件经 UTF-8 镜像读取，行号与原文件一致；引用一律写原工程路径）。

方法：逐函数静态阅读，逐条核对缓冲区大小常量与写入长度；对 ICC 链路跨文件只读核对调用方（`emv.c` 的 `EMV_GetTransLog`/`EMV_ICC_*`、`emv_application_selection.c` 的 SPI/SELECT/READ RECORD 流程、`emvbuf.c` 的 `appdata_save_iccdata`）以及 SDK 头文件契约。未编译、未运行（IAR 工程，本机无工具链），结论均来自代码本身。

影响本报告适用范围的关键事实：

- 调用链：内核层 `emvicc_*`（`src/emvicc.c`）→ `icccommand` → `EMVPORT_IccRW`（`src/inc/emvi.h:248`）→ `emvspi.c:920 icc_rw` → `API_ICC_Apdu`（`include/ks_hw.h:234`）；SPI 回调由 `g_emv_porting`（`src/emvspi.c:1276-1310`）注册，`src/emvapi.c:206` 传给 `EMV_Init`。
- 本工程只含 Entry Point 源码，contact / PayPass 内核主体不在仓库内（`src/emv_selec_kernel.c:24-33` 的 `contact_process` 注册被注释，`paypass_process` 为外部符号），因此 `emvicc_*` 与部分 SPI 回调在本仓库内无调用点；对这类函数按接口契约与同族代码推断可达性，并在条目中注明。
- `EMVSPI_DO = 0`（`src/emvspi.c:92`）：`sel_candidate_list`/`pin_entry`/`go_online`/`confirm_bypass_pin` 等的 `*_do` 实现整块被编译排除，实际回调一律返回 `EMV_OPER_REPORT_APP`（由应用接管 UI/PIN/联机）。该分支还 `#include "emv_porting_ui.h"`（`src/emvspi.c:96`），该头文件不在本工程任何 include 路径中（工程 include 路径仅 `inc`、`include`、`src/inc`），当前配置下无法编译。
- `SELF_APDU_TEST` 未定义：`src/emvicc.c:76-211` 的 APDU 自测桩（含整表回放）不参与编译。
- 容量常量：`EMV_CFG_ICC_SEND_MAXLEN = 262`、`EMV_CFG_ICC_RECV_MAXLEN = 512`（`src/inc/emvi.h:45-46`）。
- `EMV_RELEASE_V = 1`（`src/inc/emv.h:15`）：`get_termparam`/`get_termapps`/`get_aidparam` 走 `emvporting_*` 真实实现。
- `EMV_CFG_DEBUG = ENABLE`、`EMV_APDU_DEBUG = DISABLE`（`src/inc/emvdbg.h:18/21`）。

严重度汇总：**共 14 条问题：P0 × 1、P1 × 1、P2 × 6、P3 × 6。**

核心判断：APDU **报文构造**与层内缓冲区尺寸本身是正确的（短 APDU 发送最长 4+255+1=260 < 262，接收 256+SW < 512，Lc/Le 位置均符合 ISO 7816-4）。本层真正的风险集中在**接收方向没有建立长度契约**：`icccommand` 成功但响应数据为空时不回写 `*outlen`、不校验设备返回的 `rlen`、接口也没有输出容量参数，导致多处调用方拿未初始化长度去 `memcpy`/TLV 解析，卡侧数据即可触发越界（P0）。其次是 61xx/6Cxx 无兜底、SPI 密码学/随机数实现缺边界与失败处理。

---

## 问题清单（P0→P3）

### [P0] `icccommand` 成功但响应数据为空时不回写 `*outlen`，调用方使用未初始化长度造成越界读写（卡侧可控）

- 位置：`src/emvicc.c:350-360`（核心缺陷），`src/emvicc.c:295-362`（`icccommand` 全文）；可达调用点：`src/emv.c:1660/1694/1699/1717/1726`、`src/emv_application_selection.c:368/520/533`、`src/emvbuf.c:455-461`。

- 证据：

```c
/* src/emvicc.c:295-362，节选 */
    sw = EMVPORT_IccRW(g_emv_cnxt->card_type, send, slen, recv, &rlen);
...
    if (sw == 0x0000)
    {
        return sw;
    }

    if (rlen > 0)                       /* ← 只有 rlen>0 才写 *outlen */
    {
        if (output != NULL)
        {
            emv_memcpy(output, recv, rlen);
        }
        if (outlen != NULL)
        {
            *outlen = rlen;
        }
    }

    return sw;
```

`rlen` 是局部的（`src/emvicc.c:218` 初始化为 0），`*outlen` 是调用方的输出参数；当命令返回 `9000` 而数据域为空（`rlen == 0`）时，`icccommand` 成功返回，但**从不写调用方的 `outlen`**。ISO 7816-4 允许响应数据域为空，GET DATA / READ RECORD / SELECT / SPI 等命令都可能出现这种响应。调用方一侧的长度变量均未初始化：

```c
/* src/emv.c:1657-1660 */
int32_t EMV_GetTransLog(int32_t nRecNo, uint8_t* pOut, int32_t* pOutLen)
{
    int32_t len, ret;          /* len 未初始化 */
    uint8_t  buf[256];
...
/* src/emv.c:1717、1726 */
        if ((ret = emvicc_read_record(*ptmp, (uint8_t)nRecNo, buf, &len)) != 0x9000)
...
        if (pOut)
            emv_memcpy(pOut, buf, len);   /* 用未初始化 len 写调用方缓冲区 */
```

```c
/* src/emv_application_selection.c:368、520、533 */
	int32_t pack_len=0,ret,resp_len;        /* resp_len 未初始化 */
...
	ret = emvicc_spi(pack, pack_len, resp, &resp_len);
...
	ret = parse_spi_resp(resp, resp_len, ret);   /* → emvtlv_decode(resp, resp_len) */

/* src/emvbuf.c:455-461 */
    int32_t len;
    uint8_t buf[EMV_CFG_ICC_RECV_MAXLEN];
...
    if (emvicc_get_data(tag, buf, &len) == 0x9000)
    {
        if (emvtlv_decode(buf, len, tlvs, 10, SINGLE_TLVOBJ) == 0)
```

另外 `EMV_ICC_Apdu`/`EMV_ICC_ReadRecord`/`EMV_ICC_GetData` 是对外工具接口（`src/inc/emv_utils.h:167/178/190`，文档只写 `pOutLen [OU]`，未要求调用方预置初值），外部内核或应用同样会踩中。

- 影响：卡把原本应返回数据的命令回成 `90 00`（空数据）即可让终端用**栈上未初始化值**当长度：`memcpy(pOut, buf, len)` 越界写应用缓冲区（`emv.c:1726`）、`appdata_save(..., len, ...)` 把垃圾长度写进全局 TLV 缓冲（`emv.c:1699`）、`emvtlv_decode(resp, 垃圾长度)` 越界读栈并可能级联破坏 TLV 结构（`emv_application_selection.c:533`、`emvbuf.c:461`）。属卡侧可控的内存破坏，可导致交易流程崩溃或内存被改写；SPI 流程在入口应用选择阶段就会被调用（`src/emv_application_selection.c:896`），触发成本很低。

- 建议：`icccommand` 进入时 `if (outlen != NULL) *outlen = 0;`，或成功分支无条件 `*outlen = rlen;`（`rlen` 已初始化为 0），保证 `outlen` 输出参数在所有返回路径上都有定义；同时要求/审查所有调用方把长度变量初始化为 0（`emv.c:1660`、`emv_application_selection.c:368` 等），并在 `EMV_ICC_*` 头文件注释中写明"成功时 `*pOutLen` 必被赋值，无数据时为 0"。

### [P1] 全层未处理 61xx / 6Cxx（GET RESPONSE、以正确 Le 重发），完全依赖 SDK 且本层无兜底

- 位置：`src/emvicc.c:295-362`（`icccommand` 的 SW 处理）；`src/emvspi.c:920-940`（`icc_rw` 直接透传 SDK 返回值）；`src` 全目录检索无 `0x61/0x6C`、`GET RESPONSE` 处理代码。

- 证据：

```c
/* src/emvicc.c:343-348，仅对 0x0000 做了特判 */
    apduTRACE("<<<<SW1SW2<<<<[%04X]", sw);

    if (sw == 0x0000)
    {
        return sw;
    }
    /* 其余 SW 原样返回；没有 61xx → GET RESPONSE，也没有 6Cxx → 修正 Le 重发 */
```

```c
/* src/emvspi.c:926-933，icc_rw 只是转发 */
    if (type == EMV_CARD_CONTACT)
    {
		return API_ICC_Apdu(ICC_CONTSCT, input, inlen, output, outlen);
    }
    else if (type == EMV_CARD_CONTACTLESS)
    {
        return API_ICC_Apdu(ICC_NON_CONTSCT, input, inlen, output, outlen);
    }
```

- 影响：EMV Book 1 §12.2.3 与 ISO 7816-4 对 `61xx`（发 `00 C0 00 00 xx` 取响应）与 `6Cxx`（以卡指定的 Le 重发原命令）有明确的终端侧处理要求。本层把任何 `61xx/6Cxx` 当最终状态原样上抛，内核侧调用点普遍按 `sw == 0x9000` 判断成功（如 `src/emv.c:1717`、`src/emv_application_selection.c:704/725`）：若目标 SDK/驱动未代劳，这类响应会导致 SELECT / READ RECORD / GPO 失败、交易中止；若驱动已代劳则本层行为无影响。本工程源码内没有任何兜底实现。参考同族 Kingsee KOS2 驱动（外部资料，非本工程证据）使用 `iso7816_exchange(..., AUTO_GET_RSP, ...)` 自动完成 61xx 取响应，说明该职责在部分平台由驱动承担；但本项目所用平台 SDK 未提供源码，**61xx/6Cxx 是否被驱动处理尚未证实**，故本条目按"本层无兜底、行为完全依赖未证实的外部 SDK/驱动"记录，待与 SDK/驱动方确认；若确认驱动已处理 61xx 且能处理 6Cxx，应降为 P2/P3。

- 建议：与 SDK/驱动方确认 (a) 61xx 是否已由驱动自动处理；(b) 6Cxx 是否有自动重发。任一未覆盖则在本层 `icccommand` 增加 SW 循环：`61xx` 发 GET RESPONSE（`00 C0 00 00 xx`），`6Cxx` 以 `xx` 为 Le 重发原命令；循环上限建议 ≤3 次并配合超时，避免与恶意卡无限往返。同时把该循环放在 `stop_flag` 检查之内，保证取消可用。

### [P2] `emvicc_put_data` 的响应缓冲只有 4 字节，卡返回数据即栈越界写

- 位置：`src/emvicc.c:546-561`。

- 证据：

```c
int32_t emvicc_put_data(uint32_t tag, uint8_t *in, int32_t inlen)
{
    capdu_t capdu;
	uint8_t out[4]={0};          /* 响应缓冲 4 字节 */
	int32_t outlen;              /* 未初始化 */
...
    capdu.le    = LE_NULL;
    return icccommand(&capdu, out, &outlen);
}
```

`icccommand` 在 `sw != 0 && rlen > 0` 时会执行 `emv_memcpy(output, recv, rlen)`（`src/emvicc.c:350-355`），对此调用即向 4 字节栈缓冲写最多 512 字节（受 `recv[512]` 限制），且不校验 `rlen`。

- 影响：PUT DATA（80 DA）在 EMV/PBOC 中正常响应只有 SW，正常卡不会触发；但响应数据长度完全由卡决定，异常/恶意卡回 `9000 + 数据` 即可造成函数栈破坏（覆盖相邻局部变量与返回地址保护区），属于该函数暴露给外部内核（`src/inc/emvi.h:401` 声明）的潜在内存破坏点。本仓库内无调用点，触发依赖外部内核确实发送 PUT DATA，故定为 P2；若确认内核会调用，应按 P1 处理。

- 建议：改为与其它命令一致的接收缓冲（`uint8_t out[EMV_CFG_ICC_RECV_MAXLEN]`）或显式丢弃响应（`icccommand(&capdu, NULL, NULL)`，PUT DATA 本来不需要响应数据）；同时 `outlen` 初始化为 0。

### [P2] `get_random` 不校验请求长度（>256 即栈溢出）、忽略 `API_Rng` 失败且恒返回 0

- 位置：`src/emvspi.c:1042-1058`；注册 `src/emvspi.c:1293`。

- 证据：

```c
int32_t get_random(uint8_t *buf, int32_t len)
{
	uint8_t buf1[256] = {0};
	int32_t len1 = 0;
	if(len % 4)
	{
		len1 = (len/4+1)*4;          /* len=257 → len1=260 > 256 */
	}
	else
	{
		len1 = len;
	}
    API_Rng(buf1, len1);             /* 返回值未检查；失败时 buf1 全 0 */
	memcpy(buf,buf1,len);            /* len 无上限校验 */
    TRACE_HEX("random", buf, len);
    return 0;                        /* 文档约定失败返回 -1，实际永远返回 0 */
}
```

- 影响：(a) `len > 256` 时 `API_Rng` 越界写 `buf1`、`memcpy` 越界读 `buf1`/越界写 `buf`，属栈破坏；当前本仓库无调用点，外部内核对随机数的请求通常为 4/8 字节，故未定为可达 P0，但接口（`src/inc/emv.h:250` `int32_t (*get_random)(uint8_t *buf, int32_t len)`）没有任何上限约束。(b) `API_Rng` 返回 `CRYPT_ERR` 时结果被当作全 0 随机数继续使用，违反接口注释"失败返回 -1"，影响 ICC GET CHALLENGE 兜底/不可预测数质量。(c) `memcpy` 在工程自带头文件中没有原型（仅 `API_memcpy`，见 `include/ks_stdlib.h:103`），属隐式声明，与工程其它代码的 `API_*` 约定不一致。

- 建议：增加 `if (buf == NULL || len <= 0 || len > (int32_t)sizeof(buf1)) return -1;`；检查 `API_Rng` 返回值并失败返回 -1；直接 `API_Rng(buf, 按 4 向上取整后的长度)` 或使用 `API_memcpy`，不再经过中间缓冲。

### [P2] `recover` 只实现 RSA，SM2/ECC 算法直接返回 -1；且不检查 `API_Crypt` 返回值、不校验模长

- 位置：`src/emvspi.c:952-982`；算法枚举 `src/inc/emv_type.h:472-478`；工程内对 SM2 CAPK 的支持见 `src/emvporting.c:1258-1281`，SM3 散列见 `src/emvspi.c:1021-1026`。

- 证据：

```c
int32_t recover(int32_t alg, EMV_PUBLICKEY* pk, uint8_t* input, uint8_t* output)
{
    int32_t outlen = 0;
    int32_t ret;

    if (alg == EMV_PUBKEY_ALG_RSA)
    {
        RSA_PUBKEY pubkey;
        CLEAR(pubkey);
        pubkey.length = pk->rsa.modulus_len;
        API_memcpy(pubkey.modulus, pk->rsa.modulus, pk->rsa.modulus_len);
        API_memcpy(pubkey.exponent+1, pk->rsa.exponent, 3); //FIXME
        API_Crypt(CRYPT_TYPE_RSA, (uint8_t*)&pubkey, sizeof(pubkey),
                NULL, input, pk->rsa.modulus_len, output, &outlen, 0);  /* 返回值未用 */
        if (outlen != pk->rsa.modulus_len)
        {
            return -1;
        }
    }
    else
    {
        TRACE("alg Err[%d]", alg);
        return -1;                      /* SM2/ECC2/ECC4 一律失败 */
    }
    return outlen;
}
```

- 影响：(a) 工程内 `emvporting.c` 能加载并校验 SM2 CAPK（`checkCAPKhash` 的 SM2 分支），`hash()` 也实现了 SM3，但公钥恢复没有 SM2/ECC 分支。一旦终端配置了 `algorithm = 0x04`（SM2）的 CAPK，内核调用 `recover` 必然得到 -1 → 脱机数据认证失败、交易被拒或降级联机；这是工程内部能力不一致（已支持 SM2 公钥存储/校验，却不支持其验证运算）。本工程入口以 PayPass 为主（`src/emv_combination_Selection.c:123` 强制 QMSDC），若产品不要求国密则可接受，故定 P2；若目标产品要求 PBOC2018 国密（工程已有 SM2 CAPK 加载与 SM3 入口），则升为 P1。(b) `API_Crypt` 的返回值被丢弃，只靠 `outlen` 判断：若底层失败但把 `outlen` 置为模长，将把未定义内容当恢复结果返回。(c) `pk->rsa.modulus_len` 是 uint8_t，可大于 `EMV_RSAPUBKEY.modulus` 的 248 字节（`src/inc/emv_type.h:603-607`），`API_memcpy` 会越界读 key 数据；当前唯一来源 `conv_capkparam` 限制到 248（`src/emvporting.c:1372`），但作为端口回调应自行防御。

- 建议：补齐 `EMV_PUBKEY_ALG_SM2/ECC2/ECC4` 分支或明确声明不支持并让上层据此降级；检查 `API_Crypt` 返回值；`if (pk == NULL || pk->rsa.modulus_len == 0 || pk->rsa.modulus_len > sizeof(pk->rsa.modulus)) return -1;` 后再拷贝。

### [P2] `icc_rw` 未校验参数与返回值，未知卡类型静默返回 `0x0000`，且 API 文档与原型不一致

- 位置：`src/emvspi.c:920-940`；SDK 原型/文档 `include/ks_hw.h:195-234`；错误处理参考 `src/emvspi.c:922-924` 的未使用变量。

- 证据：

```c
int32_t icc_rw(int32_t type, uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen)
{
    uint32_t sw = 0;
    int32_t ret = -1;
 	int32_t retlen = 0;
    TRACE("type[%d]", type);
    if (type == EMV_CARD_CONTACT)
    {
		return API_ICC_Apdu(ICC_CONTSCT, input, inlen, output, outlen);
    }
    else if (type == EMV_CARD_CONTACTLESS)
    {
        return API_ICC_Apdu(ICC_NON_CONTSCT, input, inlen, output, outlen);
    }
    else
    {

    }
    TRACE("ret:%d", ret);
    return 0x0000;
}
```

- 影响：(a) 未校验 `input/inlen/output/outlen` 非空；`output == NULL` 或 `outlen == NULL` 时由 SDK 直接写空指针（内核正常路径都传有效指针，属防御缺口）。(b) 未知 `type`（如 `EMV_CARD_UNKNOWN = 0`，`src/inc/emv_type.h:369-374`）落到 `return 0x0000`，按本层约定 `0x0000` 表示"无响应/失败"，但调用方若只判断 `ret < 0`（如 `src/emv_application_selection.c:523/1280`）会绕过传输错误分支，把"无响应"当正常 SW 继续往后解析，错误语义不闭环。(c) `sw/ret/retlen` 三个局部变量全部未使用，日志恒打印 `ret:-1`，说明错误分支是未完成的半成品，误导排障。(d) `ks_hw.h:195-234` 的文档描述包含 `pOutSize`/`pSW` 参数（7 参数语义），而实际原型是 5 参数、返回 SW，说明头文件文档陈旧；`API_ICC_Apdu` 没有输出缓冲容量参数，其缓冲区上限完全依赖驱动假设。

- 建议：入口校验指针；未知类型返回明确的负错误码（不要复用 `0x0000`）；删除未使用变量并让日志打印真实返回；在移植层注释中写明所用 SDK 的 `API_ICC_Apdu` 版本与语义（返回 SW、`outlen` 不含 SW、输出缓冲需按 512 准备），避免后续换 SDK 时语义错配。

### [P2] ICC 层没有"输出缓冲容量"契约，固定长度期望的命令（如 GET CHALLENGE 8 字节）无长度上限校验

- 位置：`src/emvicc.c:350-355`（`emv_memcpy(output, recv, rlen)` 不知道 output 容量）；接口声明 `src/inc/emv_utils.h:167/178/190`（`pOut` 无容量参数）；固定长度约定见 `src/inc/emvi.h:323-326`（GET CHALLENGE "返回数据长度（固定为8字节）"）。

- 证据：

```c
/* src/emvicc.c:350-355：rlen 来自设备/卡，output 容量未知 */
    if (rlen > 0)
    {
        if (output != NULL)
        {
            emv_memcpy(output, recv, rlen);
        }
```

```c
/* src/inc/emvi.h:320-326 */
 * 获取随机数（GET CHALLENGE）.
 * @param out       返回随机数.
 * @param outlen    返回数据长度（固定为8字节）.
```

- 影响：`emvicc_get_challenge` 等按"固定长度"约定接收数据的命令，会把卡返回的任意 `rlen` 原样拷进调用方缓冲区。外部内核通常按规范准备 8 字节缓冲，若卡返回更长数据（非规范或异常情况），越界写由调用方承受，而本层既无容量参数也无上限校验；`icccommand` 也未校验 `rlen <= EMV_CFG_ICC_RECV_MAXLEN(512)`，驱动异常时 `rlen` 过大同样会越界读 `recv` 并越界写 `output`。本仓库可见调用方（`emv.c:1660` 的 256 字节、`emv_application_selection.c`/`emvbuf.c` 的 512 字节）暂不会触发（短 APDU 数据上限 256），但这是接口层面的系统性缺口。

- 建议：为 `emvicc_*` 内部接口增加 `out_maxlen` 参数（或至少在 `icccommand` 中对 `rlen` 做 `0 <= rlen && rlen <= sizeof(recv)` 校验并在超出时返回错误）；对 GET CHALLENGE 等固定长度命令按期望长度校验（不足补 0、超出截断或报错），并把外部 `EMV_ICC_*` 接口补齐输出容量参数。

### [P2] `EMVSPI_DO=1` 分支不可编译，且其中的 PIN/选应用实现存在多处越界（一旦启用即为 P0/P1）

- 位置：`src/emvspi.c:92-687`（整体被编译排除）；具体缺陷：`src/emvspi.c:362`、`385-399`、`329/431/511`；依赖缺失 `src/emvspi.c:96`。

- 证据：

```c
/* src/emvspi.c:96：本工程 include 路径中不存在该头文件 */
#include "emv_porting_ui.h"

/* src/emvspi.c:357-362：EMV_GetData 返回的 5A 值缓冲恰为 panlen 字节，写 pan[panlen] 越界 1 字节 */
	pan = EMV_GetData(TAG_005A_APPLICATION_PAN, &panlen);
	if(pan==NULL || panlen>10)
	{
		return EMV_OPER_UNSUPPORTED;
	}
	pan[panlen] = 0x00;

/* src/emvspi.c:385-387：cardNo[20]；panlen 最大 10 时 cardLen 可达 20，
   写 cardNo[20] 越界（若 API_Conv_BcdToStr 还会补 NUL，则越界更多；该函数原型不在本工程头文件中） */
	API_Conv_BcdToStr(pan, panlen, (char*)cardNo);
...
	cardNo[cardLen] = 0x00;

/* src/emvspi.c:391-399：cardLen 若为 0（panlen==0 时循环不执行，cardLen 未初始化），
   panBuff[17] 的写入偏移可达 4+13-0=17，随后 memcpy 12 字节越界 */
	API_memset(panBuff, '0', 16);
	if(cardLen < 13)
	{
		API_memcpy(panBuff+4+13-cardLen, cardNo, 12-(13-cardLen));
	}

/* src/emvspi.c:329 / 431 / 511：24 字节数组无结尾 0，API_strlen 越界读到栈上其它字节，
   得到的 keylen 传给 API_Crypt，密钥长度取决于相邻栈内容 */
    uint8_t key[24] = {"\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"};//{"\x00...};
...
            API_Crypt(CRYPT_TYPE_DES, key, API_strlen((const char*)key), NULL, output, 8, output, sizeof(buf), (uint32_t*)&ret, CRYPT_MODE_ENCRYPT);
```

- 影响：该分支当前被 `EMVSPI_DO = 0` 关闭，且本身无法编译（`emv_porting_ui.h`、`PUI_*`、`API_PCI_GetPin`、`API_Conv_*` 原型在本工程头文件中缺失），因此不影响当前构建；但它是随代码交付的"可用实现"，一旦有人把 `EMVSPI_DO` 置 1（并补头文件），上述越界会立即变成实际漏洞：19 位 PAN（10 字节）是正常卡形态，`pan[panlen]=0` 与 `cardNo[cardLen]=0`（cardLen=20）各越界 1 字节会稳定触发；`panlen==0` 时 `cardLen` 未初始化可造成 12 字节栈越界；`key` 的 `strlen` 越界读使 DES 密钥长度取决于相邻栈内容。此外 `sel_candidate_list_do` 的 `item[32]`/`release_name[32][33]`（`src/emvspi.c:99/118-155`）对 `count` 无校验，当前内核候选上限 12（`src/inc/emvdbg.h:20`）不会触发，但属防御缺口。

- 建议：要么删除该分支（连同 `EMVSPI_DO` 宏）避免"死代码带病"，要么修复：`pan[panlen]` 改为局部拷贝后再补 0、`cardNo` 扩到 `2*EMV_PAN_MAXLEN+1`、`cardLen` 初值置 0 并在 `panlen==0` 时返回不支持、`key` 用显式长度（或补 NUL）、`count` 与 `APPS_MAX_NUM` 做上限校验。

### [P3] `stop_flag`：set/get 未在任何头文件声明（`emv.c:952` 隐式调用）、全工程无置 `true` 路径、全局非 `volatile`/非 `static`

- 位置：`src/emvicc.c:46`、`374-383`；声明缺失：`src/inc/emvi.h:289-402`（EMVICC 原型段无这两个函数）；唯一调用 `src/emv.c:952`。

- 证据：

```c
/* src/emvicc.c:46 */
bool stop_flag = false;

/* src/emvicc.c:374-383 */
void emvicc_set_stop_flag(bool flag)
{
	eTRACE("app set stop flag = %d", flag);
	stop_flag = flag;
}

bool emvicc_get_stop_flag()
{
	return stop_flag;
}
```

```c
/* src/emv.c:952，emv.c 未包含这两个函数的声明，属隐式声明（C99 违规，IAR 告警） */
	emvicc_set_stop_flag(false);
```

- 影响：工程内没有任何代码把标志置为 `true`（检索 `stop_flag`/`emvicc_set_stop_flag` 仅命中定义、复位和 `icccommand` 的两处检查），因此 `icccommand` 里的取消/中断机制在当前工程内是死路径；`-106`（`src/emvicc.c:292/340`）这个非标准返回码却已被 `emv_application_selection.c:1282` 当作超时识别，说明设计意图存在但链路未接通。`bool` 全局非 `volatile`，若由中断/其它任务置位，编译器可能缓存；非 `static` 还存在与外部同名符号冲突风险。

- 建议：在 `src/inc/emvi.h` 中补充两个函数原型（与 `emv.c` 的调用一致）；明确由谁在什么上下文置 `true` 并在交易结束复位；将标志改为 `static volatile bool`；把 `-106` 定义成具名错误码并删除魔法数字。

### [P3] APDU 敏感数据掩码被注释，日志标签仍为 `Mask_C_APDU`（`EMV_APDU_DEBUG` 一旦开启即明文记录 PAN/二磁道）

- 位置：`src/emvicc.c:245-284`、`297-342`（掩码循环整段注释）、日志调用 `src/emvicc.c:284/342`。

- 证据：

```c
/* src/emvicc.c:245-282：原本对 0x57(二磁道)、0x5A(PAN) 的掩码循环已整段注释 */
//	emv_memset(send_copy, 0, sizeof(send_copy));
//	emv_memcpy(send_copy, send, slen);
//
//	while (i < slen)
//	{
//		if(0x57 == send_copy[i]) ...
...
	apduTRACE_HEX("<<<<Mask_C_APDU<<<<", send, slen);   /* 名字仍叫 Mask，实际未掩码 */
```

- 影响：`EMV_APDU_DEBUG` 当前为 `DISABLE`（`src/inc/emvdbg.h:21`），所以当前不输出；但一旦为排障打开，接收方向 `apduTRACE_HEX("<<<<Mask_R_APDU<<<<", recv, rlen)`（`src/emvicc.c:342`）会把 READ RECORD 中的 5A/57 等卡数据明文写日志，发送方向同理，构成 PAN/二磁道泄露与日志误标。掩码逻辑既然保留为注释，说明属有意二开删除，建议留下明确的编译期开关与告警。

- 建议：恢复掩码循环（针对 0x57/0x5A 的 TLV 反射掩码），或至少把日志标签改为 `C_APDU`/`R_APDU` 并在 `EMV_APDU_DEBUG` 旁注明"会记录敏感数据，仅限实验室使用"。

### [P3] `emvicc_apdu` 直接解引用 `g_emv_cnxt`，无 NULL/初始化保护

- 位置：`src/emvicc.c:385-388`；对照 `src/emv.c:1818-1826`（`EMV_ICC_GetData` 有 `g_emv_cnxt == NULL` 与状态检查）。

- 证据：

```c
/* src/emvicc.c:385-388 */
int32_t emvicc_apdu(uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen)
{
    return EMVPORT_IccRW(g_emv_cnxt->card_type, input, inlen, output, outlen);
}
```

- 影响：`EMV_ICC_Apdu`（`src/emv.c:1813-1816`，对外声明于 `src/inc/emv_utils.h:167`）未像 `EMV_ICC_GetData`/`EMV_ICC_ReadRecord` 那样检查 `g_emv_cnxt` 或状态；在 `EMV_Init` 之前或 `EMV_ProcessExit` 之后调用会空指针解引用，属可稳定复现的崩溃（应用误用即可触发，代价低）。

- 建议：`emvicc_apdu` 内 `if (g_emv_cnxt == NULL) return EMV_UNINITIALIZED;`，并在 `EMV_ICC_Apdu` 中与兄弟接口保持一致的状态检查。

### [P3] 悬空/死代码与未用配置：`emv_initprepara` 只有声明、`CFG_EMV_L2TEST_USE_SDKV2` 未使用、`get_termparam` 未注册、`icc_rw` 未用变量

- 位置：`src/inc/emvspi.h:63`；`src/emvspi.c:34`、`1130-1138`、`1297`、`922-924`。

- 证据：

```c
/* src/inc/emvspi.h:63 */
extern void emv_initprepara(void);          /* 全工程无定义、无调用 → 若外部按此头调用将链接失败 */

/* src/emvspi.c:33-34 */
/** 是否使用SDK2.0的API */
#define CFG_EMV_L2TEST_USE_SDKV2        0    /* 宏定义后从未被引用，属残留 */

/* src/emvspi.c:1296-1298：get_termparam 已实现但注册行被注释，成为死函数 */
//    .get_termparam = get_termparam,
    .get_termapps = get_termapps,
```

- 影响：不影响运行时行为，但会误导集成方（`emv_initprepara` 是移植层常见入口名），并在换 SDK 版本时让人误以为切宏即可适配（`CFG_EMV_L2TEST_USE_SDKV2` 实际无代码分支）。`get_termparam` 未注册意味着内核只能绕开 `EMV_PORTING` 直接通过 `emvporting_*` 取参数（如 `src/emvapi.c:648-651` 的 TLV 接口），与 `EMV_PORTING` 结构设计不一致。

- 建议：删除 `emv_initprepara` 声明或补实现；删除未用宏或补齐 SDK V2 分支（7 参数 `API_ICC_Apdu`、`API_Hash` 带输出长度等）；恢复 `get_termparam` 注册（需同步放开 `src/inc/emv.h:280` 的字段）或删除该函数；清理 `icc_rw` 未使用变量。

### [P3] 9F41 计数器回绕到 1 造成重复；日期时间无有效性校验；`FMT_NUMTOBCD` 宏参数两次求值

- 位置：`src/emvspi.c:1101-1119`、`1069-1091`、`78`。

- 证据：

```c
/* src/emvspi.c:1106-1116：到 99,999,999 后回绕到 1，9F41 会重复 */
    tsc = emvporting_get_tsc();
    tsc++;
    if (tsc > 99999999 || tsc <= 0)
    {
        tsc = 1;
    }
...
    API_sprintf((char *)tempbuf, "%08d", tsc);
```

```c
/* src/emvspi.c:1069-1091：直接信任 RTC，无 Year/Month/Day 有效性校验 */
    API_TimeGet(&curDateTime);
    ...
        *(date + 2) = FMT_NUMTOBCD((curDateTime.Month));

/* src/emvspi.c:78：宏参数被求值两次 */
#define FMT_NUMTOBCD(hex) ((((hex) / 10) << 4) + ((hex) % 10))
```

- 影响：9F41 是交易序列计数器，回绕后与历史交易重复，影响对账/防重放类应用逻辑（PBOC 层面一般不校验，但重复序列号对收单侧日志不友好）；RTC 掉电/未校时时可能产生 2000-00-00 之类的非法日期，影响 9A/应用有效期判断和风控；宏参数两次求值目前实参均为无副作用的整数表达式，仅属风格隐患。

- 建议：9F41 到上限时置错误/告警而不是静默回绕（或按 BCD 4 字节最大 99,999,999 处理，回绕前打日志）；日期输出前校验 `Year >= 2000 && Month in 1..12 && Day in 1..31`，非法时返回 -1（由内核决定降级策略）并至少记录一次错误；宏改为 `static inline` 或加括号（`(((hex)%10) + (((hex)/10)<<4))` 已带括号，主要问题是多次求值）。

### [P3] 各 APDU 构造函数把 `int32_t` 长度隐式截断为 `uint8_t` Lc（>255 时静默截断）

- 位置：`src/emvicc.c:397`、`410`、`449`、`462`、`514`、`527`、`540`、`557`。

- 证据：

```c
/* 以 GAC 为例（src/emvicc.c:410-411） */
    capdu.lc    = (uint8_t)cdolvlen;
    capdu.pdata = cdolv;

/* PUT DATA（src/emvicc.c:557）连强转都省略，直接赋值 */
    capdu.lc    = (inlen);
```

- 影响：短 APDU 的 Lc 只有 1 字节，EMV/PBOC 的 PDOL/CDOL 数据理论上限 255，正常不会超；但 `cdolvlen/pdolvlen/inlen` 都是 `int32_t`，一旦上游打包逻辑出错（例如 DOL 长度算错）传入 256+，本层会静默按 `len & 0xFF` 发送错误数据，卡片拒绝后表现为"莫名其妙的 6A80/6700"，排障困难；`emvicc_put_data` 的隐式转换还会触发编译器告警。

- 建议：每个构造函数入口加 `if (len < 0 || len > 255) return 错误码;`（PUT DATA 如确需 >255 应改用扩展长度或拆包），并消除隐式窄化告警。

---

## 已核对无明显问题的项

1. **APDU 报文构造**：`emvicc_select`（00 A4 04 P2 Lc data 00）、`read_record`（00 B2 recno ((sfi&0x1F)<<3|0x04) 00）、`get_processing_options`（80 A8 00 00 Lc data 00）、`generate_ac`（80 AE P1 00 Lc data 00，P1 的 CDA 位 0x10 正确）、`get_data`（80 CA P1P2 00）、`get_challenge`（00 84 00 00 00）、`internal_authenticate`（00 88 00 00 Lc data 00）、`external_authenticate`（00 82 00 00 Lc data）、`verify`（00 20 00 type Lc data）、`spi`（80 1A 00 00 Lc data 00）、`relay_resistance_protocol`（80 EA 00 00 Lc data 00）、`compute_crypyographic_checksum`（80 2A 8E 80 Lc data 00）、`put_data`（80 DA P1P2 Lc data）均符合 ISO 7816-4 的 case 与 EMV 命令编码。

2. **发送/接收缓冲尺寸**：`send[262]` 覆盖最长短 APDU 260 字节（`4 + 255 + 1`），`lc` 为 `uint8_t` 不会超过 255，无栈溢出；`recv[512]` 覆盖"256 字节数据 + 2 字节 SW"（SDK 层 `outlen` 不含 SW，见 `src/inc/emv_utils.h:164` 与 `src/emvspi.c:916` 注释），容量充足。

3. **SW 语义**：`icc_rw` 返回的即 SDK 的 SW（`include/ks_hw.h:216-234` 文档与同族 SDK 实现一致），`icccommand` 以 `sw == 0x0000` 表示"无响应/失败"、其它值原样上抛给内核，与各调用点"`== 0x9000` 才算成功"的用法一致；`-106` 被 `src/emv_application_selection.c:1282` 识别为超时/取消，链路闭合（见 P3 关于魔法数字的建议）。

4. **卡类型映射**：`icc_rw` 把 `EMV_CARD_CONTACT(1)`/`EMV_CARD_CONTACTLESS(2)` 分别映射到 `ICC_CONTSCT(1)`/`ICC_NON_CONTSCT(2)`（`include/ks_hw.h:124-135`，`src/inc/emv_type.h:369-374`），映射正确。

5. **全局缓冲容量**：`weak_alloc_buffer` 返回 14×1024+256 = 14592 字节（`src/emvspi.c:895-904`），大于 `EMV_Init` 的最小要求 `sizeof(EMV_CONTEXT)+2048`（`src/emv.c:994`）和移植注释建议的 7168 字节，且每次初始化前清零。

6. **日期时间格式**：`get_trans_date_time` 输出 4 字节 BCD `YYYYMMDD` + 3 字节 BCD `hhmmss`（`src/emvspi.c:1069-1091`），与内核 `emv_date_compare` 的 `curr[4]` 期望一致（`src/emv.c:483-515`），世纪/月份/日期 BCD 转换公式正确（`FMT_NUMTOBCD`，2026 → 0x20）。

7. **散列算法映射**：`hash()` 把 EMV 算法号 0x01/0x03/0x05/0x07 分别映射到 `HASH_TYPE_SHA1/SHA256/SHA512/SM3`、输出长度 20/32/64（与 `include/ks_crypt.h:32-56` 枚举一致；SM3 长度契约不一致见文末跨模块观察第 3 条），SHA 系列正确。

8. **RSA 恢复入参**：`recover` 把 EMV 3 字节大端指数写到 `pubkey.exponent+1`（`RSA_PUBKEY.exponent[4]` 左补零约定，`include/ks_crypt.h:118-123`），`pubkey.length` 回填模长、以 `outlen` 校验恢复长度，逻辑正确。

9. **`message()`**：消息 ID 0..`EMV_MSG_MAX-1` 的上界检查正确（`src/emvspi.c:691-703`），`g_emvapi_messageinx` 由 `src/emvapi.c:76` 定义并被 `emv_process2` 消费，无越界。

10. **SPI 透传类回调**：`get_termapps/get_aidparam/get_capkparam/get_translog_amount/sav_translog/fnd_blackcard/fnd_blackcert/get_kernel_index/get_termparam` 均为薄封装，返回值与 `src/inc/emv.h`/`emvspi.h` 的函数指针契约一致；`EMV_PORTING` 注册字段与结构定义逐一对应（`.get_termparam` 除外，见 P3）。

11. **`icc_pwron` 恒返回 -1**：与同族 entrypoint 移植层实现一致，属"接触式上电由内核负责"的刻意桩（本工程 contact 流程未启用），不按缺陷计。

12. **`EMVSPI_DO = 0` 下的回调契约**：`sel_candidate_list/sel_account_type/deactivate_card/check_id/pin_entry/confirm_bypass_pin/confirm_force_online/go_online/issuer_referral` 统一返回 `EMV_OPER_REPORT_APP(-7)`（`src/emvspi.c:718-881`），与 `src/inc/emv_type.h:457-467` 的契约及"内核报应用、应用回填结果"的设计一致。

---

## 审查局限

- 工程源码编码不统一/损坏（UTF-8 转换丢失中文、部分 GBK 文件），注释不可依赖；本报告只引用行号与代码结构，未把编码问题计为缺陷（Lead 已单独记录）。
- 本机无 IAR 工具链，未编译、未运行；IAR 下隐式函数声明等告警程度无法实测（`emv.c:952`、`src/emvspi.c:1055` 等依赖隐式声明）。
- 本工程只含 Entry Point 源码与移植层；contact/PayPass 内核主体、SDK 实现（`API_ICC_Apdu`/`API_Rng`/`API_Hash`/`API_Crypt`）不在仓库内。`emvicc_*` 中除 `read_record/select/get_data/spi/apdu` 外多数函数在本仓库内无调用点，其可达性依据 `src/inc/emv_utils.h`、`emvi.h` 的接口契约与同族代码推断，无法追踪外部预编译内核的全部调用路径。
- 无法从本工程验证 SDK/驱动行为：61xx/6Cxx 是否由驱动代劳、APDU 响应长度上限、`API_Rng`/`API_Hash` 在失败与 SM3 下的实际输出长度等，相关条目标注"待确认"，建议由 Lead 向 SDK/驱动方确认后再定级。
- 未修改任何工程源码/头文件/构建文件；本报告为本任务唯一写入产物。

---

## 附：跨模块观察（不计入本报告问题数，供 Lead 分派）

1. **接触式路径空指针**：`src/emv_selec_kernel.c:22-33` 中 `contact_process` 的注册被注释，`emv_k_interface.emv_process` 保持 NULL，而 `src/emv.c:1403/1407` 直接调用该函数指针；同时 `src/emv_combination_Selection.c:123` 无条件把 `contactless_type` 置为 `EMV_CONTACTLESSTYPE_QMSDC`。若产品仅做 PayPass 可忽略，否则接触式流程会空指针调用。
2. **CAPK 校验强度**：`src/emvporting.c:1282-1305` 对 SHA-256/512 的 CAPK 校验只比较前 20 字节（`API_memcmp(digest, cert.checksum, 20)`），与 `src/emvporting.c:1432` 允许 checksum 为 20/64 字节不匹配，可能接受前 20 字节相同的伪造公钥；属数据/移植层范围。
3. **`hash()` 的 SM3 长度**：`src/emvspi.c:1021-1026` 返回 32，而 SDK 头文件 `include/ks_crypt.h:55` 声明 `HASH_RESULT_LEN_SM3 = 20`；当前工程内唯一使用者 `checkCAPKhash` 只比较 20 字节，故暂未造成可见错误，但契约不一致（未在本报告主清单单列，建议随 SDK 确认一并处理）。
