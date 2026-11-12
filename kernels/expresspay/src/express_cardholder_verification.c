/**
************************************************************************************************************************
* @file     express_cardholder_verification.c
* @brief    Expresspay 内核：持卡人验证（Cardholder Verification / CVM）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_cardholder_verification.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     emv_cardholder_verification.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    持卡人验证（Cardholder Verification）
*
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "express_priv.h"
//#include "emvapi.h"
#include "emvoutcome.h"


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

//#define CV_FAIL      1
//#define CV_NOT_SUPP  2
//#define CV_UN_RECOG  3
//#define CV_OMIT      4
//#define CV_SUCC      5
//#define CV_ONLINE    6
//#define CV_SIGNATURE 7
//#define CV_KEYCANCEL 8
//
//#define CV_PIN_RETRY_DEFAULT 3
//#define CV_PIN_SUCC       0
//#define CV_PIN_BYPASS    -2
//#define CV_PIN_TIMEOUT   -3
//#define CV_PIN_FAIL      -4
//#define CV_PIN_OUTLIMIT  -5
//#define CV_PIN_GETDATA0  -6

#define NO_CVM_PERFORMED 0x3F

/* 终端是否"离线唯一"：与 pre-PDOL 使用同一份 9F35（appdata），低 3 位 011/110
 * （Expresspay v4.2 p33 §4.3.1；Table 21-6 p89）。Online PIN 只能用于可联机终端。 */
static bool express_term_is_offline_only(void)
{
    u8 *tt = appdata_find(TAG_9F35_TERMINAL_TYPE, NULL);

    if (tt == NULL)
    {
        return FALSE;
    }
    return ((*tt & 0x07) == 0x03) || ((*tt & 0x07) == 0x06);
}

typedef struct{
	unsigned char method;
	unsigned char condition;
}CVR;

typedef struct{
	CVR CVRList[120];
	unsigned char CVRListLen;
	unsigned char CVM_X[6];
	unsigned char CVM_Y[6];
}CVMSTRCUT;

int express_process_online_pin(void)
{
    int ret;
    u8  epin[256+4];
    int epinlen = 0;
    int status = g_express_cnxt->status;

    while (1)
    {
        if (status == EMV_REQ_ONLINE_PIN)
        {
            status = 0;
            ret = g_express_cnxt->req_online_pin_result;
        }
        else
        {
            g_express_cnxt->req_online_pin_result = EMV_OPER_UNSUPPORTED;
            return _CV_REQ_ONLINE_ENC_PIN;
        }

        if (ret == EMV_OPER_OK)
        {
            return CV_ONLINE;
        }
        else if (ret == EMV_OPER_UNSUPPORTED || ret == EMV_OPER_TIMEOUT)
        {
            return CV_FAIL;
        }

        break;
    }

    return rERR(CV_ONLINEPINFAIL); /* terminate */
}

