/**
************************************************************************************************************************
* @file     express_priv.h
* @brief    Expresspay 内核私有总头文件（kernels/expresspay/src/*.c 唯一入口）
*
* 作用（详见 doc/AMEX_PORT_PLAN.md §3/§5/§6）：
*   1. 把 Entrypoint 的公共服务头（emvi.h → emv.h / emv_type.h / emv_tag.h / emv_ics.h /
*      emv_utils.h / emvoutcome.h）与本内核的 5 个公共头一次拉齐；
*   2. 提供"美运源码 → Entrypoint 服务"的兼容垫片：状态码别名、交易类型常量、AIP/TTQ/TVR/TSI
*      位访问宏（Entrypoint 上下文没有内核私有字段）、SDK_EXPRESS_DRL 类型别名、内核私有 helper；
*   3. 声明内核全部阶段函数（express_*），供 9 个 L2 模块互相调用（不做隐式声明）。
*
* 约束：
*   - 本头文件不定义任何函数/变量；全部实现放在 src/*.c。
*   - 不包含美运的 emvi.h / emv.h / emv_type.h / emv_tag.h（与 Entrypoint 同名且冲突）。
************************************************************************************************************************
*/
#ifndef EXPRESS_PRIV_H_
#define EXPRESS_PRIV_H_

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * 1) Entrypoint 公共服务（必须最先包含：ks_type.h 提供 u8/u16/u32/s32 与 TRUE/FALSE）
 * ------------------------------------------------------------------------- */
#include "emvi.h"                       /* emv.h/emv_type.h/emv_tag.h/emv_ics.h/emv_utils.h/emvoutcome.h */

/* u8/u16/u32/s32 已由 ks_type.h 提供，禁止重复 typedef（C99 不允许同名 typedef 重定义） */
#define EXPRESS_COMPAT_TYPES_DEFINED 1
#include "express_compat.h"             /* API_* → emv_*、UNIQUE/OVERRIDED、EXPRESS_WEAK */
#include "express_tags.h"               /* 美运私有标签 + TDOL 0x1F03 覆盖 */
#include "express_param.h"              /* EXPRESS_TERM_PARAM / EXPRESS_AID_PARAM / EXPRESS_DRL */
#include "express_types.h"              /* EXPRESS_CONTEXT / 阶段原型 / 常量 */
#include "emv_kernel.h"                 /* Entrypoint 内核注册表（EMV_RegisterKernel/EMV_KERNEL_OPS） */

/* ---------------------------------------------------------------------------
 * 2) 美运类型名别名
 * ------------------------------------------------------------------------- */
typedef EXPRESS_DRL        SDK_EXPRESS_DRL;        /* emvi.h:105-118 */
typedef EXPRESS_DRL_PARAMS SDK_EXPRESS_DRL_PARAMS; /* emvi.h:120-124 */

/* ---------------------------------------------------------------------------
 * 3) 状态码 / 交易类型常量别名（方案 §2.3：数值域一致，仅 4 处名不同）
 * ------------------------------------------------------------------------- */
#ifndef EMV_REQ_SEEPHONE
#define EMV_REQ_SEEPHONE            EMV_SEEPHONE            /* 107（emv_type.h:276 → Entrypoint :310） */
#endif
#ifndef EMV_SW_INTERFACE
#define EMV_SW_INTERFACE            EMV_SWITCH_INTERFACE    /* 106（emv_type.h:275 → Entrypoint :309） */
#endif
#ifndef EMV_REQ_EC_CONFIRM
#define EMV_REQ_EC_CONFIRM          25                      /* Entrypoint 无同值状态，内核内部消化 */
#endif
#ifndef EMV_STA_SELECT_NEXT
#define EMV_STA_SELECT_NEXT         EMV_REQ_SELECT_NEXT     /* 26（同名同值） */
#endif

#ifndef EMV_MODE
#define EMV_MODE                    1                       /* 美运 emvi.h:95 EMV 模式（msdoremv） */
#endif
#ifndef MSD_MODE
#define MSD_MODE                    0                       /* 美运 emvi.h:94 MSD 模式 */
#endif

