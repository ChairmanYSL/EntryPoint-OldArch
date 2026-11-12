/**
************************************************************************************************************************
* @file     express_terminal_action_analysis.c
* @brief    Expresspay 内核：终端行为分析（Terminal Action Analysis / GAC）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_terminal_action_analysis.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     emv_terminal_action_analysis.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    终端行为分析（Terminal Action Analysis）
*
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "express_priv.h"

/* P1 修复用：TVR 缺省副本（避免空指针） */
static u8 k_zero_tvr[5] = {0, 0, 0, 0, 0};


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


void express_calc_tc_hash(u8 * dol, int len)
{
	int i = 0;
	u8 * pdata = NULL;
	u8 tdolval[EMV_CFG_ICC_RECV_MAXLEN] = {0};
	u8 hashv[20] = {0};

	for (i = 0; i < len; i ++)
    {
		if (*(dol + i) == 0x98)
        {
			if ((pdata = appdata_find(TAG_0097_TDOL, &i)) == NULL)
            {
				if (ics_option(CP_Support_Default_TDOL))
                {
					pdata = appdata_find(TAG_DEFAULT_TDOL, &i);
					if (pdata && i != 0)
                    {
						tvr_set(DEFAULT_TDOL_USED);
					}
				}
			}
			if ((i = emvdol_packet(pdata, i, tdolval, EMV_CFG_ICC_RECV_MAXLEN)) < 0)
            {
				return;
			}

            EMVPORT_Hash(EMV_HASH_ALG_SHA1, tdolval, i, hashv);

//			appdata_save(0x98, hashv, 20, OVERRIDED);
			appdata_save(TAG_0098_TC_HASH_VALUE, hashv, 20, OVERRIDED);

			return;
		}
	}
	return;
}
int express_cid_msg(u8 cid)
{
    switch (Resp_Info(cid)) {
        case 0x03:
            EMVPORT_Message(EMV_MSG_IssuerAuthenticationFailed);
            break;
        case 0x02:
            EMVPORT_Message(EMV_MSG_PinTryLimitExceeded);
            break;
        case 0x01:
            EMVPORT_Message(EMV_MSG_ServiceNotAllowed);
            return -1;
        default:
            break;
    }
    return 0;
}



static u8 express_check_tvr(u8 * auth_code)
{
	u8 * g_tvr = g_express_cnxt->tvr;
	int i = 0;
//	u8 iac_default_def[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//	u8 iac_denial_def[] = {0x00, 0x00, 0x00, 0x00, 0x00}; // default value
	u8 *trans_type = NULL,*iac_denial,*iac_default,*iac_online,*term_type;
    u8 *tac_denail,*tac_default,*tac_online;
	bool match_iac = FALSE, match_tac = FALSE;

	if (auth_code == NULL)
    {
		return AC_AAC;
	}

	trans_type = appdata_find(TAG_009C_TRANSACTION_TYPE, NULL);
	if(trans_type != NULL)
	{
		if(*trans_type == 0x20)
		{
			tvr_set(TRANSACTION_EXCEEDS_FLOOR_LIMIT);
			return AC_ARQC;
		}
	}


	eTRACE_HEX("AMEX-info: TVR:", g_tvr, 5);

	iac_denial = appdata_find(TAG_9F0E_ISSUER_ACTION_CODE_DENIAL, NULL);
	tac_denail = appdata_find(TAG_DF13_TAC_DENIAL, NULL);

	if(iac_denial == NULL)
	{
        for(i=0; i<5; i++)
        {
            if((g_tvr[i] & tac_denail[i]) != 0)
            {
                return AC_AAC;
            }
        }
	}
	else
	{
        for(i=0; i<5; i++)
        {
            if((g_tvr[i] & iac_denial[i]) != 0)
            {
                return AC_AAC;
            }
            if((g_tvr[i] & tac_denail[i]) != 0)
            {
                return AC_AAC;
            }
        }
	}


	term_type = appdata_find(TAG_9F35_TERMINAL_TYPE, NULL);
    if((*term_type != 0x23) && (*term_type != 0x26) && (*term_type != 0x36) && \
       (*term_type != 0x13) && (*term_type != 0x16)) //Terminal has Online capability
    {
		tac_online = appdata_find(TAG_DF12_TAC_ONLINE, NULL);
		iac_online = appdata_find(TAG_9F0F_ISSUER_ACTION_CODE_ONLINE, NULL);

		if((g_tvr[2] & 0x20) && g_express_cnxt->bis_unable_online)
		{
			eTRACE("AMEX-info: reader unable online, go to check default");
		}
		else
		{
			if(iac_online == NULL)
			{
				for(i=0; i<5; i++)
				{
					if((g_tvr[i]&tac_online[i])!=0)
					{
						return AC_ARQC;
					}
				}
				return AC_TC;
			}
			else
			{
				for(i=0; i<5; i++)
				{
					if((g_tvr[i] & iac_online[i])!=0)
					{
						return AC_ARQC;
					}
					if((g_tvr[i] & tac_online[i])!=0)
					{
						return AC_ARQC;
					}
				}

				return AC_TC;
			}
		}
	}


	iac_default = appdata_find(TAG_9F0D_ISSUER_ACTION_CODE_DEFAULT, NULL);
	tac_default = appdata_find(TAG_DF11_TAC_DEFAULT, NULL);

	if(iac_default == NULL)
	{
        for(i=0; i<5; i++)
        {
            if((g_tvr[i]& tac_default[i])!=0)
            {
                return AC_AAC;
            }
        }
        return AC_TC;
	}
	else
	{
        for(i=0; i<5; i++)
        {
            if((g_tvr[i] & iac_default[i])!=0)
			{
                return AC_AAC;
				match_iac = TRUE;
			}
            if((g_tvr[i] & tac_default[i])!=0)
			{
                return AC_AAC;
				match_tac = TRUE;
			}
        }

//		if(match_iac == TRUE || match_tac == TRUE)
//		{
//			if()
//		}

		eTRACE("AMEX-info: IAC-Default dose not match with TVR");
		return AC_TC;

	}

}



