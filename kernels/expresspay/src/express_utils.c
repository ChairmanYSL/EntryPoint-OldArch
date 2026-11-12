/**
************************************************************************************************************************
* @file     express_utils.c
* @brief    Expresspay 内核公共 helper（TVR/TSI/AIP 位访问、appdata 适配、PDOL 打包、Outcome、DRL、CRC32）
*
* 移植来源：美运 V4.2.3
*   - submodule/emvco/src/emvoutcome.c          → express_outcome_*（OUTCOME_FLAG=0 等价返回）
*   - submodule/emvco/src/emvtlv.c:498          → express_pdol_packet（等价 Entrypoint emvdol_packet）
*   - emvi.h:363-366 的 tvr_set/tsi_set 宏      → 写穿透 appdata（修 P1-6 悬垂指针）
*
* 约束：不定义任何 emv_/EMV_ 前缀全局符号；只用 Entrypoint 已导出服务与 SDK API_*。
************************************************************************************************************************
*/
#include "express_priv.h"

/* ---------------------------------------------------------------------------
 * 上下文
 * ------------------------------------------------------------------------- */
EXPRESS_CONTEXT *g_express_cnxt = NULL;

/* Expresspay Outcome（替代美运 g_express_outcome/g_emv_express_uireq；由 Entrypoint 合并，方案 G5） */
static EXPRESS_OUTCOME g_express_outcome;

EXPRESS_OUTCOME *express_outcome_state(void)
{
    return &g_express_outcome;
}

void express_outcome_reset(void)
{
    emv_memset(&g_express_outcome, 0, sizeof(g_express_outcome));
    g_express_outcome.Status = EXPRESS_OUTCOME_STATUS_NA;
    g_express_outcome.Start = EXPRESS_OUTCOME_START_NA;
    g_express_outcome.OnlineRespData = EXPRESS_OUTCOME_ONLINE_RESP_DATA_NA;
    g_express_outcome.CVM = EXPRESS_OUTCOME_CVM_NA;
    g_express_outcome.InterfacePrefer = EXPRESS_OUTCOME_AIP_NA;
    g_express_outcome.FieldoffReq = EXPRESS_OUTCOME_FIELDOFF_NA;
}

static void express_outcome_set(uint8_t status, uint8_t start, uint8_t online_data, uint8_t cvm,
                                uint8_t ui_on_outcome, uint8_t ui_on_restart, uint8_t data_record_pre,
                                uint8_t disc_data_pre, uint8_t receipt, uint8_t aip,
                                uint8_t field_off_time, const uint8_t *removal_timeout)
{
    uint8_t flag = 0x00;

    g_express_outcome.Status = status;
    g_express_outcome.Start = start;
    g_express_outcome.OnlineRespData = online_data;
    g_express_outcome.CVM = cvm;

    if (ui_on_outcome)   { flag |= 0x80; }
    if (ui_on_restart)   { flag |= 0x40; }
    if (data_record_pre) { flag |= 0x20; }
    if (disc_data_pre)   { flag |= 0x10; }
    if (receipt)         { flag |= 0x08; }

    g_express_outcome.PresentFlag = flag;
    g_express_outcome.InterfacePrefer = aip;
    g_express_outcome.FieldoffReq = field_off_time;

    if (removal_timeout != NULL)
    {
        emv_memcpy(&g_express_outcome.RemovalTimeout, removal_timeout, sizeof(g_express_outcome.RemovalTimeout));
    }
}

/* 美运 emvoutcome.c:118-263（OUTCOME_FLAG=0 → 只返回状态码，同时记录 Expresspay Outcome 供 Entrypoint 合并） */
int express_outcome_try_another_interface(void)
{
    express_outcome_set(EXPRESS_OUTCOME_STATUS_TRY_ANOTHER_INTERFACE, EXPRESS_OUTCOME_START_NA,
                        EXPRESS_OUTCOME_ONLINE_RESP_DATA_NA, EXPRESS_OUTCOME_CVM_NA,
                        1, 0, 0, 1, 0, EXPRESS_OUTCOME_AIP_CONTACT_CHIP, 0, NULL);
    return EMV_SW_INTERFACE;
}

int express_outcome_try_again(void)
{
    express_outcome_set(EXPRESS_OUTCOME_STATUS_END_APPLICATION_WITH_RESTART_CDCVM, EXPRESS_OUTCOME_START_B,
                        EXPRESS_OUTCOME_ONLINE_RESP_DATA_NA, EXPRESS_OUTCOME_CVM_NA,
                        1, 1, 0, 1, 0, EXPRESS_OUTCOME_AIP_NA, 0x13, NULL);
    return EMV_REQ_SEEPHONE;
}

int express_outcome_request_another_payment(void)
{
    express_outcome_set(EXPRESS_OUTCOME_STATUS_TRY_ANOTHER_INTERFACE, EXPRESS_OUTCOME_START_B,
                        EXPRESS_OUTCOME_ONLINE_RESP_DATA_NA, EXPRESS_OUTCOME_CVM_NA,
                        1, 1, 0, 0, 0, EXPRESS_OUTCOME_AIP_NA, 0x13, NULL);
    return EMV_USE_ANOTHER_CARD;
}

