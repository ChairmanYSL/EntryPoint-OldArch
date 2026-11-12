/**
************************************************************************************************************************
* @file     express_app_sel.c
* @brief    Expresspay 最终应用选择（裁自美运 emv_application_selection.c）
*
* 移植范围（方案 §3.1）：
*   - emv_application_selection.c:1259-1327 emv_app_sel_init   → express_app_sel_init()
*   - emv_application_selection.c:446-647  select_adf 的"最终选择"分支 → express_select_final()
*   - emv_application_selection.c:723-859  load_term_app_param → express_load_term_app_param()
*   - emv_application_selection.c:664-721  again_preprocess    → express_again_preprocess()
*   - emv_application_selection.c:862-882  load_trans_amount   → express_load_trans_amount()
* 删除：candidate/PPSE/PSE/PSE 选择、AID 列表回退、候选排序与去重（Entrypoint 应用选择层负责，方案 §3.1）。
*
* 与美运的差异：
*   - 候选来源改为 Entrypoint 的 EMV_GetSelectedCandidate()（方案 G6；Entrypoint 已校验索引，修 P0-1）；
*   - 参数来源改为 express_param_load_aid()（原始 TLV 回读，方案 G1）；
*   - 9F2A 写入完整 8 字节内核标识（修 P1-5，美运只写 1 字节 termapp 索引）；
*   - 日期/时间/ATC/UN 的 porting 返回值全部检查（修 P1-7/P1-8）；
*   - 9F66 不再注入 appdata（Expresspay v4.2 无 TTQ，方案 spec 笔记 §3.6）。
************************************************************************************************************************
*/
#include "express_priv.h"

/* ---------------------------------------------------------------------------
 * 交易金额写入（美运 emv_application_selection.c:862-882）
 * 9F04/9F03 为 other amount，81/9F02 为 authorized（= amount + other）
 * ------------------------------------------------------------------------- */
static void express_load_trans_amount(u32 amount, u32 amount_other)
{
    u8 buf[24];

    emv_memset(buf, 0, sizeof(buf));
    emv_dec2hex(amount_other, buf, 4);
    appdata_save(TAG_9F04_AMOUNT_OTHER_BINARY, buf, 4, UNIQUE);

    emv_memset(buf, 0, sizeof(buf));
    emv_dec2bcd(amount_other, buf, 6);
    appdata_save(TAG_9F03_AMOUNT_OTHER_NUMERIC, buf, 6, UNIQUE);

    amount += amount_other;
    emv_memset(buf, 0, sizeof(buf));
    emv_dec2hex(amount, buf, 4);
    appdata_save(TAG_0081_AMOUNT_AUTHORISED_BINARY, buf, 4, UNIQUE);

    emv_memset(buf, 0, sizeof(buf));
    emv_dec2bcd(amount, buf, 6);
    appdata_save(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, buf, 6, UNIQUE);
}

/* ---------------------------------------------------------------------------
 * 最终 SELECT（美运 select_adf 的 aid_index<0 分支，emv_application_selection.c:618-646）
 * 只保留 FCI 中允许保存的原始标签（84/50/87/9F38/5F2D/9F11/9F12/9F4D/DF4D）。
 * ------------------------------------------------------------------------- */
