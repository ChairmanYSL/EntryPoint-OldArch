# 移植层/参数解析与持久化 审查（模块 D）

## 概述

- 审查对象：`src/emvporting.c`（2249 行，本模块唯一实质实现）、`src/emvsdk.c`（14 行）。
- 参考头文件：`src/inc/emvporting.h`、`src/inc/emv_type.h`、`src/inc/emvapi.h`、`include/ks_stdlib.h`、`include/ks_sdk.h`、`include/ks_type.h`、`src/inc/emv_utils.h`；跨文件只读核对 `src/emv.c`、`src/emvapi.c`、`src/emvspi.c`、`src/emvtlv.c`。
- 方法：纯静态审查（工程无法在本机编译）。逐函数通读 `emvporting.c` 全文；对 `conv_param`/`conv_capkparam` 等解析函数按"外部 TLV 字段长度 → 定长目标字段容量"逐字段核对；对文件读写按"记录布局/偏移/返回值/容量上限/掉电"核对；对 `EMV_TLV_Decode`/`EMV_TLV_Find` 语义、`emv_bcd2hex` 等被调函数按实际实现核对。
- 威胁模型：按任务要求，终端参数、AID 参数、CAPK 均来自主机/应用下发的 TLV（`emv_set_termparam`/`emv_add_aidparam`/`emv_add_capkparam`），视为外部不可信输入；`ks_*` SDK 无源码，只按头文件契约评估。
- 结论：移植层存在 **2 个 P0（宿主 TLV 直接造成栈写越界）** 和若干 P1/P2。最严重的是 `conv_param` 对字段长度"不校验或只校验非零"，配合 `add_aidparam`/`set_termparam` 落盘前不做字段级校验，可在每次非接交易预处理（`emv_preprocess → EMV_PreProcess → EMVPORT_GetAppParam`）时破坏内核栈。

统计：**共 24 条：P0×2、P1×4、P2×11、P3×7。**

---

## 问题清单

### [P0] D-01 conv_param 按 TLV 值长度直接 memcpy 到定长字段，调用方栈内 EMV_AIDPARAM 被写越界

- 位置：`src/emvporting.c:325`、`:331`、`:337`、`:343`、`:434`、`:597`、`:604`、`:628`、`:634`（相关入口 `:844` `emvporting_add_aidparam`、`:670` `emvporting_set_termparam`、`:1009` `emvporting_get_aidparam`）
- 证据：

```c
// src/emvporting.c:322-345（终端参数分支）
case TAG_9F01_ACQUIRER_IDENTIFIER:
    if(tlvs[i].valuelen)
    {
        API_memcpy(tpar->acquirer_id, tlvs[i].pvalue, tlvs[i].valuelen);   // acquirer_id[6]
    }
    break;
case TAG_9F1E_IFD_SERIAL_NUMBER:
    if(tlvs[i].valuelen)
    {
        API_memcpy(tpar->term_ifd, tlvs[i].pvalue, tlvs[i].valuelen);      // term_ifd[8]
    }
    break;
...
case TAG_9F16_MERCHANT_IDENTIFIER:
    if(tlvs[i].valuelen > 0)
    {
        API_memcpy(tpar->merchant_id, tlvs[i].pvalue, tlvs[i].valuelen);   // merchant_id[15]
    }
...
// src/emvporting.c:432-435（AID 参数分支）
case TAG_9F06_AID_TERMINAL:
    apar->aid_len = tlvs[i].valuelen;
    API_memcpy(apar->aid, tlvs[i].pvalue, tlvs[i].valuelen);               // aid[16]
    break;
...
// src/emvporting.c:625-636（结构体尾部字段）
case TAG_9F33_TERMINAL_CAPABILITIES:
    if(tlvs[i].valuelen)
    {
        emv_memcpy(apar->term_caps, tlvs[i].pvalue, tlvs[i].valuelen);     // term_caps[3]
    }
    break;
case TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES:
    if(tlvs[i].valuelen)
    {
        emv_memcpy(apar->term_add_caps, tlvs[i].pvalue, tlvs[i].valuelen); // term_add_caps[5]
    }
    break;
```

- 影响：
  1. `EMV_TLV_Decode` 只保证 `valuelen ≤ 输入剩余长度`（`src/emvtlv.c:308-316`），`emvporting_set_termparam`（`:670-699`）**完全不做 TLV 解码/字段校验**即落盘，`emvporting_add_aidparam`（`:864-869`）只校验"能解码 + 含 9F06/9C/9F2A"，不校验各 tag 的值长度。因此任意长度的合法 TLV 字段都能存进 `emvterm*.par`/`emvaids*.par`，最长 `len≈1016` 字节。
  2. 内核在每次非接交易预处理时用**栈上对象**接收这些字段：`src/emv.c:1154` `EMV_AIDPARAM aid_param={0};`，`src/emv.c:1183` `EMVPORT_GetAppParam(&aid_param, i);`（`emv_preprocess` 对外 API 见 `src/emvapi.c:274-281`）→ `src/emvspi.c:1164-1171` → `emvporting_get_aidparam` → `conv_param(tpar, apar, ...)`（`src/emvporting.c:1088`）。
  3. 以同版本同源头文件（见"审查局限"）核对结构体布局：`EMV_AIDPARAM` 共 1113 字节，`term_caps[3]` 位于偏移 1099、`term_add_caps[5]` 位于偏移 1102、`aid[16]` 位于偏移 486。发送一个 9F33 值长度 200 的 AID 参数，`emv_memcpy(term_caps, pvalue, 200)` 会写到偏移 1099..1298，即**越过整个 `EMV_AIDPARAM` 约 185 字节**（长度 1016 时越界约 1000 字节）；9F06/9F01/9F16/DF8124/DF8125 同理。这是由"主机下发 TLV"直接触发的栈缓冲区写越界，可导致栈上相邻变量/返回地址被破坏（崩溃或控制流被劫持）。
  4. 同一问题也存在于 `conv_param(tpar, ...)` 路径：`emvporting_get_termparam`（`:701-737`）写入调用方 `EMV_TERMPARAM`（486 字节），9F01 长度 >6 即越界。
