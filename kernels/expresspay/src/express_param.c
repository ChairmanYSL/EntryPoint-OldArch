/**
************************************************************************************************************************
* @file     express_param.c
* @brief    Expresspay 内核参数装载（conv_param + 默认值 + 终端/AID/DRL 装载）
*
* 移植来源（美运 V4.2.3）：
*   - src/emvporting.c:273-316 init_param   → express_init_term_defaults / express_init_aid_defaults
*   - src/emvporting.c:318-573 conv_param   → express_conv_param
*   - src/emvporting.c:616-653/779-848      → express_param_load_term / express_param_load_aid
*   - src/emvporting.c:1865 emvporting_get_drl → express_param_load_drl（Entrypoint 无读取 API，方案 G2）
*
* 与美运的差异（= 方案 §9 P0/P1 修复，全部在本文件落地）：
*   P0-2 9F4E（商户名）超长写：目标 20B，先判长度；
*   P0-3 9F06（AID）超长写：目标 16B，先判长度；
*   P1-1 全部定长 memcpy 改走 express_copy_fixed()（短 TLV 不再越界读、缺省补 0）；
*   P1-2 9F40 是 5 字节（美运只拷 4 字节）；
*   另外 default_ddol/default_tdol/term_risk_mana_data/merchant_name 的边界与 0 长度判断。
*
* 数据来源：Entrypoint 把主机下发参数原样以 TLV 存盘（src/emvporting.c），内核只回读原始 TLV，
* 不拷贝 Entrypoint 的 EMV_TERMPARAM/EMV_AIDPARAM 结构体（方案 §5.5 D1-A）。
************************************************************************************************************************
*/
#include "express_priv.h"
/* 缺省 DOL（美运 emvporting.c:278-281）。用数组而不用字符串常量：避免与 libentrypoint.a
 * 的 init_param 产生同名 ??_C@ 字符串符号（符号审计的 duplicate 检查）。 */
static const u8 k_default_ddol[11] = {0x9F, 0x37, 0x04, 0x9F, 0x47, 0x01, 0x8F, 0x01, 0x9F, 0x32, 0x01};
static const u8 k_default_tdol[3]  = {0x9F, 0x08, 0x02};
/* Entrypoint 原始参数 TLV 回读（inc/emvapi.h:752-753；此处显式声明，避免引入 emvapi.h 的其他平台依赖） */
int32_t emvporting_get_termparam_tlv(uint8_t *par, int32_t *rlen);
int32_t emvporting_get_aidparam_tlv(int32_t inx, uint8_t *par, int32_t *rlen);

/* ---------------------------------------------------------------------------
 * 定长字段拷贝（P1-1）
 * 语义：最多拷 dstlen 字节并清零剩余；src 为 NULL 或 srclen<=0 时整体清零。
 * ------------------------------------------------------------------------- */
static void express_copy_fixed(uint8_t *dst, int32_t dstlen, const uint8_t *src, int32_t srclen)
{
    int32_t n;

    if (dst == NULL || dstlen <= 0)
    {
        return;
    }
    if (src == NULL || srclen <= 0)
    {
        emv_memset(dst, 0, dstlen);
        return;
    }

    n = (srclen < dstlen) ? srclen : dstlen;
    emv_memcpy(dst, src, n);
    if (n < dstlen)
    {
        emv_memset(dst + n, 0, dstlen - n);
    }
}

/* ---------------------------------------------------------------------------
 * 缺省参数（美运 emvporting.c:273-316 init_param）
 * 说明：保留日本 J03 缺省值（term_type=0x22 等），仅当参数文件缺失/解析失败时使用。
 * ------------------------------------------------------------------------- */
static void express_init_term_defaults(EXPRESS_TERM_PARAM *tpar)
{
    if (tpar == NULL)
    {
        return;
    }
    emv_memset(tpar, 0, sizeof(EXPRESS_TERM_PARAM));
    emv_memcpy(tpar->ics, "\xF4\xF0\xF0\xFC\xAF\xFE\xA0", 7);
    tpar->term_type = 0x22;
    emv_memcpy(tpar->term_caps, "\xE0\xF8\xC8", 3);
    emv_memcpy(tpar->term_add_caps, "\xFF\x80\xF0\xA0\x01", 5);
    /* Expresspay v4.2 终端能力缺省（Table 21-7 p89 / Table 21-9 p90-91）：
     * 9F6D = 1100 0000b（≥3.0 EMV，CVM Not Required；b4 由 pre-PDOL 按金额动态置位）
     * 9F6E = 9C E0 00 03：
     *   B1=1001 1100b  b8=1 支持 AEIPS 接触 / b7=0 不支持 Magstripe / b5=1 partial online /
     *                  b4=1 Mobile / b3=1 允许 Try Another Interface（b8=1 时本位必须为 1）
     *   B2=1110 0000b  b8=1 Mobile CVM / b7=1 Online PIN / b6=1 Signature / b5=0 不使用离线 PIN
     *   B3=00         每笔动态（pre-PDOL 重算）
     *   B4=03         Expresspay Kernel Version = 4.x */
    tpar->cl_capability = 0xC0;
    emv_memcpy(tpar->enhanced_cl_capability, "\x9C\xE0\x00\x03", 4);
    tpar->status_check_support = 1;
    tpar->ec_support = 0;
    tpar->sm_support = 0;
    tpar->zero_amt_allow = 0;
    tpar->is_support_excetionfile = 0;
    tpar->is_support_revoke_capk = 0;
}

