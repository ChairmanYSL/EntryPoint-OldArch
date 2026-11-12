/**
************************************************************************************************************************
* @file     express_kernel.c
* @brief    Expresspay 内核入口：init / preprocess / process / exit / post_result / 注册
*
* 移植来源（美运 V4.2.3 submodule/emvco/src/emv.c）：
*   - EMV_Init(759-829)        → express_init()（不接收 EMV_PORTING：Entrypoint 已注入 g_emv_port）
*   - EMV_initialize(516-560)  → express_reset_transaction()
*   - EMV_PreProcess(896-978)  → express_preprocess()（方案 D3：AID 限额过滤交给 Entrypoint 预处理）
*   - EMV_Process(1135-1257)   → express_process()（状态机；§2.3 状态码直接透传）
*   - EMV_Result*(1259-1410)   → express_post_result()（由 Entrypoint/App 调用）
*   - EMV_GetData(1412)/SetData(1471) → express_get_data/express_set_data
*   - EMV_ProcessExit(1767)/EMV_Exit(1779) → express_exit()
*
* 与 Entrypoint 的对接（方案 §6.2 G6/G8/G9）：
*   - 注册：amex_kernel_register() → EMV_RegisterKernel(&ops)，kernel_id = EMV_KERNEL_ID_AMEX(0x04)；
*   - 候选：express_app_sel_init() 通过 EMV_GetSelectedCandidate() 取 Entrypoint 已选候选；
*   - 结果：Entrypoint 的 EMV_Result* 目前还没有回调内核（emv_kernel.h 无 post_result 字段，G9 未落地），
*     内核先提供 express_post_result() 供 App 直接调用；G9 落地后由 Entrypoint 转发，接口不变。
************************************************************************************************************************
*/
#include "express_priv.h"

/* 内核上下文（静态分配：架构约束"禁止堆分配"；Entrypoint 的 alloc_buffer 只服务 Entrypoint 自身） */
static EXPRESS_CONTEXT g_express_context;

/* ---------------------------------------------------------------------------
 * 每笔交易复位（美运 emv.c:516-560 EMV_initialize）
 * ------------------------------------------------------------------------- */
static void express_reset_transaction(void)
{
    EXPRESS_CONTEXT *ctx = g_express_cnxt;

    ctx->status = EMV_STA_IDLE;
    ctx->force_online = 0;
    ctx->card_types_allowed = EMV_CARD_UNKNOWN;
    ctx->card_type = EMV_CARD_UNKNOWN;

    emv_memcpy(ctx->ttq, ctx->term.ttq, 4);

    ctx->trans_type = _EMV_TRANSTYPE_UNKNOWN;
    ctx->trans_amount = 0;
    ctx->trans_amount_other = 0;

    ctx->CANAI = 0;
    ctx->CVMRLEF = 0;
    ctx->TermCFLEF = 0;
    ctx->TermEFLEF = 0;
    ctx->CVMParameter = EXPRESS_OUTCOME_CVM_NA;

    ctx->msdoremv = EXPRESS_MODE_MSD;
    ctx->gExpressmobileagain = 0;

    ctx->da_readrec_failed = FALSE;
    ctx->da_sda_performed = FALSE;
    ctx->da_dda_performed = FALSE;
    ctx->da_cda_performed = FALSE;
    ctx->req_pk_algorithm = 0;
    ctx->bis_card_sup_onlinepin = FALSE;
    ctx->bis_unable_online = FALSE;

    ctx->req_online_pin_result = 0;
    ctx->req_offline_pin_result = 0;
    ctx->req_confirm_bypass_pin_result = 0;
    ctx->req_confirm_force_online_result = 0;
    ctx->req_issuer_referral_result = 0;
    ctx->online_result = 0;

    ctx->selected_aid_len = 0;
    ctx->selected_termapp_index = 0;

    emv_memset(ctx->aip, 0, sizeof(ctx->aip));
    emv_memset(ctx->tvr, 0, sizeof(ctx->tvr));
    emv_memset(ctx->tsi, 0, sizeof(ctx->tsi));
    emv_memset(ctx->offline_pin, 0, sizeof(ctx->offline_pin));
}