- 建议：为每个 tag 定义 EMV 期望长度并在 `conv_param` 内逐 tag 校验（长度不符或超 `sizeof(目标字段)` 一律拒绝该条/整个参数报文）；至少统一改为 `n = MIN(valuelen, sizeof(dst)); memcpy(dst, pvalue, n);` 并补零；把字段级校验前移到 `emvporting_set_termparam`/`emvporting_add_aidparam` 落盘时，避免坏参数进入 Flash。

### [P0] D-02 9F1B 六字节分支把 12 字节写进 4 字节栈缓冲（emv_bcd2hex 语义误用）

- 位置：`src/emvporting.c:296`、`:562-565`；被调函数 `src/emv.c:439-466`
- 证据：

```c
// src/emvporting.c:292-296
static void conv_param(EMV_TERMPARAM* tpar, EMV_AIDPARAM* apar, u8* in, int inlen)
{
    EMV_TLVOBJ tlvs[64];
    int i, tlvnum;
    unsigned char term_floor_lmt[4]={0};          // 只有 4 字节
...
// src/emvporting.c:562-565
else if(tlvs[i].valuelen == 6)
{
    emv_bcd2hex(tlvs[i].pvalue, tlvs[i].valuelen, term_floor_lmt);   // 第 2 参数=6
    API_memcpy(apar->term_floor_limit, term_floor_lmt, sizeof(apar->term_floor_limit));
}

// src/emv.c:439-466：emv_bcd2hex 对每个输入 BCD 字节输出两个 nibble 字节
for (i = 0; i < bcd_len; i++)
{
    ...
    hex_data[2 * i]     = high_nibble;
    hex_data[2 * i + 1] = low_nibble;
}
return bcd_len * 2;
```

- 影响：`emv_bcd2hex(..., 6, term_floor_lmt)` 固定写 `2*6 = 12` 字节，而 `term_floor_lmt` 只有 4 字节 → **在同一栈帧内多写 8 字节**，破坏 `conv_param` 的相邻局部变量（编译器布局不同，可能改写 `tlvs/i/tlvnum` 或调用者保存区），属可复现的栈缓冲区溢出。触发条件极具体：主机下发长度恰为 6 的 9F1B（代码显式支持该分支，说明现场会遇到）。
  附带功能错误：即使不越界，`emv_bcd2hex` 输出的是"每 digit 一字节"的展开结果，再拷前 4 字节得到的并不是 12 位 BCD 金额，`term_floor_limit` 的数值语义错误。
- 建议：12 位 BCD 金额应使用 `emv_bcd2dec()`（`src/emv.c:420-437`）转 `uint32_t` 后按终端内部编码写入；如确需 6 字节输入，缓冲至少 `2*6` 并显式校验长度/返回值，禁止"函数语义与目标缓冲尺寸靠约定"的写法。

### [P1] D-03 get_termapps 无条目数上限，向内核固定缓冲区越界写入

- 位置：`src/emvporting.c:1143-1245`（写入循环 `:1160-1241`）；调用方 `src/emv.c:1013-1030`、`:1055`；缓冲区 `src/emvspi.c:895-904`
- 证据：

```c
// src/emvporting.c:1143-1151
int32_t emvporting_get_termapps(EMV_TERMAPP* apps)
{
    int fd, ret;
    u8 apps_num = 0;
    EMV_TERMAPP* tmp = apps;
...
// src/emvporting.c:1239-1240：每读到一条合法记录就写出一个元素，无任何上限
        apps_num++;
        tmp++;
...
// src/emv.c:1013-1030：先让移植层往 g_emv_cnxt->termapps 写，之后才检查数量
    g_emv_cnxt->termapps = (EMV_TERMAPP*)current_ptr;
    g_emv_cnxt->termapps_num = EMVPORT_GetTermApps(g_emv_cnxt->termapps);
    if(g_emv_cnxt->termapps_num < 1 || g_emv_cnxt->termapps_num > EMV_CFG_MAX_APPS)
    { ... ret = rERR(EMV_GETTERMAPPSERR); }
    uint32_t termapps_size = sizeof(EMV_TERMAPP) * g_emv_cnxt->termapps_num;
    current_ptr += ((termapps_size + 3) & ~3);

// src/emvspi.c:895-903：内核全局缓冲固定 14KB+256，且其后的 TLVBUFFER_SIZE=10KB
uint8_t* weak_alloc_buffer(int32_t* size)
{
    static uint8_t kernel_global_buffer[14*1024+256];
    ...
    *size = sizeof(kernel_global_buffer);
```

- 影响：`emvporting_get_termapps` 按 AID 文件记录数（每记录 1024 字节，条数不受 `add_aidparam` 限制）向 `g_emv_cnxt->termapps` 连续写 `sizeof(EMV_TERMAPP)` 字节（同版本头文件为 23 字节）。该区域位于 14592 字节全局静态缓冲的头部，其后紧接 10KB 的 TLV 缓冲与候选表；按缓冲布局估算 termapps 实际可用量级约 1~2KB，**AID 文件约 50~90 条记录即可越过 `kernel_global_buffer` 末尾写入相邻 BSS/全局变量**。`EMV_CFG_MAX_APPS(128)` 与 `next_ptr > ptr+len`（`src/emv.c:1055`）都是"写完之后"才检查，无法阻止已发生的越界写；`apps_num` 为 `u8`，≥256 条时回绕。
- 建议：接口增加容量参数（`max_num`）或读取文件头中的条数并先校验；移植层写满即停止并返回实际条数；`EMV_Init` 在调用前按缓冲区余量计算上限并传入。

### [P1] D-04 CAPK 校验结果被丢弃；RSA 校验哈希算法/比较长度与 EMV 不一致

- 位置：`src/emvporting.c:1438-1441`（丢弃结果）、`:1288-1301`（RSA 分支）、`:1258-1281`（SM2 分支）
- 证据：

```c
// src/emvporting.c:1422-1442
    if (!(pos = EMV_TLV_Find(0, TAG_DF03_CAPK_CHECKSUM, tlvs, SEARCH_ALL_DESC)))
    {
        API_memset(out->checksum, 0x00, 20);
        out->hash_algorithm = 0x00;
    }
    else
    {
    if (tlvs[pos].valuelen != 20 && tlvs[pos].valuelen != 64)
    {
        return -91;
    }
    API_memcpy(out->checksum, tlvs[pos].pvalue, tlvs[pos].valuelen);
    if(checkCAPKhash(out))          // ← 校验失败被静默忽略
    {
        //return -92;
    }
    }
    return 0;

// src/emvporting.c:1282-1304（RSA 分支用 CAPK 自带的 hash_algorithm，然后只比较 20 字节）
    else if (cert.algorithm == EMV_PUBKEY_ALG_RSA)
    {
        ...
        hash(cert.hash_algorithm, mesg, len, digest);       // 可为 SHA-256/SM3/SHA-512
        if (API_memcmp(digest, cert.checksum, 20))
        {
            return rERR(DA_CAPKCHKERR);
        }
    }
```

