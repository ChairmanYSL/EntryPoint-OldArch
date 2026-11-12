/**
************************************************************************************************************************
* @file     express_param.h
* @brief    Expresspay (AMEX ZT_AMEX_CORE_V4.2.3) 参数结构 —— 精简最小集
*
* 设计依据（全部为美运 V4.2.3 源码检索结果，逐字段读写点见 doc/AMEX_PORT_PLAN.md §5）：
*   - EMV_TERMPARAM（emv_type.h:510-531）15 个字段中，内核真正读取的只有 6 个；
*   - EMV_AIDPARAM（emv_type.h:536-598）46 个字段中，零使用 3 个（zero_amt_allow 等）、
*     其余使用点集中在 emv_application_selection.c:664-721/723-859 两处；
*   - 嵌入的 EMV_TERMPARAM（apar->term）只用到 4 个字段（9F7A/9F7B/DF69）；
*   - EMV_AIDPARAM 大小从 712B 降到 EXPRESS_AID_PARAM 的 677B，其中 504B 是 DDOL/TDOL
*     规格上限（252+252，EMV Book 3），属于"必须保留但可后续改为按需解析"的部分。
*
* 约束：
*   1. 本头文件不依赖 Entrypoint 任何头文件（kernels/expresspay 可独立包含）；
*      与 Entrypoint 共享的类型（EMV_TERMAPP/EMV_TLVOBJ/EMV_CAPKPARAM 等）由 .c 文件包含
*      Entrypoint 的 emv_type.h，避免二次定义。
*   2. 结构体内字段顺序 = 解析顺序，禁止依赖隐式 padding（全 uint8_t，无 padding）。
*   3. 所有 "exist" 标志语义：1=主机下发过该 TLV，0=未下发（使用默认值）。
************************************************************************************************************************
*/
#ifndef EXPRESS_PARAM_H_
#define EXPRESS_PARAM_H_

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * 容量/长度常量
 * ------------------------------------------------------------------------- */

/** 美运参数文件单条记录长度上限（emvporting.h / emvporting.c:589,590,679 的 PARA_MAXLEN） */
#define EXPRESS_PARA_MAXLEN         1020
/** 单条 AID 参数 TLV 缓冲区（emv.c:898 使用 aidtlv[1020]） */
#define EXPRESS_PARA_SAVLEN         1024
/** Dynamic Reader Limit 条目数（emvi.h:103 EXPRESS_DRL_NUM） */
#define EXPRESS_DRL_NUM             16
/** DDOL/TDOL 规格上限（EMV Book 3：DOL 长度最大 252 字节） */
#define EXPRESS_DOL_MAX             252
/** AID 个数上限，必须与 Entrypoint EMV_CFG_MAX_APPS 保持一致（emvi.h:59 = 128） */
#define EXPRESS_MAX_APPS            128

/* ---------------------------------------------------------------------------
 * Dynamic Reader Limit（Amex 4.2 非接读卡器限额，emvi.h:105-124 / emvporting.c:1707-1865）
 * 使用点：emv_processing_restrictions.c:89-235（按 AID 的 AppProgID 选择 DRL 并重算限额）
 * ------------------------------------------------------------------------- */
typedef struct
{
    uint8_t ifenable;             /* 1=该 DRL 生效（emv_processing_restrictions.c:272,301,305） */
    uint8_t AppProgID;            /* 与卡片 9F70/DF81xx 中的 Application Program ID 低 4 位比较
                                     （emv_processing_restrictions.c:305：(CardInfAndPayCapab[1]&0x0F)） */
    uint8_t SupportRCTLCheck;     /* 1=TransLimit 有效（emv_processing_restrictions.c:212） */
    uint8_t SupportCVMLimitCheck; /* 1=CVMLimit 有效（emv_processing_restrictions.c:228） */
    uint8_t SupportFloorLimitCheck;/* 1=FloorLimit 有效（emv_processing_restrictions.c:220） */
    uint8_t TransLimitlen;
    uint8_t TransLimit[6];        /* BCD，Contactless Reader Transaction Limit（DF20 的 DRL 版本） */
    uint8_t CVMLimitlen;
    uint8_t CVMLimit[6];          /* BCD，CVM Required Limit（DF21 的 DRL 版本；0xFF..FF 表示无效） */
    uint8_t FloorLimitlen;
    uint8_t FloorLimit[6];        /* BCD，Reader Contactless Floor Limit（DF19 的 DRL 版本） */
} EXPRESS_DRL;

