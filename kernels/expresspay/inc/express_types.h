/**
************************************************************************************************************************
* @file     express_types.h
* @brief    Expresspay (AMEX ZT_AMEX_CORE_V4.2.3) 内核内部类型与流程常量
*
* 内核对 Entrypoint 的依赖约定：
*   - 本文件不包含 Entrypoint 头文件；需要 EMV_TLVOBJ / EMV_CANDIDATE / EMV_TERMAPP /
*     EMV_CAPKPARAM / EMV_* 状态码的 .c 文件自行包含 Entrypoint 的 emv.h / emv_type.h / emv_tag.h。
*   - 内核自己的全局变量统一使用 express_ 前缀，禁止再定义 g_emv_cnxt（与 libentrypoint.a 冲突）。
*
* 数据来源：美运 V4.2.3 submodule/emvco/src/emvi.h（EMV_CONTEXT/EXPRESS_TERM_PRESET）、
*           src/emverror.h、submodule/emvco/inc/emv_type.h。字段逐一对应关系见
*           doc/AMEX_PORT_PLAN.md §5.3。
************************************************************************************************************************
*/
#ifndef EXPRESS_TYPES_H_
#define EXPRESS_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "express_param.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * 内核版本（emv.c:39 EMV_VERSION）
 * ------------------------------------------------------------------------- */
#define EXPRESS_KERNEL_VERSION        "ZT_AMEX_CORE_V4.2.3"
#define EXPRESS_KERNEL_ID_0           0x04    /* 9F2A 内核标识首字节：AMEX（emv_combination_Selection.c:17 KERNEL_ID_AMEX） */

/* ---------------------------------------------------------------------------
 * 交易类型（emvi.h:71-79；与 Entrypoint EMV_TransTypes 数值一致）
 * ------------------------------------------------------------------------- */
#define EXPRESS_TRANS_GOODS           0x00
#define EXPRESS_TRANS_CASH            0x01
#define EXPRESS_TRANS_INQUIRY         0x04
#define EXPRESS_TRANS_TRANSFER        0x05
#define EXPRESS_TRANS_PAYMENT         0x06
#define EXPRESS_TRANS_ADMIN           0x07
#define EXPRESS_TRANS_CASHBACK        0x09
#define EXPRESS_TRANS_CARD_RECORD     0x0A
#define EXPRESS_TRANS_EC_BALANCE      0x0B

/* ---------------------------------------------------------------------------
 * 支付模式：Expresspay v4.2 已移除 Mag-stripe/MSD（[P p10 版本历史；P p43 §7.2.2]），
 * 内核只有 EMV 模式，故不再保留 msdoremv 字段与 EXPRESS_MODE_* 常量。
 * ------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * 离线数据认证结果类型（合并原 EMV_CONTEXT.da_sda/dda/cda_performed 三个 bool）
 * ------------------------------------------------------------------------- */
typedef enum
{
    EXPRESS_ODA_NONE = 0,
    EXPRESS_ODA_SDA  = 1,
    EXPRESS_ODA_DDA  = 2,
    EXPRESS_ODA_CDA  = 3
} EXPRESS_ODA_TYPE;

/* ---------------------------------------------------------------------------
 * 内核阶段执行结果（内部用；对外仍复用 Entrypoint EMV_Status 数值）
 * ------------------------------------------------------------------------- */
typedef enum
{
    EXPRESS_STAGE_OK       = 0,
    EXPRESS_STAGE_NEXT     = 1,       /* 需要上层再次调用（对应 EMV_* 1..99 中间状态） */
    EXPRESS_STAGE_DONE     = 2,       /* 交易结束（对应 EMV_* >=100 结束状态） */
    EXPRESS_STAGE_ERROR    = 3
} EXPRESS_STAGE_RESULT;

/* ---------------------------------------------------------------------------
 * 内核私有错误码（美运 emvi.h:81-94 _ErrCodes / src/emverror.h）
 * 说明：对外返回值仍用 EMV_ERR+i 数值；内核内部按模块细分，便于日志。
 * ------------------------------------------------------------------------- */
#define EXPRESS_ERR_TLV_BASE          (-10100)
#define EXPRESS_ERR_BUF_BASE          (-10200)