unsigned char express_analy_cvmlist_cmpamount(unsigned char *CVMamount,unsigned char type)
{
	u8 *trans_cur_code = NULL, *app_cur_code=NULL;
	int trans_cur_code_len, app_cur_code_len;
	u32 x_code_ui, trans_amount_ui;

	trans_cur_code =appdata_find(TAG_5F2A_TRANSACTION_CURRENCY_CODE, &trans_cur_code_len);
	app_cur_code = appdata_find(TAG_9F42_APPLICATION_CURRENCY_CODE, &app_cur_code_len);

	if(trans_cur_code == NULL || trans_cur_code_len <= 0 || app_cur_code == NULL || app_cur_code_len <= 0)
	{
		eTRACE("AMEX-info: trans currency code or apll currency code absent");
        return 0;
	}

	eTRACE_HEX("AMEX-info: input amount X", CVMamount, 6);

	if(!emv_memcmp(trans_cur_code, app_cur_code, 2))
	{
		x_code_ui = emv_hex2dec(CVMamount+2, 4);
		eTRACE("AMEX-info: after swich X amount is: %d", x_code_ui);
		eTRACE("AMEX-info: trans amount is: %d", g_express_cnxt->trans_amount);

        if(type == 0)
        {
            if(g_express_cnxt->trans_amount < x_code_ui)
            {
                return 1;
            }
        }
        else
        {
            if(g_express_cnxt->trans_amount > x_code_ui)
            {
                return 1;
            }
        }
	}

	return 0;
}
unsigned char express_check_if_support_cvm(unsigned char method)
{
    unsigned char temp;
    unsigned char TermCapab[3];
    unsigned char EnhancedClessReaderCapab[4];
	u8 * ptmp = NULL;
	int len =0;
    temp = method & 0x3F;

    emv_memset(EnhancedClessReaderCapab,0,sizeof(EnhancedClessReaderCapab));
	ptmp = appdata_find(TAG_EnhancedClessReaderCapab,&len);
	if((ptmp != NULL) && (len > 0))
	{
		emv_memcpy(EnhancedClessReaderCapab,ptmp,len);
	}
	ptmp = appdata_find(TAG_9F33_TERMINAL_CAPABILITIES,&len);
	if((ptmp != NULL) && (len > 0))
	{
		emv_memcpy(TermCapab,ptmp,len);
	}
    switch(temp)
    {
        case 0x00:
        {
            return 1;
        }
        break;

        case 0x01: //mobile cvm

            if((EnhancedClessReaderCapab[1]&0x80)==0)
            {

                return 0;
            }
            else
            {
                return 1;
            }
            break;

        case 0x02: //enciphered PIN verification online
        {
            /* 9F6E b2b7=1 表示终端支持 Online PIN（Table 21-9 p91）。
             * Expresspay v4.2：Online PIN 必须联机（P p49 §8.2.3.1），离线唯一/当前无法联机的终端
             * 不得把 Online PIN 放进可用方法集合。 */
            if((EnhancedClessReaderCapab[1] & 0x40) == 0)
            {
                return 0;
            }
            else if(express_term_is_offline_only() || g_express_cnxt->bis_unable_online)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        break;

        /* Expresspay v4.2 禁止离线 PIN（P p23 §2.10.1 "must not process Offline PIN"；
         * Table 2-3 p23-24 无 VERIFY 命令；Table 21-16 p94-95 中离线 PIN 方法码不受支持）。 */
        case 0x03: //Plaintext PIN verification performed by ICC and signature(paper)
            return 0;
            break;

        case 0x04: //enciphered PIN verification performed by ICC
            return 0;
            break;

        case 0x05: //enciphered PIN verification performed by ICC and signature(paper)
            return 0;
            break;

        case 0x1E: //signature (paper)
        {
            if((EnhancedClessReaderCapab[1] & 0x20) == 0)
            {
                return 0;
            }
            else
            {
                return 1;
            }
        }
        break;

        case 0x1F: //no CVM required
        {
        	//return 1;//change by cj
        	if((TermCapab[1] & 0x08) == 0)
        	{

				return 0;
        	}
			else
			{

				return 1;
			}

            break;
        }

        default:
        {
            return 0;
        }
        break;
    }

    return 0;
}

unsigned char express_analy_cvmlist(unsigned char method, unsigned char condition, unsigned char TransType, unsigned char TermType, unsigned char *CVM_X, unsigned char *CVM_Y)
{
    unsigned char needcvm = 0, nextcvm = 0;
    unsigned char returnvalue;
    unsigned char TermAmtOther[6] = {0};
    unsigned char TermAmtOthernotzero = 0;
    unsigned char ret;
	u8 * ptmp = NULL;
	int len = 0;

	eTRACE("AMEX-info: (for Debug only) method: %02X", method);
	eTRACE("AMEX-info: (for Debug only) condition: %02X", condition);
    switch(condition)
    {
        case 0x00:       //always
        {
            needcvm = 1;
        }
        break;

        case 0x01:       //if cash or cashback
        {
            if(TransType == TRANS_CASH && (TermType & 0x0F) > 3 )
            {
                needcvm = 1;
            }
            else
            {
                nextcvm = 1;
            }
        }
        break;

        case 0x02:       //if not cash or cashback
        {
            emv_memset(TermAmtOther, 0, sizeof(TermAmtOther));
			ptmp = appdata_find(TAG_9F03_AMOUNT_OTHER_NUMERIC,&len);
			if((ptmp != NULL) && (len > 0))
			{
				emv_memcpy(TermAmtOther,ptmp,len);
			}

            if(emv_memcmp(TermAmtOther, "\x00\x00\x00\x00\x00\x00", 6))
            {
                TermAmtOthernotzero = 1;
            }

            if(TransType != TRANS_CASH  && TransType != TRANS_CASHBACK && TermAmtOthernotzero == 0)
            {
                needcvm = 1;
            }
            else
            {
                nextcvm = 1;
            }
        }
        break;

        case 0x03:       //if terminal support CVM   //sxl arm_linux
        {
            if(express_check_if_support_cvm(method))
            {
                needcvm = 1;
            }
            else
            {
                nextcvm = 1;
            }
        }
        break;

        case 0x04:       //if manual cash (added in EMV4.1,SU16)
        {
            if(TransType == TRANS_CASH && (TermType & 0x0F) <= 3 )
            {
                needcvm = 1;
            }
            else
            {
                nextcvm = 1;
            }
        }
        break;

        case 0x05:       //if purchase with cashback (added in EMV4.1,SU16)
        {
            emv_memset(TermAmtOther, 0, sizeof(TermAmtOther));
			ptmp = appdata_find(TAG_9F03_AMOUNT_OTHER_NUMERIC,&len);
			if((ptmp != NULL) && (len > 0))
			{
				emv_memcpy(TermAmtOther,ptmp,len);
			}

            if(emv_memcmp(TermAmtOther, "\x00\x00\x00\x00\x00\x00", 6))
            {
                TermAmtOthernotzero = 1;
            }

            if(TransType == TRANS_CASHBACK|| TermAmtOthernotzero )
            {
                needcvm = 1;
            }
            else
            {
                nextcvm = 1;
            }
        }
        break;

        case 0x06:       //if trans is in App currency and under X value
        {
			eTRACE_HEX("AMEX-info: Amount X: ", CVM_X, 6);
            ret  = express_analy_cvmlist_cmpamount(CVM_X, 0);
			eTRACE("AMEX-info: express_analy_cvmlist_cmpamount ret = %d", ret);
            if(ret == 0)
            {
                nextcvm = 1;
            }
            else
            {
                needcvm = 1;
            }
        }
        break;

        case 0x07:       //if trans is in App currency and over X value
        {
            ret  = express_analy_cvmlist_cmpamount(CVM_X, 1);
			eTRACE("AMEX-info: express_analy_cvmlist_cmpamount ret = %d", ret);
            if(ret == 0)
            {
                nextcvm = 1;
            }
            else
            {
                needcvm = 1;
            }
        }
        break;

        case 0x08:       //if trans is in App currency and under Y value
        {
            ret  = express_analy_cvmlist_cmpamount(CVM_Y, 0);
			eTRACE("AMEX-info: express_analy_cvmlist_cmpamount ret = %d", ret);
            if(ret == 0)
            {
                nextcvm = 1;
            }
            else
            {
                needcvm = 1;
            }
        }
        break;

        case 0x09:       //if trans is in App currency and over Y value
        {
            ret  = express_analy_cvmlist_cmpamount(CVM_Y, 1);
			eTRACE("AMEX-info: express_analy_cvmlist_cmpamount ret = %d", ret);
            if(ret == 0)
            {
                nextcvm = 1;
            }
            else
            {
                needcvm = 1;
            }
        }
        break;

        default:
        {
            nextcvm = 1;
        }
        break;
    }

	eTRACE("AMEX-info: express_analy_cvmlist nextcvm = %d\r\n", nextcvm);
	eTRACE("AMEX-info: express_analy_cvmlist needcvm = %d\r\n", needcvm);

	returnvalue = 0;
	if(nextcvm)
	{
		returnvalue |= 0x0F;
	}
	if(needcvm)
	{
		returnvalue |= 0xF0;
	}

    return returnvalue;
}

unsigned char express_perform_cvm(unsigned char method, unsigned char condition)
{

    unsigned char temp;
    unsigned char CVMResult[3];
    unsigned char MobileCVMResult[3];
	u8 EnhancedClessReaderCapab[4]={0};
	u8 * pdata_MobileCVMResult = NULL,*pdata_EnhancedClessReaderCapab = NULL;
	int len = 0;
	int ret = 0;

    temp = method&0x3F;
	eTRACE("AMEX-info: cvm method: %02X", temp);

    emv_memset(MobileCVMResult,0,sizeof(MobileCVMResult));

    pdata_MobileCVMResult = appdata_find(TAG_9F71_MB_CVMResult,&len);
	if((pdata_MobileCVMResult !=NULL) && (len >0))
		emv_memcpy(MobileCVMResult,pdata_MobileCVMResult,len);

	pdata_EnhancedClessReaderCapab = appdata_find(TAG_EnhancedClessReaderCapab,&len);
	if((pdata_EnhancedClessReaderCapab !=NULL) && (len >0))
		emv_memcpy(EnhancedClessReaderCapab,pdata_EnhancedClessReaderCapab,len);

    switch(temp)
    {
        case 0x00://cvm failed
        {
			return CV_FAIL_ALWALS;
        }
        break;

        case 0x01:           //mobile cvm
        {
        	if(!AIP_MOB_SUPPORT())	//card is not mobile card
        	{
                tvr_set(PIN_PAD_NOT_PRESENT_OR_NOT_WORKING);
				eTRACE("AMEX-info: Term goto CDCVM but Card is not Mobile device");
                return CV_NOT_SUPPORTED;
        	}
        	else	//if card type is moblie device,process as moblie cvm flow
        	{
				if(0 == (EnhancedClessReaderCapab[1] & 0x80))//term not support mobile cvm
				{
					eTRACE("AMEX-info: Term dont support CDCVM");
					return CV_NOT_SUPPORTED;
				}
				if((pdata_MobileCVMResult == NULL) && (len == 0))
				{
					eTRACE("AMEX-info: 9F71 CVM Result absent or len invalid");
					return CV_NOT_SUPPORTED;
				}
				if(MobileCVMResult[2]==0x03)
				{
					eTRACE("AMEX-info: 9F71 CVM Result indicate Mobile CVM Blocked");
					return CV_NOT_SUPPORTED;
				}
				else
				{
					if(g_express_cnxt->gExpressmobileagain == 0)  // Mobile CVM
					{
						if((MobileCVMResult[0]==0x01 && MobileCVMResult[2] !=0x02) ||(MobileCVMResult[0]== 0x3F)||((MobileCVMResult[0]!= 0x3F)&&(MobileCVMResult[0]!= 0x01)))
						{
							g_express_cnxt->gExpressmobileagain = 1;
							return express_outcome_try_again();	//17.2.3 Try Again
						}
					}
					if(MobileCVMResult[0]==0x3F)
					{
						return CV_NOT_SUPPORTED;
					}
					else if(MobileCVMResult[0]==0x01)
					{
						if(MobileCVMResult[2]==0x02)
						{
							g_express_cnxt->CVMParameter = OUTCOME_CVM_CDCVM;
							return CV_SUCCESS;
						}
						else
						{
							return CV_NOT_SUPPORTED;
						}
					}
					else
					{
						return CV_FAIL;
					}
				}
        	}
        }
        break;

        case 0x02:          //enciphered PIN verification online
            if((EnhancedClessReaderCapab[1]&0x40) !=0 && !express_term_is_offline_only() && !g_express_cnxt->bis_unable_online)
			{
				/* §8.2.3.1：Online PIN → TVR b3b3=1、CVM Results '09'、本笔必须联机。
				 * 后者由 express_action_analyse() 依据 bis_online_pin_entered 强制 ARQC。 */
				g_express_cnxt->CVMParameter = OUTCOME_CVM_ONLINE_PIN;
				g_express_cnxt->bis_online_pin_entered = TRUE;
				tvr_set(ONLINE_PIN_ENTERED);
				return express_process_online_pin();
			}

            tvr_set(PIN_PAD_NOT_PRESENT_OR_NOT_WORKING);
            return CV_NOT_SUPPORTED;

            break;

        /* Expresspay v4.2 禁止离线 PIN（P p23 §2.10.1）：以下三种方法一律"不支持"，
         * 由 CVM List 继续尝试下一个双方支持的方法。 */
        case 0x03:  //Plaintext PIN verification performed by ICC and signature(paper)
            eTRACE("AMEX-info: offline PIN (plaintext ICC) rejected: not supported by Expresspay v4.2");
            return CV_NOT_SUPPORTED;
            break;
        case 0x04:  //enciphered PIN verification performed by ICC
            eTRACE("AMEX-info: offline PIN (enciphered ICC) rejected: not supported by Expresspay v4.2");
            return CV_NOT_SUPPORTED;
            break;
        case 0x05: //enciphered PIN verification performed by ICC and signature(paper)
            eTRACE("AMEX-info: offline PIN (enciphered ICC + signature) rejected: not supported by Expresspay v4.2");
            return CV_NOT_SUPPORTED;
            break;
        case 0x1E://signature (paper)
            //if (!tc_option(TC_Signature_Paper))
            if((EnhancedClessReaderCapab[1]&0x20)==0)  //sxl EMV4.1e
            {
                return CV_NOT_SUPPORTED;
            }
			g_express_cnxt->CVMParameter = OUTCOME_CVM_OBTAIN_SIGNATURE;

            return CV_SIGNATURE;
            break;
        case 0x1F://no CVM required
			g_express_cnxt->CVMParameter = OUTCOME_CVM_NOCVM;
             return CV_SUCCESS;
        default:
            tvr_set(UNRECOGNISED_CVM);
            return CV_FAIL;

    }
}

static int express_check_mobile_cvm_result()
{
    /* 美运源码该函数体为空（未被调用）；补齐返回值以满足 -Werror=return-type */
    return EMV_OK;
}


int express_cvm_processing(bool skip_nocvm)
{
	u8 *cvm_list = NULL,*term_type = NULL,*enc_cap=NULL,*card_itr_payment_cap= NULL;
	int len,ret;
	u8 cvm_res[3]={0x3F,0x00,0x01};
	int i,j = -1;
	CVMSTRCUT CVMdata;
	u8 needcvm = 0;
    u8 needcvmnextcvm = 0;
	u8 sign_indicator = 0x00;

	eTRACE("AMEX-info: skip_nocvm = %d", skip_nocvm);

	cvm_list = appdata_find(TAG_008E_CVM_LIST, &len);
	if(cvm_list == NULL || len < 10 || (0 != (len % 2)))
	{
		eTRACE("AMEX-info: card dont return CVM List");
		cvm_res[0] = NO_CVM_PERFORMED;
		cvm_res[1] = 0x00;
		cvm_res[2] = 0x00;
		appdata_save(TAG_9F34_CVM_RESULTS, cvm_res, 3, OVERRIDED);
		tvr_set(ICC_DATA_MISSING);
		g_express_cnxt->CVMParameter = OUTCOME_CVM_NOCVM;
		return EMV_OK;
	}

	eTRACE_HEX("AMEX-info: CVM List", cvm_list, len);
	enc_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
	card_itr_payment_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);


	//8.2.3 CVM List Processing
	CVMdata.CVRListLen = (len-8)/2;

	eTRACE("AMEX-info: CVRListLen = %d", (len-8)/2);

	emv_memcpy(CVMdata.CVM_X+2, cvm_list, 4);
	emv_memcpy(CVMdata.CVM_Y+2, cvm_list+4, 4);

	for(i = 0; i < CVMdata.CVRListLen; i++)
	{
        CVMdata.CVRList[i].method = cvm_list[i*2+8];
        CVMdata.CVRList[i].condition = cvm_list[i*2+9];

		eTRACE("AMEX-info: CVRList[%d].method = %02X", i, cvm_list[i*2+8]);
		eTRACE("AMEX-info: CVRList[%d].condition = %02X", i, cvm_list[i*2+9]);

		if((CVMdata.CVRList[i].method & 0x1F) == 0x1F)
		{
			j = i;
		}
		if((CVMdata.CVRList[i].method & 0x02) == 0x02)
		{
			g_express_cnxt->bis_card_sup_onlinepin = TRUE;
		}
	}

	term_type = appdata_find(TAG_9F35_TERMINAL_TYPE, NULL);
	eTRACE("AMEX-info: Term Type = %02X", *term_type);
	eTRACE("AMEX-info: no cvm index = %d", j);

	if(skip_nocvm == FALSE && j >= 0)		//if not exceed cvm limit,priority use no cvm
	{
		eTRACE("AMEX-info: not exceed cvm limit,priority use no cvm");

		needcvmnextcvm = 0;
		needcvmnextcvm = express_analy_cvmlist(CVMdata.CVRList[j].method, CVMdata.CVRList[j].condition, g_express_cnxt->trans_type, (*term_type), CVMdata.CVM_X, CVMdata.CVM_Y);
		eTRACE("AMEX-info: express_analy_cvmlist ret = %02X", needcvmnextcvm);

		needcvm = needcvmnextcvm & 0xF0;

		if(needcvm)
		{
			cvm_res[0] = CVMdata.CVRList[j].method;
			cvm_res[1] = CVMdata.CVRList[j].condition;

			eTRACE_HEX("AMEX-info: CVM Result left most two byte", cvm_res, 2);

			ret = express_perform_cvm(CVMdata.CVRList[j].method, CVMdata.CVRList[j].condition);
			eTRACE("express_perform_cvm real ret = %d", ret);
			if(ret == CV_SUCCESS)
			{
				cvm_res[2] = CVM_RESULT_SUCCESSFUL;

				eTRACE_HEX("AMEX-info: after set TVR", g_express_cnxt->tvr, 5);
				eTRACE_HEX("AMEX-info: after CVM List process 9F34", cvm_res, 3);
				appdata_save(TAG_9F34_CVM_RESULTS, cvm_res, 3, OVERRIDED);
				appdata_save(TAG_SIGNATURE_INDICATOR, &sign_indicator, 1, UNIQUE);

				return EMV_OK;
			}
		}
	}

	for(i = 0; i < CVMdata.CVRListLen; i++)
	{
		if((skip_nocvm == TRUE) && ((CVMdata.CVRList[i].method & 0x1F) == 0x1F))
		{
			continue;
		}

		needcvmnextcvm = 0;
		needcvmnextcvm = express_analy_cvmlist(CVMdata.CVRList[i].method, CVMdata.CVRList[i].condition, g_express_cnxt->trans_type, (*term_type), CVMdata.CVM_X, CVMdata.CVM_Y);
		eTRACE("AMEX-info: express_analy_cvmlist ret = %02X", needcvmnextcvm);

		needcvm = needcvmnextcvm & 0xF0;

		if(needcvm)
		{
			cvm_res[0] = CVMdata.CVRList[i].method;
			cvm_res[1] = CVMdata.CVRList[i].condition;

			eTRACE_HEX("AMEX-info: CVM Result left most two byte", cvm_res, 2);
			ret = express_perform_cvm(CVMdata.CVRList[i].method, CVMdata.CVRList[i].condition);
			eTRACE("express_perform_cvm real ret = %d", ret);
			if(ret == CV_SUCCESS)
			{
				cvm_res[2] = CVM_RESULT_SUCCESSFUL;
				break;
			}

			if(ret == EMV_REQ_SEEPHONE || ret == _CV_REQ_ONLINE_ENC_PIN)
			{
				return ret;
			}
			else if (ret == CV_SIGNATURE || ret == CV_ONLINE)
			{
				cvm_res[2] = CVM_RESULT_UNKNOWN;
				break;
			}
			else if(ret == CV_FAIL_ALWALS)
			{
				cvm_res[0] = 0x3F;
				cvm_res[1] = 0x00;
				cvm_res[2] = CVM_RESULT_FAILED;
				tsi_set(CV_PERFORMED);

				tvr_set(CV_NOT_SUCCESSFUL);
				eTRACE("AMEX-error: set TVR B3b8 = 1");
				needcvm = 0;
				break;
			}
			else if(ret == CV_NOT_SUPPORTED)	//if this cvm kernel not support,reset cvm result to default value
			{
				eTRACE("AMEX-info: kernel don't support this cvm");
				if((cvm_res[0] & 0x02) == 0x02 && g_express_cnxt->trans_type == 0x09)
				{
					eTRACE("AMEX-info: cashback do not support online pin,switch interface");
					needcvm = 0;
					break;
				}

				if(i == CVMdata.CVRListLen - 1)
				{
					eTRACE("AMEX-info: no more cvm function");
					needcvm = 0;
					break;
				}
				else
				{
					needcvm = 1;
					continue;
				}
			}

			if((CVMdata.CVRList[i].method & 0x40) == 0x00)
			{
				tsi_set(CV_PERFORMED);
				tvr_set(CV_NOT_SUCCESSFUL);
				cvm_res[2] = CVM_RESULT_FAILED;
				eTRACE("AMEX-error: set TVR B3b8 = 1");
				break;
			}
		}
	}

	//8.2.4 Cardholder Verification is unable to complete over the Contactless interface
	if(!needcvm)
	{
		eTRACE("AMEX-info: none cvm function was satisfy");
		cvm_res[0] = NO_CVM_PERFORMED;
		cvm_res[1] = 0x00;
		cvm_res[2] = CVM_RESULT_FAILED;
		tvr_set(CV_NOT_SUCCESSFUL);
		g_express_cnxt->CVMParameter = OUTCOME_CVM_NOCVM;

		if((enc_cap[0] & 0x80) && \
			((g_express_cnxt->aip[1] & 0x40) == 0x00) && ((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
			((card_itr_payment_cap == NULL) || (card_itr_payment_cap[0] & 0x20)))
		{
			return EMV_SW_INTERFACE;
		}
	}


	if(g_express_cnxt->CVMParameter == OUTCOME_CVM_OBTAIN_SIGNATURE)
	{
		sign_indicator = 0x01;
	}


	eTRACE_HEX("AMEX-info: after set TVR", g_express_cnxt->tvr, 5);
	eTRACE_HEX("AMEX-info: after CVM List process 9F34", cvm_res, 3);
	appdata_save(TAG_9F34_CVM_RESULTS, cvm_res, 3, OVERRIDED);
	appdata_save(TAG_SIGNATURE_INDICATOR, &sign_indicator, 1, UNIQUE);

	return EMV_OK;
}

int express_cardholder_verify(void)
{
	u8 cvm_res[3]={0};
	u8 *enhan_ctl_reader_cap = NULL,*card_itr_payment_cap = NULL;
	u8 *mob_cvm_res = NULL;
	int ret,len;

	eTRACE("AMEX-info: in emv_cardholder_verify_temp g_express_cnxt->CVMRLEF  = %d", g_express_cnxt->CVMRLEF);


	if(g_express_cnxt->CVMRLEF)	//Reader CVM Limit Reached or Exceeded
	{
		if(AIP_CV_SUPPORT())
		{
			eTRACE("AMEX-info: card support CVM");
			//8.2.2
			ret = express_cvm_processing(TRUE);
			if(ret != EMV_OK)
			{
				if(ret == EMV_REQ_SEEPHONE)
				{
					return rNXT(ret);
				}
				else if(ret == _CV_REQ_ONLINE_ENC_PIN)
				{
					return express_outcome_request_online_pin();
					return rNXT(EMV_REQ_ONLINE_PIN);
				}
				else if(ret == EMV_SW_INTERFACE)
				{
					return express_outcome_try_another_interface();
				}
			}
		}
		else
		{
			eTRACE("AMEX-info: card not support CVM");
			//8.2.4
			//The Terminal must set TVR, byte 3 bit 8 to 鈥?鈥?(Cardholder Verification was not successful) and sets the CVM
			//Results as per [EMV4.3iv] Section 6.3.4.5, when No CVM Conditions in the CVM List is satisfied
			tvr_set(CV_NOT_SUCCESSFUL);
			emv_memcpy(cvm_res, "\x3F\x00\x01", 3);
			appdata_save(TAG_9F34_CVM_RESULTS, cvm_res, 3, OVERRIDED);

			enhan_ctl_reader_cap = appdata_find(TAG_EnhancedClessReaderCapab, NULL);
			card_itr_payment_cap = appdata_find(TAG_9F70_CARD_INTERFACEANDPAYMENT_CAP, NULL);

			if(enhan_ctl_reader_cap[0] & 0x80)	//if Terminal has contact interface
			{
				if( ((g_express_cnxt->aip[1] & 0x40) == 0x00) && \
					((g_express_cnxt->aip[1] & 0x20) == 0x00) && \
					((card_itr_payment_cap == NULL) || (card_itr_payment_cap[0] & 0x20)) )
				{
					return express_outcome_try_another_interface();
//					return rEND(EMV_SW_INTERFACE);
				}
			}
		}
	}
	else	//CVM Limit Not Exceeded
	{
		//8.2.5
		if(AIP_MOB_SUPPORT())	//8.2.5.1  CVM Limit Not Exceeded - Mobile
		{
			eTRACE("AMEX-info: this is mobile device");
			mob_cvm_res = appdata_find(TAG_9F71_MB_CVMResult, &len);
			if(mob_cvm_res != NULL && len > 0)
			{
				if((mob_cvm_res[0] & 0x3F) == 0x01)	//Mobile CVM performed
				{
					if((mob_cvm_res[2] == 0x01) && (g_express_cnxt->gExpressmobileagain == 0))	//Mobile CVM Successful
					{
						g_express_cnxt->gExpressmobileagain = 1;
						return express_outcome_try_again(); //17.2.3 Try Again
					}
				}
			}

			if(AIP_CV_SUPPORT())
			{
				eTRACE("AMEX-info: card support CVM");
				ret = express_cvm_processing(FALSE);
				if(ret != EMV_OK)
				{
					if(ret == EMV_REQ_SEEPHONE)
					{
						return rNXT(ret);
					}
					else if(ret == _CV_REQ_ONLINE_ENC_PIN)
					{
						return express_outcome_request_online_pin();
						return rNXT(EMV_REQ_ONLINE_PIN);
					}
					else if(ret == EMV_SW_INTERFACE)
					{
						return express_outcome_try_another_interface();
					}
				}
			}
			else
			{
				emv_memcpy(cvm_res, "\x3F\x00\x00", 3);
				appdata_save(TAG_9F34_CVM_RESULTS, cvm_res, 3, OVERRIDED);
			}
		}
		else	//8.2.5.2 CVM Limit Not Exceeded - Card
		{
			eTRACE("AMEX-info: this is card");
			if(AIP_CV_SUPPORT())
			{
				eTRACE("AMEX-info: card support CVM");
				ret = express_cvm_processing(FALSE);
				if(ret != EMV_OK)
				{
					if(ret == EMV_REQ_SEEPHONE)
					{
						return rNXT(ret);
					}
					else if(ret == _CV_REQ_ONLINE_ENC_PIN)
					{
						return express_outcome_request_online_pin();
						return rNXT(EMV_REQ_ONLINE_PIN);
					}
					else if(ret == EMV_SW_INTERFACE)
					{
						return express_outcome_try_another_interface();
					}
				}
			}
			else
			{
				emv_memcpy(cvm_res, "\x3F\x00\x00", 3);
				appdata_save(TAG_9F34_CVM_RESULTS, cvm_res, 3, OVERRIDED);
			}
		}
	}


	return rNXT(EMV_STA_CARDHOLDER_VERIFY_COMPLETED);
}
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
