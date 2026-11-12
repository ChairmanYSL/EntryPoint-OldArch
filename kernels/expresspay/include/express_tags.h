/**
************************************************************************************************************************
* @file     express_tags.h
* @brief    Expresspay (AMEX ZT_AMEX_CORE_V4.2.3) 私有/厂商扩展标签常量
*
* 说明：
*   1. 本文件只定义 Entrypoint 公共 emv_tag.h 可能不包含、或被 Expresspay 内核按
*      "厂商私有值"使用的标签；全部带 #ifndef 保护，可与 Entrypoint emv_tag.h 共存。
*   2. 标签值一律来自美运 V4.2.3 源码 submodule/emvco/inc/emv_tag.h，不做"猜测修正"。
*      其中 DF14(Default DDOL) 在美运源码中被改成了 0xDF55（emv_tag.h:455），
*      移植时保持该厂商值，不得改回 EMVCo 的 0xDF14，否则现场参数文件下发失效。
*   3. 本文件不包含任何函数/变量定义，只含宏。
************************************************************************************************************************
*/
#ifndef EXPRESS_TAGS_H_
#define EXPRESS_TAGS_H_

/* ---------------------------------------------------------------------------
 * Expresspay 终端能力类标签
 * ------------------------------------------------------------------------- */
#ifndef TAG_9F6D_CLESS_READER_CAPAB
#define TAG_9F6D_CLESS_READER_CAPAB         0x9F6D  /* ClessReaderCapab        (emv_tag.h:391) */
#endif
#ifndef TAG_9F6E_ENHANCED_CLESS_READER_CAPAB
#define TAG_9F6E_ENHANCED_CLESS_READER_CAPAB 0x9F6E /* EnhancedClessReaderCapab(emv_tag.h:394) */
#endif
/* 美运源码沿用的别名（Entrypoint 公共头使用 TAG_9F6D_/TAG_9F6E_ 长名） */
#ifndef TAG_ClessReaderCapab
#define TAG_ClessReaderCapab                TAG_9F6D_CLESS_READER_CAPAB
#endif
#ifndef TAG_EnhancedClessReaderCapab
#define TAG_EnhancedClessReaderCapab        TAG_9F6E_ENHANCED_CLESS_READER_CAPAB
#endif
/* 0x9F70 Card Interface and Payment Capabilities（Expresspay §7.2.4/§8.2.4 使用；
 * Entrypoint 公共 emv_tag.h 未定义） */
#ifndef TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP
#define TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP 0x9F70 /* emv_tag.h（美运 4.2.3 私有扩展） */
#endif
/* 0x9F71 Mobile CVM Result（Expresspay §4.3.3/§8.2.3.2 使用） */
#ifndef TAG_9F71_MB_CVMResult
#define TAG_9F71_MB_CVMResult               0x9F71
#endif
/* 0xFF8178 Terminal Type - Modified（pre-PDOL 临时值，美运私有） */
#ifndef TAG_FF8178_TERM_TYPE_MODIFY
#define TAG_FF8178_TERM_TYPE_MODIFY         0xFF8178
#endif
/* 0xDF8114 GAC Request Type（美运 TAA 内部使用） */
#ifndef TAG_GAC_REQUEST_TYPE
#define TAG_GAC_REQUEST_TYPE                0xDF8114
#endif
#ifndef TAG_9F66_TERMINAL_TRANSACTION_QUALIFIERS
#define TAG_9F66_TERMINAL_TRANSACTION_QUALIFIERS 0x9F66 /* TTQ (4 bytes) */
#endif

/* ---------------------------------------------------------------------------
 * 终端/应用参数（主机下发 TLV）
 * ------------------------------------------------------------------------- */
#ifndef TAG_DF11_TAC_DEFAULT
#define TAG_DF11_TAC_DEFAULT                0xDF11  /* emv_tag.h:446 */
#endif
#ifndef TAG_DF12_TAC_ONLINE
#define TAG_DF12_TAC_ONLINE                 0xDF12  /* emv_tag.h:449 */
#endif
#ifndef TAG_DF13_TAC_DENIAL
#define TAG_DF13_TAC_DENIAL                 0xDF13  /* emv_tag.h:452 */
#endif
/* 注意：美运把 Default DDOL 的标签改成了 0xDF55（emv_tag.h:455，注释说明与 V2CA0130004V4.1a 案例冲突） */
#ifndef TAG_DF55_DEFAULT_DDOL
#define TAG_DF55_DEFAULT_DDOL               0xDF55  /* emv_tag.h:455 */
#endif
/* 注意：Entrypoint 公共 emv_tag.h 把 TAG_DEFAULT_TDOL 定义为 PayPass 的 0xDF8102；
 * 美运 V4.2.3 用 0x1F03（emv_tag.h:568），必须覆盖，否则 TDOL 装载/注入全部失效。 */
