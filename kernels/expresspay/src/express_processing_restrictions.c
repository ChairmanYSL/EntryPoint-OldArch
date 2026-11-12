/**
************************************************************************************************************************
* @file     express_processing_restrictions.c
* @brief    Expresspay 内核：处理限制（Processing Restrictions）+ DRL + 9F6D/9F6E 能力位
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_processing_restrictions.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     express_processing_restrictions.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015锟斤拷10锟斤拷21锟斤拷
* @brief    锟斤拷锟斤拷锟斤拷锟狡ｏ拷Processing Restrictions锟斤拷
*
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "express_priv.h"
/* 0xFF x6 表示"该 DRL 限额无效"（美运 emv_processing_restrictions.c:98/109/122 等）。
 * 用数组而不用字符串常量：避免在 COFF 下与 libentrypoint.a 产生同名 ??_C@ 字符串符号。 */
static const u8 k_drl_invalid[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


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




int express_is_a_atm(void)
{
	u8 * pdata = appdata_find(0x9F35, NULL);
	if (*pdata == 0x14 || *pdata == 0x15 || *pdata == 0x16)
    {
		pdata = appdata_find(0x9F40, NULL);
		if (*pdata&ATC_Cash)
        {
			return 1;
		}
	}
	return 0;
}

void express_get_dynamic_reader_limit(SDK_EXPRESS_DRL_PARAMS * drl_params)
{
	BCTC_DRL drl={0};
	int i = 0;

	if(express_drl_get(&drl,0x80) == 0)	//get default DRL first
	{
		drl_params->defaultDRL.ifenable = 1;
		drl_params->defaultDRL.AppProgID = 0;
		if(emv_memcmp(drl.CVMLimit,k_drl_invalid,6) == 0)
		{
			drl_params->defaultDRL.SupportCVMLimitCheck = 0;
			drl_params->defaultDRL.CVMLimitlen = 0;
		}
		else
		{
			drl_params->defaultDRL.SupportCVMLimitCheck = 1;
			drl_params->defaultDRL.CVMLimitlen = 6;
			emv_memcpy(drl_params->defaultDRL.CVMLimit,drl.CVMLimit,6);
		}
		if(emv_memcmp(drl.CLROFFLimit,k_drl_invalid,6) == 0)
		{
			drl_params->defaultDRL.SupportFloorLimitCheck = 0;
			drl_params->defaultDRL.FloorLimitlen = 0;
		}
		else
		{
			drl_params->defaultDRL.SupportFloorLimitCheck = 1;
			drl_params->defaultDRL.FloorLimitlen = 6;


			emv_memcpy(drl_params->defaultDRL.FloorLimit,drl.CLROFFLimit,6);
		}
		if(emv_memcmp(drl.CLRTLimit,k_drl_invalid,6) == 0)
		{
			drl_params->defaultDRL.SupportRCTLCheck = 0;
			drl_params->defaultDRL.TransLimitlen = 0;
		}
		else
		{
			drl_params->defaultDRL.SupportRCTLCheck = 1;
			drl_params->defaultDRL.TransLimitlen = 6;

			emv_memcpy(drl_params->defaultDRL.TransLimit,drl.CLRTLimit,6);
		}
	}
	else
	{
		drl_params->defaultDRL.ifenable = 0;
	}

//	eTRACE_HEX("AMEX-info :drl.CVMLimit in BCD", drl.CVMLimit, 6);
//
//	eTRACE_HEX("AMEX-info :drl.CLRTLimit in BCD", drl.CLRTLimit, 6);
//
//	eTRACE_HEX("AMEX-info :drl.CLROFFLimit in BCD", drl.CLROFFLimit, 6);

	for(i=0;i<EXPRESS_DRL_NUM;i++)
	{
		emv_memset(&drl,0,sizeof(BCTC_DRL));
		if(express_drl_get(&drl,i) == 0)
		{
			drl_params->DRLs[i].ifenable = 1;
			drl_params->DRLs[i].AppProgID = i;

			eTRACE("AMEX-info :drl.AppProgID: %d", drl_params->DRLs[i].AppProgID);
			if(emv_memcmp(drl.CVMLimit,k_drl_invalid,6) == 0)
			{
				drl_params->DRLs[i].SupportCVMLimitCheck = 0;
				drl_params->DRLs[i].CVMLimitlen = 0;
			}
			else
			{
				drl_params->DRLs[i].SupportCVMLimitCheck = 1;
				drl_params->DRLs[i].CVMLimitlen = 6;

				emv_memcpy(drl_params->DRLs[i].CVMLimit,drl.CVMLimit,6);
			}
			if(emv_memcmp(drl.CLROFFLimit,k_drl_invalid,6) == 0)
			{
				drl_params->DRLs[i].SupportFloorLimitCheck = 0;
				drl_params->DRLs[i].FloorLimitlen = 0;
			}
			else
			{
				drl_params->DRLs[i].SupportFloorLimitCheck = 1;
				drl_params->DRLs[i].FloorLimitlen = 6;

				emv_memcpy(drl_params->DRLs[i].FloorLimit,drl.CLROFFLimit,6);
			}
			if(emv_memcmp(drl.CLRTLimit,k_drl_invalid,6) == 0)
			{
				drl_params->DRLs[i].SupportRCTLCheck = 0;
				drl_params->DRLs[i].TransLimitlen = 0;
			}
			else
			{
				drl_params->DRLs[i].SupportRCTLCheck = 1;
				drl_params->DRLs[i].TransLimitlen = 6;

				emv_memcpy(drl_params->DRLs[i].TransLimit,drl.CLRTLimit,6);
			}
		}
		else
		{
			drl_params->DRLs[i].ifenable = 0;
		}
	}
}

int express_again_drl_preprocess(SDK_EXPRESS_DRL * drl)
{
	eTRACE("AMEX-info :g_express_cnxt->trans_amount: %d", g_express_cnxt->trans_amount);

	eTRACE("AMEX-info :drl->SupportCVMLimitCheck: %d", drl->SupportCVMLimitCheck);
	eTRACE_HEX("AMEX-info :drl->CVMLimit in BCD", drl->CVMLimit, 6);

	eTRACE("AMEX-info :drl->SupportRCTLCheck: %d", drl->SupportRCTLCheck);
	eTRACE_HEX("AMEX-info :drl->TransLimit in BCD", drl->TransLimit, 6);

	eTRACE("AMEX-info :drl->SupportFloorLimitCheck: %d", drl->SupportFloorLimitCheck);
	eTRACE_HEX("AMEX-info :drl->FloorLimit in BCD", drl->FloorLimit, 6);

	if (drl->SupportRCTLCheck)
	{
		if (g_express_cnxt->trans_amount >= emv_bcd2dec(drl->TransLimit, 6))
		{
			g_express_cnxt->CANAI = 1;
		}
	}

	if (drl->SupportFloorLimitCheck)
	{
		if (g_express_cnxt->trans_amount > emv_bcd2dec(drl->FloorLimit, 6))
		{
			g_express_cnxt->TermEFLEF = 1;
		}
	}

	if (drl->SupportCVMLimitCheck)
	{
		if (g_express_cnxt->trans_amount >= emv_bcd2dec(drl->CVMLimit, 6))
		{
			g_express_cnxt->CVMRLEF = 1;
		}
	}
	return 0;
}
unsigned char express_set_ttq(void)
{
    unsigned char k;
    unsigned char tempTermTransPredicable;

    if(g_express_cnxt->CVMRLEF == 1)
    {
        appdata_save_or(TAG_ClessReaderCapab,0,0x08);
        appdata_save_or(TAG_EnhancedClessReaderCapab,2,0x40);
    }

    return EMV_OK;
}


static int express_dynamic_reader_limit(void)
{
	u8 * pdataEnhance = NULL;
	u8 * pdataCardCap = NULL;
	int len;
    u8 EnhancedClessReaderCapab[4];
	u8 CardInfAndPayCapab[2];
	SDK_EXPRESS_DRL_PARAMS  drl_params;
	int i=0,match =0;
	int ret;
    SDK_EXPRESS_DRL stsdkExpressDrl;

    eTRACE("AMEX-info :Start DRL Process");
	express_get_dynamic_reader_limit(&drl_params);

	pdataCardCap= appdata_find(0x9F70,&len);
	if(pdataCardCap != NULL)
		emv_memcpy(CardInfAndPayCapab,pdataCardCap,len);

	eTRACE("\r\n drl_params.defaultDRL.ifenable %d\r\n",drl_params.defaultDRL.ifenable);
	if(drl_params.defaultDRL.ifenable == 0)
	{
        eTRACE("\r\n user AID Reader Limits\r\n");
		if(g_express_cnxt->CANAI)
		{
			emv_memset(EnhancedClessReaderCapab,0,sizeof(EnhancedClessReaderCapab));
			pdataEnhance= appdata_find(TAG_EnhancedClessReaderCapab,&len);
			if((pdataEnhance != NULL) && (len>0))
				emv_memcpy(EnhancedClessReaderCapab,pdataEnhance,len);

			eTRACE("\r\n user AID Reader Limits\r\n");
			if((EnhancedClessReaderCapab[0]&0x80)&&(!AIP_MOB_SUPPORT())&&(g_express_cnxt->msdoremv == EMV_MODE)&&((CardInfAndPayCapab[0]&0x20) ||(pdataCardCap == NULL)))//for 4.1--7.2.2
            {
				return express_outcome_try_another_interface();
				return EMV_SW_INTERFACE;
			}
			else
			{
				return EMV_FAIL;
			}
		}
        return EMV_OK;
	}

    if(pdataCardCap !=NULL)
    {
        eTRACE_HEX("EMVTAG_CardInfAndPayCapab", CardInfAndPayCapab, 2);
        for(i = 0; i < EXPRESS_DRL_NUM; i++)
        {
			if(drl_params.DRLs[i].ifenable == 1)
			{
				eTRACE("AMEX-info : compare drl_params.AppProgID: %d", drl_params.DRLs[i].AppProgID);
			}
            if((drl_params.DRLs[i].ifenable == 1)&&((CardInfAndPayCapab[1]&0x0F) == drl_params.DRLs[i].AppProgID))
            {
                emv_memcpy(&stsdkExpressDrl,&drl_params.DRLs[i],sizeof(SDK_EXPRESS_DRL));
                match = 1;
                eTRACE("\r\n DRL match SUCCESS\r\n");
                eTRACE("AMEX-info : match index: %d", i);
                break;
            }
        }
    }
	g_express_cnxt->CVMRLEF = 0;
	g_express_cnxt->TermCFLEF = 0;
	g_express_cnxt->TermEFLEF = 0;
	g_express_cnxt->CANAI = 0;
	appdata_save_and(TAG_ClessReaderCapab,0,0xF7);   //要锟斤拷锟斤拷锟叫讹拷锟斤拷锟斤拷锟斤拷位20161205
    appdata_save_and(TAG_EnhancedClessReaderCapab,2,0xBF);

	eTRACE("AMEX-info: match flag = %d\r\n", match);

	if(match)
	{
		ret = express_again_drl_preprocess(&stsdkExpressDrl);
    }
	else
	{
		ret = express_again_drl_preprocess(&drl_params.defaultDRL);
	}

	eTRACE("AMEX-info: g_express_cnxt->TermEFLEF = %d\r\n", g_express_cnxt->TermEFLEF);
	eTRACE("AMEX-info: g_express_cnxt->CVMRLEF = %d\r\n", g_express_cnxt->CVMRLEF);
	eTRACE("AMEX-info: g_express_cnxt->CANAI = %d\r\n", g_express_cnxt->CANAI);

    express_set_ttq();
	emv_memset(EnhancedClessReaderCapab,0,sizeof(EnhancedClessReaderCapab));
    pdataEnhance= appdata_find(TAG_EnhancedClessReaderCapab,&len);
	if((pdataEnhance != NULL) && (len>0))
		emv_memcpy(EnhancedClessReaderCapab,pdataEnhance,len);

	if(g_express_cnxt->CANAI)
	{
		if((EnhancedClessReaderCapab[0]&0x80)&&(!AIP_MOB_SUPPORT())&&(g_express_cnxt->msdoremv == EMV_MODE)&&((CardInfAndPayCapab[0]&0x20) ||(pdataCardCap == NULL)))//for 4.1--7.2.2
        {
			return EMV_SW_INTERFACE;
		}
		else
		{
			return EMV_FAIL;
		}
	}


	return EMV_OK;
}
static const u32 ExpresspayMSDmandatoryTag[] =
{
    TAG_5F25_APPLICATION_EFFECTIVE_DATE,
    TAG_5F20_CARDHOLDER_NAME,
    TAG_0057_TRACK2_EQUIVALENT_DATA,
    TAG_9F36_ATC,
    0
};

int express_check_data_miss_msd(void)
{
    int i=0;
	int len =0;

    while(1)
    {
        if(ExpresspayMSDmandatoryTag[i] == 0)
        {
            break;
        }

        if(	(appdata_find(ExpresspayMSDmandatoryTag[i],&len) == NULL) && (len == 0))
        {
            return EMV_FAIL;
        }

        i++;
    }
    return EMV_OK;
}


unsigned char express_process_restrict(void)
{
	unsigned char asPAN[20] = {0};
    int retCode;


    if(g_express_cnxt->msdoremv == MSD_MODE) // 锟斤拷锟斤拷模式Magstripe Mode transaction
    {
        if(express_check_data_miss_msd()!=EMV_OK)
        {
			tvr_set(ICC_DATA_MISSING);
            return EMV_FAIL;
        }
    }

    return EMV_OK;
}

int express_processing_restrictions(void)
{
	u8 * pdata = NULL, * pdata2 = NULL, * pdata3 = NULL, * pdata4 = NULL,*pdataEnhance=NULL;
	int invalid = 0;
	int ret = 0;
	int len;
    u8 *EnhancedClessReaderCapab = NULL;
	u8 *issuer_contry_code=NULL,*term_contry_code=NULL,*card_itrface_cap=NULL;


    eTRACE("AMEX-info :Start process_restrictions");
	// Application Version Number

	if ((pdata = appdata_find(TAG_9F08_APPLICATION_VERSION_NUMBER_CARD, NULL)) != NULL &&
		(pdata2 = appdata_find(TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, NULL)) != NULL)
	{
		if (emv_memcmp(pdata, pdata2, 2) != 0)
        {
			tvr_set(APP_VER_NOT_MATCHED);
            eTRACE( "Application Version Number isn`t identical");
		}
	}
	// Application Usage Control
	if ((pdata = appdata_find(TAG_9F07_AUC, NULL)))
    {
		if (express_is_a_atm())
        {
			if (!AUC_For_ATMs(pdata))
            {
				invalid = 1;
                eTRACE( "ATM, No AUC_For_ATMs");
			}
		}
        else
        {
			if (!AUC_For_Not_ATMs(pdata))
            {
                eTRACE( "Not ATM, No AUC_For_Not_ATMs");
				invalid = 1;
			}
		}
		if (!invalid)
        {
			pdata2 = appdata_find(TAG_5F28_ISSUER_COUNTRY_CODE, NULL);

//			eTRACE("AMEX-info: 5F28 addr = %p", pdata2);
			if(pdata2 != NULL)
            {
				// Terminal Country Code and Transaction Type will be set by the POS,
				// so they always exist
				pdata3 = appdata_find(TAG_9F1A_TERMINAL_COUNTRY_CODE, NULL);
				pdata4 = appdata_find(TAG_009C_TRANSACTION_TYPE, NULL);

//                eTRACE("AMEX-info: 9F1A addr = %p", pdata3);

                // domestic transaction
				if (!emv_memcmp(pdata3, pdata2, 2))
                {
					switch (*pdata4)
                    {
					    case TRANS_CASH:
                        {
						    if (!AUC_For_Domestic_Cash(pdata))
                            {
                                eTRACE( "Terminal Country Code is identical, No AUC_For_Domestic_Cash");
							    invalid = 1;
						    }
						    break;
                        }
    					case TRANS_CASHBACK:
                        {
    						if (!AUC_For_Domestic_CashBack(pdata))
                            {
                                eTRACE( "Terminal Country Code is identical, No AUC_For_Domestic_CashBack");
    							invalid = 1;
    						}
    						break;
                        }
    					case TRANS_GOODS_SERVICE:
                        {
    						if (!AUC_For_Domestic_Goods_or_Services(pdata))
                            {
                                eTRACE( "Terminal Country Code is identical, No AUC_For_Domestic_Goods_or_Services");
    							invalid = 1;
    						}
    						break;
                        }
					default:
						break;
					}
				}
                else
				{
				    // international transaction
					switch (*pdata4)
                    {
    					case TRANS_CASH:
                        {
    						if (!AUC_For_International_Cash(pdata))
                            {
                                eTRACE( "Terminal Country Code isn`t identical, No AUC_For_International_Cash");
    							invalid = 1;
    						}
    						break;
                        }
    					case TRANS_CASHBACK:
                        {
    						if (!AUC_For_International_CashBack(pdata))
                            {
                                eTRACE( "Terminal Country Code isn`t identical, No AUC_For_International_CashBack");
    							invalid = 1;
    						}
    						break;
                        }
    					case TRANS_GOODS_SERVICE:
                        {
    						if (!AUC_For_International_Goods_or_Services(pdata))
                            {
                                eTRACE( "Terminal Country Code isn`t identical, No AUC_For_International_Goods_or_Services");
    							invalid = 1;
    						}
    						break;
                        }
    					default:
    						break;
					}
				}
			}
		}
	}
	if (invalid)
    {
		tvr_set(REQUESTED_SERVICE_NOT_ALLOWED);
        eTRACE( "Set TVR[Requested service not allowed]");
	}
	pdata = appdata_find(TAG_5F25_APPLICATION_EFFECTIVE_DATE, NULL);
	if(pdata != NULL)
	{
		ret = emv_date_compare(pdata, 3);
        eTRACE("AMEX-info: 5F25 compare result: %d", ret);
		if(ret > 0)
		{
			tvr_set(APP_NOT_EFFECTIVE);
		}
	}

//	if (pdata && emv_date_compare(pdata, 3) > 0)
//	{
//		tvr_set(APP_NOT_EFFECTIVE);
//	}

	/* Application Expiration Date must be existing */
	pdata = appdata_find(TAG_5F24_APPLICATION_EXPIRATION_DATE, NULL);
	if (emv_date_compare(pdata, 3) < 0)
	{
		tvr_set(EXPIRED_APP);
	}

	//TODO:Excption file check

	//Section 7.2.4 Supplementary Processing Restrictions
	card_itrface_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, &len);
	if((card_itrface_cap != NULL) && (len > 0))
	{
		EnhancedClessReaderCapab = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
		if((EnhancedClessReaderCapab[3] & 0x40) &&(card_itrface_cap[1] & 0x80))
		{
			issuer_contry_code = appdata_find(TAG_5F28_ISSUER_COUNTRY_CODE, NULL);
			term_contry_code = appdata_find(TAG_9F1A_TERMINAL_COUNTRY_CODE, NULL);

//			eTRACE("AMEX-info: 5F28 addr = %p", issuer_contry_code);
//			eTRACE("AMEX-info: 9F1A addr = %p", term_contry_code);

			invalid = 0;
			if(!emv_memcmp(issuer_contry_code, term_contry_code, 2))
			{
				if((card_itrface_cap[1] & 0x40) != 0x40)
				{
					invalid = 1;
				}
			}
			else
			{
				if((card_itrface_cap[1] & 0x20) != 0x20)
				{
					invalid = 1;
				}
			}
			if(invalid)
			{
				tvr_set(REQUESTED_SERVICE_NOT_ALLOWED);
				eTRACE( "Set TVR[Requested service not allowed]");
			}
		}
	}

    return rNXT(EMV_STA_PROCESS_RESTRICT_COMPLETED);
}
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