/* 交易类型（美运 emvi.h:71-79；与 Entrypoint EMV_TransTypes 数值一致） */
#ifndef TRANS_GOODS_SERVICE
#define TRANS_GOODS_SERVICE         0x00
#endif
#ifndef TRANS_CASH
#define TRANS_CASH                  0x01
#endif
#ifndef TRANS_INQUIRY
#define TRANS_INQUIRY               0x04
#endif
#ifndef TRANS_TRANSFER
#define TRANS_TRANSFER              0x05
#endif
#ifndef TRANS_PAYMENT
#define TRANS_PAYMENT               0x06
#endif
#ifndef TRANS_ADMIN
#define TRANS_ADMIN                 0x07
#endif
#ifndef TRANS_CASHBACK
#define TRANS_CASHBACK              0x09
#endif
#ifndef TRANS_CARD_RECORD
#define TRANS_CARD_RECORD           0x0A
#endif
#ifndef TRANS_EC_BALANCE
#define TRANS_EC_BALANCE            0x0B
#endif

/* ---------------------------------------------------------------------------
 * 4) 位访问宏（Entrypoint 的宏引用 g_emv_cnxt->config.*，内核上下文改为 ->term.*）
 * ------------------------------------------------------------------------- */
#undef  ics_option
#undef  tc_option
#undef  atc_option
#undef  ttq_get
#undef  ttq_set
#define ics_option(nr)              BIT_GET((nr), g_express_cnxt->term.ics)
#define tc_option(nr)               BIT_GET((nr), g_express_cnxt->term.term_caps)
#define atc_option(nr)              BIT_GET((nr), g_express_cnxt->term.term_add_caps)
#define ttq_get(nr)                 BIT_GET((nr), g_express_cnxt->ttq)
#define ttq_set(nr)                 BIT_SET((nr), g_express_cnxt->ttq)

/* 终端类型判定（美运 emvi.h:258-265） */
#define TT_ATTENDED()               ((g_express_cnxt->term.term_type & 0x0F) < 4)
#define TT_UNATTENDED()             ((g_express_cnxt->term.term_type & 0x0F) > 3)
#define TT_ONLINE_ONLY()            (((g_express_cnxt->term.term_type & 0x0F) == 0x01) || \
                                     ((g_express_cnxt->term.term_type & 0x0F) == 0x04))
#define TT_OFFLINE_ONLY()           (((g_express_cnxt->term.term_type & 0x0F) == 0x03) || \
                                     ((g_express_cnxt->term.term_type & 0x0F) == 0x06))

/* AIP 位（美运 emvi.h:278-285；移植后用上下文里的 2 字节副本，避免悬垂指针 P1-6） */
#define AIP_SDA_SUPPORT()           (g_express_cnxt->aip[0] & 0x40)
#define AIP_DDA_SUPPORT()           (g_express_cnxt->aip[0] & 0x20)
#define AIP_CV_SUPPORT()            (g_express_cnxt->aip[0] & 0x10)
#define AIP_TRM_SUPPORT()           (g_express_cnxt->aip[0] & 0x08)
#define AIP_IA_SUPPORT()            (g_express_cnxt->aip[0] & 0x04)
#define AIP_CDA_SUPPORT()           (g_express_cnxt->aip[0] & 0x01)
#define AIP_MSD_SUPPORT()           (g_express_cnxt->aip[1] & 0x80)
#define AIP_MOB_SUPPORT()           (g_express_cnxt->aip[1] & 0x40)

/* TVR/TSI：写穿透（同时更新 appdata 与上下文副本） */
#define tvr_set(nr)                 express_tvr_set((nr))
#define tvr_get(nr)                 express_tvr_get((nr))
#define tsi_set(nr)                 express_tsi_set((nr))
#define tsi_get(nr)                 express_tsi_get((nr))

/* ---------------------------------------------------------------------------
 * 5) 内核私有 helper（实现在 express_utils.c / express_param.c / express_kernel.c）
 * ------------------------------------------------------------------------- */