/* ---------------------------------------------------------------------------
 * Expresspay 内核上下文（美运 EMV_CONTEXT 精简版，emvi.h:141-218）
 *
 * 删除说明（见方案 §5.3）：
 *   - emvbuffer_size/tlvbuffer/tlvbuffer_size/candidates/candidates_num/termapps/termapps_num/
 *     kernel_checksum/config_checksum：由 Entrypoint 共享上下文或服务替代：
 *       EMV_PORTING.alloc_buffer 只由 Entrypoint 调用一次；tlvbuffer/appdata 由 Entrypoint 的
 *       emvbuf.c 管理；termapps/candidates 由 Entrypoint 应用选择层管理；checksum 由 Entrypoint 提供。
 *   - req_sel_candidates_result / req_sel_accounttype_result / req_check_id_result / req_pin_* /
 *     req_issuer_referral_result / req_confirm_*：改为统一由 Entrypoint 的 EMV_Result* 写入
 *     express_kernel_post_result() 通知内核（见 express_api.h）。
 *   - aip/tvr/tsi 指针：改为每次经 appdata_find() 取用（避免 appdata 压缩后悬垂指针，P1-6）。
 * ------------------------------------------------------------------------- */
typedef struct
{
    /* --- 参数 --- */
    EXPRESS_TERM_PARAM          term;             /* 全局终端参数（emv.c:791-797 config） */
    EXPRESS_TERM_PRESET         preset;           /* 终端预设覆盖（emvi.h:232；emv.c:766 清零） */
    uint8_t                     kernel_index;     /* emv.c:821 EMVPORT_GetKernelIndex() */

    /* --- 交易上下文（Entrypoint 已通过 EMV_Set* 提供，kernal 只读） --- */
    uint8_t                     trans_type;       /* emv.c:846 */
    uint32_t                    trans_amount;     /* emv.c:879 */
    uint32_t                    trans_amount_other;/* emv.c:891 */
    bool                        bis_unable_online;/* emv.c:997-1002：终端不支持联机（原 unable_online，按美运字段名 1:1 移植）；
                                                     消费者 emv_initiate_application.c:86,93；
                                                     emv_terminal_action_analysis.c:187；emv_complete.c:140 */
    uint8_t                     force_online;     /* emv.c:924 qpboc_force_online（0/1，来自 EMV_PreProcess(bOnline)） */

    /* --- Expresspay 专有交易标志（emvi.h:209-217，全部有实际读取点） --- */
    uint8_t                     CANAI;            /* Contactless App Not Allowed Indicator：
                                                     emv_application_selection.c:676 置位；
                                                     emv_processing_restrictions.c:275,343 读取；
                                                     :318 清零 */
    uint8_t                     CVMRLEF;          /* CVM Required Limit Exceeded Flag：
                                                     emv_application_selection.c:694；emv_processing_restrictions.c:232,242,315；
                                                     emv_cardholder_verification.c:835；emv_initiate_application.c:78 */
    uint8_t                     TermCFLEF;        /* Terminal CVM/Floor Limit Exceeded Flag：
                                                     emv_processing_restrictions.c:316 清零（当前无置位点，见 P2-5） */
    uint8_t                     TermEFLEF;        /* Terminal (Reader) Contactless Floor Limit Exceeded：
                                                     emv_application_selection.c:703,712；emv_terminal_risk_management.c:146 */
    uint8_t                     gExpressmobileagain;/* Mobile CVM 重试标志：
                                                     emv_cardholder_verification.c:532-536,895-897；
                                                     emv_terminal_action_analysis.c:719-721；emv.c:1145-1148 */
    uint8_t                     CVMParameter;     /* Outcome CVM 值：emv_cardholder_verification.c:548..800；
                                                     emvoutcome.c:214,230 */

    /* --- 应用选择结果（Entrypoint 组合选择后由内核读取） --- */
    uint8_t                     selected_aid[16];
    uint8_t                     selected_aid_len;
    uint8_t                     selected_termapp_index;  /* 对应美运 EMV_CANDIDATE.rsv[0] */

    /* --- 内核状态机（与 Entrypoint 状态码同值；Entrypoint 每次 dispatch 后回读） --- */
    int32_t                     status;            /* 对应美运 g_emv_cnxt->status（emv.c:523/1168-1257） */
    uint8_t                     card_type;         /* 内核只处理非接交易（Expresspay v4.2），恒为 EMV_CARD_CONTACTLESS */
    uint8_t                     card_types_allowed;/* 对应美运 emv.c:925；内核 init/preprocess 内部使用 */

    /* --- appdata 缓存副本（替代美运的裸指针，修 P1-6 悬垂指针） ---
     * 说明：美运把 aip/tvr/tsi 以裸指针缓存；Entrypoint 的 appdata_save 会搬移缓冲区。
     *       移植后改为"小数组副本"：aip 在 GPO 解析后刷新，tvr/tsi 由 express_tvr_set/
     *       express_tsi_set 写穿透（同时写 appdata 与副本），保证与 appdata 一致。 */
    uint8_t                     aip[2];            /* [82] */
    uint8_t                     tvr[5];            /* [95] */
    uint8_t                     tsi[3];            /* [9B] */

    /* --- 离线数据认证（美运 emvi.h:197-199 三个 bool；保留原名以逐行移植） --- */
    bool                        da_sda_performed;
    bool                        da_dda_performed;
    bool                        da_cda_performed;

    /* --- 杂项（原 EMV_CONTEXT） --- */
    uint8_t                     pay_mode;         /* emv.c:552 仅初始化，当前无读取（P2-6） */
    uint8_t                     ec_onlinpin_falg; /* emv.c:558 仅初始化，当前无读取（P2-6） */
    uint8_t                     cv_rule_index;    /* emvi.h:177 声明未使用（P2-6） */

    /* --- 认证状态 --- */
    bool                        da_readrec_failed;/* emv_read_application_data.c:456-461,499；
                                                     emv_offline_data_authentication.c:966,1037,1301 */
    uint8_t                     oda_type;         /* 合并 emvi.h:197-199 三个 bool；emv_offline_data_authentication.c:1402-1426 */
    uint8_t                     req_pk_algorithm; /* emv_offline_data_authentication.c:982..1352 */
    bool                        bis_card_sup_onlinepin; /* 卡支持联机 PIN：emv_cardholder_verification.c:667 置位；
                                                             emv_complete.c:226 读取 */
    bool                        bis_online_pin_entered; /* 本笔已选 Online PIN（§8.2.3.1）→ 必须联机；
                                                             由 express_perform_cvm 置位，express_action_analyse 强制 ARQC */

    /* --- 交互结果（Entrypoint EMV_Result* 转交） --- */
    int32_t                     req_sel_accounttype_result;
    int32_t                     req_check_id_result;
    int32_t                     req_online_pin_result;   /* emv_cardholder_verification.c:110,114；emv_complete.c:228,232 */
    int32_t                     req_offline_pin_result;  /* Expresspay v4.2 禁止离线 PIN（P p23 §2.10.1）：
                                                             恒为 EMV_OPER_UNSUPPORTED，仅为 API 兼容保留 */
    int32_t                     req_confirm_bypass_pin_result; /* 离线 PIN 旁路：v4.2 不使用 */
    int32_t                     req_confirm_force_online_result;
    int32_t                     req_issuer_referral_result;/* emv_complete.c:284 */
    int32_t                     online_result;    /* emv_complete.c:200-202,267；emv.c:1379 */

    /* --- 每笔交易初始化（emv.c:516-560 EMV_initialize） --- */
    bool                        in_use;
    bool                        cardtype_locked;
} EXPRESS_CONTEXT;

