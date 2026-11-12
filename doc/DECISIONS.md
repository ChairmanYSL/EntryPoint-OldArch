# 改造决策记录（ADR）

| # | 决策 | 理由 | 状态 |
|---|---|---|---|
| D1 | 工作在主仓库 `feature/amex-entrypoint` 分支；`master` 仅保留 2026-11-10 原始审查基线 | 用户要求不污染 master | 已执行（master 仅 1 个基线提交） |
| D2 | 美运源码仓库 `emv_core_amex_japan` 切 `feature/entrypoint-port` 分支、全程只读 | 同上；移植产物落在 Entrypoint 仓库 `kernels/expresspay/` | 已执行 |
| D3 | 删除 `emv_set/get_current_code`、`emv_set/get_current_exponpent` 四个 API；货币代码/指数改用 5F2A/5F36 标签 | 用户明确要求；避免重复的货币状态源 | 完成 |
| D4 | 内核调度改为注册表（`EMV_KERNEL_OPS` + `EMV_RegisterKernel`），Entrypoint 不引用任何内核符号 | 解耦硬要求：Entrypoint 独立编译、内核链接 Entrypoint | 完成；符号审计强制 |
| D5 | `emv_k_interface` 结构体/全局变量删除 | 与注册表重复，且原实现未赋值即空指针调用 | 完成 |
| D6 | 未注册/未知内核 → 返回 `EMV_ERR` → EP 以"Invalid Kernel"结束应用，不再调用空指针 | P1 崩溃修复 + EP Book B outcome | 完成 |
| D7 | AMEX 不使用 TTQ 9F66，使用 9F6D/9F6E（Expresspay v4.2） | 规范笔记：v4.2 全文无 9F66；Book B 脚注 9F66 属 Kernel 3/6/7 | 已写入规范笔记与移植方案 |
| D8 | 候选列表溢出（>12）→ 拒绝该卡并返回错误（EP outcome '1C' other interface），不做截断 | 避免静默改变应用优先级语义 | 完成（模块 A） |
| D9 | SDOL 展开 >255 字节 → 返回错误（SPI 命令 Lc 单字节，扩展长度 APDU 不在本层） | ICC 层 Lc 单字节约束；禁止截断 | 完成（模块 A/E） |
| D10 | `EMV_TERMAPP.rsv` 默认置 1（终端支持 9F29 扩展选择），AID 级覆盖待产品参数确定 | 原实现无写入方导致 9F29 永不触发 | 完成（默认值）；AID 级 override TBD |
| D11 | 发卡行脚本 71/72 改为 `appdata_save_raw` 存 EP 私有 tag `0xDF8F10/0xDF8F20`（不用 DF8100/DF8101） | DF8101 与 DS Summary2 冲突；原方案会静默丢脚本 | 完成 |
| D12 | 内核与 EP 全程禁止堆分配（`malloc/free` 从符号审计层面禁止） | 嵌入式/确定性要求；替换为静态/调用方缓冲 | 完成 |
| D13 | Entrypoint 结构体两步精简：第 1 步去重复内嵌（AIDPARAM 1118→632B、TERMAPP 33→22B）；第 2 步最小必要集 + 编译期尺寸断言（AIDPARAM 55B/18 字段、TERMPARAM 23B/6 字段），内核参数走原始 TLV 回读不受影响 | 先保证可编译与行为，再做激进精简；方案/清单见 doc/STRUCT_MINIMIZATION.md | 完成（2026-11-12） |
| D14 | AMEX 内核库当前交付"可编译+可链接+已注册"基线（13 文件/约 7.2k 行）；完整 L2 阶段逻辑与规范一致性按 `doc/AMEX_PORT_PLAN.md` S2-S6 继续 | 需要真卡/认证环境逐阶段验证 | 进行中 |
| D18 | 按 Expresspay v4.2 规范收敛内核能力：无 MSD/Mag-stripe、无 TTQ(9F66) 用 9F6D/9F6E、禁止离线 PIN、2nd GAC/71-72 脚本/DDX 明确不支持（脚本归 Entrypoint） | v4.2 规范明示移除/不支持，保留半成品分支会误导后续认证；缺省 9F6D=C0、9F6E=9C E0 00 03 | 完成（2026-11-12，内核 0 warning） |
| D17 | 最终 Outcome 由 EP 统一组装（`outcome_final_transaction`）：内核只给 scheme 私有 Outcome，EP 按 Book B 11 参数合并并只解释 UI Request / Field-off / Start 三项 | EP 是 Outcome 的唯一出口（Book B §3.5），避免 App 重复组装 | 完成（2026-11-12） |
| D16 | 内核参数/结果通道补齐：`EMV_KERNEL_OPS` 增加可选 `exit`/`post_result`/`get_outcome`；Entrypoint 每个 `EMV_Result*` 转发结果、`EMV_ProcessExit` 通知内核退出、`EMV_GetKernelOutcome` 暴露 scheme 私有 Outcome | G9/G5 集成缺口；可选钩子保证未实现的内核零改动 | 完成（2026-11-12，提交 9fb6295/5a63dc3） |
| D15 | 构建门禁以 `scripts/build.ps1`（host clang + NDK r23b）+ `symbol_audit.ps1` 为准；IAR 工程只同步文件列表，本机无 IAR 不能验证 | 用户要求"编译期排除静态错误"且本机可用工具限制 | 已完成（IAR 待有环境复编） |
