# 主机桩 ICC 端到端测试与真链接门禁（TESTING）

> 分支：`feature/amex-entrypoint`。本文对应交付：
> `tests/stub/stub_platform.{c,h}`、`tests/host_e2e.c`、`scripts/test_host.ps1`。
> 测试只读 `libentrypoint.a` / `libexpresspay.a`，不修改 `src/**`、`inc/**`、`include/**`、`kernels/**`。

---

## 1. 目的

现有门禁只有「编译 + 符号审计」，没有运行验证。本测试补上两件事：

1. **真链接门禁**：用真实 `libentrypoint.a` + `libexpresspay.a` 与桩平台函数链接成可执行文件。
   `test_host.ps1` 用 `/WHOLEARCHIVE:` 强制把**两个库的全部目标文件**拉入链接；
   Windows `lld-link` 对未解析外部符号默认报错，**链接成功即证明两库所有 TU 的外部依赖被完整覆盖**。
2. **运行验证**：用脚本化 ICC（无真实硬件）驱动 AMEX/Expresspay 内核，覆盖注册、候选列表、
   应用选择、恶意卡数据、候选上限、DOL 边界、appdata 复位等关键路径。

---

## 2. 如何运行

### 2.1 一键（推荐）

```powershell
cd E:\Work\ZT\Entrypoint
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_host.ps1
```

该命令依次执行：

1. `scripts\build.ps1 -Target host -NoAudit`（产出 `build_host\lib\libentrypoint.a`、`libexpresspay.a`）；
2. 编译 `tests/stub/stub_platform.c`、`tests/host_e2e.c`（产物在 `tests\build\`）；
3. 链接 `host_e2e.exe`（**链接门禁**）；
4. 运行测试并返回退出码：`0`=PASS，`1`=FAIL。

常用变体：

```powershell
# 复用已有 build_host 库，只重编测试
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_host.ps1 -SkipBuild

# 使用 build_host_<tag> 下的库（与并行开发隔离）
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_host.ps1 -Tag amex
```

输出示例：

```
[a] link gate / kernel registration
[b] AMEX registration / dispatch (candidate list -> APP_SELECTED)
[c] malicious card SDOL (9F3F) must be rejected, not crash
[d] candidate list upper bound (>12 PPSE templates)
[e] DOL packing boundary (EMV_TLV_PacketByDOL)
[f] appdata reset (EMV_ProcessExit -> new transaction)
assertions: 75, failures: 0, known-fails (documented gaps): 3
RESULT: PASS
HOST E2E: PASS (assertions=75, failures=0, known-fails=3)
```

### 2.2 手工

```powershell
$clang = 'D:\Program Files\LLVM\bin\clang.exe'
$inc = @('-I','include','-I','inc','-I','src\inc',
         '-I','kernels\expresspay\inc','-I','kernels\expresspay\include',
         '-I','tests\stub')
& $clang -std=c99 -Os @inc -c tests\stub\stub_platform.c -o tests\build\stub_platform.o
& $clang -std=c99 -Os @inc -c tests\host_e2e.c        -o tests\build\host_e2e.o
& $clang tests\build\host_e2e.o tests\build\stub_platform.o `
         '-Wl,/WHOLEARCHIVE:build_host\lib\libexpresspay.a' `
         '-Wl,/WHOLEARCHIVE:build_host\lib\libentrypoint.a' `
         -o tests\build\host_e2e.exe
& tests\build\host_e2e.exe
```

> `/WHOLEARCHIVE:` 会把两个静态库的所有目标文件强制链接进来（而不只是测试路径可达的对象），
> 因此任何 TU 的未解析外部符号都会让链接失败。这是比「按需拉取」更强的真链接门禁。

### 2.3 产物与隔离

| 路径 | 说明 |
|---|---|
| `tests/build/stub_platform.o`、`host_e2e.o`、`host_e2e.exe` | 测试自身产物 |
| `build_host/`（或 `build_host_<tag>/`） | 复用现有构建门禁产物，测试不写入 |
| `build_ndk/` | 与本测试无关，不触碰 |

