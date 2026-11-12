/**
************************************************************************************************************************
* @file     express_api.h
* @brief    Expresspay (AMEX ZT_AMEX_CORE_V4.2.3) 子内核对外接口 / 内核注册
*
* 架构约定（详见 doc/AMEX_PORT_PLAN.md §6/§7）：
*   - libentrypoint.a 负责：卡类型识别、PPSE/PSE 应用选择、组合选择、内核选择、Outcome/UIRequest、
*     共享服务（TLV/Tag/appdata/ICC/参数持久化/porting 注入）。
*   - libexpresspay.a 负责：AMEX 4.2 内核 9 个 L2 阶段（initiate→complete），只暴露本文件的
*     init/preprocess/process/exit 四个入口，并由 App 或 Entrypoint 注册进内核表。
*   - 内核内部不得导出 emv_ / EMV_ 前缀的同名符号（与 libentrypoint.a 有 180 个同名全局符号，见方案 §3.4）
*
* 与 Entrypoint 注册表的对接：
*   本文件用 __has_include 探测 Lead 落地的 entrypoint_kernel.h：
*     探测到  → 直接使用其 ENTRYPOINT_KERNEL_OPS 类型，express_kernel_ops() 返回该类型指针；
*     未探测到 → 使用本文件的 exp_kernel_ops 兼容定义（字段顺序与建议接口一致），
*                Lead 落地头文件后只需保证字段顺序一致即可零改动切换。
************************************************************************************************************************
*/
#ifndef EXPRESS_API_H_
#define EXPRESS_API_H_

#include <stdint.h>
#include <stdbool.h>
#include "express_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * 内核注册描述符（建议接口；Lead 的 entrypoint_kernel.h 按此字段顺序落地）
 * ------------------------------------------------------------------------- */
typedef struct exp_kernel_ops
{
    uint8_t  kernel_id[8];        /* Entry Point Kernel Identifier（9F2A），AMEX 首字节 0x04，其余 0x00 */
    uint8_t  kernel_id_len;       /* 固定 8 */
    uint8_t  contactless_type;    /* Entrypoint EMV_CONTACTLESSTYPE_AMEX = 0x04 */
    uint8_t  reserved[5];

    int32_t (*init)(void);                    /* 内核初始化：分配/清零内核上下文，装载全局参数 */
    int32_t (*preprocess)(uint8_t b_online);  /* 预处理：按 AID 非接限额做"本 AID 是否可用"过滤、
                                                 装载当前 AID 参数到 appdata（原 emv.c:896-978 的等价物） */
    int32_t (*process)(void);                 /* 主状态机：返回 EMV_* 状态码（与 Entrypoint 数值域一致） */
    void    (*exit)(void);                    /* 交易结束清理（原 EMV_ProcessExit/EMV_Exit） */

    /* 结果回传：Entrypoint 的 EMV_Result*(选卡/PIN/联机/发卡行参考) 之后由 Entrypoint 调用，
       内核把结果写入上下文（替代美运 app→emv_set_*_result 的直连调用）。 */
    void    (*post_result)(int32_t kind, int32_t result, const uint8_t *data, int32_t len);
} EXPRESS_KERNEL_OPS;

/* post_result 的 kind（与 Entrypoint EMV_Result* 一一对应） */
#define EXPRESS_RESULT_SEL_CANDIDATES        1  /* EMV_ResultSelCandidates */
#define EXPRESS_RESULT_SEL_ACCOUNT_TYPE      2  /* EMV_ResultSelAccountType */
#define EXPRESS_RESULT_CHECK_ID              3  /* EMV_ResultCheckID */
#define EXPRESS_RESULT_ONLINE_PIN            4  /* EMV_ResultOnlinePIN（data=pin, len=pinlen） */
#define EXPRESS_RESULT_OFFLINE_PIN           5  /* EMV_ResultOfflinePIN：Expresspay v4.2 禁止离线 PIN（P p23 §2.10.1），内核忽略并置 UNSUPPORTED；仅为 ABI 兼容保留 */
#define EXPRESS_RESULT_BYPASS_PIN            6  /* EMV_ResultBypassPIN */
#define EXPRESS_RESULT_FORCE_ONLINE          7  /* EMV_ResultForceOnline */
#define EXPRESS_RESULT_ONLINE                8  /* EMV_ResultOnline（data=resp 55 域, len=resplen） */
#define EXPRESS_RESULT_ISSUER_REFERRAL       9  /* EMV_ResultIssuerReferral */

/* ---------------------------------------------------------------------------
 * 内核入口（由注册表调度；均为同步、可重入单实例）
 * ------------------------------------------------------------------------- */
int32_t express_init(void);
int32_t express_preprocess(uint8_t b_online);
int32_t express_process(void);
void    express_exit(void);
void    express_post_result(int32_t kind, int32_t result, const uint8_t *data, int32_t len);

/* 注册描述符（静态常量，不需要运行时初始化） */
const EXPRESS_KERNEL_OPS *amex_kernel_ops(void);

/* 供 App 使用的注册入口：把本内核登记到 Entrypoint 内核表。
   返回 0=成功；<0=失败（Entrypoint 未提供注册接口时由 App 侧改为直接调用 amex_kernel_ops）。 */
int32_t amex_kernel_register(void);

/* ---------------------------------------------------------------------------
 * 数据访问（内核 ↔ Entrypoint appdata；两者共用同一 appdata 缓冲区）
 * ------------------------------------------------------------------------- */
int32_t express_get_data(uint32_t tag, uint8_t *out, int32_t *outlen);
int32_t express_set_data(uint32_t tag, const uint8_t *in, int32_t inlen);
int32_t express_get_version(uint8_t *ver);       /* "ZT_AMEX_CORE_V4.2.3" */
int32_t express_get_candidate_list(void *list);  /* 由 Entrypoint 候选列表服务填充，内核不实现 */

/* Expresspay 私有 Outcome（方案 G5）：Entrypoint 组装最终 Outcome 时合并 0xA0/0xB0/0xC0/0xD0 等扩展值 */
int32_t express_get_outcome(EXPRESS_OUTCOME *out);

/* ---------------------------------------------------------------------------
 * 内核运行期常量
 * ------------------------------------------------------------------------- */
#define EXPRESS_KERNEL_ID           {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define EXPRESS_KERNEL_ID_LEN       8
#define EXPRESS_CONTACTLESS_TYPE    0x04    /* EMV_CONTACTLESSTYPE_AMEX */

#ifdef __cplusplus
}
#endif

#endif /* EXPRESS_API_H_ */