/* ---------------------------------------------------------------------------
 * 内核初始化（美运 emv.c:759-829）
 * 返回：EMV_OK / EMV_GETTERMPARAMERR（终端参数缺失，已回填缺省值）
 * ------------------------------------------------------------------------- */
int32_t express_init(void)
{
    EXPRESS_CONTEXT *ctx = &g_express_context;
    int32_t init_ret = EMV_OK;

    g_express_cnxt = ctx;
    emv_memset(ctx, 0, sizeof(EXPRESS_CONTEXT));

    /* 终端预设：由 App 通过 express_set_term_* 覆盖（本版未导出，保持"未预设"） */
    emv_memset(&ctx->preset, 0, sizeof(ctx->preset));

    /* 终端参数（原始 TLV → 内核最小集；失败时 express_param_load_term 已回填缺省值） */
    if (express_param_load_term(&ctx->term) != 0)
    {
        eTRACE("AMEX-warning: get term param failed, use built-in default");
        init_ret = EMV_GETTERMPARAMERR;
    }

    /* 内核索引（美运 emv.c:821 EMVPORT_GetKernelIndex） */
    ctx->kernel_index = (g_emv_port != NULL && g_emv_port->get_kernel_index != NULL)
                            ? g_emv_port->get_kernel_index()
                            : 0;

    /* DRL（方案 G2：Entrypoint 暂无读取 API → 全 disable，限额回退 AID 级） */
    (void)express_param_load_drl(NULL);

    express_outcome_reset();
    express_reset_transaction();

    ctx->in_use = true;

    return init_ret;
}

/* ---------------------------------------------------------------------------
 * 预处理（美运 emv.c:896-978）
 *
 * 方案 D3：美运"遍历全部 AID 的 DF20 判断是否有 AID 限额够用，全不够返回 EMV_AMOUNTOVER"
 * 由 Entrypoint 预处理（EMV_PreProcess → preprocess_result[i].contactless_app_not_allowed）取代，
 * 内核不再重复实现，避免两套金额超限判定不一致。本函数只做 TTQ/卡类型准备。
 * ------------------------------------------------------------------------- */
int32_t express_preprocess(uint8_t b_online)
{
    if (g_express_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
    }
    if (g_express_cnxt->status != EMV_STA_IDLE && g_express_cnxt->status != EMV_REQ_SEEPHONE)
    {
        return rERR(EMV_NONIDLE);
    }

    /* TTQ 运行期副本（Expresspay v4.2 不再把 9F66 发给卡；此处仅供内核决策位使用） */
    emv_memcpy(g_express_cnxt->ttq, g_express_cnxt->term.ttq, 4);

    g_express_cnxt->force_online = b_online;

    /* D1: copy the transaction data from the Entry Point before the kernel resets
     * appdata in express_app_sel_init(); amounts are in minor units (9F02/81). */
    {
        uint8_t trans_type = 0;
        uint8_t* p;
        int32_t  len = 0;

        if (EMV_GetTransType(&trans_type) == EMV_OK)
        {
            g_express_cnxt->trans_type = trans_type;
        }
        p = EMV_GetData(TAG_0081_AMOUNT_AUTHORISED_BINARY, &len);
        if (p != NULL && len == 4)
        {
            g_express_cnxt->trans_amount = emv_hex2dec(p, 4);
        }
        p = EMV_GetData(TAG_9F04_AMOUNT_OTHER_BINARY, &len);
        if (p != NULL && len == 4)
        {
            g_express_cnxt->trans_amount_other = emv_hex2dec(p, 4);
        }
    }
    g_express_cnxt->card_types_allowed = EMV_CARD_UNKNOWN;

    if (ttq_get(BN_TTQ_SUP_PBOC))
    {
        g_express_cnxt->card_types_allowed |= EMV_CARD_CONTACT;
    }
    if (ttq_get(BN_TTQ_SUP_QPBOC))
    {
        g_express_cnxt->card_types_allowed |= EMV_CARD_CONTACTLESS;
        g_express_cnxt->card_type = EMV_CARD_CONTACTLESS;
    }
    else
    {
        /* 非接 QPBOC 能力位关闭：AMEX 内核无法处理该终端（美运 :967-970） */
        return rERR(EMV_QPBOCUNSUPPORTED);
    }

    return EMV_OK;
}