测试不依赖 `build_host` 以外的目录，也不依赖真实读卡器/卡片。

---

## 3. 桩平台（`tests/stub/stub_platform.c`）

### 3.1 覆盖的 SDK 符号

以 `llvm-nm` 统计的两库「全库未定义外部符号」为准逐项实现：

| 类别 | 符号 |
|---|---|
| 日志 | `API_Trace`、`API_TraceHex`（空实现，断言场景而非日志文本） |
| 内存/字符串 | `API_memcpy`、`API_memmove`、`API_memset`、`API_memcmp`、`API_strlen`、`API_strcpy`、`API_sprintf`、`API_snprintf` |
| 文件 IO | `API_fopen`、`API_fclose`、`API_fread`、`API_fwrite`、`API_fseek`、`API_fsize`、`API_fremove`（RAM 映像文件表） |
| ICC | `API_ICC_Apdu`（与 `EMV_PORTING.icc_rw` 共用同一脚本卡引擎） |
| 随机/时间 | `API_Rng`、`API_TimeGet`（确定性，便于复现） |
| 密码学 | `API_Hash`、`API_Crypt`（返回 `CRYPT_NOT_SUPPORTED`，本测试不走 ODA/PIN 密码学） |
| BCD | `API_Conv_StrToBcd_Left`、`API_Conv_StrToBcd_Right` |
| 其他 | `API_Beep`、`ics_option`、`CreateEMVDiscretionaryData` |

libc 基础符号（`memcpy/memset/memmove/strlen/__stdio_common_vsprintf` 等）由 MSVC/UCRT 运行时提供，
因此该可执行文件是**真链接**而不是「全桩自嗨」：两库的真实代码都被链接进来执行。

### 3.2 RAM 文件系统

`emvporting` 的参数文件读写是真实的，格式与产品一致：

- 文件名：`emv.par`（apipara）、`emvterm0.par`（终端参数）、`emvaids0.par`（AID 参数）、`emvcapk0.par` 等；
- 终端参数文件：`int32 length`（本机小端）+ `<length>` 字节原始 TLV；
- AID 参数文件：每条记录固定 `1024` 字节（`int32 length` + TLV + 余量），按 `AID 下标 * 1024` 定位。

`host_e2e` 启动时先调用公开 API `emv_set_termparam()` / `emv_add_aidparam()` 写入参数，
因此 `express_init()` / `express_app_sel_init()` 读到的参数来自真实的 `emvporting` 文件路径。

### 3.3 脚本化 ICC

`stub_icc_apdu()` 解析 C-APDU 后返回脚本响应：

| 命令 | 行为 |
|---|---|
| `00 A4 04 00 0E "2PAY.SYS.DDF01" 00` | 返回预设 PPSE FCI + `9000` |
| `00 A4 04 00 06 A0 00 00 00 25 01 00` | 返回预设 ADF FCI + `9000` |
| `80 A8 ...` | 返回预设 GPO 响应 + `9000`（当前用例未用到） |
| `80 AE ...` | 返回预设 GENERATE AC 响应 + `9000`（当前用例未用到） |
| 其他 | `6A82/6A86/6985/6D00` 等 |

测试通过 `stub_icc_set_ppse()` / `stub_icc_set_adf()` 注入响应，并可统计 SELECT 次数、
最后的卡片类型参数、最后的 C-APDU。

---

## 4. 用例与覆盖清单

