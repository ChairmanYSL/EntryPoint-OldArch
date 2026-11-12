/**
************************************************************************************************************************
* @file     express_issuer_script_processing.c
* @brief    Expresspay 内核：发卡行脚本处理（Issuer Script Processing）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_issuer_script_processing.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     emv_issuer_script_processing.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    发卡行脚本处理
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

#define MAX_SCRIPT 20


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

int express_script_process(int when, u8* res, int* reslen)
{
	u32 tagname = 0;
	u8 * pdata = NULL;
	int len = 0, totallen = 0, script_i = 0, i = 0, ret = 0;
	int failflg = 0;
    u8 *u9C = NULL;
	EMV_TLVOBJ pobj[MAX_SCRIPT + 4];
	u8 script_rest[MAX_SCRIPT * 5] = {0};
	u8 resp[EMV_CFG_ICC_RECV_MAXLEN] = {0};

	*reslen = 0;
	if (when == 0) 
    {
		tagname = TAG_0071_ISSUER_SCRIPT_TEMPLATE_1; 
	} 
    else
    {
		tagname = TAG_0072_ISSUER_SCRIPT_TEMPLATE_2; 
	}

	emv_memset(script_rest, 0x00, sizeof (script_rest));
	while ((pdata = appdata_find(tagname, &i)) != NULL) 
    {
		if ((tagname & 0x00FF) == MAX_SCRIPT) 
        {
			return -1;
		}

		if ((ret = emvtlv_decode(pdata, i, pobj, MAX_SCRIPT + 4, STRING_TLVOBJ)) < 0) 
        {
			failflg = 1; // tlv decoded failed here, not performed
		}
		if (pobj[1].tagname == 0x9F18) 
        {
			emv_memcpy(script_rest + script_i * 5 + 1, pobj[1].pvalue, pobj[1].valuelen); /*  Script Identifier */
			len = 2;
		}
        else
        {
			len = 1;
		}
		if (ret == 0) 
        {
			totallen += i + 2; // calc the dev limit
			if (totallen > 128) 
            {
				failflg = 1;
				script_i ++; // still not performed
				break;       // terminate the issuer processing
			}
			i = 1;
			for ( ; len <= pobj[0].childnum; len ++) 
            {
				if (pobj[len].tagname != 0x86) 
                {
					failflg = 1;
					goto next;
				}

                u9C = appdata_find(0x9C, NULL);
                if(u9C == NULL)
                {
                    return EMV_TERMINATE;
                }
                ret = EMVPORT_IccRW(g_express_cnxt->card_type, pobj[len].pvalue, pobj[len].valuelen, resp, NULL);
				if ((ret >= 2) && ((resp[ret - 2] == 0x90) ||
					 (resp[ret - 2] == 0x63) || (resp[ret - 2] == 0x62))) 
			    {
						i ++;
						continue;
				}
				*(script_rest + script_i * 5) = (i > 15 ? 0x0F : i) | 0x10;
				failflg = 1;
				break;
			}
			if (len == pobj[0].childnum + 1) 
            {
				*(script_rest + script_i * 5) = 0x20;
			}
		}
next:
		script_i ++;
		tagname ++;
	}

	if (failflg) 
    {
		if (when) 
        {
			len = SCRIPT_PROCESSING_FAILED_AFTER_2GENAC;
		}
        else 
        {
			len = SCRIPT_PROCESSING_FAILED_BEFORE_2GENAC;
		}
		tvr_set(len);
	}
	if (script_i) 
    {
		tsi_set(SCRIPT_PROCESSING_PERFORMED);
		emv_memcpy(res, script_rest, script_i * 5);
		*reslen = script_i * 5;
	}
	return 0;
}



/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