typedef struct
{
    EXPRESS_DRL defaultDRL;                 /* 默认 DRL（无 AID 匹配时使用，emv_processing_restrictions.c:96-137） */
    EXPRESS_DRL DRLs[EXPRESS_DRL_NUM];      /* 按 AppProgID 的 DRL 表（emv_processing_restrictions.c:151-194） */
} EXPRESS_DRL_PARAMS;

/* ---------------------------------------------------------------------------
 * 全局终端参数（由 emvtermN.par 的原始 TLV 解析；字段 = 美运 EMV_TERMPARAM 的实际读取子集）
 *
 * 字段取舍依据（AMEX_PORT_PLAN.md §5.1）：
 *   保留：ics / term_type / term_caps / term_add_caps / 9F6D / 9F6E / status_check_support /
 *         ec_* / sm_support / zero_amt_allow / is_support_excetionfile / is_support_revoke_capk
 *   删除：TTQ(9F66) —— Expresspay v4.2 全文无该 tag（见 spec 笔记 §3.6），终端能力由 9F6D/9F6E 表达。
 * ------------------------------------------------------------------------- */
typedef struct
{
    /* --- 内核逻辑直接读取 --- */
    uint8_t ics[7];                       /* ICS 位图：ics_option() 宏（emvi.h:359），
                                             使用点 emv_application_selection.c:599,1019;
                                             emv_offline_data_authentication.c:109,116,144,529,748,1164,1412;
                                             emv_terminal_action_analysis.c:79 */
    uint8_t term_type;                    /* [9F35] TT_ATTENDED/TT_ONLINE_ONLY 等宏（emvi.h:258-265）；
                                             emv_initiate_application.c:86,91 决定在线/离线能力 */
    uint8_t term_caps[3];                 /* [9F33] 仅经 tc_option() 宏使用（当前生效调用点均已注释，
                                             emv.c:630,647；保留作为 9F33 注入 appdata 的兜底值） */
    uint8_t term_add_caps[5];             /* [9F40] emv_application_selection.c:90,92,94
                                             （扩展选择 1F30 / ICTI 判定），以及 9F40 注入 */
    uint8_t cl_capability;                /* [9F6D] Contactless Reader Capabilities（1B，
                                             Expresspay v4.2 Table 21-7 p89）；preprocess 能力门禁 */
    uint8_t enhanced_cl_capability[4];    /* [9F6E] Enhanced Contactless Reader Capabilities（4B，
                                             Table 21-9 p90-91）；preprocess 能力门禁 + CVM 能力位 */

    /* --- 注入 appdata 的终端能力/指示器（原从 EMV_AIDPARAM.term.* 读取，
           emv_application_selection.c:816-819、795-796） --- */
    uint8_t status_check_support;         /* [1F02] → TAG_STATUS_CHECK_SUPPORT_INDICATOR */
    uint8_t ec_support;                   /* [9F7A] → TAG_9F7A */
    uint8_t ec_trans_limit_exist;         /* [9F7B] 存在性 */
    uint8_t ec_trans_limit[6];            /* [9F7B] */
    uint8_t sm_support;                   /* [DF69] → TAG_DF69 */

    /* --- 美运私有支持位 --- */
    uint8_t zero_amt_allow;               /* [DF8307] 下载保存；内核当前无读取（见 P2-3） */
    uint8_t is_support_excetionfile;      /* [DF8308] emv_terminal_risk_management.c:119（例外文件） */
    uint8_t is_support_revoke_capk;       /* [DF8309] emv_offline_data_authentication.c:263（CAPK 撤销） */
} EXPRESS_TERM_PARAM;                     /* sizeof = 34 字节 */

/* ---------------------------------------------------------------------------
 * 单 AID 参数（= emvtermN.par 原始 TLV 覆盖 + emvaidsN.par 中该 AID 的原始 TLV 覆盖）
 *
 * 美运原始行为：emvporting_get_aidparam()（emvporting.c:779-848）先用终端参数文件填
 * EMV_TERMPARAM+EMV_AIDPARAM，再用该 AID 的 TLV 二次 conv_param 覆盖 → 移植后
 * express_param_load_aid() 必须保持"先全局、后 AID"的覆盖顺序。
 *
 * 每个字段的唯一消费者是 emv_application_selection.c:723-859（load_term_app_param）与
 * emv_application_selection.c:664-721（again_preprocess）。其余 L2 模块只读 appdata，不读本结构。
 * ------------------------------------------------------------------------- */