/* ---------------------------------------------------------------------------
 * 主状态机（美运 emv.c:1135-1257）
 * Entrypoint 每次 EMV_Process 调用一次本函数，返回值即新状态（同一数值域，方案 §2.3）。
 * ------------------------------------------------------------------------- */
int32_t express_process(void)
{
    EXPRESS_CONTEXT *ctx;
    int32_t st;

    if (g_express_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
    }
    ctx = g_express_cnxt;
    st = ctx->status;

    if (st < EMV_STA_IDLE || st >= EMV_COMPLETED)
    {
        /* "see phone again" 重入路径（美运 :1145-1148，方案 §2.2 契约） */
        if (st == EMV_REQ_SEEPHONE && ctx->gExpressmobileagain == 1)
        {
            ctx->status = EMV_STA_IDLE;
            st = EMV_STA_IDLE;
        }
        else
        {
            return rERR(EMV_PROCESSOVER);
        }
    }

    /* 卡类型必须在进入状态机前确定（美运 :1162-1165；Entrypoint 组合选择已选中 AMEX 内核） */
    if (st == EMV_STA_IDLE || st == EMV_STA_CANDIDATES_BUILT)
    {
        if (ctx->card_type == EMV_CARD_UNKNOWN)
        {
            ctx->card_type = EMV_CARD_CONTACTLESS;
        }
    }

    switch (st)
    {
        /* Entrypoint 已完成 PPSE/候选/组合选择；内核在此做最终 SELECT + 参数装载 */
        case EMV_STA_IDLE:
        case EMV_STA_CANDIDATES_BUILT:
        case EMV_REQ_SEL_CANDIDATES:
        case EMV_REQ_SEL_CANDIDATES_AGAIN:
        case EMV_REQ_SEL_ACCOUNTTYPE:
        case EMV_STA_SELECT_NEXT:
            eTRACE("AMEX-info: App select (final SELECT + load param)");
            ctx->status = express_app_sel_init();
            break;

        case EMV_STA_APP_SELECTED:
            eTRACE("AMEX-info: App Init (GPO)");
            ctx->status = express_initiate_application();
            break;

        case EMV_STA_APP_INITIALIZED:
            eTRACE("AMEX-info: Read app data");
            ctx->status = express_read_application_data();
            break;

        case EMV_STA_READ_APP_DATA_COMPLETED:
            /* 卡片信息确认占位（美运 :1191-1193） */
            ctx->status = EMV_REQ_CARDINFO_CONFIRM;
            break;

        case EMV_REQ_CARDINFO_CONFIRM:
            ctx->status = express_offline_data_auth();
            break;

        case EMV_STA_DATA_AUTH_COMPLETED:
            ctx->status = express_processing_restrictions();
            break;

        case EMV_REQ_ONLINE_PIN:
            ctx->status = express_cardholder_verify();
            break;

        case EMV_STA_PROCESS_RESTRICT_COMPLETED:
        case EMV_REQ_CHECK_ID:
        case EMV_REQ_OFFLINE_PIN:
        case EMV_REQ_OFFLINE_PIN_LAST:
        case EMV_REQ_CONFIRM_BYPASS_PIN:
            ctx->status = express_cardholder_verify();
            break;

        case EMV_STA_CARDHOLDER_VERIFY_COMPLETED:
        case EMV_REQ_CONFIRM_FORCE_ONLINE:
            ctx->status = express_terminal_risk_manage();
            break;

        case EMV_STA_RISK_MANAGEMENT_COMPLETED:
            ctx->status = express_action_analyse();
            break;

        case EMV_STA_ACTION_ANALYSE_COMPLETED:
            ctx->status = express_pre_complete();
            break;

        case EMV_REQ_GO_ONLINE:
        case EMV_REQ_ISSUER_REFERRAL:
            ctx->status = express_complete();
            break;

        default:
            return rERR(EMV_UNKNOWNSTATUS);
    }

    eTRACE("AMEX-info: status[%d]", (int)ctx->status);
    return ctx->status;
}

/* ---------------------------------------------------------------------------
 * 交易结束清理（美运 emv.c:1767-1784）
 * 不释放上下文（静态分配），只把状态置为已结束；参数缓存保留到下一次 express_init。
 * ------------------------------------------------------------------------- */
