/**
************************************************************************************************************************
* @file     express_initiate_application.c
* @brief    Expresspay 内核：应用初始化（Initiate Application Processing / GPO）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_initiate_application.c（逐行移植，只改符号前缀与共享服务调用）
* 符号约定：全部非 static 入口改名为 express_*（避免与 libentrypoint.a 的 180 个同名全局符号冲突）；
*           共享服务（appdata_* / emvtlv_* / emvicc_* / emv_*）直接引用 Entrypoint 已导出实现。
* 修改点（相对美运源码）：
*   - 平台/持久化调用改由 Entrypoint 的 EMV_PORTING（g_emv_port）与 appdata 服务提供；
*   - 美运私有 outcome_* 改为 express_outcome_*（美运 OUTCOME_FLAG=0 时只返回状态码）；
*   - appdata_save2 → express_appdata_save2（Entrypoint 未导出该函数）；
*   - 上下文 g_emv_cnxt → g_express_cnxt（内核私有 EXPRESS_CONTEXT，方案 §5.3）。
* 逐条差异见 doc/EXPRESSPAY_PORT_STATUS.md。
************************************************************************************************************************
*/
/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     express_initiate_application.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    应用初始化（Initiate Application Processing）
*
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "express_priv.h"


/*
************************************************************************************************************************
*                                                     LOCAL DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                   LOCAL DATA TYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                    LOCAL CONSTANTS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                               LOCAL FUNCTION PROTOTYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                   LOCAL FUNCTIONS
************************************************************************************************************************
*/



/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/


int express_pre_pdol_process(u8 term_type)
{
    appdata_save_and(TAG_ClessReaderCapab, 0, ~0x08);
    appdata_save_and(TAG_EnhancedClessReaderCapab, 2, 0x00);
    if (g_express_cnxt->CVMRLEF)
    {
        appdata_save_or(TAG_ClessReaderCapab,0,0x08);
        appdata_save_or(TAG_EnhancedClessReaderCapab,2,0x40);
    }

	eTRACE("AMEX-info: bis_unable_online = %d", g_express_cnxt->bis_unable_online);

    if ((term_type & 0x07) == 0x03 || (term_type & 0x07) == 0x06 || g_express_cnxt->bis_unable_online == TRUE)
    {
		appdata_save_or(TAG_EnhancedClessReaderCapab, 2, 0x80);	//Terminal is offline only
    }

	if(g_express_cnxt->force_online == 1)	//For Online Only Terminal
	{
		if(g_express_cnxt->bis_unable_online)
		{
			return express_outcome_declined_transactions(EMV_DENIALED_OFFLINE);
		}
	}


	return EMV_OK;
}