- 影响：
  1. 校验失败的 CA 公钥照样被 `emvporting_add_capkparam` 写入 Flash 并返回成功（`:1544-1605`），DDA/发卡行认证将使用未通过完整性校验的 CAPK → 违反 EMV Book 4 "CAPK Load Fail Action" 相关行为，属安全/规范违规；注释掉的 `return -92` 说明是有意绕过，需确认是否应保留。
  2. EMV Book 2 规定 RSA CAPK 的 checksum 是对 `RID||Index||Modulus||Exponent` 计算 **SHA-1（20 字节）**；代码用 CAPK 报文里的 `hash_algorithm` 字段作为校验哈希，在 `hash_algorithm=SHA-256/SM3/SHA-512` 时会用错误哈希，再与 20 字节 `checksum` 比较（只比前 20 字节），结果要么误拒合法 CAPK、要么仅比较 160 bit；而在校验结果被丢弃的情况下，实际是"校验形同虚设"。
  3. SM2 分支固定 `hash(EMV_HASH_ALG_SHA1, ...)`（`:1271`），完全忽略 `hash_algorithm`；PBOC SM2 CAPK 的校验哈希应为 SM3（32 字节）还是 SHA-1，建议对照 `E:\Work\SVN Project\emv_doc` 中 PBOC 2018 规范确认后统一。
- 建议：恢复 `checkCAPKhash` 失败时的拒绝（保留可配置开关并默认拒绝）；RSA 校验固定 SHA-1、按 20 字节比较；SM2/ECC 按其规范算法与长度校验；`checksum` 长度按算法校验并保证结构体字段足够（当前 `EMV_CAPKPARAM.checksum` 只有 20 字节，见 D-15）。

### [P1] D-05 EMV_FNAME 未处理 _PARAM 且 kernel_index 恒为 0：内核索引/TSC 无法保存，多内核共用同一组参数文件，并对 SDK 传 NULL 文件名

- 位置：`src/emvporting.c:109-159`（文件名构造）、`:179-256`（apipara 初始化/保存、get/set_kernel_index）、`:258-268`（TSC）；调用方 `src/emvapi.c:783-795`、`src/emvspi.c:1255-1261`
- 证据：

```c
// src/emvporting.c:109-159
static char* EMV_FNAME(int32_t type)
{
    static char fname[64] = {0};
    uint8_t kernel_index = 0;                 // ← 恒为 0
/*
    switch (type)
    {
        case _PARAM:
            API_strcpy(fname, "emv.par");
            return fname;
        default:
            emvporting_get_kernel_index(&kernel_index);   // ← 整段被注释
            ...
    }
*/
    switch (type)
    {
        case _TERMPARAM:
            API_sprintf(fname, "emvterm%d.par", kernel_index);   // 永远 0
...
        default:
            return NULL;                      // _PARAM → NULL
    }
    return fname;
}

// src/emvporting.c:188-190（apipara_init，每次 get_tsc/get_kernel_index 都会走到）
    if ((fd = API_fopen(EMV_FNAME(_PARAM), F_RDWR)) <= 0)          // EMV_FNAME(_PARAM) == NULL
    {
        if ((fd = API_fopen(EMV_FNAME(_PARAM), F_RDWR|F_CREAT)) <= 0)  // 仍为 NULL
        {
            return rERR(ErFOPEN);
        }
```

- 影响：
  1. `_PARAM` 在枚举中值为 0，但 `switch` 里被整体注释掉，`default` 返回 NULL：`emvporting_apipara_init`/`emvporting_apipara_save` 永远失败，`emvporting_get_tsc/set_tsc`、`emvporting_get/set_kernel_index` 都失效。TSC（9F41 交易序列计数器）掉电后回退，重复/回退的 TSC 可能触发收单侧重复交易判断；内核索引设置也无法生效（`emv_set_kernel_index` 返回 EMV_OK 但无持久化）。
  2. 即使 `_PARAM` 分支恢复，`kernel_index` 局部变量也从未从 `g_apipara_ctx` 取值，所有文件名恒为 `*0.par`：多内核终端（`KERNEL_MAX_INX=7`）的参数/AID/CAPK/黑名单全部共用内核 0 的文件。
  3. `API_fopen(NULL, ...)` 被传给平台 API；SDK 头文件（`include/ks_stdlib.h:180-187`）未定义 NULL 文件名行为，静态看属未定义行为，若平台实现直接 `strcpy` 文件名则调用即崩溃。
- 建议：恢复 `_PARAM` 的文件名分支（注意 SDK 8.3 文件名限制，如 `emv.par`/`emvcfg.par`）；`EMV_FNAME` 内先 `emvporting_get_kernel_index(&kernel_index)` 并校验 `kernel_index <= KERNEL_MAX_INX`；`emvporting_apipara_*` 对 `EMV_FNAME` 返回 NULL 做防护；`set_kernel_index` 失败时向调用方返回错误。

### [P1] D-06 transwater 保存/读取对长度不做任何校验即交给 API_fwrite/API_fread

- 位置：`src/emvporting.c:1985-2010`（save）、`:2020-2048`（get）；SDK 契约 `include/ks_stdlib.h:207-225`
- 证据：

```c
// src/emvporting.c:1985-2006（len 来自应用，未校验）
int32_t emvporting_save_transwater(uint8_t* buf, int32_t len)
{
    ...
    API_fseek(fd, 0, F_SEEK_END);
    if (API_fwrite(fd, (uint8_t*)&len, 4) != 4) { ... }
    if (API_fwrite(fd, buf, len) != len) { ... }          // len 为负/超大 → u32 count 巨值

// src/emvporting.c:2020-2042
int32_t emvporting_get_transwater(int32_t inx, uint8_t* buf, int32_t *len)
{
    ...
    API_fseek(fd, inx, F_SEEK_SET);                       // 返回值未检查，inx 也未校验
    if (API_fread(fd, (uint8_t*)&length, 4) != 4) { ... }
    if (API_fread(fd, buf, length) != length)             // length 来自文件，可为负/超大
    { ... }

// include/ks_stdlib.h:215
extern int 	API_fread(int fd,void * buf, u32 count);
```