typedef struct
{
    /* --- 应用标识 --- */
    uint8_t aid[16];                      /* [9F06] emv_application_selection.c:736 */
    uint8_t aid_len;
    uint8_t app_sel_indicator;            /* [DF01] 仅被 emvporting.c:404 写；消费方是 EMV_TERMAPP.asi
                                             （emvporting.c:956-963），本结构保留仅供日志/兼容 */
    uint8_t app_ver[2];                   /* [9F09] → 9F09（emv_application_selection.c:821） */

    /* --- 目标命中率（风控） --- */
    uint8_t target_percent;               /* [DF17] → DF17（:822） */
    uint8_t max_target_percent;           /* [DF16] → DF16（:823） */
    uint8_t threshold_value[4];           /* [DF15] → DF15（:824） */

    /* --- 默认 DOL（离线数据认证用） --- */
    uint8_t default_ddol_len;
    uint8_t default_ddol[EXPRESS_DOL_MAX];/* [DF55] 注意：美运 tag 值为 0xDF55，非 0xDF14
                                             （emv_tag.h:455；conv_param emvporting.c:418-424）
                                             消费者：emv_offline_data_authentication.c:748
                                             （ics_option(DA_Support_Default_DDOL)） */
    uint8_t default_tdol_len;
    uint8_t default_tdol[EXPRESS_DOL_MAX];/* [1F03]（conv_param emvporting.c:425-431）
                                             消费者：emv_terminal_action_analysis.c:79 */

    /* --- 收单方/商户 --- */
    uint8_t acquirer_id[6];               /* [9F01] → 9F01（:848） */
    uint8_t term_id[8];                   /* [9F1C] → 9F1C（:831） */
    uint8_t term_ifd[8];                  /* [9F1E] → 9F1E（:832） */
    uint8_t term_country_code[2];         /* [9F1A] → 9F1A（:833） */
    uint8_t merchant_category_code[2];    /* [9F15] → 9F15（:849） */
    uint8_t merchant_id[15];              /* [9F16] → 9F16（:850） */
    uint8_t merchant_name_len;
    uint8_t merchant_name[20];            /* [9F4E] → 9F4E（:851）；V4.2.3 缓冲区为 20B（emv_type.h:578）
                                             注意：conv_param（emvporting.c:483）未做长度上限校验，是 P0-2 */

    /* --- 货币 --- */
    uint8_t trans_currency_code[2];       /* [5F2A] → 5F2A（:836） */
    uint8_t trans_currency_exponent;      /* [5F36] → 5F36（:837） */
    uint8_t trans_ref_currency_code[2];   /* [9F3C] → 9F3C（:838） */
    uint8_t trans_ref_currency_exponent;  /* [9F3D] → 9F3D（:839） */

    /* --- Terminal Action Codes --- */
    uint8_t term_action_code_default_exist;
    uint8_t term_action_code_default[5];  /* [DF11] → DF11（:841） */
    uint8_t term_action_code_online_exist;
    uint8_t term_action_code_online[5];   /* [DF12] → DF12（:843） */
    uint8_t term_action_code_denial_exist;
    uint8_t term_action_code_denial[5];   /* [DF13] → DF13（:845） */

    uint8_t term_risk_mana_data_len;
    uint8_t term_risk_mana_data[8];       /* [9F1D] → 9F1D（:847） */

    /* --- 非接限额（Entrypoint 预处理也会用同一批 TLV，见 §5.2 复核） --- */
    uint8_t cl_trans_limit_exist;
    uint8_t cl_trans_limit[6];            /* [DF20] 消费者 again_preprocess（:670-678）→ CANAI */
    uint8_t cl_floor_limit_exist;
    uint8_t cl_floor_limit[6];            /* [DF19] 消费者 again_preprocess（:697-705）→ TermEFLEF */
    uint8_t cvm_limit_exist;
    uint8_t cvm_limit[6];                 /* [DF21] 消费者 again_preprocess（:681-695）→ CVMRLEF */
    uint8_t term_floor_limit[4];          /* [9F1B] 消费者 again_preprocess（:706-714）+ 9F1B 注入（:794） */

    /* --- 终端能力（per-AID 覆盖，兜底用全局 EXPRESS_TERM_PARAM） --- */
    uint8_t enhanced_cl_capability[4];    /* [9F6E] emv_application_selection.c:778（g_express_preset 优先） */
    uint8_t cl_capability;                /* [9F6D] emv_application_selection.c:787（g_express_preset 优先） */

    uint8_t online_pin_support_indicator; /* [DF18] → DF18（:853） */
    uint8_t extend_sel_sup;               /* [1F30] → 1F30（:854）；同时进 EMV_TERMAPP.rsv（emvporting.c:965-968） */
    uint8_t status_check_sup;             /* [1F02] → 1F02（:796） */
    uint8_t zero_amt_allow;               /* [DF8307] 写于 emvporting.c:381，内核零读取（可删，见 P2-3） */
    uint8_t term_type;                    /* [9F35] → 9F35（:750，J03 特殊需求） */
    uint8_t term_cap[3];                  /* [9F33] → 9F33（:759） */
    uint8_t term_add_cap[5];              /* [9F40] → 9F40（:769） */
} EXPRESS_AID_PARAM;                      /* sizeof = 670 字节（其中 DDOL+TDOL = 504） */