void    express_aip_refresh(void);                          /* [82] → g_express_cnxt->aip（GPO 解析后调用） */
int32_t express_tvr_set(int32_t bit);                       /* [95] 置位（index = bit>>8, mask = bit&0xFF） */
uint8_t express_tvr_get(int32_t bit);
int32_t express_tsi_set(int32_t bit);                       /* [9B] */
uint8_t express_tsi_get(int32_t bit);
int32_t express_refresh_tvr_tsi(void);                      /* 从 appdata 重新同步 tvr/tsi 副本 */
int32_t express_appdata_save2(EMV_TLVOBJ *tlv, int32_t override); /* 美运 appdata_save2（Entrypoint 未导出） */
int     express_pdol_packet(u8 *pdol, int pdollen, u8 *val, int val_max_len); /* 美运 emvtlv.c:498 */
int     express_drl_get(BCTC_DRL *drl, int index);          /* Entrypoint 无 DRL 读取 API（方案 G2） */
EXPRESS_DRL_PARAMS *express_drl_params(void);               /* express_param_load_drl 的落盘处 */
int32_t express_crc32(uint32_t crc, const uint8_t *buf, int32_t len);
EXPRESS_OUTCOME *express_outcome_state(void);
void    express_outcome_reset(void);

/* Outcome（美运 emvoutcome.c；OUTCOME_FLAG=0 时等价返回状态码，另存 Expresspay Outcome 供 Entrypoint 合并 G5） */
int     express_outcome_try_another_interface(void);
int     express_outcome_try_again(void);
int     express_outcome_request_another_payment(void);
int     express_outcome_request_online_pin(void);
int     express_outcome_approved_transactions(int result);
int     express_outcome_declined_transactions(int result);

/* ---------------------------------------------------------------------------
 * 6) 9 个 L2 阶段 + 应用选择（全部 express_ 前缀，禁止 emv_/EMV_ 全局符号）
 * ------------------------------------------------------------------------- */
int  express_app_sel_init(void);                    /* express_app_sel.c：最终 SELECT + 参数装载 */

int  express_pre_pdol_process(u8 term_type);        /* express_initiate_application.c */
int  express_pdol_process(u8 *apdu_send, int *send_len, u8 term_type);
int  express_parse_gpo_resp(u8 *resp, int resp_len);
int  express_deal_gpo_resp(void);
int  express_initiate_application(void);

int  express_check_read_afl(void);                  /* express_read_application_data.c */
int  express_read_application_data(void);

int  express_offline_data_auth(void);               /* express_offline_data_authentication.c */
int  express_process_sda(void);
int  express_process_dda(int forcda);
int  express_process_cda(int nstep, u8 *gac_resp, int gac_len);
int  express_encrypt_pin(u8 *pin, u8 *epin);

int  express_processing_restrictions(void);         /* express_processing_restrictions.c */
int  express_is_a_atm(void);
void express_get_dynamic_reader_limit(SDK_EXPRESS_DRL_PARAMS *drl_params);
int  express_again_drl_preprocess(SDK_EXPRESS_DRL *drl);
unsigned char express_set_ttq(void);
int  express_check_data_miss_msd(void);
unsigned char express_process_restrict(void);

int  express_cardholder_verify(void);               /* express_cardholder_verification.c */
int  express_process_online_pin(void);
unsigned char express_analy_cvmlist_cmpamount(unsigned char *CVMamount, unsigned char type);
unsigned char express_check_if_support_cvm(unsigned char method);
unsigned char express_analy_cvmlist(unsigned char method, unsigned char condition,
                                     unsigned char TransType, unsigned char TermType,
                                     unsigned char *CVM_X, unsigned char *CVM_Y);
unsigned char express_perform_cvm(unsigned char method, unsigned char condition);
int  express_cvm_processing(bool skip_nocvm);

int  express_terminal_risk_manage(void);            /* express_terminal_risk_management.c */

int  express_action_analyse(void);                  /* express_terminal_action_analysis.c */
void express_calc_tc_hash(u8 *dol, int len);
int  express_cid_msg(u8 cid);
unsigned char express_term_analy_trans_res(void);
int  express_parse_gac_resp(u32 template, bool cda_flag);
int  express_analyze_gac_resp(u8 *gac_resp, int len, bool cdaflag);
int  express_deal_gac_resp(u8 *resp, int resp_len, int sw, bool cda_flag);
int  express_process_cdol(void);

int  express_pre_complete(void);                    /* express_complete.c */
int  express_complete(void);
int  express_send_trans_result(int result);

int  express_script_process(int when, u8 *res, int *reslen); /* express_issuer_script_processing.c */

#endif /* EXPRESS_PRIV_H_ */