int express_pdol_process(u8 *apdu_send, int *send_len, u8 term_type)
{
	u8 *pdol=NULL,*ptr=NULL;
	int pdol_len, dol_value_len;
	int temp_len;
	u8 dol_value[EMV_CFG_ICC_RECV_MAXLEN] = {0};
	u8 term_type_origin = term_type;
	bool term_type_modified = FALSE;

	if(apdu_send == NULL || send_len == NULL)
	{
		return EMV_FAIL;
	}

	pdol = appdata_find(TAG_9F38_PDOL, &pdol_len);
	if(pdol != NULL)
	{
		if (emvdol_tag_exists(TAG_9F35_TERMINAL_TYPE, pdol, pdol_len) && !emvdol_tag_exists(TAG_EnhancedClessReaderCapab, pdol, pdol_len))
		{
			eTRACE("AMEX-info: PDOL dont return 9F6E when return 9F35");

			if ((ptr = appdata_find(TAG_ClessReaderCapab, &temp_len)) != NULL)
			{
				eTRACE("AMEX-info: return Terminal Type -Modified instead of 9F35");
				term_type |= ptr[0];
				term_type_modified = TRUE;
				//for authenticate ,use self-define tag to save modified term type
				appdata_save(TAG_FF8178_TERM_TYPE_MODIFY, &term_type, 1, OVERRIDED);
				/* 仅为本次 GPO 组包临时写入 Modified 值；组包后立即恢复原始 9F35（P p34：
				 * "the modified value shall only be used for this transaction"，
				 * 授权/请款报文必须用未修改的 9F35）。 */
				appdata_save(TAG_9F35_TERMINAL_TYPE, &term_type, 1, OVERRIDED);
			}
		}

		if (term_type_modified)
		{
			/* appdata_save 可能搬移缓冲区，重新获取 PDOL 指针后再组包 */
			pdol = appdata_find(TAG_9F38_PDOL, &pdol_len);
			if (pdol == NULL)
			{
				appdata_save(TAG_9F35_TERMINAL_TYPE, &term_type_origin, 1, OVERRIDED);
				return rERR(IA_PDOLPACKERR);
			}
		}

		dol_value_len = express_pdol_packet(pdol, pdol_len, dol_value, EMV_CFG_ICC_RECV_MAXLEN);
		eTRACE("AMEX-info: Pack PDOL len = %d", dol_value_len);

		if (term_type_modified)
		{
			/* appdata_save 可能搬移缓冲区：恢复原始 9F35 前无需再用 pdol */
			appdata_save(TAG_9F35_TERMINAL_TYPE, &term_type_origin, 1, OVERRIDED);
		}

		if (dol_value_len < 0)
		{
			return rERR(IA_PDOLPACKERR);
		}

		appdata_save(TAG_PDOL_VALUE, dol_value, dol_value_len, UNIQUE);


		if (dol_value_len >= 128)
		{
			*(apdu_send) = (u8)TAG_0083_COMMAND_TEMPLATE;
			*(apdu_send+1) = 0x81;
			*(apdu_send + 2) = (u8)dol_value_len;
			emv_memcpy(apdu_send+3, dol_value, dol_value_len);
			*send_len = dol_value_len + 3;
		}
		else
		{
			*(apdu_send) = (u8)TAG_0083_COMMAND_TEMPLATE;
			*(apdu_send+1) = (u8)dol_value_len;
			emv_memcpy(apdu_send+2, dol_value, dol_value_len);
			*send_len = dol_value_len + 2;
		}
	}
	else
	{
		*apdu_send = (u8)TAG_0083_COMMAND_TEMPLATE;
		*(apdu_send + 1) = 0x00;
		*send_len = 2;
	}

	return EMV_OK;
}

int express_parse_gpo_resp(u8 *resp, int resp_len)
{
	EMV_TLVOBJ tlvs[32]={0};
	int i, pos;

	// decode the response to fetch AIP and AFL
	if (emvtlv_decode(resp, resp_len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ) < 0)
    {
        eTRACE("AMEX-error: parse gpo resp error");
		return IA_GPORSPERR;
	}

	if (tlvs[1].tagname == 0x80)
    {
		if (tlvs[1].valuelen % 4 != 2 || tlvs[1].valuelen == 2)
        {
			// the length should be 2 bytes AIP + 4N bytes AFL,
			// An AFL with no entries is not allowed (see Book3 7.5)
            eTRACE( "The length should be 2 bytes AIP + 4N bytes AFL");
			return IA_GPORSP80;
		}
		appdata_save(TAG_0082_AIP, tlvs[1].pvalue, 2, UNIQUE);
		appdata_save(TAG_0094_AFL, tlvs[1].pvalue + 2, tlvs[1].valuelen - 2, UNIQUE);
	}
    else if (tlvs[1].tagname == 0x77)
    {
        for (i = 0; i < tlvs[1].childnum; ++i)
        {
            express_appdata_save2(tlvs + i + 2, UNIQUE);
        }

		if ((pos = emvtlv_get(1, TAG_0082_AIP, tlvs, SEARCH_ONLY_SON)) == 0)
        {
            eTRACE( "Missing Application Interchange Profile[0x82]");
			return IA_GPORSP77NOAIP;
        }

        if (tlvs[pos].valuelen != 2)
        {
            eTRACE( "The length of AIP should be 2 bytes");
            return IA_GPORSP77AIPLENERR;
		}
		express_appdata_save2(tlvs + pos, UNIQUE);

		if ((pos = emvtlv_get(1, TAG_0094_AFL, tlvs, SEARCH_ONLY_SON)) == 0)
        {
            eTRACE( "Missing Application File Locator (AFL)[0x94]");
			return IA_GPORSP77NOAFL;
		}
		if (tlvs[pos].valuelen % 4 != 0 || tlvs[pos].valuelen == 0)
        {
            eTRACE( "The length of AFL should be 4 bytes");
            return IA_GPORSP77AFLLENERR;
		}
		express_appdata_save2(tlvs + pos, UNIQUE);
	}
    else
    {
		// if not 80 or 77 encoded
        eTRACE( "The template of GPO response should be 77 or 80");
		return IA_GPORSPERR;
	}

	return EMV_OK;
}