static void express_init_aid_defaults(EXPRESS_AID_PARAM *apar)
{
    if (apar == NULL)
    {
        return;
    }
    emv_memset(apar, 0, sizeof(EXPRESS_AID_PARAM));
    apar->default_ddol_len = 11;
    emv_memcpy(apar->default_ddol, k_default_ddol, sizeof(k_default_ddol));
    apar->default_tdol_len = 3;
    emv_memcpy(apar->default_tdol, k_default_tdol, sizeof(k_default_tdol));
    emv_memcpy(apar->term_country_code, "\x03\x92", 2);
    emv_memcpy(apar->trans_currency_code, "\x03\x92", 2);
    apar->trans_currency_exponent = 0;
    emv_memcpy(apar->trans_ref_currency_code, "\x03\x92", 2);
    apar->trans_ref_currency_exponent = 0;
    apar->online_pin_support_indicator = 1;
    apar->cl_trans_limit_exist = 0;
    apar->cl_floor_limit_exist = 0;
    apar->cvm_limit_exist = 0;
    emv_memcpy(apar->term_floor_limit, "\x00\x00\x01\x00", 4);
    apar->extend_sel_sup = 1;
    apar->term_type = 0x22;
    emv_memcpy(apar->term_cap, "\xE0\xF8\xC8", 3);
    emv_memcpy(apar->term_add_cap, "\xFF\x80\xF0\xA0\x01", 5);
    apar->cl_capability = 0xC0;
    emv_memcpy(apar->enhanced_cl_capability, "\x9C\xE0\x00\x03", 4);
    apar->status_check_sup = 1;
}

/* ---------------------------------------------------------------------------
 * conv_param（美运 emvporting.c:318-573 + §9 修复）
 * tpar 与 apar 可单独为 NULL；值为 NULL 时不解析对应目标。
 * ------------------------------------------------------------------------- */