void express_exit(void)
{
    if (g_express_cnxt == NULL)
    {
        return;
    }
    g_express_cnxt->in_use = false;
    g_express_cnxt->status = EMV_COMPLETED;
    express_outcome_reset();
}

/* ---------------------------------------------------------------------------
 * 结果回传（美运 emv.c:1259-1410 EMV_Result*）
 * kind = EXPRESS_RESULT_*（见 express_api.h）
 * ------------------------------------------------------------------------- */
void express_post_result(int32_t kind, int32_t result, const uint8_t *data, int32_t len)
{
    EXPRESS_CONTEXT *ctx = g_express_cnxt;

    if (ctx == NULL)
    {
        return;
    }

    switch (kind)
    {
        case EXPRESS_RESULT_SEL_CANDIDATES:
            /* Entrypoint 应用选择层持有候选结果；内核只读 EMV_GetSelectedCandidate() */
            break;

        case EXPRESS_RESULT_SEL_ACCOUNT_TYPE:
            ctx->req_sel_accounttype_result = result;
            break;

        case EXPRESS_RESULT_CHECK_ID:
            ctx->req_check_id_result = result;
            break;

        case EXPRESS_RESULT_ONLINE_PIN:
            ctx->req_online_pin_result = result;
            if (result == EMV_OPER_OK && data != NULL && len > 0 && len <= 256)
            {
                appdata_save(TAG_ONLINE_PIN, (uint8_t *)data, len, UNIQUE);
            }
            break;

        case EXPRESS_RESULT_OFFLINE_PIN:
            ctx->req_offline_pin_result = result;
            if (result == EMV_OPER_OK)
            {
                ctx->offline_pin[0] = '\0';
                if (data != NULL && len >= 4 && len <= 12)
                {
                    emv_memcpy(ctx->offline_pin, data, len);
                    ctx->offline_pin[len] = '\0';
                }
                else
                {
                    ctx->req_offline_pin_result = EMV_OPER_UNSUPPORTED;
                }
            }
            break;

        case EXPRESS_RESULT_BYPASS_PIN:
            ctx->req_confirm_bypass_pin_result = result;
            break;

        case EXPRESS_RESULT_FORCE_ONLINE:
            ctx->req_confirm_force_online_result = result;
            break;

        case EXPRESS_RESULT_ONLINE:
        {
            /* 美运 emv.c:1365-1398：授权响应 55 域，71/72 模板改存 DF8100/DF8200 */
            EMV_TLVOBJ tlvs[64];
            int32_t i;
            const uint32_t tag1 = TAG_ISSUER_SCRIPT_71_BASE;
            const uint32_t tag2 = TAG_ISSUER_SCRIPT_72_BASE;

            ctx->online_result = result;

            if ((result == EMV_ONLINE_SUCC_ACCEPT || result == EMV_ONLINE_SUCC_ISSREF ||
                 result == EMV_ONLINE_SUCC_DENIAL) &&
                data != NULL && len > 0)
            {
                emv_memset(tlvs, 0, sizeof(tlvs));
                if (emvtlv_decode((uint8_t *)data, len, tlvs,
                                  (int32_t)(sizeof(tlvs) / sizeof(tlvs[0])), DECODE_LEVEL1) == 0)
                {
                    for (i = 1; i <= tlvs[0].childnum; i++)
                    {
                        if (tlvs[i].tagname == TAG_0071_ISSUER_SCRIPT_TEMPLATE_1)
                        {
                            tlvs[i].tagname = tag1;
                        }
                        else if (tlvs[i].tagname == TAG_0072_ISSUER_SCRIPT_TEMPLATE_2)
                        {
                            tlvs[i].tagname = tag2;
                        }
                        (void)express_appdata_save2(tlvs + i, OVERRIDED);
                    }
                }
            }
            break;
        }

        case EXPRESS_RESULT_ISSUER_REFERRAL:
            ctx->req_issuer_referral_result = result;
            break;

        default:
            break;
    }
}

/* ---------------------------------------------------------------------------
 * 数据访问（美运 emv.c:1412-1492 EMV_GetData/EMV_SetData 的内核子集）
 * ------------------------------------------------------------------------- */