- 影响：`API_fread/API_fwrite` 的 `count` 是 `u32`，负数 `len/length` 会转换为约 4G 的计数：
  - `emv_save_transwater(buf, -1)` → 从 `buf` 读 4G 字节写入文件，必然越界读或崩溃；正数超大值同理。
  - 文件头与数据分两次写（`:1996-2006`），掉电半写后 `length` 可能是垃圾；`emv_get_transwater` 直接以该值读入调用方缓冲区，可越界写。
  - `inx` 作为字节偏移直接 `API_fseek(fd, inx, ...)`，负值/超界时 seek 失败被忽略，之后从当前偏移（0）读取 → 返回错误数据。
- 建议：`len` 限制 `1 <= len <= 上限（如 1024/2048）`；读取时校验 `length` 与文件剩余大小、与调用方缓冲容量（接口增加输入容量参数）；检查 `API_fseek` 返回值；写文件使用"长度+数据"单次写或先写数据后写长度并校验。

### [P2] D-07 定长字段"只校验非零"就整段拷贝，短值会用后续 TLV 字节污染参数（含 ICS 等安全相关配置）

- 位置：`src/emvporting.c:313`、`:363`、`:369`、`:375`、`:381`、`:451`、`:469`、`:489`、`:502`、`:509`、`:516`、`:536`、`:543`、`:550`
- 证据：

```c
// src/emvporting.c:312-314（ICS 固定拷 7 字节）
case TAG_ICS:
    API_memcpy(tpar->ics, tlvs[i].pvalue, 7);
    break;
// src/emvporting.c:366-370（9F1C 固定拷 sizeof=8）
case TAG_9F1C_TERMINAL_IDENTIFICATION:
    if(tlvs[i].valuelen)
    {
        API_memcpy(tpar->term_id, tlvs[i].pvalue, sizeof(tpar->term_id));
    }
// src/emvporting.c:498-503（DF11 TAC 固定拷 5，只校验非零）
case TAG_DF11_TAC_DEFAULT:
    if(tlvs[i].valuelen)
    {
        apar->term_action_code_default_exist = 1;
        API_memcpy(apar->term_action_code_default, tlvs[i].pvalue, 5);
    }
```

- 影响：长度不足的 TLV（如 1 字节 ICS、2 字节 9F1C、3 字节 DF11）会把"该值之后的 tag/len 字节"当成配置数据拷入，静默改掉终端 ICS/TAC/限额等安全相关参数；若该值位于输入缓冲末尾，还会从 `buf[PARA_SAVLEN]` 越界读出最多 19 字节栈内容。虽然不直接破坏内存，但能让终端行为与主机预期不一致（例如 ICS 的 DA 支持位被意外置位）。
- 建议：每个 tag 校验精确长度（EMV 规范长度）或"补零 + `MIN(valuelen, sizeof)`"；对关键字段（ICS/TAC/限额）长度不符时报错拒绝整包。

### [P2] D-08 get_termapps 跳过的记录与 get_aidparam 的文件索引不对应

- 位置：`src/emvporting.c:1183-1237`（跳过无效记录）与 `:1064`（按文件记录下标读取）；调用方 `src/emv.c:1178-1183`
- 证据：

```c
// src/emvporting.c:1183-1187：解码失败则 continue，不计数
    if (EMV_TLV_Decode(par, parlen, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), STRING_TLVOBJ))
    {
        TRACE("emvport-error: get aid param is not invalid TLV format");
        continue;
    }
...
// src/emvporting.c:1194-1237：缺少 9F06 也 continue；9F06 长度>16 时该记录仍被计数
    if (tlvs[pos].valuelen <= 16) { ... tmp->enable = 1; }
...
// src/emvporting.c:1064：内核按 termapps 下标 i 直接取文件第 i 条记录
    if (API_fseek(fd, inx * PARA_SAVLEN, F_SEEK_SET) < 0)
```

- 影响：只要 AID 文件里有 1 条被 `get_termapps` 跳过的记录（文件被 `emv_write_file` 直接改写、掉电损坏、或 9F06>16 字节使该条成为 `aid_len=0/enable=0` 但仍计数），`termapps[i]` 与文件第 i 条记录就不再对应，`EMVPORT_GetAppParam(&aid_param, i)` 会拿到**另一个 AID 的参数**（限额/CVM/内核 ID 错配），属实际交易可触发的选应用/限额判断错误。
- 建议：`get_termapps` 与 `get_aidparam` 使用同一套"记录有效性"判定；跳过记录时把文件索引一起带回（或让 get_aidparam 也跳过同样记录）；9F06 长度非法时整条视为无效且不计数。

### [P2] D-09 多文件写入忽略 API_fseek 返回值，seek 失败时覆盖已有数据（含文件头/首记录）

- 位置：`src/emvporting.c:923`、`:1723`、`:1932`、`:1994`、`:2030`、`:2077`、`:2205`；`API_fseek` 契约 `include/ks_stdlib.h:197-205`
- 证据：

```c
// src/emvporting.c:1932（sav_translog 追加写，seek 失败未检查）
    API_fseek(fd, 0, F_SEEK_END);
    if (API_fwrite(fd, &log, sizeof(TRANSLOG)) != sizeof(TRANSLOG))
// src/emvporting.c:2077（write_file 追加写）
    API_fseek(fd, 0, F_SEEK_END);
    if (API_fwrite(fd, buf, buflen) != buflen)
```

- 影响：`API_fseek` 返回 -1 时写位置仍在 0（或上一读位置），后续 `fwrite` 会覆盖文件头/第 0 条记录而不是追加：交易日志被覆盖、`emv_write_file` 下载的 CAPK/AID 文件开头被改写。静默数据损坏，且无任何错误上报。
- 建议：所有 seek 检查返回值；写前把"当前位置 == 预期偏移"作为失败条件返回错误；对关键文件先写临时文件再原子改名（SDK 有 `API_fchange`，`include/ks_stdlib.h:250-257`）。

### [P2] D-10 参数/记录写入无 fsync、无掉电保护；半写后果被放大