extern EXPRESS_CONTEXT *g_express_cnxt;          /* 唯一全局上下文（替代美运 g_emv_cnxt） */

/* ---------------------------------------------------------------------------
 * Expresspay 专有 Outcome（美运 emvoutcome.h:4-36 / EMV_OUTCOME_DATA）
 *
 * Entrypoint 的 Outcome 枚举（src/inc/emvoutcome.h）没有 0xA0/0xB0/0xC0/0xD0 等 Expresspay 值
 * （方案 G5），因此内核把 Expresspay 扩展字段单独保存，由 Entrypoint 组装最终 Outcome 时合并。
 * 内核不调用 Entrypoint 的 EMV_SetOutCome/EMV_SetUIRequest（方案 §7.7）。
 * ------------------------------------------------------------------------- */
#define EXPRESS_OUTCOME_STATUS_NA                                        0xF0
#define EXPRESS_OUTCOME_STATUS_APPROVED                                  0x10
#define EXPRESS_OUTCOME_STATUS_DECLINED                                  0x20
#define EXPRESS_OUTCOME_STATUS_ONLINE_REQUEST                            0x30
#define EXPRESS_OUTCOME_STATUS_END_APPLICATION                           0x40
#define EXPRESS_OUTCOME_STATUS_SELECT_NEXT                               0x50
#define EXPRESS_OUTCOME_STATUS_TRY_ANOTHER_INTERFACE                     0x60
#define EXPRESS_OUTCOME_STATUS_END_APPLICATION_WITH_RESTART_COMMUNICATION_ERROR 0xA0
#define EXPRESS_OUTCOME_STATUS_END_APPLICATION_WITH_RESTART_CDCVM        0xB0
#define EXPRESS_OUTCOME_STATUS_ONLINE_REQUEST_TWO_PRESENTMENTS           0xC0
#define EXPRESS_OUTCOME_STATUS_ONLINE_REQUEST_PRESENT_AND_HOLD           0xD0