int32_t express_get_data(uint32_t tag, uint8_t *out, int32_t *outlen)
{
    uint8_t *p;
    int32_t len = 0;

    if (outlen == NULL)
    {
        return -1;
    }

    if (tag == TAG_KERNEL_VERSION)
    {
        len = (int32_t)emv_strlen(EXPRESS_KERNEL_VERSION);
        if (out != NULL && *outlen >= len)
        {
            emv_memcpy(out, EXPRESS_KERNEL_VERSION, len);
        }
        *outlen = len;
        return 0;
    }

    if (g_express_cnxt == NULL || g_express_cnxt->status == EMV_STA_IDLE)
    {
        *outlen = 0;
        return -1;
    }

    p = appdata_find(tag, &len);
    if (p == NULL)
    {
        *outlen = 0;
        return -1;
    }

    if (out != NULL && *outlen >= len)
    {
        emv_memcpy(out, p, len);
    }
    *outlen = len;
    return 0;
}

int32_t express_set_data(uint32_t tag, const uint8_t *in, int32_t inlen)
{
    if (g_express_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
    }
    if (in == NULL || inlen < 0)
    {
        return -1;
    }
    return appdata_save(tag, (uint8_t *)in, inlen, OVERRIDED);
}

int32_t express_get_version(uint8_t *ver)
{
    const char *v = EXPRESS_KERNEL_VERSION;
    int32_t len = (int32_t)emv_strlen(v);

    if (ver == NULL)
    {
        return len;
    }
    emv_memcpy(ver, v, len + 1);
    return len;
}

/* 候选列表由 Entrypoint 持有（方案 §6.2 G11：内核不缓存候选） */
int32_t express_get_candidate_list(void *list)
{
    (void)list;
    return -1;
}

int32_t express_get_outcome(EXPRESS_OUTCOME *out)
{
    if (out == NULL)
    {
        return -1;
    }
    emv_memcpy(out, express_outcome_state(), sizeof(EXPRESS_OUTCOME));
    return 0;
}

/* ---------------------------------------------------------------------------
 * Entrypoint 内核注册表（方案 §6.3 / G8）
 * ------------------------------------------------------------------------- */
static int32_t express_kernel_preprocess(bool b_online)
{
    return express_preprocess(b_online ? 1 : 0);
}

/* G5: expose the Expresspay outcome through the generic Entry Point outcome hook. */
static int32_t express_kernel_get_outcome(EMV_KERNEL_OUTCOME *out)
{
    EXPRESS_OUTCOME eo;

    if (out == NULL)
    {
        return -1;
    }
    if (express_get_outcome(&eo) != 0)
    {
        return -1;
    }

    out->status               = eo.Status;
    out->start                = eo.Start;
    out->online_resp_data     = eo.OnlineRespData;
    out->cvm                  = eo.CVM;
    out->present_flag         = eo.PresentFlag;
    out->interface_preference = eo.InterfacePrefer;
    out->field_off_req        = eo.FieldoffReq;
    out->removal_timeout[0]   = eo.RemovalTimeout[0];
    out->removal_timeout[1]   = eo.RemovalTimeout[1];
    return 0;
}
static const EMV_KERNEL_OPS g_amex_kernel_entrypoint_ops =
{
    EMV_KERNEL_ID_AMEX,                    /* kernel_id = 0x04 */
    "AMEX Expresspay V4.2.3",
    express_init,
    express_kernel_preprocess,
    express_process
};

int32_t amex_kernel_register(void)
{
    return EMV_RegisterKernel(&g_amex_kernel_entrypoint_ops);
}

/* express_api.h 的兼容描述符（字段顺序与方案 §6.3 建议接口一致） */
static const EXPRESS_KERNEL_OPS g_amex_kernel_ops =
{
    EXPRESS_KERNEL_ID,                     /* uint8_t kernel_id[8] */
    EXPRESS_KERNEL_ID_LEN,
    EXPRESS_CONTACTLESS_TYPE,
    {0, 0, 0, 0, 0},
    express_init,
    express_preprocess,
    express_process,
    express_exit,
    express_post_result
};

const EXPRESS_KERNEL_OPS *amex_kernel_ops(void)
{
    return &g_amex_kernel_ops;
}