| 用例 | 覆盖内容 | 关键断言 |
|---|---|---|
| **a. 链接门禁 + 注册** | 两库 + 桩平台 `/WHOLEARCHIVE` 链接；`amex_kernel_register()` → `EMV_RegisterKernel`；RAM 参数文件落盘 | 可执行文件成功运行；`EMV_FindKernel(0x04)` 返回的 ops 含非空 `init/preprocess/process`；桩符号地址均有效；`emvterm0.par`/`emvaids0.par`（1024B 记录）真实写入桩文件系统 |
| **b. AMEX 注册与调度** | `EMV_Init` → `EMV_PreProcess` → `EMV_SetCardType(CONTACTLESS)` → `SetTransType/SetTransAmount` → `EMV_Process` | 第 1 次 `EMV_Process` = `EMV_STA_CANDIDATES_BUILT(2)`；候选 1 条且 AID=`A00000002501`、9F2A=`04 00...`；第 2 次 = `EMV_STA_APP_SELECTED(5)`；`EMV_GetSelectedKernel()==0x04`；最终 SELECT 到达卡片；9F06/9F37/TVR 已写入 appdata |
| **c. 恶意 SDOL** | PPSE 携带 9F3F SDOL（252 字节 DOL，每项请求 0xFF 字节，展开 > 20KB） | 不崩溃；`EMV_Process` 返回 `EMV_ENDAPPLICATION(111)`；SDOL 已入 appdata；候选数为 0；DF8115 错误指示已记录 |
| **d. 候选上限** | 13 个最小 61 模板（仅 4F）；另加 10 个完整 61 模板（4F/50/87/9F2A）对照 | 13 模板时 `EMV_Process` 结束应用；`EMV_GetData(TAG_CANDIDATE_LIST)` 长度 = 12 × `sizeof(EMV_CANDIDATE)`，不越界；10 个完整模板暴露解码池上限（见 §6 D2） |
| **e. DOL 打包边界** | 直接调用公开 `EMV_TLV_PacketByDOL` | 超长 DOL（请求 64B/65536B）返回 `EMV_FAIL`；`pOutLen` 不被改写；输出缓冲 canary 完好；正常 DOL 仍能正确打包（正对照） |
| **f. appdata 复位** | `EMV_ProcessExit` 后重新 `EMV_Init` 并跑第二笔 | `EMV_ProcessExit` 后 appdata 为空；第二笔再次到达 `CANDIDATES_BUILT` → `APP_SELECTED`；AMEX 内核再次被选中 |

断言计数与退出码：每个用例内 `CHECK` 失败会打印 `FAIL` 并增加失败数；`KNOWN_FAIL` 只记录
已确认真实缺陷、不使整体失败。最终 `RESULT: PASS/FAIL`，退出码 `0/1`。

---

## 5. 当前未覆盖项

1. **真实卡片 / 读卡器 / L1 时序**：全部 ICC 交互来自 RAM 脚本，不验证射频、ATR、时序、重试。
2. **完整交易流程**：只到 `EMV_STA_APP_SELECTED`；GPO → READ RECORD → ODA → 处理限制 →
   CVM → TRM → TAA → GENERATE AC → 联机 → 脚本 尚未跑通（受 §6 D1 与桩数据量限制）。
3. **规范一致性 / EMVCo TTA**：不验证 Book B / Expresspay v4.2 的逐条合规性。
4. **密码学**：`API_Hash` / `API_Crypt` 返回 `CRYPT_NOT_SUPPORTED`，SDA/DDA/CDA、CAPK 校验、
   联机 PIN 加密均未覆盖。
5. **IAR**：本机无 IAR，未验证 IAR 工程/编译。
6. **61xx / 6Cxx 自动重试**：Entrypoint 的 `emvicc.c` 已实现（`src/emvicc.c:404-445`），
   但桩卡不返回 61xx/6Cxx，未覆盖。
7. **多线程/重入、掉电持久化**：RAM 文件系统在进程内有效，不验证 Flash 持久化。
8. **NDK 运行时**：测试只在 host 运行；NDK 仍由 `scripts\build.ps1 -Target ndk` 做编译/审计门禁。
9. **金额相关分支**：受 §6 D1 影响，9F02/81/9C 在进入内核后为 0/0xFF，限额、状态检查、
   TAA 等金额分支当前无法做有效断言。

---

## 6. 桩平台与 Entrypoint 的契约假设

以下假设按 ISO 7816 与现有 `src/emvicc.c` 行为实现，若目标平台语义不同需同步调整桩：

1. **`icc_rw` 的 APDU 形态**：`input` 是完整 C-APDU（`CLA INS P1 P2 [Lc data] [Le]`），
   `output` 只放响应数据（不含 SW1/SW2），返回值是 `SW1SW2`，`*outlen` 是响应数据长度。
   `src/emvicc.c:337` 按此约定调用。