static void express_conv_param(EXPRESS_TERM_PARAM *tpar, EXPRESS_AID_PARAM *apar, uint8_t *in, int32_t inlen)
{
    EMV_TLVOBJ tlvs[64];
    int32_t i;
    int32_t tlvnum;

    if (in == NULL || inlen <= 0)
    {
        return;
    }

    emv_memset(tlvs, 0, sizeof(tlvs));
    if (emvtlv_decode(in, inlen, tlvs, (int32_t)(sizeof(tlvs) / sizeof(tlvs[0])), STRING_TLVOBJ) < 0)
    {
        return;
    }

    tlvnum = tlvs[0].childnum;
    if (tlvnum > (int32_t)(sizeof(tlvs) / sizeof(tlvs[0])) - 1)
    {
        tlvnum = (int32_t)(sizeof(tlvs) / sizeof(tlvs[0])) - 1;
    }

    for (i = 1; i <= tlvnum; ++i)
    {
        if (tlvs[i].pvalue == NULL || tlvs[i].valuelen < 0)
        {
            continue;
        }

        if (tpar != NULL)
        {
            switch (tlvs[i].tagname)
            {
                case TAG_ICS:
                    express_copy_fixed(tpar->ics, sizeof(tpar->ics), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F35_TERMINAL_TYPE:
                    tpar->term_type = tlvs[i].pvalue[0];
                    break;
                case TAG_9F33_TERMINAL_CAPABILITIES:
                    express_copy_fixed(tpar->term_caps, sizeof(tpar->term_caps), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES:
                    /* P1-2: 9F40 是 5 字节（美运只拷 4 字节） */
                    express_copy_fixed(tpar->term_add_caps, sizeof(tpar->term_add_caps), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F66_TERMINAL_TRANSACTION_QUALIFIERS:
                    /* Expresspay v4.2 无 TTQ（9F66 全文 0 次，spec 笔记 §3.6）。主机若仍下发，
                     * 内核**忽略**；终端能力只认 9F6D/9F6E（见下两个 case）。 */
                    eTRACE("AMEX-warning: TTQ(9F66) is not used by Expresspay v4.2, ignored");
                    break;
                case TAG_ClessReaderCapab:
                    tpar->cl_capability = tlvs[i].pvalue[0];
                    break;
                case TAG_EnhancedClessReaderCapab:
                    express_copy_fixed(tpar->enhanced_cl_capability, sizeof(tpar->enhanced_cl_capability),
                                       tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_STATUS_CHECK_SUPPORT_INDICATOR:
                    tpar->status_check_support = tlvs[i].pvalue[0];
                    break;
                case TAG_9F7A_EC_TERMINAL_SUPPORT_INDICATOR:
                    tpar->ec_support = tlvs[i].pvalue[0];
                    break;
                case TAG_9F7B_EC_TERMINAL_TRANSACTION_LIMIT:
                    tpar->ec_trans_limit_exist = 1;
                    express_copy_fixed(tpar->ec_trans_limit, sizeof(tpar->ec_trans_limit), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_DF69_SM_SUPPORT_INDICATOR:
                    tpar->sm_support = tlvs[i].pvalue[0];
                    break;
                case TAG_DF8307_ZERO_AMT_ALLOWED_OFFLINE:
                    tpar->zero_amt_allow = tlvs[i].pvalue[0];
                    break;
                case TAG_DF8308_EXCEPTION_FILE_SUPPORT:
                    tpar->is_support_excetionfile = tlvs[i].pvalue[0];
                    break;
                case TAG_DF8309_REVOKE_CAPK_SUPPORT:
                    tpar->is_support_revoke_capk = tlvs[i].pvalue[0];
                    break;
                default:
                    break;
            }
        }

        if (apar != NULL)
        {
            switch (tlvs[i].tagname)
            {
                case TAG_9F06_AID_TERMINAL:
                    /* P0-3: AID 缓冲区 16 字节 */
                    if (tlvs[i].valuelen <= (int32_t)sizeof(apar->aid))
                    {
                        apar->aid_len = (uint8_t)tlvs[i].valuelen;
                        emv_memcpy(apar->aid, tlvs[i].pvalue, tlvs[i].valuelen);
                    }
                    break;
                case TAG_DF01_ASI:
                    apar->app_sel_indicator = tlvs[i].pvalue[0];
                    break;
                case TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL:
                    express_copy_fixed(apar->app_ver, sizeof(apar->app_ver), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_DF17_TARGET_PERCENTAGE:
                    apar->target_percent = tlvs[i].pvalue[0];
                    break;
                case TAG_DF16_MAX_TARGET_PERCENTAGE:
                    apar->max_target_percent = tlvs[i].pvalue[0];
                    break;
                case TAG_DF15_THRESHOLD_VALUE:
                    express_copy_fixed(apar->threshold_value, sizeof(apar->threshold_value), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_DF14_DEFAULT_DDOL:
                    if (tlvs[i].valuelen <= (int32_t)sizeof(apar->default_ddol))
                    {
                        apar->default_ddol_len = (uint8_t)tlvs[i].valuelen;
                        emv_memset(apar->default_ddol, 0, sizeof(apar->default_ddol));
                        emv_memcpy(apar->default_ddol, tlvs[i].pvalue, tlvs[i].valuelen);
                    }
                    break;
                case TAG_DEFAULT_TDOL:
                    if (tlvs[i].valuelen <= (int32_t)sizeof(apar->default_tdol))
                    {
                        apar->default_tdol_len = (uint8_t)tlvs[i].valuelen;
                        emv_memset(apar->default_tdol, 0, sizeof(apar->default_tdol));
                        emv_memcpy(apar->default_tdol, tlvs[i].pvalue, tlvs[i].valuelen);
                    }
                    break;
                case TAG_9F1C_TERMINAL_IDENTIFICATION:
                    express_copy_fixed(apar->term_id, sizeof(apar->term_id), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F1E_IFD_SERIAL_NUMBER:
                    express_copy_fixed(apar->term_ifd, sizeof(apar->term_ifd), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F1A_TERMINAL_COUNTRY_CODE:
                    express_copy_fixed(apar->term_country_code, sizeof(apar->term_country_code), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_5F2A_TRANSACTION_CURRENCY_CODE:
                    express_copy_fixed(apar->trans_currency_code, sizeof(apar->trans_currency_code), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_5F36_TRANSACTION_CURRENCY_EXPONENT:
                    apar->trans_currency_exponent = tlvs[i].pvalue[0];
                    break;
                case TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY:
                    express_copy_fixed(apar->trans_ref_currency_code, sizeof(apar->trans_ref_currency_code), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT:
                    apar->trans_ref_currency_exponent = tlvs[i].pvalue[0];
                    break;
                case TAG_DF11_TAC_DEFAULT:
                    apar->term_action_code_default_exist = 1;
                    express_copy_fixed(apar->term_action_code_default, sizeof(apar->term_action_code_default), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_DF12_TAC_ONLINE:
                    apar->term_action_code_online_exist = 1;
                    express_copy_fixed(apar->term_action_code_online, sizeof(apar->term_action_code_online), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_DF13_TAC_DENIAL:
                    apar->term_action_code_denial_exist = 1;
                    express_copy_fixed(apar->term_action_code_denial, sizeof(apar->term_action_code_denial), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F1D_TERMINAL_RISK_MANGEMENT_DATA:
                    if (tlvs[i].valuelen <= (int32_t)sizeof(apar->term_risk_mana_data))
                    {
                        apar->term_risk_mana_data_len = (uint8_t)tlvs[i].valuelen;
                        emv_memset(apar->term_risk_mana_data, 0, sizeof(apar->term_risk_mana_data));
                        emv_memcpy(apar->term_risk_mana_data, tlvs[i].pvalue, tlvs[i].valuelen);
                    }
                    break;
                case TAG_9F01_ACQUIRER_IDENTIFIER:
                    express_copy_fixed(apar->acquirer_id, sizeof(apar->acquirer_id), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F15_MERCHANT_CATEGORY_CODE:
                    express_copy_fixed(apar->merchant_category_code, sizeof(apar->merchant_category_code), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F16_MERCHANT_IDENTIFIER:
                    express_copy_fixed(apar->merchant_id, sizeof(apar->merchant_id), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F4E_MERCHANT_NAME:
                    /* P0-2: 商户名缓冲区 20 字节（美运原版无上限校验） */
                    if (tlvs[i].valuelen <= (int32_t)sizeof(apar->merchant_name))
                    {
                        apar->merchant_name_len = (uint8_t)tlvs[i].valuelen;
                        emv_memset(apar->merchant_name, 0, sizeof(apar->merchant_name));
                        emv_memcpy(apar->merchant_name, tlvs[i].pvalue, tlvs[i].valuelen);
                    }
                    break;
                case TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR:
                    apar->online_pin_support_indicator = tlvs[i].pvalue[0];
                    break;
                case TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT:
                    if (tlvs[i].valuelen > 0)
                    {
                        apar->cl_trans_limit_exist = 1;
                        if (tlvs[i].valuelen >= 6)
                        {
                            emv_memcpy(apar->cl_trans_limit, tlvs[i].pvalue, 6);
                        }
                        else
                        {
                            emv_memset(apar->cl_trans_limit, 0, 6);
                            emv_memcpy(apar->cl_trans_limit + (6 - tlvs[i].valuelen), tlvs[i].pvalue, tlvs[i].valuelen);
                        }
                    }
                    break;
                case TAG_DF19_CONTACTLESS_FLOOR_LIMIT:
                    if (tlvs[i].valuelen > 0)
                    {
                        apar->cl_floor_limit_exist = 1;
                        if (tlvs[i].valuelen >= 6)
                        {
                            emv_memcpy(apar->cl_floor_limit, tlvs[i].pvalue, 6);
                        }
                        else
                        {
                            emv_memset(apar->cl_floor_limit, 0, 6);
                            emv_memcpy(apar->cl_floor_limit + (6 - tlvs[i].valuelen), tlvs[i].pvalue, tlvs[i].valuelen);
                        }
                    }
                    break;
                case TAG_DF21_CVM_REQUIRED_LIMIT:
                    if (tlvs[i].valuelen > 0)
                    {
                        apar->cvm_limit_exist = 1;
                        if (tlvs[i].valuelen >= 6)
                        {
                            emv_memcpy(apar->cvm_limit, tlvs[i].pvalue, 6);
                        }
                        else
                        {
                            emv_memset(apar->cvm_limit, 0, 6);
                            emv_memcpy(apar->cvm_limit + (6 - tlvs[i].valuelen), tlvs[i].pvalue, tlvs[i].valuelen);
                        }
                    }
                    break;
                case TAG_9F1B_TERMINAL_FLOOR_LIMIT:
                    express_copy_fixed(apar->term_floor_limit, sizeof(apar->term_floor_limit), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_ClessReaderCapab:
                    apar->cl_capability = tlvs[i].pvalue[0];
                    break;
                case TAG_EnhancedClessReaderCapab:
                    express_copy_fixed(apar->enhanced_cl_capability, sizeof(apar->enhanced_cl_capability), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_EXTENDED_SELECT_SUPPORT:
                    apar->extend_sel_sup = tlvs[i].pvalue[0];
                    break;
                case TAG_9F35_TERMINAL_TYPE:
                    apar->term_type = tlvs[i].pvalue[0];
                    break;
                case TAG_9F33_TERMINAL_CAPABILITIES:
                    express_copy_fixed(apar->term_cap, sizeof(apar->term_cap), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES:
                    /* P1-2: 5 字节 */
                    express_copy_fixed(apar->term_add_cap, sizeof(apar->term_add_cap), tlvs[i].pvalue, tlvs[i].valuelen);
                    break;
                case TAG_STATUS_CHECK_SUPPORT_INDICATOR:
                    apar->status_check_sup = tlvs[i].pvalue[0];
                    break;
                default:
                    break;
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * 终端参数装载
 * ------------------------------------------------------------------------- */
int express_param_load_term(EXPRESS_TERM_PARAM *term)
{
    uint8_t buf[EXPRESS_PARA_SAVLEN];
    int32_t len = 0;

    if (term == NULL)
    {
        return -1;
    }

    express_init_term_defaults(term);

    /* 原始 TLV（Entrypoint src/emvporting.c:938 emvporting_get_termparam_tlv） */
    if (emvporting_get_termparam_tlv(buf, &len) == 0 && len > 0 && len <= (int32_t)sizeof(buf))
    {
        express_conv_param(term, NULL, buf, len);
        return 0;
    }

    /* 文件缺失/损坏：保持美运 init_param 的缺省值（emvporting.c:301-315） */
    return -1;
}

/* ---------------------------------------------------------------------------
 * 单 AID 参数装载：先终端参数文件、再该 AID 的 TLV 覆盖（与美运 emvporting.c:779-848 同序）
 * ------------------------------------------------------------------------- */
int express_param_load_aid(EXPRESS_AID_PARAM *aid, uint8_t termapp_index)
{
    uint8_t buf[EXPRESS_PARA_SAVLEN];
    int32_t len = 0;

    if (aid == NULL)
    {
        return -1;
    }

    express_init_aid_defaults(aid);

    if (emvporting_get_termparam_tlv(buf, &len) == 0 && len > 0 && len <= (int32_t)sizeof(buf))
    {
        express_conv_param(NULL, aid, buf, len);
    }

    emv_memset(buf, 0, sizeof(buf));
    len = 0;
    if (emvporting_get_aidparam_tlv((int32_t)termapp_index, buf, &len) == 0 && len > 0 && len <= (int32_t)sizeof(buf))
    {
        express_conv_param(NULL, aid, buf, len);
        return 0;
    }

    return -1;
}

/* ---------------------------------------------------------------------------
 * DRL 装载
 * 方案 G2：Entrypoint 没有 DRL 读取 API（只有 emv_add_drl/emv_clr_drl），当前返回"无 DRL"。
 * 等价于美运在未下发 drlN.par 时的行为（ExpressGetDynamicReaderLimit 走 AID 级限额回退）。
 * ------------------------------------------------------------------------- */
int express_param_load_drl(EXPRESS_DRL_PARAMS *drl)
{
    EXPRESS_DRL_PARAMS *dst = express_drl_params();

    if (drl == NULL)
    {
        drl = dst;
    }

    emv_memset(drl, 0, sizeof(EXPRESS_DRL_PARAMS));
    if (dst != drl)
    {
        emv_memset(dst, 0, sizeof(EXPRESS_DRL_PARAMS));
    }

    /* TODO(port): Entrypoint 增加 emv_get_drl() 后在此逐条读取 DRL（方案 G2）。
     * 当前 ifenable 全 0 → Expresspay 使用 AID 级 DF19/DF20/DF21（v4.2 已删除 DRL）。 */
    return -1;
}

int express_param_get_aid_count(void)
{
    EMV_TERMAPP apps[EXPRESS_MAX_APPS];
    int32_t n;

    emv_memset(apps, 0, sizeof(apps));
    if (g_emv_port == NULL || g_emv_port->get_termapps == NULL)
    {
        return 0;
    }

    n = g_emv_port->get_termapps(apps);
    if (n < 0 || n > (int32_t)EXPRESS_MAX_APPS)
    {
        return 0;
    }
    return (int)n;
}