static int express_select_final(const EMV_CANDIDATE *cand)
{
    EMV_TLVOBJ pobj[80];
    uint8_t tmpbuf[EMV_CFG_ICC_RECV_MAXLEN];
    int32_t outlen = 0;
    int32_t ret;
    int32_t pos1;
    int32_t pos2;
    int32_t pos3;
    int32_t pos;
    uint8_t *adf_name;

    if (cand == NULL || cand->aid_len == 0)
    {
        return express_outcome_request_another_payment();
    }

    adf_name = (uint8_t *)cand->aid;
    emv_memset(tmpbuf, 0, sizeof(tmpbuf));
    ret = emvicc_select(adf_name, cand->aid_len, FALSE, tmpbuf, &outlen);
    eTRACE("AMEX-info: final select ADF ret [%04X]", (int)ret);

    if (ret == 0x0000)
    {
        return EMV_APDU_TIMEOUT;
    }
    if (ret != 0x9000)
    {
        /* 方案 §2.5：候选由 Entrypoint 决定，最终 SELECT 失败即换卡（不做 AID-list 回退） */
        return express_outcome_request_another_payment();
    }

    emv_memset(pobj, 0, sizeof(pobj));
    if (emvtlv_decode(tmpbuf, outlen, pobj, (int32_t)(sizeof(pobj) / sizeof(pobj[0])), SINGLE_TLVOBJ) < 0)
    {
        return AS_ERR;
    }
    if ((pos1 = emvtlv_get(0, 0x6F, pobj, SEARCH_ONLY_SON)) == 0)
    {
        return AS_FCINO6F;
    }
    if ((pos2 = emvtlv_get(pos1, 0x84, pobj, SEARCH_ONLY_SON)) == 0)
    {
        return AS_FCINO84;
    }
    if ((pos3 = emvtlv_get(pos1, 0xA5, pobj, SEARCH_ONLY_SON)) == 0)
    {
        return AS_FCINOA5;
    }

    /* 9F11/9F12 必须在 A5 内且不重复（美运 select_adf:509-524 的检查保留） */
    if ((pos = emvtlv_get(0, 0x9F11, pobj, SEARCH_ALL_DESC)) != 0)
    {
        if (pobj[pos].parent != pos3 || emvtlv_find_dup(pos, pobj))
        {
            return AS_ERR;
        }
    }
    if ((pos = emvtlv_get(0, 0x9F12, pobj, SEARCH_ALL_DESC)) != 0)
    {
        if (pobj[pos].parent != pos3 || emvtlv_find_dup(pos, pobj))
        {
            return AS_POSERR9F12;
        }
    }

    /* 保存 FCI 允许的原始标签（美运 :620-643） */
    for (pos1 = pos2; pos1 <= pobj[0].childnum; pos1++)
    {
        if (pobj[pos1].childnum == -1)
        {
            switch (pobj[pos1].tagname)
            {
                case 0x84:
                case 0x50:
                case 0x87:
                case 0x9F38:
                case 0x5F2D:
                case 0x9F11:
                case 0x9F12:
                case 0x9F4D:
                case 0xDF4D:
                    (void)express_appdata_save2(pobj + pos1, OVERRIDED);
                    break;
                default:
                    break;
            }
        }
    }

    return EMV_OK;
}

/* ---------------------------------------------------------------------------
 * AID 级限额预处理（美运 emv_application_selection.c:664-721 again_preprocess）
 * 结果写入上下文标志 CANAI/CVMRLEF/TermEFLEF（供 express_set_cvm_required / TRM 使用）。
 * ------------------------------------------------------------------------- */