unsigned char express_term_analy_trans_res()
{
	u8 *IACDenial,*TACDenial,*IACOnline,*TACOnline,*IACDefault,*TACDefault;
	u8 *TVR, *TermType;
	u8 *tmp=NULL;
	int len, i;
	u8 term_type;

	tmp = appdata_find(TAG_009C_TRANSACTION_TYPE, &len);
	/* P1 修复：tmp 可能为 NULL（美运原版直接 tmp[0]） */
	if(tmp != NULL && 0x20 == tmp[0])
	{
		appdata_save_or(TAG_0095_TVR, 3, 0x80);	//set 'transaction exceeds floor limit' bit 1.
		return AC_ARQC;
	}

	//compare IAC and TAC with TVR
	IACDenial = appdata_find(TAG_9F0E_ISSUER_ACTION_CODE_DENIAL, &len);
	TACDenial = appdata_find(TAG_DF13_TAC_DENIAL, &len);
	TVR = appdata_find(TAG_0095_TVR, &len);
	/* P1 修复：TVR 未建立时用全 0 副本，绝不空指针解引用 */
	if (TVR == NULL)
	{
		TVR = k_zero_tvr;
	}

	if(NULL == IACDenial)
	{
		if (TACDenial != NULL)
		{
			for(i = 0; i < 5; i++)
			{
				if(0 != (TVR[i] & TACDenial[i]))
				{
					return AC_AAC;
				}
			}
		}
	}
	else
	{
		for(i = 0; i < 5; i++)
		{
			if(0 != (TVR[i] & IACDenial[i]))
			{
				return AC_AAC;
			}
			if(TACDenial != NULL && 0 != (TVR[i] & TACDenial[i]))
			{
				return AC_AAC;
			}
		}
	}

	TermType = appdata_find(TAG_9F35_TERMINAL_TYPE, &len);
	/* P1 修复：美运把"指针"与 0x23/0x26/... 做整数比较（恒不等）→ 永远走"可联机"分支。
	 * 这里先解引用；9F35 缺失时按 0x22（attended / offline+online）处理。 */
	term_type = (TermType != NULL) ? TermType[0] : 0x22;
	if((0x23 != term_type) && (0x26 != term_type) && (0x36 != term_type) && (0x13 != term_type) && (0x16 != term_type)) //Terminal has Online capability
	{
		IACOnline = appdata_find(TAG_9F0F_ISSUER_ACTION_CODE_ONLINE, &len);
		TACOnline = appdata_find(TAG_DF12_TAC_ONLINE, &len);
		if(NULL == IACOnline)
		{
			if (TACOnline != NULL)
			{
				for(i = 0; i < 5; i++)
				{
					if(0 != (TVR[i] & TACOnline[i]))
					{
						return AC_ARQC;
					}
				}
			}
			return AC_TC;
		}
		else
		{
			for(i=0; i<5; i++)
			{
				if(0 != (TVR[i] & IACOnline[i]))
				{
					return AC_ARQC;
				}
				if(TACOnline != NULL && 0 != (TVR[i] & TACOnline[i]))
				{
					return AC_ARQC;
				}
			}
			return AC_TC;
		}
	}

	IACDefault= appdata_find(TAG_9F0D_ISSUER_ACTION_CODE_DEFAULT, &len);
	TACDefault = appdata_find(TAG_DF11_TAC_DEFAULT, &len);

	if(NULL == IACDefault)
	{
		if (TACDefault != NULL)
		{
			for(i = 0; i < 5; i++)
			{
				if(0 != (TVR[i] & TACDefault[i]))
				{
					return AC_AAC;
				}
			}
		}
		return AC_TC;
	}
	else
	{
		for(i = 0; i < 5; i++)
		{
			if(0 != (TVR[i] & IACDefault[i]))
			{
				return AC_AAC;
			}
			if(TACDefault != NULL && 0 != (TVR[i] & TACDefault[i]))
			{
				return AC_AAC;
			}
		}
		return AC_TC;
	}
}
int express_parse_gac_resp(u32 template, bool cda_flag)
{
	u8 *cid = NULL, *atc = NULL, *card_itr_cap = NULL, *ac = NULL;
	u8 *enc_cap = NULL,*cmd_param = NULL, *sign_dyn_app_data = NULL;
	u8 gac_type;
	int len;

	eTRACE("AMEX-info: call express_parse_gac_resp");

	cid = appdata_find(TAG_9F27_CRYPTOGRAM_INFO_DATA, NULL);
	atc = appdata_find(TAG_9F36_ATC, NULL);
	ac = appdata_find(TAG_9F26_APPLICATION_CRYPTOGRAM, NULL);
	enc_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
	card_itr_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);
	cmd_param = appdata_find(TAG_GAC_REQUEST_TYPE, NULL);
	sign_dyn_app_data = appdata_find(TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA, &len);