int express_outcome_request_online_pin(void)
{
    express_outcome_set(EXPRESS_OUTCOME_STATUS_ONLINE_REQUEST_PRESENT_AND_HOLD, EXPRESS_OUTCOME_START_D,
                        EXPRESS_OUTCOME_ONLINE_RESP_DATA_ANY, EXPRESS_OUTCOME_CVM_ONLINE_PIN,
                        1, 1, 1, 1, 0, EXPRESS_OUTCOME_AIP_NA, 0x13, NULL);
    return EMV_REQ_ONLINE_PIN;
}

int express_outcome_approved_transactions(int result)
{
    const uint8_t *cvm_res = appdata_find(TAG_9F34_CVM_RESULTS, NULL);
    uint8_t cvm = g_express_cnxt->CVMParameter;

    if ((cvm_res != NULL) && !(cvm_res[0] == 0x00 || cvm_res[0] == 0x02))
    {
        cvm = EXPRESS_OUTCOME_CVM_NA;
    }

    express_outcome_set(EXPRESS_OUTCOME_STATUS_APPROVED, EXPRESS_OUTCOME_START_NA,
                        EXPRESS_OUTCOME_ONLINE_RESP_DATA_ANY, cvm,
                        1, 1, 1, 1, 0, EXPRESS_OUTCOME_AIP_NA, 0x13, NULL);
    return result;
}

int express_outcome_declined_transactions(int result)
{
    express_outcome_set(EXPRESS_OUTCOME_STATUS_DECLINED, EXPRESS_OUTCOME_START_NA,
                        EXPRESS_OUTCOME_ONLINE_RESP_DATA_ANY, EXPRESS_OUTCOME_CVM_NA,
                        1, 0, 1, 1, 0, EXPRESS_OUTCOME_AIP_NA, 0, NULL);
    return result;
}

/* ---------------------------------------------------------------------------
 * AIP / TVR / TSI
 *
 * 美运把 aip/tvr/tsi 以裸指针缓存在上下文（emvbuf.c:42-46）。Entrypoint 的 appdata_save
 * 会在同一 tlvbuffer 内追加/紧凑搬移，裸指针会悬垂（P1-6）。移植后：
 *   - aip 用 2 字节副本，GPO 解析后调用 express_aip_refresh()；
 *   - tvr/tsi 用副本 + 写穿透（先写 appdata 再更新副本），读取直接读副本。
 * ------------------------------------------------------------------------- */
void express_aip_refresh(void)
{
    uint8_t *p = appdata_find(TAG_0082_AIP, NULL);

    if (g_express_cnxt == NULL)
    {
        return;
    }
    if (p != NULL)
    {
        emv_memcpy(g_express_cnxt->aip, p, 2);
    }
    else
    {
        emv_memset(g_express_cnxt->aip, 0, 2);
    }
}

static int32_t express_bit_set(uint32_t tag, uint8_t *cache, int32_t cache_len, int32_t bit)
{
    int32_t index = (bit >> 8) & 0x7F;
    uint8_t mask = (uint8_t)(bit & 0xFF);
    uint8_t *p = appdata_find(tag, NULL);
    int32_t ret;

    if (g_express_cnxt == NULL || index >= cache_len)
    {
        return -1;
    }

    cache[index] |= mask;

    if (p == NULL)
    {
        /* 标签尚未建立（理论上前置阶段已建立）：用副本初始化为全 0 后写入 */
        return appdata_save(tag, cache, cache_len, UNIQUE);
    }

    ret = appdata_save_or(tag, index, mask);
    return ret;
}

static uint8_t express_bit_get(uint8_t *cache, int32_t cache_len, int32_t bit)
{
    int32_t index = (bit >> 8) & 0x7F;

    if (g_express_cnxt == NULL || index >= cache_len)
    {
        return 0;
    }
    return (uint8_t)(cache[index] & (uint8_t)(bit & 0xFF));
}

int32_t express_tvr_set(int32_t bit)
{
    return express_bit_set(TAG_0095_TVR, g_express_cnxt ? g_express_cnxt->tvr : NULL,
                           g_express_cnxt ? (int32_t)sizeof(g_express_cnxt->tvr) : 0, bit);
}

uint8_t express_tvr_get(int32_t bit)
{
    return express_bit_get(g_express_cnxt ? g_express_cnxt->tvr : NULL,
                           g_express_cnxt ? (int32_t)sizeof(g_express_cnxt->tvr) : 0, bit);
}

int32_t express_tsi_set(int32_t bit)
{
    return express_bit_set(TAG_009B_TSI, g_express_cnxt ? g_express_cnxt->tsi : NULL,
                           g_express_cnxt ? (int32_t)sizeof(g_express_cnxt->tsi) : 0, bit);
}

uint8_t express_tsi_get(int32_t bit)
{
    return express_bit_get(g_express_cnxt ? g_express_cnxt->tsi : NULL,
                           g_express_cnxt ? (int32_t)sizeof(g_express_cnxt->tsi) : 0, bit);
}