#define EXPRESS_OUTCOME_START_NA     0xF0
#define EXPRESS_OUTCOME_START_A      0x00
#define EXPRESS_OUTCOME_START_B      0x10
#define EXPRESS_OUTCOME_START_C      0x20
#define EXPRESS_OUTCOME_START_D      0x30

#define EXPRESS_OUTCOME_ONLINE_RESP_DATA_NA      0xF0
#define EXPRESS_OUTCOME_ONLINE_RESP_DATA_EMVDATA 0x10
#define EXPRESS_OUTCOME_ONLINE_RESP_DATA_ANY     0x20

#define EXPRESS_OUTCOME_CVM_NA               0xF0
#define EXPRESS_OUTCOME_CVM_NOCVM            0x00
#define EXPRESS_OUTCOME_CVM_OBTAIN_SIGNATURE 0x10
#define EXPRESS_OUTCOME_CVM_ONLINE_PIN       0x20
#define EXPRESS_OUTCOME_CVM_CDCVM            0x30

#define EXPRESS_OUTCOME_AIP_NA           0xF0
#define EXPRESS_OUTCOME_AIP_CONTACT_CHIP 0x10

#define EXPRESS_OUTCOME_FIELDOFF_NA 0xFF

typedef struct
{
    uint8_t Status;               /* EXPRESS_OUTCOME_STATUS_* */
    uint8_t Start;                /* EXPRESS_OUTCOME_START_* */
    uint8_t OnlineRespData;       /* EXPRESS_OUTCOME_ONLINE_RESP_DATA_* */
    uint8_t CVM;                  /* EXPRESS_OUTCOME_CVM_* */
    uint8_t PresentFlag;          /* b8 UI on outcome / b7 UI on restart / b6 data record / b5 disc data / b4 receipt */
    uint8_t InterfacePrefer;      /* EXPRESS_OUTCOME_AIP_* */
    uint8_t FieldoffReq;          /* Field-off time（0x13 表示 1.3s，0xFF 无请求） */
    uint8_t RemovalTimeout[2];
} EXPRESS_OUTCOME;

/* ---------------------------------------------------------------------------
 * 内核各阶段入口（美运 emvi.h:591-666 的 extern 声明；移植后改为 express_ 前缀）
 * 返回值：EMV_* 状态码（见模型 §2.3）
 * ------------------------------------------------------------------------- */
int32_t express_app_sel_init(void);              /* emv_application_selection.c:1259（裁剪：不做候选，仅最终 SELECT+参数装载） */
int32_t express_initiate_application(void);      /* emv_initiate_application.c:306 */
int32_t express_read_application_data(void);     /* emv_read_application_data.c:337 */
int32_t express_offline_data_auth(void);         /* emv_offline_data_authentication.c:1395 */
int32_t express_processing_restrictions(void);   /* emv_processing_restrictions.c:408 */
int32_t express_cardholder_verify(void);         /* emv_cardholder_verification.c:825（原 _temp 版本） */
int32_t express_terminal_risk_manage(void);      /* emv_terminal_risk_management.c:110 */
int32_t express_action_analyse(void);            /* emv_terminal_action_analysis.c:803 emv_action_analyse_temp */
int32_t express_pre_complete(void);              /* emv_complete.c:110 emv_pre_complete */
int32_t express_complete(void);                  /* emv_complete.c:189 emv_compelte_temp */
int32_t express_script_process(int32_t when, uint8_t *res, int32_t *reslen); /* emv_issuer_script_processing.c */

/* ---------------------------------------------------------------------------
 * 内核内部工具（原 emv.c:218-512，可直接复用 Entrypoint 同名实现）
 * 仅列出移植后仍需要、且 Entrypoint 侧签名不一致需适配函数的说明：
 *   emv_memcmp/emv_memcpy/emv_memset/emv_strlen   → Entrypoint emvi.h:278-281（count 由 unsigned→uint32_t 无差异）
 *   emv_bcd2str/emv_str2bcd/emv_dec2hex/emv_hex2dec/emv_dec2bcd/emv_bcd2dec → Entrypoint emvi.h:283-290
 *   emv_date_compare                              → Entrypoint emvi.h:293（语义：>0 大于当前日期）
 * ------------------------------------------------------------------------- */
int32_t express_crc32(uint32_t crc, const uint8_t *buf, int32_t len);  /* 原 emv.c:218（仅用于版本/参数校验和） */

#ifdef __cplusplus
}
#endif

#endif /* EXPRESS_TYPES_H_ */