//	if(cid == NULL || atc == NULL)
//	{
//		eTRACE("AMEX-error: GAC resp doesnt contain 9F27 or 9F36 or 9F26");
//		if( (enc_cap[0] & 0x80) && \
//			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
//			((card_itr_cap[0] & 0x20) || (card_itr_cap == NULL)) )
//		{
//			eTRACE("AMEX-error: Card support other interface");
//			return express_outcome_try_another_interface();
//		}
//		else
//		{
//			eTRACE("AMEX-error: Card doesn't support other interface");
//			return express_outcome_request_another_payment();
//		}
// 	}

	if(template == TAG_0080_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1)
	{
		if(ac == NULL || cid == NULL || atc == NULL)
		{
			eTRACE("AMEX-error: GAC 80 template does not contain 9F26 or 9F36 or 9F27");
//			return EMV_ENDAPPLICATION;

			if( (enc_cap[0] & 0x80) && \
				((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
				((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)) )
			{
				eTRACE("AMEX-error: Card support other interface");
				return express_outcome_try_another_interface();
			}
			else
			{
				eTRACE("AMEX-error: Card doesn't support other interface");
				return express_outcome_request_another_payment();
			}
		}
	}
	else if(template == TAG_0077_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2)
	{
		if(cid == NULL || atc == NULL)
		{
			eTRACE("AMEX-error: GAC 77 template does not contain 9F36 or 9F27");
//			return EMV_ENDAPPLICATION;
			if( (enc_cap[0] & 0x80) && \
				((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
				((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)) )
			{
				eTRACE("AMEX-error: Card support other interface");
				return express_outcome_try_another_interface();
			}
			else
			{
				eTRACE("AMEX-error: Card doesn't support other interface");
				return express_outcome_request_another_payment();
			}
		}

//		if(cda_flag == FALSE)
//		{
//			if(ac == NULL)
//			{
//				if( (enc_cap[0] & 0x80) && \
//					((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
//					((card_itr_cap[0] & 0x20) || (card_itr_cap == NULL)) )
//				{
//					eTRACE("AMEX-error: Card support other interface");
//					return express_outcome_try_another_interface();
//				}
//				else
//				{
//					eTRACE("AMEX-error: Card doesn't support other interface");
//					return express_outcome_request_another_payment();
//				}
//			}
//		}
//		else
//		{
//
//		}
//
//		if((cda_flag == FALSE && ac == NULL) || ((cda_flag == TRUE) && (((ac != NULL) && (sign_dyn_app_data == NULL)) || (ac == NULL))))
//		{
//			eTRACE("AMEX-error: GAC 77 template does not contain 9F26 or 9F4B");
////			return EMV_ENDAPPLICATION;
//
//		}
	}

	gac_type = (*cmd_param) & 0xEF;
	eTRACE("cid = %x, gac_type = %x",*cid, gac_type);

	if(Resp_TC(*cid) && (gac_type == AC_TC))
	{

	}
	else if(Resp_ARQC(*cid) && ((gac_type == AC_TC) || (gac_type == AC_ARQC)))
	{

	}
	else if(Resp_AAC(*cid))
	{
//		appdata_save(TAG_008A_ARC, "Z1", 2, OVERRIDED);
		if( (enc_cap[0] & 0x80) && \
			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
			((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)) )
		{
			eTRACE("AMEX-info: Card return AAC and Term support other interface");
			return express_outcome_try_another_interface();
		}
//		else
//		{
//			eTRACE("AMEX-info: Card return AAC and Term don't support other interface");
//			return EMV_DENIALED_OFFLINE;
//		}
	}
	else
	{
		if(!Resp_AAC(*cid) && !Resp_TC(*cid) && !Resp_ARQC(*cid))
		{
			if( (enc_cap[0] & 0x80) && \
				((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
				((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)) )
			{
				eTRACE("AMEX-error: CID format invalid and Card support other interface");
				return express_outcome_try_another_interface();
			}
			else
			{
				eTRACE("AMEX-error: CID format invalid");
				return express_outcome_request_another_payment();
			}
 		}
		else
		{
			eTRACE("AMEX-error: CID format doesn't match GAC type");
			return EMV_DENIALED_OFFLINE;
		}
	}

	//
	if(sign_dyn_app_data == NULL || len <= 0)
	{
		eTRACE("AMEX-info: GAC resp does not contain 9F4B");
		if(ac == NULL)
		{
			eTRACE("AMEX-error: GAC resp does not contain 9F26");
			if( (enc_cap[0] & 0x80) && \
				((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
				((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)) )
			{
				eTRACE("AMEX-error: CID format invalid and Card support other interface");
				return express_outcome_try_another_interface();
			}
			else
			{
				eTRACE("AMEX-error: CID format invalid");
				return express_outcome_request_another_payment();
			}
		}

		if(Resp_AAC(*cid))
		{
			if(gac_type == AC_AAC && cda_flag)
			{
				eTRACE("AMEX-error: CID indicate AAC when CDA is requested");
				return EMV_ENDAPPLICATION;
			}
		}
		else
		{
			if(cda_flag)
			{
				eTRACE("AMEX-error: 9F4B absent when CDA is requested");
				return EMV_ENDAPPLICATION;
			}
		}
	}


//	if(cda_flag && (sign_dyn_app_data == NULL || len <= 0))
//	{
//		eTRACE("AMEX-info: GAC resp does not contain 9F4B");
//		if(ac == NULL)
//		{
//			eTRACE("AMEX-error: GAC resp does not contain 9F26");
//			return EMV_ENDAPPLICATION;
//		}
//
//
//	}

	tsi_set(CARD_RISK_MANA_PERFORMED);

	if(Resp_Info(*cid) == 0x01)	//Service not allowed
	{
		tvr_set(REQUESTED_SERVICE_NOT_ALLOWED);
		eTRACE("AMEX-error: CID indicate Service not allowed");
		return EMV_ENDAPPLICATION;
	}

	return EMV_OK;
}


int express_analyze_gac_resp(u8 * gac_resp, int len, bool cdaflag)
{
	int pos = 0, dlen = 0,i;
	u8 cid = 0;
	u8 * ptmp = NULL,*enc_cap = NULL,*card_itr_cap = NULL;
    EMV_TLVOBJ pobj[80];
	u8 buf[24] = {0};

    eTRACE("Call express_analyze_gac_resp");

    if (emvtlv_decode(gac_resp, len, pobj, sizeof(pobj)/sizeof(pobj[0]), SINGLE_TLVOBJ) < 0)
    {
        eTRACE( "Failed to decode tlv object string to fetch AIP and AFL");
		return AA_ERR;
	}

	if (pobj[1].tagname == TAG_0080_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1)
    {
		if (pobj[1].valuelen < 11)
        {
            eTRACE( "The length of Tag[0x80] is smaller than 11");
			return rERR(AA_GACRSP80LENERR);
		}

		if((*(pobj[1].pvalue)==0x00)
            && (*(pobj[1].pvalue + 1)==0x00)
            &&(*(pobj[1].pvalue + 2)==0x00)
            &&(*(pobj[1].pvalue + 3)==0x00)
            &&(*(pobj[1].pvalue + 4)==0x00)) //2009/08/05
        {
            eTRACE( "The length of Tag[0x80] is illegal");
			return rERR(AA_GACRSP80LENERR);
        }

		//应用密文信息
		appdata_save(TAG_9F27_CRYPTOGRAM_INFO_DATA, pobj[1].pvalue, 1, OVERRIDED);
//		if (Resp_AAR(*(pobj[1].pvalue)))
//        {
//			eTRACE("AMEX-info: CID indicate AAR\r\n");
//			return rERR(AA_CIDAAR);
//		}

		appdata_save(TAG_9F36_ATC, pobj[1].pvalue + 1, 2, OVERRIDED);
		appdata_save(TAG_9F26_APPLICATION_CRYPTOGRAM, pobj[1].pvalue + 3, 8, OVERRIDED);
		if (pobj[1].valuelen > 11)
		{
			if (pobj[1].valuelen - 11 > 32)
			{
				return AA_ERR;
			}
			appdata_save(TAG_9F10_IAD, pobj[1].pvalue + 11, pobj[1].valuelen - 11, OVERRIDED);
        }

		eTRACE("AMEX-info: cda flag = %d", cdaflag);
		return express_parse_gac_resp(pobj[1].tagname, cdaflag);
	}
    else if (pobj[1].tagname == TAG_0077_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2)
    {
		for(i = 0; i < pobj[1].childnum; i++)
		{
			express_appdata_save2(pobj + i + 2, UNIQUE);
		}

		pos = emvtlv_get(1, 0x9F4B, pobj, SEARCH_ONLY_SON);
		if(pos)
		{
			ptmp = pobj[pos - 1].pvalue + pobj[pos - 1].valuelen;
			dlen = pobj[pos].pvalue + pobj[pos].valuelen - ptmp;
			emv_memcpy(ptmp, ptmp + dlen, gac_resp + len - ptmp - dlen);
			len = pobj[1].valuelen -  dlen;
			appdata_save(TAG_CDAAUTH_VALUE, pobj[1].pvalue, len, OVERRIDED);
		}

		eTRACE("AMEX-info: cda flag = %d", cdaflag);
		return express_parse_gac_resp(pobj[1].tagname, cdaflag);
	}
    else
    {

		eTRACE("AMEX-error: GAC template != 77 or 80");

		card_itr_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);
		enc_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);

		if( (enc_cap[0] & 0x80) && \
			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
			((card_itr_cap[0] & 0x20) || (card_itr_cap == NULL)) )
		{
			eTRACE("AMEX-error: Card support other interface");
			return express_outcome_try_another_interface();
		}
		else
		{
			eTRACE("AMEX-error: Card doesn't support other interface");
			return express_outcome_request_another_payment();
		}

//		return rERR(AA_GACRSPERR);
	}


	return EMV_OK;
}



int express_deal_gac_resp(u8 *resp, int resp_len, int sw, bool cda_flag)
{
	u8 *enc_cap = NULL,*card_itr_cap = NULL;
	int ret;

	enc_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
	card_itr_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);

	if(sw == 0x6984)
	{
		if(AIP_MOB_SUPPORT())
		{
			if(g_express_cnxt->gExpressmobileagain == 0)
			{
				g_express_cnxt->gExpressmobileagain = 1;
				return EMV_REQ_SEEPHONE;
			}
		}

		if((enc_cap[0] & 0x80) && \
			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
			((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)))
		{
			return express_outcome_try_another_interface();
		}
		else
		{
			return express_outcome_request_another_payment();
		}
 	}
	else if(sw != 0x9000)
	{
		if((enc_cap[0] & 0x80) && \
			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
			((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)))
		{
			return express_outcome_try_another_interface();
		}
		else
		{
			return express_outcome_request_another_payment();
 		}
 	}

	eTRACE("AMEX-info: cda flag = %d", cda_flag);
	ret = express_analyze_gac_resp(resp, resp_len, cda_flag);
	eTRACE("AMEX-info: express_analyze_gac_resp ret = %d", ret);

	if(ret != EMV_OK)
	{
		if(ret > 100)
		{
			return rEND(ret);
		}

		if((enc_cap[0] & 0x80) && \
			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
			((card_itr_cap == NULL) || (card_itr_cap[0] & 0x20)))
		{
			return express_outcome_try_another_interface();
		}
		else
		{
			return express_outcome_request_another_payment();
 		}
	}

	return EMV_OK;
}

int express_process_cdol()
{
	u8 *col1 = NULL;
	u8 cdol1_val[EMV_CFG_ICC_RECV_MAXLEN] = {0};
	int len,cdol1vlen;

	//process CDOL
	col1 = appdata_find(TAG_008C_CDOL_1, &len);
    if(NULL == col1 || len <= 0)	//if CDOL1 miss,trans terminate
    {
		eTRACE("AMEX-error: CDOL1 absent or len invalid");
		return EMV_ENDAPPLICATION;
	}

	cdol1vlen = emvdol_packet(col1, len, cdol1_val, EMV_CFG_ICC_RECV_MAXLEN);
	if (cdol1vlen <= 0)
    {
		eTRACE("AMEX-error: pack CDOL1 error");
		return AA_CDOLPACKERR;
	}

	appdata_save(TAG_CDOL1_VALUE, cdol1_val, cdol1vlen, OVERRIDED);

	return EMV_OK;
}

int express_action_analyse()
{
	u8 *col1_v_prt = NULL;
	u8 cdol1_val[EMV_CFG_ICC_RECV_MAXLEN] = {0};
	u8 gac_resp[EMV_CFG_ICC_RECV_MAXLEN] = {0};
	u8 auth_code[2]={0};
	u8 gac_type;
	int len,cdol1vlen,sw,ret;
	bool cda_flag = FALSE;

	//process CDOL

	ret = express_process_cdol();
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

//	col1 = appdata_find(TAG_008C_CDOL_1, &len);
//    if(NULL == col1 || len <= 0)	//if CDOL1 miss,trans terminate
//    {
//		eTRACE("AMEX-error: CDOL1 absent or len invalid");
//		return rEND(EMV_ENDAPPLICATION);
//	}
//
//	cdol1vlen = emvdol_packet(col1, len, cdol1_val, EMV_CFG_ICC_RECV_MAXLEN);
//	if (cdol1vlen <= 0)
//    {
//		eTRACE("AMEX-error: pack CDOL1 error");
//		return rERR(AA_CDOLPACKERR);
//	}
//
//	appdata_save(TAG_CDOL1_VALUE, cdol1_val, cdol1vlen, UNIQUE);

	//confirm GAC command type
    gac_type = express_check_tvr(auth_code);
	eTRACE("AMEX-info: gac_type = %02x",gac_type);

	if(gac_type == AC_AAC)
	{
		appdata_save(TAG_008A_ARC, "Z1", 2, OVERRIDED);
	}
	else if(gac_type == AC_TC)
	{
		appdata_save(TAG_008A_ARC, "Y1", 2, OVERRIDED);
	}

    if (g_express_cnxt->da_cda_performed == TRUE)
    {
		// if CDA Failure,do not send CDA in GAC command
		if ((gac_type == AC_AAC) || tvr_get(CDA_FAILED))
        {
            g_express_cnxt->da_cda_performed = FALSE;
		}
        else
        {
			cda_flag = TRUE;
			tsi_set(OFFLINE_DA_PERFORMED);
		}

		if ((gac_type == AC_AAC) && !tvr_get(CDA_FAILED))
        {
			// request an AAC, and any CDA errors not detected yet
			tvr_set(OFFLINE_DA_NOT_PERFORMED);
		}
	}

	eTRACE("AMEX-info: cda flag = %d", cda_flag);
	if(g_express_cnxt->force_online)
	{
		tvr_set(MERCHANT_FORCED_TRANSACTION_ONLINE);
		gac_type = AC_ARQC;
	}

//	appdata_save(TAG_CDOL1_VALUE, cdol1_val, cdol1vlen, UNIQUE);
	if(cda_flag)
	{
		gac_type |= 0x10;
	}

	appdata_save(TAG_GAC_REQUEST_TYPE, &gac_type, 1, UNIQUE);

	//pack cdol again for if cdol contains tag 8A
	ret = express_process_cdol();
	col1_v_prt = appdata_find(TAG_CDOL1_VALUE, &cdol1vlen);
	emv_memcpy(cdol1_val, col1_v_prt, cdol1vlen);

	sw = emvicc_generate_ac(gac_type, cda_flag, cdol1_val, cdol1vlen, gac_resp, &len);

	if(sw == 0x0000)
	{
		return EMV_APDU_TIMEOUT;
	}

	ret = express_deal_gac_resp(gac_resp, len, sw, cda_flag);
	eTRACE("AMEX-info: express_deal_gac_resp ret = %d", ret);
	if(ret < EMV_OK)
	{
		return rEND(EMV_ENDAPPLICATION);
	}
	else if(ret >= EMV_COMPLETED)
	{
		return rEND(ret);
	}
	else if(ret > EMV_OK && ret < EMV_COMPLETED)
	{
		return rNXT(ret);
	}

	return rNXT(EMV_STA_ACTION_ANALYSE_COMPLETED);
}



//int emv_action_analyse(void)
//{
//	u8 genactype;
//	u8 authcode[2+1] = {0};
//	u8 buf[128] = {0};
//    int   cdol1vlen = 0, len = 0, tmplen = 0, cdaflag = 0, ret = 0, i = 0;
//	u8 * ptmp = NULL;
//	u8 * pcid = NULL;
//    u8 * pdata = NULL;
//    u8  advice_indicator = 0;
//    unsigned char CardInfAndPayCapab[2],EnhancedClessReaderCapab[4];
//	u8 * pdata_CardInfAndPayCapab = NULL,*pdata_EnhancedClessReaderCapab= NULL;
//	u8 * pATC = NULL;
//	int gret = 0;
//	u8 cdol1val[EMV_CFG_ICC_RECV_MAXLEN] = {0};
//	u8 gac_resp[EMV_CFG_ICC_RECV_MAXLEN] = {0};
//	int gac_resp_no9F4B_len;
//
//    emv_memset(CardInfAndPayCapab,0,sizeof(CardInfAndPayCapab));
//	pdata_CardInfAndPayCapab = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP,&len);
//
//	if((pdata_CardInfAndPayCapab !=NULL) && (len >0))
//	{
//		emv_memcpy(CardInfAndPayCapab,pdata_CardInfAndPayCapab,len);
//	}
//
//    emv_memset(EnhancedClessReaderCapab,0,sizeof(EnhancedClessReaderCapab));
//	pdata_EnhancedClessReaderCapab = appdata_find(TAG_EnhancedClessReaderCapab,&len);
//
//	if((pdata_EnhancedClessReaderCapab !=NULL) && (len >0))
//	{
//		emv_memcpy(EnhancedClessReaderCapab,pdata_EnhancedClessReaderCapab,len);
//	}
//
//	eTRACE("emv_action_analyse-------->");
//    ptmp = appdata_find(0x8C, &len);
//
//    if(NULL == ptmp || len <= 0)	//if CDOL1 miss,trans terminate
//    {
//		return EMV_ENDAPPLICATION;
//	}
//
//	//CDOL Process
//	cdol1vlen = emvdol_packet(ptmp, len, cdol1val, EMV_CFG_ICC_RECV_MAXLEN);
//	if (cdol1vlen < 0)
//    {
//		gret = AA_CDOLPACKERR;
//        goto __call_return;
//	}
//
////	express_calc_tc_hash(ptmp, len);
//
//    genactype = express_check_tvr(authcode);
//	eTRACE("genactype = %x",genactype);
//
//	if (genactype != AC_ARQC)
//    {
//		appdata_save(0x8A, authcode, 2, UNIQUE);
//	}
//
//    if (g_express_cnxt->da_cda_performed == TRUE)
//    {
//		// if we can detect the CDA Failure, we shuold check tvr bits,
//		// for we performed CDA preprocessing before TAA
//		if ((genactype == AC_AAC) || tvr_get(CDA_FAILED))
//        {
//			// if CDA has failed or we request an AAC,  CDA is not required again
//            g_express_cnxt->da_cda_performed = FALSE;
//		}
//        else
//        {
//			cdaflag = 1;
//			tsi_set(OFFLINE_DA_PERFORMED); //EMV4.2C
//		}
//
//		if ((genactype == AC_AAC) && !tvr_get(CDA_FAILED))
//        {
//			// request an AAC, and any CDA errors not detected yet
//			tvr_set(OFFLINE_DA_NOT_PERFORMED);
//		}
//	}
//
//	appdata_save(TAG_CDOL1_VALUE, cdol1val, cdol1vlen, UNIQUE);
//
//	ret = emvicc_generate_ac(genactype, cdaflag, cdol1val, cdol1vlen, gac_resp, &len);
//
//	if(ret == 0)
//	{
//		return EMV_PAYPASS_TRY_AGAIN;
//	}
//	else if(ret == 0x6984)
//    {
//        if(AIP_MOB_SUPPORT())  // Mobile
//        {
//            if(g_express_cnxt->gExpressmobileagain == 0)//只第一次Try Again
//            {
//                g_express_cnxt->gExpressmobileagain = 1;
//                return EMV_REQ_SEEPHONE; //?????? Try Again 如何处理
//            }
//            else
//            {
//                return AA_ERR;
//			}
//        }
//        if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) &&( g_express_cnxt->msdoremv == EMV_MODE)&&((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//        {
//            return EMV_SW_INTERFACE;
//        }
//
//        return AA_ERR;
//    }
//    else if(ret != 0x9000)
//    {
//        if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) && ( g_express_cnxt->msdoremv == EMV_MODE)&&((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//        {
//            return EMV_SW_INTERFACE;
//        }
//        return AA_ERR;
//    }
//
//	tsi_set(CARD_RISK_MANA_PERFORMED);
//	ret = express_analyze_gac_resp(gac_resp, &len, cdaflag);
//	if (ret < 0)
//    {
//        eTRACE( "express_analyze_gac_resp return [%04X]", ret);
//		pcid = appdata_find(TAG_9F27_CRYPTOGRAM_INFO_DATA, NULL);
//		pATC = appdata_find(TAG_9F36_ATC, NULL);
//
//		if((pcid == NULL)||(pATC == NULL))
//		{
//			if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) && (g_express_cnxt->msdoremv == EMV_MODE) && ((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//			{
//				return EMV_SW_INTERFACE;
//			}
//		}
//
//		gret=ret;
//        goto __call_return;
//	}
//	else if(EMV_DENIALED_OFFLINE == ret)
//	{
//		return ret;
//	}
//
//	gac_resp_no9F4B_len = len;
//	eTRACE("AMEX-info: gac resp(no 9F4B) len = %d", gac_resp_no9F4B_len);
//
//
//	pcid = appdata_find(0x9F27, NULL);
//
//	// check the CID logic against the requested one
//	if ((Resp_TC(*pcid) && genactype < AC_TC) ||(!Resp_AAC(*pcid) && genactype == AC_AAC))
//    {
//			gret = AA_ERR;
//            goto __call_return;
//	}
//
//	if (express_cid_msg(*pcid) < 0)
//    {
//		eTRACE( "EMV_TERMINATE");
//		gret = EMV_TERMINATE;
//        goto __call_return;
//	}
//
//    if (Resp_Advice(*pcid))
//    {
//        advice_indicator = 1;
//    }
//
//    appdata_save(TAG_ADVICE_INDICATOR, &advice_indicator, 1, UNIQUE);
//
//    if(Resp_TC(*pcid)&&(genactype&AC_TC))
//    {
//
//    }
//    else if(Resp_ARQC(*pcid)&&((genactype&AC_TC) || (genactype&AC_ARQC)))
//    {
//
//    }
//	else if (Resp_AAC(*pcid))
//    {
//		eTRACE( "EMV_DENIAL");
//		appdata_save(0x8A, (u8 *)"Z1", 2, OVERRIDED);
//		gret = EMV_DENIALED_OFFLINE;
//        goto __call_return;
//	}
//	else
//	{
//		if(!Resp_TC(*pcid) && !Resp_AAC(*pcid) && !Resp_ARQC(*pcid) )
//		{
//			if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) && (g_express_cnxt->msdoremv == EMV_MODE) && ((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//			{
//				return EMV_SW_INTERFACE;
//			}
//			gret = AA_ERR;
//            goto __call_return;
//		}
//		else
//		{
//			return EMV_DENIALED_OFFLINE;
//		}
//	}
//
//    ptmp = appdata_find(TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA, &len);
//	if(ptmp == NULL && len == 0)
//	{
//		ptmp = appdata_find(TAG_9F26_APPLICATION_CRYPTOGRAM, &len);
//		if(ptmp == NULL && len == 0)
//		{
//			if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) && (g_express_cnxt->msdoremv == EMV_MODE) && ((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//			{
//				return EMV_SW_INTERFACE;
//			}
//		}
//
//		if(Resp_AAC(*pcid)) //AAC
//		{
//			if(genactype == AC_AAC)
//			{
//				if(cdaflag)
//				{
//					if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) && (g_express_cnxt->msdoremv == EMV_MODE) && ((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//					{
//						return EMV_SW_INTERFACE;
//					}
//				}
//			}
//		}
//		else
//		{
//			if(cdaflag)
//			{
//				if((EnhancedClessReaderCapab[0]&0x80)&& (!AIP_MOB_SUPPORT()) && (g_express_cnxt->msdoremv == EMV_MODE) && ((CardInfAndPayCapab[0]&0x20) ||(pdata_CardInfAndPayCapab == NULL)))
//				{
//					return EMV_SW_INTERFACE;
//				}
//			}
//		}
//	}
//
//	// now peform the CDA veification, only for TC or ARQC
//	if (cdaflag)
//    {
//        eTRACE("AMEX-info:start perfrom CDA");
//		if ((ret = express_process_cda(0, gac_resp, gac_resp_no9F4B_len)) < 0)
//        {
//			tvr_set(CDA_FAILED);
//
//			ptmp = appdata_find(TAG_9F27_CRYPTOGRAM_INFO_DATA, &len);
//			if(NULL == ptmp)
//			{
//				eTRACE("AMEX-error: GAC response dont contain CID(9F27)\r\n");
//				return rERR(AA_GACRSP77NOCID);
//			}
//
//			if(DA_NOSDAD == ret)
//			{
//				eTRACE("AMEX-error: GAC response has no 9F4B\r\n");
//
//				if(0x00 == (ptmp[0] & 0xC0))
//				{
//					if(AC_AAC == genactype) //Request CDA and Card return AAC
//					{
//						return rERR(DA_ERR);
//					}
//				}
//				else
//				{
//					return rERR(DA_ERR);
//				}
//			}
//			else if(DA_CDAHASHV2ERR == ret)
//			{
//				eTRACE("AMEX-error: CDA Hash verify failed\r\n");
//				if(((0x40 == (ptmp[0] & 0xC0)) && (AC_TC == genactype)) || ((0x80 == (ptmp[0] & 0xC0)) &&(AC_ARQC == genactype || AC_TC == genactype)))	//if trans result is offline approve or online deal when CDA signature verify failed,trans should be declined
//				{
//					eTRACE("AMEX-info: Trans Offline Declined\r\n");
//					return rEND(EMV_DENIALED_OFFLINE);
//				}
//			}
//			else	//code by wangzhen
//			{
//				pcid = gac_resp;
//				if(genactype == AC_ARQC)
//				{
//					appdata_save(0x8A, (u8 *)"Z1", 2, OVERRIDED);
//				}
//				else if(Resp_TC(*pcid))
//				{
//					eTRACE( "EMV_DENIAL");
//					gret = EMV_DENIALED_OFFLINE;
//					goto __call_return;
//				}
//				else
//				{
//					eTRACE( "EMV_2GAC_AAC");
//					gret = _EMV_2GAC_AAC;
//					goto __call_return;
//				}
//			}
//		}
//	}
//
//	if (Resp_TC(*pcid))
//    {
//		eTRACE( "EMV_ACCEPT");
//		appdata_save(0x8A, (u8 *)"Y1", 2, OVERRIDED);	//FQ ADD
//		gret = EMV_ACCEPTED_OFFLINE;
//        goto __call_return;
//	}
//	else
//	{
//	    ret = EMVPORT_GoOnline(gac_resp, &len);
//	    if (ret == EMV_OPER_REPORT_APP)
//	    {
//	        g_express_cnxt->online_result = EMV_ONLINE_NOT;
//	        return rNXT(EMV_REQ_GO_ONLINE);
//	    }
//	    else
//	    {
//	        g_express_cnxt->online_result = ret;
//
//	        if (g_express_cnxt->online_result == EMV_ONLINE_SUCC_ACCEPT
//	                || g_express_cnxt->online_result == EMV_ONLINE_SUCC_ISSREF
//	                || g_express_cnxt->online_result == EMV_ONLINE_SUCC_DENIAL)
//	        {
//	            if (gac_resp > 0)
//	            {
//	                int i;
//	                u32 tag1 = TAG_ISSUER_SCRIPT_71_BASE;
//	                u32 tag2 = TAG_ISSUER_SCRIPT_72_BASE;
//	                EMV_TLVOBJ tlvs[64];
//
//	                if (emvtlv_decode((u8*)gac_resp, len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), DECODE_LEVEL1) == 0)
//	                {
//	                    for (i = 1; i <= tlvs[0].childnum; i++)
//	                    {
//	                        if (tlvs[i].tagname == TAG_0071_ISSUER_SCRIPT_TEMPLATE_1)
//	                            tlvs[i].tagname = tag1++;
//	                        else if (tlvs[i].tagname == TAG_0072_ISSUER_SCRIPT_TEMPLATE_2)
//	                            tlvs[i].tagname = tag2++;
//	                        express_appdata_save2(tlvs + i, OVERRIDED);
//	                    }
//	                }
//	            }
//	        }
//
//	        return rNXT(EMV_STA_ACTION_ANALYSE_COMPLETED);
//	    }
//	}
//
//__call_return:
//    eTRACE( "action_analyze ret = %02x",gret);
//
//	if(gret==0)
//	{
//	    return rNXT(EMV_STA_ACTION_ANALYSE_COMPLETED);
//	}
//
//	return gret;
//
//}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