#undef  TAG_DEFAULT_TDOL
#define TAG_DEFAULT_TDOL                    0x1F03  /* emv_tag.h:568（覆盖 Entrypoint 的 DF8102） */
#ifndef TAG_DF15_THRESHOLD_VALUE
#define TAG_DF15_THRESHOLD_VALUE            0xDF15  /* emv_tag.h:458 */
#endif
#ifndef TAG_DF16_MAX_TARGET_PERCENTAGE
#define TAG_DF16_MAX_TARGET_PERCENTAGE      0xDF16  /* emv_tag.h:461 */
#endif
#ifndef TAG_DF17_TARGET_PERCENTAGE
#define TAG_DF17_TARGET_PERCENTAGE          0xDF17  /* emv_tag.h:464 */
#endif
#ifndef TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR
#define TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR 0xDF18 /* emv_tag.h:467 */
#endif
#ifndef TAG_DF19_CONTACTLESS_FLOOR_LIMIT
#define TAG_DF19_CONTACTLESS_FLOOR_LIMIT    0xDF19  /* emv_tag.h:470 */
#endif
#ifndef TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT
#define TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT 0xDF20 /* emv_tag.h:473 */
#endif
#ifndef TAG_DF21_CVM_REQUIRED_LIMIT
#define TAG_DF21_CVM_REQUIRED_LIMIT         0xDF21  /* emv_tag.h:476 */
#endif
#ifndef TAG_DF69_SM_SUPPORT_INDICATOR
#define TAG_DF69_SM_SUPPORT_INDICATOR       0xDF69  /* emv_tag.h:488 */
#endif
#ifndef TAG_1F02_STATUS_CHECK_SUPPORT
#define TAG_1F02_STATUS_CHECK_SUPPORT       0x1F02  /* Status Check Support Indicator (emv_tag.h:566) */
#endif
#ifndef TAG_1F04_ZERO_AMT_ALLOWED
#define TAG_1F04_ZERO_AMT_ALLOWED           0x1F04  /* zero amount allowed（Entrypoint 公共 emv_type.h 里叫 zero_amt_allow_flag） */
#endif
#ifndef TAG_1F30_EXTENDED_SELECT_SUPPORT
#define TAG_1F30_EXTENDED_SELECT_SUPPORT    0x1F30  /* Extended Selection Support (emv_tag.h:569) */
#endif
#ifndef TAG_9F7A_EC_TERMINAL_SUPPORT_INDICATOR
#define TAG_9F7A_EC_TERMINAL_SUPPORT_INDICATOR 0x9F7A /* emv_tag.h:413 */
#endif
#ifndef TAG_9F7B_EC_TERMINAL_TRANSACTION_LIMIT
#define TAG_9F7B_EC_TERMINAL_TRANSACTION_LIMIT 0x9F7B /* emv_tag.h:416 */
#endif

/* ---------------------------------------------------------------------------
 * 美运私有支持位（DF8307/08/09）
 * ------------------------------------------------------------------------- */
#ifndef TAG_DF8307_ZERO_AMT_ALLOWED_OFFLINE
#define TAG_DF8307_ZERO_AMT_ALLOWED_OFFLINE 0xDF8307 /* emv_tag.h:494 */
#endif
#ifndef TAG_DF8308_EXCEPTION_FILE_SUPPORT
#define TAG_DF8308_EXCEPTION_FILE_SUPPORT   0xDF8308 /* emv_tag.h:496 */
#endif
#ifndef TAG_DF8309_REVOKE_CAPK_SUPPORT
#define TAG_DF8309_REVOKE_CAPK_SUPPORT      0xDF8309 /* emv_tag.h:498 */
#endif

/* ---------------------------------------------------------------------------
 * 卡片返回 / 候选列表扩展标签
 * ------------------------------------------------------------------------- */
#ifndef TAG_9F0A_ASRP_DATA
#define TAG_9F0A_ASRP_DATA                  0x9F0A  /* emv_tag.h:505 */
#endif
#ifndef TAG_9F0C_IINE
#define TAG_9F0C_IINE                       0x9F0C  /* emv_tag.h:509 */
#endif
#ifndef TAG_5F55_ICC2
#define TAG_5F55_ICC2                       0x5F55  /* emv_tag.h:511 */
#endif
#ifndef TAG_5F56_ICC3
#define TAG_5F56_ICC3                       0x5F56  /* emv_tag.h:513 */
#endif
#ifndef TAG_9F7D_CPD
#define TAG_9F7D_CPD                        0x9F7D  /* emv_tag.h:515 */
#endif
#ifndef TAG_9F71_MB_CVM_RESULT
#define TAG_9F71_MB_CVM_RESULT              0x9F71  /* Mobile CVM Result（emv_initiate_application.c:248） */
#endif

/* ---------------------------------------------------------------------------
 * Expresspay 状态/结束类（值来自 emv_type.h；Entrypoint 中同名不同值/不同名，见移植方案 §2.3）
 * ------------------------------------------------------------------------- */
#define EXPRESS_REQ_EC_CONFIRM              25      /* emv_type.h:264（Entrypoint 无同值状态） */
#define EXPRESS_STA_SELECT_NEXT             26      /* emv_type.h:265（Entrypoint: EMV_REQ_SELECT_NEXT=26） */
#define EXPRESS_SW_INTERFACE                106     /* emv_type.h:275（Entrypoint: EMV_SWITCH_INTERFACE=106） */
#define EXPRESS_REQ_SEEPHONE                107     /* emv_type.h:276（Entrypoint: EMV_SEEPHONE=107） */

#endif /* EXPRESS_TAGS_H_ */