int32_t express_refresh_tvr_tsi(void)
{
    uint8_t *p;
    int32_t len = 0;

    if (g_express_cnxt == NULL)
    {
        return -1;
    }

    p = appdata_find(TAG_0095_TVR, &len);
    if (p != NULL && len >= (int32_t)sizeof(g_express_cnxt->tvr))
    {
        emv_memcpy(g_express_cnxt->tvr, p, sizeof(g_express_cnxt->tvr));
    }
    else
    {
        emv_memset(g_express_cnxt->tvr, 0, sizeof(g_express_cnxt->tvr));
    }

    p = appdata_find(TAG_009B_TSI, &len);
    if (p != NULL && len >= (int32_t)sizeof(g_express_cnxt->tsi))
    {
        emv_memcpy(g_express_cnxt->tsi, p, sizeof(g_express_cnxt->tsi));
    }
    else
    {
        emv_memset(g_express_cnxt->tsi, 0, sizeof(g_express_cnxt->tsi));
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * appdata_save2：Entrypoint 未导出（libentrypoint.a 只有 appdata_save/_save_or/_save_and/
 * _save_tlv/_save_singlebyte/_save_raw）。语义与美运 emvbuf.c appdata_save2 一致。
 * ------------------------------------------------------------------------- */
int32_t express_appdata_save2(EMV_TLVOBJ *tlv, int32_t override)
{
    if (tlv == NULL)
    {
        return -1;
    }
    return appdata_save(tlv->tagname, tlv->pvalue, tlv->valuelen, override);
}

/* ---------------------------------------------------------------------------
 * PDOL 打包：美运 emvtlv.c:498 emvpdol_packet 与 Entrypoint emvdol_packet 语义等价
 * （同源；Entrypoint 版多了 DOL 长度与输出容量校验）。直接复用 Entrypoint 实现。
 * ------------------------------------------------------------------------- */
int express_pdol_packet(u8 *pdol, int pdollen, u8 *val, int val_max_len)
{
    return emvdol_packet(pdol, pdollen, val, val_max_len);
}

/* ---------------------------------------------------------------------------
 * DRL（Dynamic Reader Limit）
 *
 * 方案 G2：Entrypoint 只保留 BCTC_DRL 存储与 emv_add_drl/emv_clr_drl，没有读取 API
 * （src/emvporting.c:2402-2470 只有 clr/add；libentrypoint.a 无 getter）。
 * 因此内核侧 express_param_load_drl() 目前返回"无 DRL"，等价于美运运行在
 * "未下发 drlN.par" 的配置：限额全部回退到 AID 级 DF19/DF20/DF21（Expresspay v4.2
 * 已删除 DRL，属可接受的降级）。TODO(port): Entrypoint 增加 emv_get_drl 后改为读取。
 * ------------------------------------------------------------------------- */
static EXPRESS_DRL_PARAMS g_express_drl_params;

int express_drl_get(BCTC_DRL *drl, int index)
{
    const EXPRESS_DRL *src = NULL;

    if (drl == NULL)
    {
        return -1;
    }
    emv_memset(drl, 0, sizeof(BCTC_DRL));

    if (index == 0x80)
    {
        src = &g_express_drl_params.defaultDRL;
    }
    else if (index >= 0 && index < EXPRESS_DRL_NUM)
    {
        src = &g_express_drl_params.DRLs[index];
    }
    else
    {
        return -1;
    }

    if (src == NULL || src->ifenable == 0)
    {
        return -1;
    }

    drl->Flag = (index == 0x80) ? 1 : 0;
    drl->Index = src->AppProgID;

    if (src->SupportCVMLimitCheck)
    {
        emv_memcpy(drl->CVMLimit, src->CVMLimit, 6);
    }
    else
    {
        emv_memset(drl->CVMLimit, 0xFF, 6);
    }
    if (src->SupportFloorLimitCheck)
    {
        emv_memcpy(drl->CLROFFLimit, src->FloorLimit, 6);
    }
    else
    {
        emv_memset(drl->CLROFFLimit, 0xFF, 6);
    }
    if (src->SupportRCTLCheck)
    {
        emv_memcpy(drl->CLRTLimit, src->TransLimit, 6);
    }
    else
    {
        emv_memset(drl->CLRTLimit, 0xFF, 6);
    }
    return 0;
}

EXPRESS_DRL_PARAMS *express_drl_params(void)
{
    return &g_express_drl_params;
}

/* ---------------------------------------------------------------------------
 * CRC32（IEEE 802.3，多项式 0xEDB88320；美运 emv.c:218）
 * 仅用于 express_get_version 的内核校验和（方案 §5.3 已删除 config_checksum）。
 * ------------------------------------------------------------------------- */
int32_t express_crc32(uint32_t crc, const uint8_t *buf, int32_t len)
{
    int32_t i;
    int32_t j;

    if (buf == NULL || len < 0)
    {
        return (int32_t)crc;
    }

    crc = ~crc;
    for (i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (j = 0; j < 8; j++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return (int32_t)~crc;
}