2. **空响应**：`icc_rw` 在 `outlen != NULL` 时若返回 `9000` 但 `*outlen==0`，
   `emvicc` 会判为 `EMV_VALUENULL_LENERR`（`src/emvicc.c:465-470`）。桩对所有需要的 SELECT/GPO/GAC
   都返回非空数据。
3. **SW 规则**：未匹配 SELECT 返回 `6A82`；`6283`/`6A81` 等由 Entrypoint 分支处理，桩不主动产生；
   `61xx`/`6Cxx` 由 `emvicc` 处理，桩不产生。
4. **`ics_option`**：默认全 0；但 `MISC_Paypass_Support_SPI(0x0608)` 与
   `MISC_Paypass_SPI_ACTIVE_FLAG(0x0604)` 返回 1，以便卡下发 9F3F 时真正进入
   `sdol_process()`（恶意 SDOL 用例需要）。正常 PPSE 不含 9F3F/9F3E，SPI 分支自动跳过。
5. **文件 IO 语义**：`API_fopen` 成功返回 `>0`；`F_RDONLY=0/F_WRONLY=1/F_RDWR=2/F_CREAT=0x100/F_APPEND=0x2000`；
   `F_SEEK_SET/CUR/END=1/2/4`；`API_fread` 在 EOF 处可短读；AID 参数文件必须能读满 1024 字节。
6. **参数文件布局**：32 位长度头 + 原始 TLV；AID 参数每条 1024 字节；内核索引 0 对应
   `emvterm0.par` / `emvaids0.par`。
7. **AID 参数中的 9F2A**：测试数据把 AID 级 9F2A 写成 `00`（表示「未在 AID 级指定内核」，
   由 AID/RID 组合选择回退）。PPSE 候选仍带显式 9F2A=`04 00...`，因此调度用例走
   Kernel Identifier 路径。这是测试数据选择，不是产品配置建议。
8. **候选模板裁剪**：`deal_ppse_resp()` 的解码池是 `EMV_TLVOBJ tlvs[40]`
   （`src/emv_application_selection.c:882`）。13 个「完整」61 模板（各 4 个 TLV 对象）会产生
   4+13×4=56 个对象，超出解码池；因此「>12 模板」用例用最小 61（仅 4F）来触达候选上限，
   另外用 10 个完整模板单独记录解码池上限（§6 D2）。
9. **内存**：`alloc_buffer` 提供 16KB 静态池（Entrypoint 需要 14KB+），内核上下文静态分配，无堆。
10. **确定性**：时间固定 `2026-11-12 10:30:00`，ATC 固定 `00000001`，RNG 为确定性序列。

---

## 7. 发现的真实缺陷 / 集成缺口

> 均**未修改** `src/**`、`kernels/**`，仅记录证据。测试中以 `KNOWN-FAIL` 标注（不计入失败）。

### D1（高）AMEX 内核拿不到交易类型/金额，反而覆盖 Entrypoint 已写入的值

- 证据：
  - `kernels/expresspay/src/express_kernel.c:41-42`：`express_reset_transaction()` 把
    `ctx->trans_type = _EMV_TRANSTYPE_UNKNOWN(0xFF)`、`ctx->trans_amount = 0`。
  - `kernels/expresspay/src/express_kernel.c:123-140`：`express_preprocess()` 只复制
    `ttq` 与 `force_online`，**没有**从 `g_emv_cnxt` 复制 `trans_type/trans_amount/trans_amount_other`。
  - `kernels/expresspay/src/express_app_sel.c:431-432`：最终应用选择后
    `appdata_save(TAG_009C, &ctx->trans_type, 1)` 与
    `express_load_trans_amount(ctx->trans_amount, ctx->trans_amount_other)`。
  - Entrypoint 在 `EMV_Process` 的 `EMV_STA_IDLE` 分支写入 9C/金额（`src/emv.c:1410-1411`），
    随后被内核用 0/0xFF 覆盖。