- 位置：`src/emvporting.c:684-697`（终端参数"长度+数据"两次写）、`:1909-1941`（translog）、`:1985-2009`（transwater）、`:1934`、`:1996-2006`、`:2079-2085`；SDK 提供 `API_fsync`（`include/ks_stdlib.h:258-261`）但全文未调用
- 证据：

```c
// src/emvporting.c:684-694
    if (API_fwrite(fd, (void*)&len, PARA_LENLEN) != PARA_LENLEN) { ... }
    if (API_fwrite(fd, par, len) != len) { ... }
    API_fclose(fd);
```

- 影响：长度头与数据分两次写且无 flush/原子性；掉电后可能出现"头长度 > 实际数据"，下次 `emvporting_get_termparam` 会把未初始化栈数据当参数解析（见 D-12）；CAPK/AID 文件半写记录被 `fsize % record` 检查发现后**整文件删除**（`:911-916`、`:1550-1555`、`:1815-1820`、`:2192-2197`），一次掉电可能丢掉全部已下载 CAPK（终端无法做 DDA）。
- 建议：`emvporting_write_file` 用于整包下载时写临时文件+校验长度+`API_fsync`+改名；记录型文件采用"写新记录->校验->标记有效"；关键参数文件加双备份/CRC。

### [P2] D-11 读取参数文件未校验实际读入长度，未初始化栈数据被解析或返回给应用

- 位置：`src/emvporting.c:704-735`（get_termparam）、`:781-816`（get_termparam_tlv）
- 证据：

```c
// src/emvporting.c:719-735
    if (API_fread(fd, buf, PARA_SAVLEN) < PARA_LENLEN)     // 只要求 >=4
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }
    API_fclose(fd);
    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN) { return rERR(ILL_TERMPARAMLEN); }
    ptr = buf + PARA_LENLEN;
    init_param(par, NULL);
    conv_param(par, NULL, ptr, len);                        // len 可能 > 实际读入字节
...
// src/emvporting.c:798-815（get_termparam_tlv）
    if (API_fread(fd, buf, PARA_SAVLEN) < PARA_LENLEN) { ... }
    ...
    API_memcpy(par, buf + PARA_LENLEN, len);                // 未初始化栈内容直接返回给应用
```

- 影响：文件被截断/半写（D-10）或长度头异常时，`len` 声称的字节数大于实际读入量，`conv_param` 会解析 `buf` 中未初始化栈数据 → 参数随机；`emv_get_termparam` 对外 API 会把未初始化栈内容拷给主机应用（信息泄露），且返回 `EMV_OK`。
- 建议：记录 `API_fread` 实际返回值并校验 `ret == 4 + len`（不足则按错误处理）；`buf` 先清零；对返回给应用的接口同时核对调用方容量。

### [P2] D-12 CAPK checksum 长度处理自相矛盾：读入允许 20/64、结构只有 checksum[20]、导出只写 20

- 位置：`src/emvporting.c:1432-1436`、`:1517-1520`；结构 `src/inc/emv_type.h:635-636`
- 证据：

```c
// 读入：允许 20 或 64 字节，64 会写穿 checksum[20] 进入 rsv
    if (tlvs[pos].valuelen != 20 && tlvs[pos].valuelen != 64)
    {
        return -91;
    }
    API_memcpy(out->checksum, tlvs[pos].pvalue, tlvs[pos].valuelen);
// 导出：固定按 20 字节
    conv_tag_to_buf(TAG_DF03_CAPK_CHECKSUM, 0x14, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), in->checksum, 20);
```

- 影响：64 字节 checksum 写入 44 字节到 `rsv[228]`（未跨结构体，但数据错位）；而 32 字节 checksum（PBOC SHA-256/SM3 CAPK 常见格式）会被 `return -91` 直接拒绝，导致此类 CAPK 无法导入；`emv_get_capkparam` 导出的 CAPK 只有 20 字节 checksum，重新导入后校验值被截断。三种宽度互不兼容。
- 建议：按 `algorithm/hash_algorithm` 确定 checksum 长度（20/32/64），结构体改用可变长度存储（如 `checksum[64]` + `checksum_len`），导入/导出/校验三处统一。

### [P2] D-13 交易日志：PAN>10 字节时截断存储但保存原长度；比较越界且金额累计错误；文件无容量上限

- 位置：`src/emvporting.c:1909-1942`（保存）、`:1944-1973`（读取累计）
- 证据：

```c
// src/emvporting.c:1912-1925
    TRANSLOG log;                       // { uint32_t amt; uint8_t pan[10]; uint8_t panlen; uint8_t rsv; }
    CLEAR(log);
    log.amt = amount;
    if(panlen>10)  { API_memcpy(log.pan, pan, 10); }    // 只存 10 字节
    else           { API_memcpy(log.pan, pan, panlen); }
    log.panlen = panlen;                                // 却保存原始长度（可 >10）
// src/emvporting.c:1961-1962
        if (panlen == log.panlen
                && API_memcmp(pan, log.pan, panlen) == 0)   // panlen 可 > sizeof(log.pan)
```

- 影响：代码注释自述"V2CL0620010 记录的是 257 字节数据，PAN 5A 最多 248 字节"（`:1917`），即真实卡片可传 >10 字节 PAN。保存时截成 10 字节但 `log.panlen` 记原始长度，读取时 `memcmp(pan, log.pan, panlen)` 从 16 字节 `TRANSLOG` 栈对象越界读最多两百多字节；比较结果随机 → 离线累计金额（`policy=0/1`）错误，可能让超限交易被当作未超限。另：每笔交易追加 16 字节、无条数/大小上限，长期使用会耗尽 Flash，且 `get_translog_amount` 每次全表扫描。
- 建议：`panlen` 限制 ≤10（与 `EMV_CARDBLACK` 一致）或不截断存储（`pan[19]`）；`memcmp` 长度统一取 `MIN(panlen, sizeof(log.pan))`；增加最大记录数/环形覆盖或清理策略。

### [P2] D-14 write_file 的 filesize 上限未覆盖本次 buflen，static size 还会跨文件残留

- 位置：`src/emvporting.c:2051-2089`；公开 API `src/emvapi.c:797-801`、`inc/emvapi.h:697-708`
- 证据：