static int express_again_preprocess(EXPRESS_AID_PARAM *param)
{
    if (param == NULL)
    {
        return -1;
    }

    eTRACE("AMEX-info: trans amount: %d", g_express_cnxt->trans_amount);

    if (param->cl_trans_limit_exist)
    {
        if (g_express_cnxt->trans_amount >= (uint32_t)emv_bcd2dec(param->cl_trans_limit, 6))
        {
            eTRACE("AMEX-info: Amount exceed or equal to Contactless trans limit");
            g_express_cnxt->CANAI = 1;
        }
    }

    /* Section 4.3.1：Reader CVM Required Limit 未定义 → 视为最大值（spec 笔记 §4.3） */
    if (!param->cvm_limit_exist)
    {
        emv_memcpy(param->cvm_limit, "\x99\x99\x99\x99\x99\x99", 6);
    }

    if (g_express_cnxt->trans_amount >= (uint32_t)emv_bcd2dec(param->cvm_limit, 6))
    {
        appdata_save_or(TAG_ClessReaderCapab, 0, 0x08);
        appdata_save_or(TAG_EnhancedClessReaderCapab, 2, 0x40);
        g_express_cnxt->CVMRLEF = 1;
    }

    if (param->cl_floor_limit_exist)
    {
        if ((g_express_cnxt->trans_amount > (uint32_t)emv_bcd2dec(param->cl_floor_limit, 6)) ||
            (emv_memcmp(param->cl_floor_limit, "\x00\x00\x00\x00\x00\x00", 6) == 0))
        {
            g_express_cnxt->TermEFLEF = 1;
        }
    }
    else
    {
        if (g_express_cnxt->trans_amount > emv_hex2dec(param->term_floor_limit, 4))
        {
            g_express_cnxt->TermEFLEF = 1;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * 终端/AID 参数注入 appdata（美运 emv_application_selection.c:723-859 load_term_app_param）
 * 参数来源：express_param_load_aid()（Entrypoint 原始 TLV），预设 g_express_cnxt->preset 优先。
 * ------------------------------------------------------------------------- */
static int express_load_term_app_param(uint8_t termapp_index)
{
    EXPRESS_AID_PARAM param;
    uint8_t pos_entry_mode;
    int ret;

    ret = express_param_load_aid(&param, termapp_index);
    if (ret != 0)
    {
        /* 美运 :730-734 用内置默认参数继续；express_param_load_aid 失败时 param 已是缺省值 */
        eTRACE("AMEX-info: Get App Param Error, use default param");
    }

    (void)express_again_preprocess(&param);

    pos_entry_mode = EMV_POSENTRYMODE_QPBOC_CL;
    appdata_save(TAG_9F39_POS_ENTRY_MODE, &pos_entry_mode, 1, UNIQUE);

    if (g_express_cnxt->preset.exist_term_type)
    {
        appdata_save(TAG_9F35_TERMINAL_TYPE, &g_express_cnxt->preset.term_type, 1, OVERRIDED);
    }
    else
    {
        appdata_save(TAG_9F35_TERMINAL_TYPE, &param.term_type, 1, OVERRIDED);
    }

    if (g_express_cnxt->preset.exist_term_caps)
    {
        appdata_save(TAG_9F33_TERMINAL_CAPABILITIES, g_express_cnxt->preset.term_caps, 3, UNIQUE);
    }
    else
    {
        appdata_save(TAG_9F33_TERMINAL_CAPABILITIES, param.term_cap, 3, OVERRIDED);
    }

    if (g_express_cnxt->preset.exist_term_add_caps)
    {
        appdata_save(TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, g_express_cnxt->preset.term_add_caps, 5, UNIQUE);
    }
    else
    {
        appdata_save(TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, param.term_add_cap, 5, OVERRIDED);
    }

    if (g_express_cnxt->preset.exist_EnhancedClessReaderCapab)
    {
        appdata_save(TAG_EnhancedClessReaderCapab, g_express_cnxt->preset.EnhancedClessReaderCapab, 4, OVERRIDED);
    }
    else
    {
        appdata_save(TAG_EnhancedClessReaderCapab, param.enhanced_cl_capability, 4, OVERRIDED);
    }

    if (g_express_cnxt->preset.exist_ClessReaderCapab)
    {
        appdata_save(TAG_ClessReaderCapab, &g_express_cnxt->preset.ClessReaderCapab, 1, OVERRIDED);
    }
    else
    {
        appdata_save(TAG_ClessReaderCapab, &param.cl_capability, 1, OVERRIDED);
    }

    appdata_save(TAG_9F1B_TERMINAL_FLOOR_LIMIT, param.term_floor_limit, 4, UNIQUE);
    appdata_save(TAG_STATUS_CHECK_SUPPORT_INDICATOR, &param.status_check_sup, 1, UNIQUE);

    if (param.cl_trans_limit_exist)
    {
        appdata_save(TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT, param.cl_trans_limit, 6, UNIQUE);
    }
    if (param.cl_floor_limit_exist)
    {
        appdata_save(TAG_DF19_CONTACTLESS_FLOOR_LIMIT, param.cl_floor_limit, 6, UNIQUE);
    }
    if (param.cvm_limit_exist)
    {
        appdata_save(TAG_DF21_CVM_REQUIRED_LIMIT, param.cvm_limit, 6, UNIQUE);
    }
    else
    {
        appdata_save(TAG_DF21_CVM_REQUIRED_LIMIT, (uint8_t*)"\x99\x99\x99\x99\x99\x99", 6, UNIQUE);
    }

    appdata_save(TAG_9F7A_EC_TERMINAL_SUPPORT_INDICATOR, &g_express_cnxt->term.ec_support, 1, UNIQUE);
    if (g_express_cnxt->term.ec_trans_limit_exist)
    {
        appdata_save(TAG_9F7B_EC_TERMINAL_TRANSACTION_LIMIT, g_express_cnxt->term.ec_trans_limit, 6, UNIQUE);
    }
    appdata_save(TAG_DF69_SM_SUPPORT_INDICATOR, &g_express_cnxt->term.sm_support, 1, UNIQUE);

    appdata_save(TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, param.app_ver, 2, UNIQUE);
    appdata_save(TAG_DF17_TARGET_PERCENTAGE, &param.target_percent, 1, UNIQUE);
    appdata_save(TAG_DF16_MAX_TARGET_PERCENTAGE, &param.max_target_percent, 1, UNIQUE);
    appdata_save(TAG_DF15_THRESHOLD_VALUE, param.threshold_value, 4, UNIQUE);

    if (param.default_ddol_len > 0 && param.default_ddol_len <= sizeof(param.default_ddol))
    {
        appdata_save(TAG_DF14_DEFAULT_DDOL, param.default_ddol, param.default_ddol_len, UNIQUE);
    }
    if (param.default_tdol_len > 0 && param.default_tdol_len <= sizeof(param.default_tdol))
    {
        appdata_save(TAG_DEFAULT_TDOL, param.default_tdol, param.default_tdol_len, UNIQUE);
    }

    appdata_save(TAG_9F1C_TERMINAL_IDENTIFICATION, param.term_id, 8, UNIQUE);
    appdata_save(TAG_9F1E_IFD_SERIAL_NUMBER, param.term_ifd, 8, UNIQUE);
    appdata_save(TAG_9F1A_TERMINAL_COUNTRY_CODE, param.term_country_code, 2, UNIQUE);

    appdata_save(TAG_5F2A_TRANSACTION_CURRENCY_CODE, param.trans_currency_code, 2, OVERRIDED);
    appdata_save(TAG_5F36_TRANSACTION_CURRENCY_EXPONENT, &param.trans_currency_exponent, 1, UNIQUE);
    appdata_save(TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY, param.trans_ref_currency_code, 2, UNIQUE);
    appdata_save(TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT, &param.trans_ref_currency_exponent, 1, UNIQUE);

    if (param.term_action_code_default_exist)
    {
        appdata_save(TAG_DF11_TAC_DEFAULT, param.term_action_code_default, 5, UNIQUE);
    }
    if (param.term_action_code_online_exist)
    {
        appdata_save(TAG_DF12_TAC_ONLINE, param.term_action_code_online, 5, UNIQUE);
    }
    if (param.term_action_code_denial_exist)
    {
        appdata_save(TAG_DF13_TAC_DENIAL, param.term_action_code_denial, 5, UNIQUE);
    }
    if (param.term_risk_mana_data_len > 0 && param.term_risk_mana_data_len <= sizeof(param.term_risk_mana_data))
    {
        appdata_save(TAG_9F1D_TERMINAL_RISK_MANGEMENT_DATA, param.term_risk_mana_data, param.term_risk_mana_data_len, UNIQUE);
    }

    appdata_save(TAG_9F01_ACQUIRER_IDENTIFIER, param.acquirer_id, 6, UNIQUE);
    appdata_save(TAG_9F15_MERCHANT_CATEGORY_CODE, param.merchant_category_code, 2, UNIQUE);
    appdata_save(TAG_9F16_MERCHANT_IDENTIFIER, param.merchant_id, 15, UNIQUE);
    appdata_save(TAG_9F4E_MERCHANT_NAME, param.merchant_name, param.merchant_name_len, UNIQUE);

    appdata_save(TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR, &param.online_pin_support_indicator, 1, UNIQUE);
    appdata_save(TAG_EXTENDED_SELECT_SUPPORT, &param.extend_sel_sup, 1, UNIQUE);

    return 0;
}

/* ---------------------------------------------------------------------------
 * 应用初始化入口（美运 emv_application_selection.c:1259-1327 emv_app_sel_init）
 * 状态：Entrypoint 在 EMV_STA_CANDIDATES_BUILT 时通过注册表调用本函数 → 返回 EMV_STA_APP_SELECTED。
 * ------------------------------------------------------------------------- */
int express_app_sel_init(void)
{
    const EMV_CANDIDATE *cand;
    uint8_t buf[EMV_CFG_ICC_RECV_MAXLEN];
    uint8_t tim[8];
    uint8_t un[4];
    uint8_t kid[EXPRESS_KERNEL_ID_LEN] = EXPRESS_KERNEL_ID;
    const uint8_t *kernel_id = kid;
    uint8_t kernel_id_len = EXPRESS_KERNEL_ID_LEN;
    int ret;

    cand = EMV_GetSelectedCandidate();
    if (cand == NULL || cand->aid_len == 0)
    {
        eTRACE("AMEX-error: no selected candidate");
        return express_outcome_request_another_payment();
    }

    /* 美运 :1267：交易开始清 appdata（Entrypoint 在 IDLE 阶段写入的 9C/金额随后由本函数重建） */
    appdata_init();

    /* ===== P0/P1 修复（移植新增）：补建 TSI/TVR =====
     * 美运 emvbuf.c:42-43 在 appdata_init 内预建 TSI/TVR；Entrypoint 的 appdata_init 不再预建。
     * 不补建则 TRM/TAA 的 appdata_find(TAG_0095_TVR) 可能返回 NULL → 空指针解引用。
     * TSI 用 3 字节（EMV Book 3；美运误用 2 字节，一并修正）。 */
    {
        uint8_t zero_tvr[5] = {0};
        uint8_t zero_tsi[3] = {0};

        (void)appdata_save(TAG_0095_TVR, zero_tvr, (int32_t)sizeof(zero_tvr), UNIQUE);
        (void)appdata_save(TAG_009B_TSI, zero_tsi, (int32_t)sizeof(zero_tsi), UNIQUE);
        emv_memset(g_express_cnxt->tvr, 0, sizeof(g_express_cnxt->tvr));
        emv_memset(g_express_cnxt->tsi, 0, sizeof(g_express_cnxt->tsi));
    }

    ret = express_select_final(cand);
    if (ret != EMV_OK)
    {
        if (ret < 0)
        {
            return rERR(ret);
        }
        else if (ret > 0 && ret < 100)
        {
            return rNXT(ret);
        }
        else
        {
            return rEND(ret);
        }
    }

    g_express_cnxt->selected_aid_len = cand->aid_len;
    emv_memcpy(g_express_cnxt->selected_aid, cand->aid, cand->aid_len);
    g_express_cnxt->selected_termapp_index = cand->rsv[0];

    eTRACE("AMEX-info: Start load term param (termapp index=%d)", cand->rsv[0]);
    (void)express_load_term_app_param(cand->rsv[0]);

    /* AID 相关数据（美运 :1293-1299） */
    appdata_save(TAG_9F06_AID_TERMINAL, (uint8_t *)cand->aid, cand->aid_len, UNIQUE);
    appdata_save(TAG_004F_AID, (uint8_t *)cand->aid, cand->aid_len, UNIQUE);
    if (cand->lable_len > 0)
    {
        appdata_save(TAG_0050_APPLICATION_LABEL, (uint8_t *)cand->lable, cand->lable_len, UNIQUE);
    }
    if (cand->preferred_name_len > 0)
    {
        appdata_save(TAG_9F12_APPLICATION_PREFERRED_NAME, (uint8_t *)cand->preferred_name, cand->preferred_name_len, UNIQUE);
    }
    appdata_save(TAG_0087_API, (uint8_t *)&cand->priority, 1, UNIQUE);

    /* P1-5：9F2A 必须是完整 8 字节内核标识，不能用 termapp 索引占位 */
    if (cand->kernel_id_len > 0 && cand->kernel_id_len <= EXPRESS_KERNEL_ID_LEN)
    {
        kernel_id = cand->kernel_id;
        kernel_id_len = cand->kernel_id_len;
    }
    appdata_save(TAG_9F2A_KERNEL_IDETIFIER, (uint8_t *)kernel_id, kernel_id_len, UNIQUE);

    /* 保存交易类型和交易金额（美运 :1302-1303） */
    appdata_save(TAG_009C_TRANSACTION_TYPE, &g_express_cnxt->trans_type, 1, UNIQUE);
    express_load_trans_amount(g_express_cnxt->trans_amount, g_express_cnxt->trans_amount_other);

    /* Transaction Date | Transaction Time（美运 :1307-1315，P1-8：先判返回值再写字段） */
    emv_memset(tim, 0, sizeof(tim));
    if (g_emv_port != NULL && g_emv_port->get_trans_date_time != NULL)
    {
        if (g_emv_port->get_trans_date_time(tim, tim + 4) < 0)
        {
            eTRACE("AMEX-error: get_trans_date_time failed");
            return rEND(EMV_ENDAPPLICATION);
        }
    }
    appdata_save(TAG_009A_TRANSACTION_DATE, tim + 1, 3, UNIQUE);
    appdata_save(TAG_9F21_TRANSACTION_TIME, tim + 4, 3, UNIQUE);

    /* Transaction Sequence Counter（9F41，BCD 4 字节） */
    emv_memset(buf, 0, sizeof(buf));
    if (g_emv_port != NULL && g_emv_port->get_trans_sequence_counter != NULL)
    {
        if (g_emv_port->get_trans_sequence_counter(buf) < 0)
        {
            eTRACE("AMEX-warning: get_trans_sequence_counter failed, use 00000000");
        }
    }
    appdata_save(TAG_9F41_TRANSACTION_SEQUENCE_COUNTER, buf, 4, UNIQUE);

    /* Unpredictable Number（9F37，P1-7：检查 get_random 返回值） */
    emv_memset(un, 0, sizeof(un));
    if (g_emv_port != NULL && g_emv_port->get_random != NULL)
    {
        if (g_emv_port->get_random(un, 4) < 0)
        {
            eTRACE("AMEX-warning: get_random failed, UN stays 00000000");
        }
    }
    appdata_save(TAG_9F37_UNPREDICATABLE_NUMBER, un, 4, UNIQUE);

    return rNXT(EMV_STA_APP_SELECTED);
}