- 影响：`9F02/81 = 000000000000`、`9C = FF`；状态检查、Reader 限额、DRL、TAA 等金额相关判定
  全部基于 0，真实交易会得到错误结果。
- 测试证据：用例 b 的两条 `KNOWN-FAIL`（`9F02` 期望 `000000001234`，`9C` 期望 `00`）。
- 建议修法（由 Lead 决策）：内核 `preprocess` 从 `g_emv_cnxt` 复制交易类型/金额；
  或在注册接口增加显式数据注入；Entrypoint 侧保证在 `emv_select_kernel()` 前金额已写入。

### D2（中）PPSE 解码池 `tlvs[40]` 小于候选上限所需容量

- 证据：`src/emv_application_selection.c:882` `EMV_TLVOBJ tlvs[40]={0};`；
  解码失败在 `:885` 返回 `AS_FCIFMTILL`；候选上限检查在 `:1073`。
- 对象数：4 个容器（6F/84/A5/BF0C）+ 每个 61 模板 4 个对象（4F/50/87/9F2A）。
  `EMV_CFG_MAX_CANDIDATES=12`，但 10 个完整模板即 44 个对象 > 40，解码先失败。
- 影响：携带 10 个以上「完整」应用模板（规范允许）的卡会被当作「FCI 格式非法」拒绝，
  而不会走到候选上限逻辑；**不造成越界**，候选数仍不会超过 12，但错误码/Outcome 不准确。
- 测试证据：用例 d2 的 `KNOWN-FAIL`（10 个完整模板期望 10 个候选，实际 0）。
- 建议修法：增大解码池或改为「先定位 61 模板、再逐个解码」；属于 `src/**`，本次不改。

### D3（低，行为记录，非缺陷）候选超限的错误形态

- 13 个最小模板触达上限时，`deal_ppse_resp()` 在 `src/emv_application_selection.c:1073`
  返回 `AS_ERR`，经 `parse_ppse_resp()` 转成 Book B 的「Other Card」Outcome
  （`outcome_other_card()` → `EMV_ENDAPPLICATION=111`），而不是专门的「候选列表满」错误码。
- 测试按实际行为断言 `EMV_ENDAPPLICATION`，并验证候选数 = 12、无越界。

---

## 8. 扩展方法

- 新增卡响应：在 `tests/stub/stub_platform.h` 增加 `stub_icc_set_*()`，或扩展
  `stub_icc_apdu()` 的 `switch (cmd[1])`。
- 新增参数：在 `tests/host_e2e.c` 的 `build_termparam()` / `build_aidparam()` 中追加 TLV。
- 新增断言：使用 `CHECK`（必须通过）或 `KNOWN_FAIL`（记录真实缺陷，不阻断）。
- 走更远的状态：补齐 GPO（77/82/AIP + 94/AFL）与 READ RECORD 响应后，可在用例 b 中继续
  `EMV_Process` 到 `APP_INITIALIZED`/`READ_APP_DATA_COMPLETED`；注意先解决 §7 D1。
- 调试：在 `scripts\build.ps1` 中用 `-DEMV_CFG_DEBUG=ENABLE` 重编库，可看到 `eTRACE`；
  桩平台当前丢弃 `API_Trace`，如需日志可改为打印。

---

## 9. 与现有门禁的关系

| 门禁 | 命令 | 结果 |
|---|---|---|
| host 编译 + 符号审计 | `powershell -File scripts\build.ps1` | 保持 `BUILD OK` + `SYMBOL AUDIT OK` |
| NDK 编译 + 符号审计 | `powershell -File scripts\build.ps1 -Target ndk` | 保持 `BUILD OK` + `SYMBOL AUDIT OK` |
| host 真链接 + E2E | `powershell -File scripts\test_host.ps1` | `HOST E2E: PASS`（75 断言 / 0 失败 / 3 KNOWN-FAIL；`/WHOLEARCHIVE` 覆盖两库全部 TU） |

本测试不修改 `scripts\build.ps1` 的任何现有编译/审计逻辑，也不改变 `build_host`/`build_ndk` 的产物布局。