```c
int32_t emvporting_write_file(char *filename, uint32_t filesize, uint8_t *buf, uint32_t buflen, uint32_t flag)
{
    static int32_t size = 0;                 // 跨调用/跨文件保留
    ...
    if(1 == flag) { size = 0; API_fremove(filename); }
    filerealsize = API_fsize(filename);
    if(size>=filesize || (filerealsize>0&&filerealsize>=filesize))
    {
        return rERR(ErWRCOMPLETE);
    }
    ...
    API_fseek(fd, 0, F_SEEK_END);
    if (API_fwrite(fd, buf, buflen) != buflen) { ... }
    size += buflen;
```

- 影响：
  1. 只比较"已写 size"与 `filesize`，不校验 `size + buflen`：最后一次分片可把文件写到超过声明大小（主机校验/后续解析按 filesize 读取时会截断或越界）。
  2. `size` 为函数级 static：连续下载文件 A、B，若 B 首次调用未带 `flag=1`，`size` 保留 A 的累计值，可能提前返回 `ErWRCOMPLETE`（文件内容不全）或把下限算错。
  3. `filename` 由主机任意指定，可绕过 `add_aidparam/add_capkparam` 的校验直接写 `emvaids*.par`/`emvcapk*.par` 等（见 D-08 的索引错配与 D-01 的坏参数来源）。
  4. `API_fremove`/`API_fseek` 返回值未检查。
- 建议：静态状态改为按文件记录（或要求调用方显式传已写长度）；校验 `buflen <= filesize - size`；限制可写文件名白名单；检查 remove/seek。

### [P2] D-15 DRL 记录仅按 Index 匹配，忽略 Flag；conv_drlparam 的 pvalue[0] 未校验长度

- 位置：`src/emvporting.c:2090-2164`（conv_drlparam）、`:2175-2243`（add_drl，匹配 `:2213`）
- 证据：

```c
// src/emvporting.c:2103-2115
    if (pos = EMV_TLV_Find(0, 0xDF22, tlvs, SEARCH_ALL_DESC))
    {
        if((tlvs[pos].pvalue[0] & 0x80) == 0x80)      // 未检查 valuelen>0
        { out->Flag = 1; out->Index = tlvs[pos].pvalue[0]; }
        else
        { out->Flag = 0; out->Index = tlvs[pos].pvalue[0] & 0x0F; }
    }
// src/emvporting.c:2211-2218：去重只看 Index
        if (ret == sizeof(BCTC_DRL))
        {
            if (rDRL.Index == DRL.Index) { TRACE("EXIST"); ... return 0; }
```

- 影响：`BCTC_DRL` 同时有 `Flag`（1-默认 DRL / 0-普通 DRL）和 `Index`，但去重只比 `Index`：先加 Default DRL(Flag=1, Index=x) 后再加普通 DRL(Index=x) 会直接当作已存在丢弃（或反之），默认/普通 DRL 分组失效，非接限额判定用错组。`pvalue[0]` 在零长度 DF22 时读到下一个字节（1 字节越界读）。
- 建议：匹配 `(Flag, Index)`；DF22/DF19/DF20/DF21 均校验 `valuelen>0` 与期望长度（6）。

### [P2] D-16 公开的卡片黑名单接口不校验 pan_len；后续按文件内 pan_len 做 memcmp 会越界读

- 位置：`src/emvporting.c:1701-1760`（add_cardblack）、`:1762-1792`（fnd_cardblack）；公开入口 `src/emvapi.c:710-717`
- 证据：

```c
// src/emvporting.c:1730-1732
            if (blk->pan_len == temp.pan_len
                    && API_memcmp(blk->pan, temp.pan, temp.pan_len) == 0   // temp.pan_len 可 >10
                    && blk->psn == temp.psn)
// src/emvporting.c:1781-1783
        if (panlen == blk.pan_len
                && API_memcmp(pan, blk.pan, panlen) == 0)
```

- 影响：`emv_add_cardblack(void* blk)` 直接把主机给的 `EMV_CARDBLACK` 落盘，未校验 `pan_len <= 10`；之后 add/find 用文件里的 `pan_len`（可达 255）做 `memcmp`，从 16 字节 `temp/blk` 栈对象越界读最多约 240 字节（可能崩溃或比较到随机内存导致黑名单误命中/漏命中——漏命中属资金风险）。
- 建议：写盘前强制 `pan_len <= sizeof(pan)`；读取后同样校验，非法记录跳过或删除。

### [P2] D-17 set_termparam 不做任何 TLV/字段校验，坏参数直达 Flash 且后续静默回退默认值

- 位置：`src/emvporting.c:670-699`（set_termparam）对比 `:859-869`（add_aidparam 至少做结构校验）
- 证据：

```c
int32_t emvporting_set_termparam(uint8_t* par, int32_t len)
{
    ...
    if (par == NULL || len <= 0 || len > PARA_MAXLEN) { return rERR(ErFUNCPAR); }
    if ((fd = API_fopen(EMV_FNAME(_TERMPARAM), F_WRONLY|F_CREAT)) <= 0) { ... }
    if (API_fwrite(fd, (void*)&len, PARA_LENLEN) != PARA_LENLEN) { ... }
    if (API_fwrite(fd, par, len) != len) { ... }
```

- 影响：长度合法即落盘，不校验是否为合法 TLV、各 tag 长度是否合法（这正是 D-01/D-02 的输入来源）；`emvporting_get_aidparam` 读到坏 TLV 时 `conv_param` 会 `memset(tlvs)` 静默使用 `init_param` 的默认 ICS（`:298-302`、`:285-290`），应用侧拿不到错误 → 终端行为与下发的参数不一致且难以排查。
- 建议：写入前做一次与 `conv_param` 相同的严格解析（结构性 + 字段长度），失败返回 `EMV_FAIL`；读取失败（含 TLV 解析失败）上报错误而不是静默用默认值。

### [P3] D-18 conv_param 中 9F05 分支 break 后残留死语句

- 位置：`src/emvporting.c:404-423`
- 证据：

```c
            case TAG_9F05_APPLICATION_DISCRETIONARY_DATA:
                ...
                if(tpar->proprietary_tag_len)
                {
                    API_memcpy(tpar->proprietary_tag, tlvs[i].pvalue, tpar->proprietary_tag_len);
                }
                break;
                tpar->trans_currency_exponent = *(tlvs[i].pvalue);   // break 之后，永不执行
```

- 影响：死代码，易误读为 9F05 会写 `trans_currency_exponent`（实际不会）；也说明该 switch 经过多人改动后未清理。
- 建议：删除该死语句。