int express_deal_gpo_resp()
{
    u8 * mobile_cvm = NULL;
	u8 * EnhancedClessReaderCapab = NULL;
	u8 tagv = 0;
    express_aip_refresh();
	mobile_cvm = appdata_find(TAG_9F71_MB_CVMResult, NULL);

	/* 9F71 只在 Mobile 交易（AIP b2b7=1）时有效；Card 交易必须忽略 9F71（P p35 §4.3.3） */
	if(AIP_MOB_SUPPORT()&&(mobile_cvm !=NULL))
    {
        if(mobile_cvm[2] == 0x03)   // Mobile CVM Blocked
        {
            tvr_set(PIN_TRY_LIMIT_EXCEEDED);
        }
    }

	/* Expresspay v4.2 只支持 EMV 模式（Mag-stripe/MSD 已在 v4.2 删除，P p10 版本历史）。
	 * AIP b2b8=1 = 卡支持 EMV 模式（Table 21-11 p92）；b2b8=0 时按 P p35 §4.3.3 分流：
	 *   终端不支持 Magstripe(b1b7=0) + 支持 AEIPS(b1b8=1) + 卡非 Mobile(b2b7=b2b6=0)
	 *     → §17.2.2 Try Another Interface；否则 → §17.2.4 Request Another Payment。 */
	if((g_express_cnxt->aip[1] & 0x80) == 0)
	{
		EnhancedClessReaderCapab = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
		if((EnhancedClessReaderCapab != NULL) &&
		   ((EnhancedClessReaderCapab[0] & 0x40) == 0) &&
		   ((EnhancedClessReaderCapab[0] & 0x80) != 0) &&
		   ((g_express_cnxt->aip[1] & 0x40) == 0) &&
		   ((g_express_cnxt->aip[1] & 0x20) == 0))
		{
			eTRACE("AMEX-info: card has no EMV mode, try another interface");
			return express_outcome_try_another_interface();
		}

		eTRACE("AMEX-info: card has no EMV mode, request another payment");
		return express_outcome_request_another_payment();
	}

	/* POS Entry Mode：非接 EMV（MSD 的 0x91 分支已随 v4.2 删除） */
	tagv = 0x07;
	appdata_save(TAG_9F39_POS_ENTRY_MODE,&tagv,1,UNIQUE);

	return EMV_OK;
}

int express_initiate_application(void)
{
	int send_len = 0, resp_len = 0, ret = EMV_FAIL;
	u8 apdu_send[EMV_CFG_ICC_RECV_MAXLEN+ 4] = {0};
	u8 gpo_resp[EMV_CFG_ICC_RECV_MAXLEN] = {0};
    int sw;
	u8 *term_type=NULL;

	term_type = appdata_find(TAG_9F35_TERMINAL_TYPE, NULL);

    // pre-pdol processing
	ret = express_pre_pdol_process(*term_type);
	if(ret != EMV_OK)
	{
		if(ret < 0)
		{
			return rERR(ret);
		}
		else if(ret > 0 && ret < 100)
		{
			return rNXT(ret);
		}
		else
		{
			return rEND(ret);
		}
	}

	ret = express_pdol_process(apdu_send, &send_len, *term_type);
	if(ret != EMV_OK)
	{
		return rERR(IA_PDOLPACKERR);
	}

	eTRACE_HEX("AMEX-info: GPO send data", apdu_send, send_len);

	// issue a gpo command
	sw = emvicc_get_processing_options(apdu_send, send_len, gpo_resp, &resp_len);

	if(sw == 0x0000)
	{
		return rERR(EMV_APDU_TIMEOUT);
	}
	else if(sw == 0x9000)
	{
		ret = express_parse_gpo_resp(gpo_resp, resp_len);
		if(ret != EMV_OK)
		{
			return rERR(ret);
		}
	}
	else
	{
		if(sw == 0x6985)
		{
			return rERR(IA_GPOSW6985);
		}
		else
		{
			return express_outcome_request_another_payment();
			return rEND(EMV_USE_ANOTHER_CARD);
		}
	}


	ret = express_deal_gpo_resp();
	if(ret != EMV_OK)
	{
		if(ret < 0)
		{
			return rERR(ret);
		}
		else if(ret > 0 && ret < 100)
		{
			return rNXT(ret);
		}
		else
		{
			return rEND(ret);
		}
	}


	return rNXT(EMV_STA_APP_INITIALIZED);
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