/* ---------------------------------------------------------------------------
 * 终端预设（应用运行期通过 API 覆盖参数文件里的 9F33/9F35/9F40/9F6D/9F6E）
 * 来源：emvi.h:220-232 EXPRESS_TERM_PRESET；写入口 emv.c:1037-1132（EMV_SetTerm 系列）与 emvapi.c:270-291
 * 消费：emv_application_selection.c:744-788（存在则覆盖 appdata，否则用 per-AID 参数）
 * 说明：ClessReaderCapab 无独立 exist 标志（emv_application_selection.c:781 直接判非 0），保持原语义。
 * ------------------------------------------------------------------------- */
typedef struct
{
    uint8_t exist_term_caps;
    uint8_t term_caps[3];                 /* 9F33 */
    uint8_t exist_term_type;
    uint8_t term_type;                    /* 9F35 */
    uint8_t exist_term_add_caps;
    uint8_t term_add_caps[5];             /* 9F40 */
    uint8_t exist_ClessReaderCapab;
    uint8_t ClessReaderCapab;             /* 9F6D */
    uint8_t exist_EnhancedClessReaderCapab;
    uint8_t EnhancedClessReaderCapab[4];  /* 9F6E */
} EXPRESS_TERM_PRESET;                    /* sizeof = 19 字节（全 uint8_t，无 padding） */

/* ---------------------------------------------------------------------------
 * 参数访问服务（由 kernels/expresspay/src/express_param.c 实现；数据来源为 Entrypoint 原始 TLV 服务）
 *
 * 返回：0=成功，<0=失败（与美运 emvporting_* 一致：非 0 即失败）
 * 说明：
 *   - express_param_load_term()      : 取全局终端参数原始 TLV（Entrypoint emv_get_termparam →
 *                                      emvporting_get_termparam_tlv），按 express_conv_param 解析；
 *                                      失败时保持美运"缺省值回填"语义（emvporting.c:273-316 init_param）。
 *   - express_param_load_aid()       : 先加载全局，再叠加该 AID 原始 TLV
 *                                      （Entrypoint emvporting_get_aidparam_tlv）。
 *   - express_param_load_drl()       : 读取 DRL（Entrypoint 侧已有 emvporting_get_drl /
 *                                      conv_drlparam 基础，需核对 TLV 格式差异，见方案 §6.2）。
 * ------------------------------------------------------------------------- */
int  express_param_load_term(EXPRESS_TERM_PARAM *term);
int  express_param_load_aid(EXPRESS_AID_PARAM *aid, uint8_t termapp_index);
int  express_param_load_drl(EXPRESS_DRL_PARAMS *drl);
int  express_param_get_aid_count(void);

/* 编译期尺寸门禁（C99 兼容写法）：布局被改坏时直接编译失败 */
#define EXPRESS_STATIC_ASSERT(cond, name) typedef char express_static_assert_##name[(cond) ? 1 : -1]

EXPRESS_STATIC_ASSERT(sizeof(EXPRESS_TERM_PARAM) == 34, express_term_param_size);
EXPRESS_STATIC_ASSERT(sizeof(EXPRESS_TERM_PRESET) == 19, express_term_preset_size);
EXPRESS_STATIC_ASSERT(sizeof(EXPRESS_AID_PARAM) == 670, express_aid_param_size);
EXPRESS_STATIC_ASSERT(sizeof(EXPRESS_DRL) == 26, express_drl_size);

#endif /* EXPRESS_PARAM_H_ */