### [P3] D-19 get_aidparam 的 for(;;){...break; return ...;} 死代码使 TERMPARAM 读取错误被静默忽略

- 位置：`src/emvporting.c:1019-1056`
- 证据：

```c
for(;;)
{
    if ((fd = API_fopen(EMV_FNAME(_TERMPARAM), F_RDONLY)) <= 0)
    {
        rERR(ErFOPEN);break;
        return rERR(ErFOPEN);        // 不可达
    }
    ...
    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN)
    {
        rERR(ILL_AIDPARAMLEN);break;
        return rERR(ILL_AIDPARAMLEN); // 不可达
    }
    ...
    conv_param(tpar, apar, ptr, len);
    break;
}
```

- 影响：终端参数文件缺失/损坏时被当作"无终端参数"继续返回 AID 参数（配合 D-17 静默默认值）；`rERR(...)` 单独成句无任何作用，误导维护者以为已处理错误。
- 建议：删除 for(;;) 与死代码，按真实意图直接 `return rERR(...)`（或明确改为"容错继续"并注释）。

### [P3] D-20 零长度 TLV 的固定 1/5 字节解引用未校验 valuelen

- 位置：`src/emvporting.c:886`、`:896`（add_aidparam 的 9C/9F2A）、`:1205`、`:1215`、`:1225`（get_termapps）、`:1336`、`:1343`、`:1353`、`:1364`（conv_capkparam）、`:527`、`:530`（conv_param apar）
- 证据：

```c
// src/emvporting.c:886
    trans_type = *(tlvs[pos_type].pvalue);          // 未判 valuelen
// src/emvporting.c:1336（RID 固定拷 5 字节）
    API_memcpy(out->rid, tlvs[pos].pvalue, 5);      // 9F06 长度可 <5
// src/emvporting.c:1343
    out->index = *(tlvs[pos].pvalue);
```

- 影响：`EMV_TLV_Decode` 允许长度 0 的 TLV（`src/emvtlv.c:224-227`，`pvalue` 指向值后第一个字节），零长度时读取 1 字节（RID 读 5 字节）会读到该 TLV 之后的数据；当该 TLV 位于输入缓冲末尾时越界读 1~4 字节。数据语义错乱（CAPK RID/Index 可能取到相邻 TLV 字节），影响 CAPK 匹配。
- 建议：统一先校验 `valuelen` 与期望长度相等（或至少 >0）再解引用/拷贝。

### [P3] D-21 clr_capkparam 忽略删除失败仍报成功；get_capkparam 未找到时返回 ErFREAD，与错误码语义不符

- 位置：`src/emvporting.c:1525-1532`、`:1608-1643`
- 证据：

```c
int32_t emvporting_clr_capkparam(void)
{
    if (API_fremove(EMV_FNAME(_CAPKPARAM)))
    {
//        return rERR(ErFREMOVE);
    }
    return 0;                                   // 删除失败也返回成功
}
...
    while(1)
    {
        ret = API_fread(fd, &pk, sizeof(EMV_CAPKPARAM));
        if (ret != sizeof(EMV_CAPKPARAM))
        {
            API_fclose(fd);
            return rERR(ErFREAD);               // 文件读完/未找到 PKI 也报读错误
        }
        if (pk.index == pki && API_memcmp(pk.rid, rid, 5) == 0) { ... }
    }
```

- 影响：`emv_clr_capkparam` 上报成功但 CAPK 可能未删（后续交易用旧密钥）；`get_capkparam` 对"未找到该 RID/PKI"返回 `ErFREAD(-5)`，上层无法区分 IO 故障与密钥不存在（`ErNOTFINDCAPK=-20` 已定义未用），可能把"缺 CAPK"误判为"读卡器/存储故障"。
- 建议：恢复 `return rERR(ErFREMOVE)`；未找到统一返回 `rERR(ErNOTFINDCAPK)`。

### [P3] D-22 set_kernel_index 的边界判断是 off-by-one

- 位置：`src/emvporting.c:250-256`；枚举 `src/inc/emv_type.h:538-548`
- 证据：

```c
void emvporting_set_kernel_index(uint8_t val)
{
	if(val > KERNEL_MAX_INX)        // KERNEL_INX_* = 0..6，KERNEL_MAX_INX = 7（"最大支持内核个数"）
		val = 0;
    g_apipara_ctx.kernel_index = val;
```

- 影响：允许 `val == 7` 写入内核索引，而合法索引为 0..6（7 是"个数"），一旦按索引生成 `*7.par` 会落到不存在的内核文件（当前因 D-05 文件名恒 0 而未显现，修复 D-05 后会暴露）。
- 建议：`if (val >= KERNEL_MAX_INX) val = 0;` 或按 `KERNEL_INX_NA..KERNEL_INX_QMSDC` 校验。

### [P3] D-23 EMV_FNAME 用函数内 static 缓冲；get_transwatername 无长度限制且非线程安全

- 位置：`src/emvporting.c:109-159`、`:2012-2018`
- 证据：

```c
static char* EMV_FNAME(int32_t type)
{
    static char fname[64] = {0};          // 全部调用共享
...
void emvporting_get_transwatername(char *name)
{
	API_strcpy(name, EMV_FNAME(_TRANSWATER));   // 无目标长度/无 NULL 检查
}
```

- 影响：后续若在同一表达式/多任务中两次调用 `EMV_FNAME`，两个文件名会互相覆盖（如 `API_fopen(EMV_FNAME(a), ...) == EMV_FNAME(b)` 之类写法）；`get_transwatername` 不校验 `name` 容量，调用方小缓冲会溢出。当前实现内单次使用，属潜伏风险。
- 建议：改为调用方传缓冲的 `EMV_FNAME(type, char* out, size_t n)`；对 `name` 增加长度/空指针检查。

### [P3] D-24 emvsdk.c 库版本接口无空指针校验

- 位置：`src/emvsdk.c:6-11`
- 证据：

```c
int GetLibVerSion(char *ver)
{
    API_strcpy(ver,libversion);
    return 0;
}
```

- 影响：`ver == NULL` 时 `API_strcpy` 崩溃；`ver` 缓冲小于 `"Emv_EntryLib_V2.0.1"` 时溢出（调用方契约未文档化）。
- 建议：判空；接口改为带长度（或文档明确要求 ≥ 20 字节）。

---

