/**
************************************************************************************************************************
* @file     express_complete.c
* @brief    Expresspay 内核：交易收尾（1st GENERATE AC 收尾 / Outcome（2nd GAC、脚本、DDX 均不支持））
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_complete.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     emv_complete.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    交易结束（Complete）
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

#define MAX_CMD    30


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

int express_send_trans_result(int result)
{
	if(tvr_get(CDA_FAILED))
	{
		appdata_save(TAG_008A_ARC, (uint8_t*)"Z1", 2, OVERRIDED);
		return express_outcome_declined_transactions(EMV_DENIALED_OFFLINE);
//		return rEND(EMV_DENIALED_OFFLINE);
	}

	switch (result)
	{
		case EMV_ACCEPTED_OFFLINE:
			return express_outcome_approved_transactions(result);

		case EMV_DENIALED_OFFLINE:
			appdata_save(TAG_008A_ARC, (uint8_t*)"Z1", 2, OVERRIDED);
			return express_outcome_declined_transactions(result);
//			return rEND(result);

		case EMV_REQ_GO_ONLINE:
			return rNXT(EMV_REQ_GO_ONLINE);

		case EMV_ACCEPTED_ONLINE:
			return express_outcome_approved_transactions(result);

		case EMV_DENIALED_ONLINE:
			return express_outcome_declined_transactions(result);

		case EMV_SW_INTERFACE:
			return express_outcome_try_another_interface();

		default:
			return rEND(EMV_ENDAPPLICATION);
	}
}

int express_pre_complete()
{
	u8 *cid = NULL,*gac_type = NULL,*cda_auth_data = NULL;
	u8 *enhan_ctl_reader_cap = NULL, *card_itr_payment_cap= NULL;
	int trans_result, ret = EMV_ENDAPPLICATION, len;

	enhan_ctl_reader_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
	card_itr_payment_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);

	cid = appdata_find(TAG_9F27_CRYPTOGRAM_INFO_DATA, NULL);
	if(Resp_AAC(*cid))
	{
		if((enhan_ctl_reader_cap != NULL) && (enhan_ctl_reader_cap[0] & 0x80))	//if Terminal has contact interface
		{
			if( ((g_express_cnxt->aip[1] & 0x40) == 0x00) && \
				((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
				((card_itr_payment_cap == NULL) || (card_itr_payment_cap[0] & 0x20)) )
			{
				return express_outcome_try_another_interface();
			}
		}

		trans_result = EMV_DENIALED_OFFLINE;
	}
	else if(Resp_TC(*cid))
	{
		trans_result = EMV_ACCEPTED_OFFLINE;
	}
	else if(Resp_ARQC(*cid))
	{
		if(g_express_cnxt->bis_unable_online)
		{
			trans_result = EMV_DENIALED_OFFLINE;
		}
		else
		{
			trans_result = EMV_REQ_GO_ONLINE;
		}
	}
	else
	{
		trans_result = EMV_ENDAPPLICATION;
	}

	eTRACE("AMEX-info: trans_result = %d", trans_result);

	gac_type = appdata_find(TAG_GAC_REQUEST_TYPE, NULL);
	eTRACE("AMEX-info: gac_type = %02X", *gac_type);
	if(*gac_type & 0x10)	//if GAC request CDA
	{
		if(trans_result == EMV_DENIALED_OFFLINE)
		{
			tsi_set(OFFLINE_DA_PERFORMED);
			return express_send_trans_result(trans_result);
		}

		cda_auth_data = appdata_find(TAG_CDAAUTH_VALUE, &len);

		ret = express_process_cda(1, cda_auth_data, len);

		eTRACE("AMEX-info: express_process_cda ret = %d", ret);
		if(ret != 0)
		{
			eTRACE("AMEX-error: verify CDA cipher ret = %d", ret);
#if !SELF_APDU_TEST
			if(trans_result == EMV_REQ_GO_ONLINE || trans_result == EMV_ACCEPTED_OFFLINE)
			{
				trans_result = EMV_DENIALED_OFFLINE;
				tvr_set(CDA_FAILED);
				return express_send_trans_result(trans_result);
			}
#endif
		}

	}

	return express_send_trans_result(trans_result);
}

int express_complete(void)
{
	int trans_result = EMV_DENIALED_OFFLINE;
	u8 *auth_code = NULL,*enc_cap = NULL, *card_itr_payment_cap = NULL;
	u8 *cvm_list=NULL;

	enc_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
	card_itr_payment_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);

	if(g_express_cnxt->status == EMV_REQ_GO_ONLINE)
	{
		if(g_express_cnxt->online_result == EMV_ONLINE_SUCC_ACCEPT || \
			g_express_cnxt->online_result == EMV_ONLINE_SUCC_DENIAL || \
			g_express_cnxt->online_result == EMV_ONLINE_SUCC_ISSREF)
		{
			auth_code =appdata_find(TAG_008A_ARC, NULL);
			if(auth_code == NULL)
			{
				return express_send_trans_result(trans_result);
			}

			//approval
			if(!emv_memcmp(auth_code, "00", 2) || !emv_memcmp(auth_code, "08", 2) || \
				!emv_memcmp(auth_code, "10", 2) || !emv_memcmp(auth_code, "11", 2))
			{
				trans_result = EMV_ACCEPTED_ONLINE;
			}
			else if(!emv_memcmp(auth_code, "05", 2))
			{
				trans_result = EMV_DENIALED_ONLINE;
			}
			else if(!emv_memcmp(auth_code, "01", 2) || !emv_memcmp(auth_code, "02", 2))	// v4.2 Table 21-25 p99: 16 类之外的 ARC 一律拒绝（无 Issuer Referral）
			{
				trans_result = EMV_DENIALED_OFFLINE;
			}
			else if(!emv_memcmp(auth_code, "13", 2))	//Request Online PIN if supported, try another interface otherwise
			{
				if((enc_cap != NULL) && (enc_cap[1] & 0x40) && g_express_cnxt->bis_card_sup_onlinepin)
				{
					if(g_express_cnxt->req_online_pin_result == EMV_OPER_OK)
					{
						trans_result = EMV_ACCEPTED_ONLINE;
					}
					else if(g_express_cnxt->req_online_pin_result == EMV_OPER_BYPASS)
					{
						trans_result = EMV_ACCEPTED_ONLINE;
					}
					else
					{
						trans_result = EMV_DENIALED_OFFLINE;
					}
				}
				else
				{
					if((card_itr_payment_cap == NULL) || ((card_itr_payment_cap != NULL) && (card_itr_payment_cap[0] & 0x20)))
					{
						trans_result = EMV_SW_INTERFACE;
					}
					else
					{
						trans_result = EMV_DENIALED_OFFLINE;
					}
				}

			}
			else if(!emv_memcmp(auth_code, "12", 2))	//Try another interface if supported, decline otherwise
			{
				if((card_itr_payment_cap == NULL) || ((card_itr_payment_cap != NULL) && (card_itr_payment_cap[0] & 0x20)))
				{
					trans_result = EMV_SW_INTERFACE;
				}
			}
			else
			{
				trans_result = EMV_DENIALED_OFFLINE;	//For all other ARCs, the transaction must be declined
			}

		}
		else if(g_express_cnxt->online_result == EMV_ONLINE_FAIL)
		{
			if((enc_cap != NULL) && (enc_cap[0] & 0x80))
			{
				if((card_itr_payment_cap == NULL) || ((card_itr_payment_cap != NULL) && (card_itr_payment_cap[0] & 0x20)))
				{
					trans_result = EMV_SW_INTERFACE;
				}
			}
		}
		else
		{
			trans_result = EMV_DENIALED_OFFLINE;
		}
	}

	/* Expresspay v4.2 无 Issuer Referral（Table 21-25 p99：ARC 01/02 属"其它值"→拒绝），
	 * 因此不再有 EMV_REQ_ISSUER_REFERRAL 分支；若外部误置该状态，同样按拒绝收尾。 */

	return express_send_trans_result(trans_result);
}


/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
