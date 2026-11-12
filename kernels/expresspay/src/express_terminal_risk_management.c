/**
************************************************************************************************************************
* @file     express_terminal_risk_management.c
* @brief    Expresspay 内核：终端风险管理（Terminal Risk Management）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_terminal_risk_management.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     emv_terminal_risk_management.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    终端风险管理（Terminal Risk Management）
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



static int express_fetch_atc(u32 * natc, u32 * nloatc)
{
	int ret = 0, len = 0;
	int res = 0;
	EMV_TLVOBJ pobj[10];
	u8 buf[EMV_CFG_ICC_RECV_MAXLEN] = {0};

	ret = emvicc_get_data(TAG_9F36_ATC, buf, &len);
	if (ret == 0x9000)
    {
		if (emvtlv_decode(buf, len, pobj, 10, SINGLE_TLVOBJ) == 0)
        {
			if ((pobj[1].tagname == 0x9F36) && (pobj[1].valuelen == 2))
            {
				*natc = ((u32)(*(pobj[1].pvalue)) << 8) + (u32)(*(pobj[1].pvalue + 1));
				res |= 0x01;
			}
		}
	}
	ret = emvicc_get_data(TAG_9F13_LAST_ONLINE_ATC_REGISTER, buf, &len);
	if (ret == 0x9000)
    {
		if (emvtlv_decode(buf, len, pobj, 10, SINGLE_TLVOBJ) == 0)
        {
			if ((pobj[1].tagname == 0x9F13) && (pobj[1].valuelen == 2))
            {
				*nloatc = ((u32)(*(pobj[1].pvalue)) << 8) + (u32)(*(pobj[1].pvalue + 1));
				res |= 0x02;
			}
		}
	}
	return res;
}

int express_terminal_risk_manage(void)
{
	u8 *pan = NULL;
	u8 *ptmp = NULL;
	int panlen = 0;

    eTRACE("AMEX-info: Start terminal_risk_manage");

	// regardless of the coding of the card’s Application Interchange Profile bit setting
	if(g_express_cnxt->term.is_support_excetionfile)
	{
		eTRACE("AMEX-info: ICS support ExceptionFile Check");
        ptmp = appdata_find(TAG_5F34_APPLICATION_PAN_SEQUENCE, NULL);
		pan = appdata_find(TAG_005A_APPLICATION_PAN, &panlen);
		if(pan != NULL && panlen > 0)
		{
			if(ptmp == NULL)
			{
				if (EMVPORT_FndBlackCard(pan, panlen, 0x00))
				{
					eTRACE("AMEX-info: Card Num in Exception File");
					tvr_set(CARD_ON_EXCEPTION_FILE);
				}
			}
			else
			{
				if (EMVPORT_FndBlackCard(pan, panlen, *ptmp))
				{
					eTRACE("AMEX-info: Card Num in Exception File");
					tvr_set(CARD_ON_EXCEPTION_FILE);
				}
			}
		}
	}

	eTRACE("AMEX-info: g_express_cnxt->TermEFLEF = %d\r\n", g_express_cnxt->TermEFLEF);
	if(g_express_cnxt->TermEFLEF)
	{
		eTRACE("AMEX-info: Amount exceed Floor Limit\r\n");
		tvr_set(TRANSACTION_EXCEEDS_FLOOR_LIMIT);
	}

	tsi_set(TERM_RISK_MANA_PERFORMED);

	ptmp = appdata_find(TAG_0095_TVR, &panlen);
	eTRACE_HEX("AMEX-info: after Term Risk Manage TVR", ptmp, panlen);

    return rNXT(EMV_STA_RISK_MANAGEMENT_COMPLETED);
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