## 已核对无明显问题的项

- 记录尺寸自洽：`PARA_SAVLEN=1024` 且 `PARA_MAXLEN=1020`（`4+1020=1024`，见 `:86-88`、`:674`、`:728`）；CAPK 记录 `sizeof(EMV_CAPKPARAM)=512`（RID5+Index1+Alg1+Hash1+union252+有效期4+checksum20+rsv228）；`EMV_CARDBLACK=16`、`EMV_CERTBLACK=16`、`BCTC_DRL=20`、`TRANSLOG=16`，各文件按 `sizeof` 整记录读写的偏移一致（`:750`、`:1064`、`:1112`、`:1665`、`:1934`、`:1957`、`:2209`）。
- 文件尾"半条记录"检测：aid/capk/cardblack/certblack/drl 在增改前都检查 `fsize % 记录长 != 0` 并删除坏文件（`:911-916`、`:1550-1555`、`:1711-1715`、`:1815-1820`、`:2192-2197`），能避免部分半写记录被继续使用。
- `EMV_FNAME` 生成的文件名均满足 SDK 8.3 限制（`emvterm0.par`、`emvaids0.par`、`emvcapk0.par`、`emvcard0.blk`、`emvcert0.blk`、`emvtrns0.wtr/.log`、`drl0.par`，`include/ks_stdlib.h:148-157` 要求名字≤8、后缀≤3）。
- AID 长度保护：`add_aidparam` 校验 9F06 值 ≤16（`:903-906`），`get_termapps` 拷贝前判 `valuelen <= 16`（`:1196`）。
- `conv_param` 中对 9F4E（`:349-357`）、008B（`:391-401`）、9F05（`:406-418`）、9F1D（`:520-524`）、DF14/默认 TDOL（`:473-484`）做了目标容量上限保护——说明作者具备该意识，D-01/D-07 属漏检。
- CAPK 字段校验：模长 `> sizeof(modulus) || >255` 拒绝（`:1372-1375`），指数只接受 1 或 3 字节（`:1384-1395`），有效期 3/4/8 字节分支合理（`:1404-1420`）；SM2 公钥经 union 以 `rsa.modulus` 存储、`checkCAPKhash` 的 `sm2.r/s` 恰好落在同一内存，取值一致（`:1260-1268`）。
- 黑名单/证书黑名单匹配：`fnd_cardblack` 用 `panlen == blk.pan_len` 短路再比较 PAN+PSN（`:1781-1783`），`fnd_certblack` 用 RID+PKI+CSN（`:1885-1887`），逻辑正确（长度校验问题见 D-16）。
- `get_capkparam` 按 RID+PKI 精确匹配、`get_aidparam_tlv`/`get_capkparam_tlv` 用 `!= 记录长` 判断越界/EOF，返回值处理正确（`:1624-1640`、`:1118-1122`、`:1671-1675`）。
- `emvporting_add_aidparam` 的"找到同 AID/9C/9F2A 记录则回退覆盖、否则文件尾追加"流程本身正确，且 9C=0xFF/9F2A=0x00 作为通配与选应用逻辑一致（`src/emv_application_selection.c:79` `termapp->trans_type == 0xFF` 通配），不按缺陷计。
- `CLEAR()` 宏定义存在（`include/ks_type.h:82`），`emvporting_sav_translog` 使用无问题。

## 审查局限

1. **交付快照本身不自洽，无法编译，结论以本快照文本为准**：
   - 本地 `src/inc/emv_type.h`（673 行）**缺失 `EMV_TERMPARAM` 定义、也缺 `EMV_AIDPARAM` 的大部分字段**，而 `src/emvporting.c` 大量使用它们（`tpar->merchant_name`、`apar->term` 等）；该头文件与 `src/emv.c` 的字段名也不一致（`aid_param.status_check_support` vs `apar->status_check_support_indicator`）。
   - `src/emv.c:1155` 存在语法错误 `int i,j,;`（无法编译）。
   - 因此本报告涉及"结构体字段偏移/大小"的判断（D-01、D-03 的具体越界距离）依据**同版本同源副本**核对：`E:\Work\ZT\unipay_international_entrypoint\src\emvporting.c` 与本地 `src/emvporting.c` SHA256 完全一致（`3C4ED1FC…D5D1B8A0`），其 `inc/emv_type.h`（另有 `E:\Work\ZT\NEW_Arch\Entrypoint-lite\old_arch_code\entrypoint\inc\emv_type.h`，SHA256 相同）给出与之匹配的 `EMV_TERMPARAM`（486 字节）与 `EMV_AIDPARAM`（1113 字节）。该副本仅用于尺寸理解，**未作为本工程证据行号引用**；若实际编译所用头文件不同，偏移数字需复核，但"定长字段 + 未校验长度"的缺陷结论不变。
   - 建议 Lead 统一记录此交付快照问题（不是被审代码的缺陷，但影响所有审查员与结论可复现性）。
2. 源码中文注释为损坏编码（U+FFFD/乱码，个别行被合并），本报告不依赖注释，未把编码问题计入缺陷；引用片段中的中文注释已略去。
3. `ks_*` SDK 只有头文件，无实现：`API_fopen(NULL)`、`API_fread` 传超大 `count`、单文件句柄等行为按头文件契约与"未定义/可能崩溃"评估，未在真机验证。
4. 部分函数在当前快照中无内核调用者（`emvporting_get_termparam`、`get_aidstr`、`get_translog_amount`、transwater 系列、`get_kernel_index`），相关条目按"对外 API 已暴露给应用/未来接线"的前提评级（D-05、D-06、D-13 的部分影响）。
5. 规范细节未逐条比对 PDF：CAPK checksum 的哈希算法（EMV Book 2 对 RSA 固定 SHA-1 vs 代码用 `hash_algorithm`）、SM2 CAPK checksum 长度/算法、9F1B 六字节 BCD 编码的业务含义，报告中已标注"待确认"，建议对照 `E:\Work\SVN Project\emv_doc\卡组织规范` 由 Lead 定稿。
6. `EMV_PREPROCESS_RESULT`、`EMV_CONTEXT` 等结构在当前快照头文件中不完整，D-03 中 termapps 可用空间按 14KB 缓冲 - `EMV_CONTEXT` - `TLVBUFFER_SIZE(10KB)` - 候选表估算为 1~2KB，精确值需以真实头文件复核。
