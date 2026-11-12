#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emvapi.c
* @author   Kingsee Development Team xinyong
* @version  V43
* @date     2015��11��16��
* @brief    TODO
*
************************************************************************************************************************
*/


//#define __DEBUG_MODULE__

/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "sdk.h"
#include "emv.h"
#include "emv_ics.h"
#include "emv_tag.h"
#include "emv_type.h"
#include "emv_utils.h"
#include "emvdbg.h"
#include "emverror.h"
#include "emvspi.h"
#include "emvapi.h"
#include "emvporting.h"
#include "emvi.h"


/*
************************************************************************************************************************
*                                                     LOCAL DEFINES
************************************************************************************************************************
*/

#define EMVAPI_VERSION      "KSEMVAPI_V0.5.38"//26

#define EMVAPI_VERSION1      "KSEMVDEVELOP_V0.5.51(T2)"


/*
************************************************************************************************************************
*                                                   LOCAL DATA TYPES
************************************************************************************************************************
*/

typedef struct _emvapi_context
{
    EMV_INIT_PARAM init;
} emvapi_context;



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

emvapi_context  g_emvapi;
int32_t g_emvapi_messageinx = -1;

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

static void throws_status(uint8_t status, uint8_t description)
{
    if(g_emvapi.init.pEMVProcessCallback != NULL)
    {
        TRACE("status:%d", status);
        uint8_t data[2];
        data[0] = status;
        data[1] = description;
        g_emvapi.init.pEMVProcessCallback(data);
    }

}


/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/

#define C_O_M_M__F_U_N_C__C


int32_t emv_get_tag_data(uint32_t tag, uint8_t* value, int32_t* length)
{
    uint8_t* ptmp;
    int32_t ilen = 0;

    ptmp = EMV_GetData(tag, &ilen);

    if (ptmp != NULL && ilen >= 0)
    {
        if (value)
            API_memcpy(value, ptmp, ilen);
        if (length)
            *length = ilen;

        return EMV_OK;
    }
    return EMV_FAIL;
}

int32_t emv_set_tag_data(uint32_t tag, uint8_t *value, int32_t length)
{
    int32_t ret;

    ret = EMV_SetData(tag, value, length);
    if(ret == EMV_OK)
    {
        return EMV_OK;
    }
    else
    {
        return EMV_FAIL;
    }
}

int32_t emv_get_version(uint8_t* ver)
{
    uint8_t* ptmp;
    int32_t ilen;

    if (ver == NULL)
    {
        return EMV_FAIL;
    }

    ptmp = EMV_GetData(TAG_KERNEL_VERSION, &ilen);
    if (ptmp == NULL)
    {
        return EMV_FAIL;
    }

    API_sprintf((char*)ver, "%s & %s", ptmp, EMVAPI_VERSION);

    return EMV_OK;
}

int32_t emv_get_candidate_list(void* list)
{
    uint8_t* ptmp;
    int32_t ilen;

    ptmp = EMV_GetData(TAG_CANDIDATE_LIST, &ilen);
    if (ptmp && ilen > 0)
    {
        if (ilen % sizeof(EMV_CANDIDATE) == 0)
        {
            API_memcpy(list, ptmp, ilen);
            return ilen / sizeof(EMV_CANDIDATE);
        }
    }
    return 0;
}


#define P_R_O_C__F_U_N_C__C


int32_t emv_initialize(EMV_INIT_PARAM* par)
{
	//insert api load code by scripts
#if EMV_ANDROID_SO
    API_Load();
#endif

    g_emvapi_messageinx = -1;
#if 1
    API_memset(&g_emvapi, 0, sizeof(g_emvapi));
    if(par != NULL)
    {
        g_emvapi.init.pCardEventOccured = par->pCardEventOccured;
        g_emvapi.init.pEMVProcessCallback = par->pEMVProcessCallback;
		g_emvapi.init.emv_cardtype = par->emv_cardtype;
    }
#endif

    TRACE("[%s]",EMVAPI_VERSION);

    TRACE("[%s]",EMVAPI_VERSION1);


	eTRACE("g_emv_porting[%p]", &g_emv_porting);

    return EMV_Init(&g_emv_porting);
}


int32_t emv_set_card_type(int32_t cardtype)
{
	if(g_emvapi.init.emv_cardtype != NULL)
		g_emvapi.init.emv_cardtype(cardtype);

    return EMV_OK;
}

int32_t emv_set_trans_type(uint8_t type)
{
    return EMV_SetTransType(type);
}

int32_t emv_get_trans_type(uint8_t *type)
{
    return EMV_GetTransType(type);
}

/* Currency code / exponent APIs removed: use tags 5F2A (Transaction Currency Code) and 5F36 (Transaction Currency Exponent) instead. */

int32_t emv_set_trans_amount(uint32_t amount)
{
    return EMV_SetTransAmount(amount);
}

int32_t emv_set_other_amount(uint32_t amount)
{
    return EMV_SetOtherAmount(amount);
}

int32_t emv_set_trans_amount_absent(bool flag)
{
	return EMV_SetTransAmountAbsent(flag);
}

int32_t emv_set_other_amount_absent(bool flag)
{
	return EMV_SetOtherAmountAbsent(flag);
}

int32_t emv_set_trans_amount_empty(bool flag)
{
	return EMV_SetTransAmountEmpty(flag);
}

int32_t emv_set_other_amount_empty(bool flag)
{
	return EMV_SetOtherAmountEmpty(flag);
}

int32_t emv_preprocess(uint8_t online)
{
    if (EMV_PreProcess(online))
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_set_cardtype(uint8_t type)
{
    if (type == 1)
        type = EMV_CARD_CONTACT;
    else if (type == 2)
        type = EMV_CARD_CONTACTLESS;
    else
        return EMV_FAIL;

	TRACE("input type = %02X", type);

    if (EMV_SetCardType(type))
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_get_kernel_id(uint8_t * val, int32_t *len)
{
    const EMV_CANDIDATE* candidate;
    int32_t index;

    if (len != NULL)
    {
        *len = 0;
    }
    if (g_emv_cnxt == NULL || val == NULL)
    {
        return EMV_UNINITIALIZED;
    }

    index = g_emv_cnxt->req_sel_candidates_result;
    if (index < 0 || index >= g_emv_cnxt->candidates_num)
    {
        return EMV_FAIL;
    }

    candidate = &g_emv_cnxt->candidates[index];
    if (candidate->kernel_id_len > sizeof(candidate->kernel_id))
    {
        return EMV_FAIL;
    }
    if (candidate->kernel_id_len > 0)
    {
        emv_memcpy(val, candidate->kernel_id, candidate->kernel_id_len);
    }
    if (len != NULL)
    {
        *len = candidate->kernel_id_len;
    }

    return EMV_OK;
}



int32_t emv_process(void)
{
	return EMV_Process(NULL);

//    int32_t ret;
//    int32_t flg = 1;
//
//    while (flg)
//    {
//        flg = 0;
//        ret = EMV_Process(NULL);
//		TRACE("after EMV_Process flg = %d", flg);
//        switch (ret)
//        {
//            case EMV_STA_CANDIDATES_BUILT:
//                //flg = 1;
//                break;
//            case EMV_REQ_SEL_CANDIDATES:
//            case EMV_REQ_SEL_CANDIDATES_AGAIN:
//                throws_status(STATUS_CONTINUE, EMV_CANDIDATE_LIST);
//                break;
//            case EMV_STA_APP_SELECTED:
//                //flg = 1;
//                throws_status(STATUS_CONTINUE, EMV_APP_SELECTED);
//                break;
//            case EMV_REQ_SEL_ACCOUNTTYPE:
//                //flg = 1; //FIXME ѡ���ʻ�����
//                //EMV_ResultSelAccountType(EMV_ACCOUNT_DEFAULT);
//                break;
//            case EMV_STA_APP_INITIALIZED:
//                //flg = 1;
//                throws_status(STATUS_CONTINUE, EMV_GET_PROC_OPTION);
//                break;
//            case EMV_REQ_DEACTIVATE_CARD:
//                //flg = 1; //FIXME ��ʾ���ƿ�
//                break;
//            case EMV_STA_CARD_DEACTIVATED:
//                //flg = 1;
//                break;
//            case EMV_STA_READ_APP_DATA_COMPLETED:
//                throws_status(STATUS_CONTINUE, EMV_READ_APP_DATA);
//                break;
//            case EMV_STA_DATA_AUTH_COMPLETED:
//                throws_status(STATUS_CONTINUE, EMV_DATA_AUTH);
//                break;
//            case EMV_STA_PROCESS_RESTRICT_COMPLETED:
//                throws_status(STATUS_CONTINUE, EMV_PROCESS_RESTRICT);
//                break;
//            case EMV_REQ_CHECK_ID:
//                throws_status(STATUS_CONTINUE, EMV_ID_CHECK);
//                break;
//            case EMV_REQ_ONLINE_PIN:
//                throws_status(STATUS_CONTINUE, EMV_ONLINE_ENC_PIN);
//                break;
//            case EMV_REQ_OFFLINE_PIN:
//            case EMV_REQ_OFFLINE_PIN_LAST:
//                //TODO
//                break;
//            case EMV_REQ_CONFIRM_BYPASS_PIN:
//                throws_status(STATUS_CONTINUE, EMV_PIN_BYPASS_CONFIRM);
//                break;
//            case EMV_STA_CARDHOLDER_VERIFY_COMPLETED:
//                throws_status(STATUS_CONTINUE, EMV_CARDHOLDER_VERIFY);
//                break;
//            case EMV_REQ_CONFIRM_FORCE_ONLINE:
//                //flg = 1;
//                EMV_ResultForceOnline(1); //FIXME ��ȷ��ֱ��ǿ������
//                break;
//            case EMV_STA_RISK_MANAGEMENT_COMPLETED:
//                throws_status(STATUS_CONTINUE, EMV_TERMINAL_RISK_MANAGEMENT);
//                break;
//            case EMV_STA_ACTION_ANALYSE_COMPLETED:
//                //flg = 1;
//                break;
//            case EMV_REQ_GO_ONLINE:
//                throws_status(STATUS_CONTINUE, EMV_PROCESS_ONLINE);
//                break;
//            case EMV_REQ_ISSUER_REFERRAL:
//                //flg = 1;
////                EMV_ResultIssuerReferral(0); //FIXME ���󷢿��вο�ֱ�Ӿܾ�
//                break;
//            case EMV_REQ_CARDINFO_CONFIRM:
//                //throws_status(STATUS_CONTINUE, EMV_REQ_CARDINFO_CONFIRM);
//                break;
//
//            /////////////////////////////
//            case EMV_ACCEPTED_OFFLINE:
//                throws_status(STATUS_COMPLETION, APPROVE_OFFLINE);
//                break;
//            case EMV_ACCEPTED_ONLINE:
//                throws_status(STATUS_COMPLETION, APPROVE_ONLINE);
//                break;
//            case EMV_DENIALED_OFFLINE:
//                throws_status(STATUS_COMPLETION, DECLINE_OFFLINE);
//                break;
//            case EMV_DENIALED_ONLINE:
//                throws_status(STATUS_COMPLETION, DECLINE_ONLINE);
//                break;
//
//            /////////////////////////////
//
//            default:
//                throws_status(STATUS_ERROR, 255); //FIXME
//                break;
//        }
//
//		TRACE("in switch ret = %d", ret);
//		TRACE("in switch flg = %d", flg);
//    }
//
//    TRACE("ret = %d", ret);
//    return ret;
}

int32_t emv_process2(int32_t *msginx)
{
    int32_t ret;
    int32_t flg = 1;

    while (flg)
    {
        flg = 0;
        ret = EMV_Process(msginx);
        switch (ret)
        {
            case EMV_STA_CANDIDATES_BUILT:
                //flg = 1;
                break;
            case EMV_REQ_SEL_CANDIDATES:
            case EMV_REQ_SEL_CANDIDATES_AGAIN:
                throws_status(STATUS_CONTINUE, EMV_CANDIDATE_LIST);
                break;
            case EMV_STA_APP_SELECTED:
                //flg = 1;
                throws_status(STATUS_CONTINUE, EMV_APP_SELECTED);
                break;
            case EMV_REQ_SEL_ACCOUNTTYPE:
                //flg = 1; //FIXME ѡ���ʻ�����
                //EMV_ResultSelAccountType(EMV_ACCOUNT_DEFAULT);
                break;
            case EMV_STA_APP_INITIALIZED:
                //flg = 1;
                throws_status(STATUS_CONTINUE, EMV_GET_PROC_OPTION);
                break;
            case EMV_REQ_DEACTIVATE_CARD:
                //flg = 1; //FIXME ��ʾ���ƿ�
                break;
            case EMV_STA_CARD_DEACTIVATED:
                //flg = 1;
                break;
            case EMV_STA_READ_APP_DATA_COMPLETED:
                throws_status(STATUS_CONTINUE, EMV_READ_APP_DATA);
                break;
            case EMV_STA_DATA_AUTH_COMPLETED:
                throws_status(STATUS_CONTINUE, EMV_DATA_AUTH);
                break;
            case EMV_STA_PROCESS_RESTRICT_COMPLETED:
                throws_status(STATUS_CONTINUE, EMV_PROCESS_RESTRICT);
                break;
            case EMV_REQ_CHECK_ID:
                throws_status(STATUS_CONTINUE, EMV_ID_CHECK);
                break;
            case EMV_REQ_ONLINE_PIN:
                throws_status(STATUS_CONTINUE, EMV_ONLINE_ENC_PIN);
                break;
            case EMV_REQ_OFFLINE_PIN:
            case EMV_REQ_OFFLINE_PIN_LAST:
                //TODO
                break;
            case EMV_REQ_CONFIRM_BYPASS_PIN:
                throws_status(STATUS_CONTINUE, EMV_PIN_BYPASS_CONFIRM);
                break;
            case EMV_STA_CARDHOLDER_VERIFY_COMPLETED:
                throws_status(STATUS_CONTINUE, EMV_CARDHOLDER_VERIFY);
                break;
            case EMV_REQ_CONFIRM_FORCE_ONLINE:
                //flg = 1;
//                EMV_ResultForceOnline(1); //FIXME ��ȷ��ֱ��ǿ������
                break;
            case EMV_STA_RISK_MANAGEMENT_COMPLETED:
                throws_status(STATUS_CONTINUE, EMV_TERMINAL_RISK_MANAGEMENT);
                break;
            case EMV_STA_ACTION_ANALYSE_COMPLETED:
                //flg = 1;
                break;
            case EMV_REQ_GO_ONLINE:
                throws_status(STATUS_CONTINUE, EMV_PROCESS_ONLINE);
                break;
            case EMV_REQ_ISSUER_REFERRAL:
                //flg = 1;
//                EMV_ResultIssuerReferral(0); //FIXME ���󷢿��вο�ֱ�Ӿܾ�
                break;
            case EMV_REQ_CARDINFO_CONFIRM:
                //throws_status(STATUS_CONTINUE, EMV_REQ_CARDINFO_CONFIRM);
                break;

            /////////////////////////////
            case EMV_ACCEPTED_OFFLINE:
                throws_status(STATUS_COMPLETION, APPROVE_OFFLINE);
                break;
            case EMV_ACCEPTED_ONLINE:
                throws_status(STATUS_COMPLETION, APPROVE_ONLINE);
                break;
            case EMV_DENIALED_OFFLINE:
                throws_status(STATUS_COMPLETION, DECLINE_OFFLINE);
                break;
            case EMV_DENIALED_ONLINE:
                throws_status(STATUS_COMPLETION, DECLINE_ONLINE);
                break;

            /////////////////////////////

            default:
                throws_status(STATUS_ERROR, 255); //FIXME
                break;
        }

        if(g_emvapi_messageinx != -1)
        {
            *msginx = g_emvapi_messageinx;
            g_emvapi_messageinx = -1;
            break;
        }
        else
        {
            *msginx = -1;
        }

    }

    TRACE("ret = %d", ret);
    return ret;
}

int32_t emv_set_candidate_list_result(int32_t index)
{
    return EMV_ResultSelCandidates(index);
}

int32_t emv_set_account_type_selected(int32_t result)
{
    return EMV_ResultSelAccountType(result);
}

int32_t emv_set_check_id_result(int32_t result)
{
    return EMV_ResultCheckID(result);
}

int32_t emv_set_online_pin_entered(int32_t result, uint8_t* pinbuf, int32_t pinlen)
{
    return EMV_ResultOnlinePIN(result, pinbuf, pinlen);
}

int32_t emv_set_offline_pin_entered(int32_t result, uint8_t* pinbuf, int32_t pinlen)
{
    return EMV_ResultOfflinePIN(result, pinbuf, pinlen);
}

int32_t emv_set_pin_bypass_confirmed(int32_t result)
{
    return EMV_ResultBypassPIN(result);
}

int32_t emv_set_force_online(int32_t result)
{
    return EMV_ResultForceOnline(result);
}

int32_t emv_set_online_result(int32_t result, uint8_t* resp, int32_t resp_len)
{
   return EMV_ResultOnline(result, resp, resp_len);
}

int32_t emv_set_issref_result(int32_t result)
{
    return EMV_ResultIssuerReferral(result);
}

int32_t emv_is_need_advice(void)
{
	int32_t ilen;
    uint8_t* ptmp = EMV_GetData(TAG_ADVICE_INDICATOR, &ilen);
    if (ptmp)
    {
        if (*ptmp)
            return 1;
    }
    return 0;
}

int32_t emv_is_need_signature(void)
{
	int32_t ilen;
    uint8_t* ptmp = EMV_GetData(TAG_SIGNATURE_INDICATOR, &ilen);
    if (ptmp)
    {
        if (*ptmp)
            return 1;
    }
    return 0;
}

int32_t emv_process_exit(void)
{
    if (EMV_ProcessExit())
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}


#define P_A_R_A__F_U_N_C__C

int32_t emv_clr_termparam(void)
{
    if (emvporting_clr_termparam())
    {
        return EMV_FAIL;
    }
//    EMV_Init(&g_emv_porting);
    return EMV_OK;
}

int32_t emv_set_termparam(uint8_t* par, int32_t len)
{
    if (emvporting_set_termparam(par, len))
    {
        return EMV_FAIL;
    }
//    EMV_Init(&g_emv_porting);
    return EMV_OK;
}

int32_t emv_get_termparam(uint8_t* par, int32_t *rlen)
{
    return emvporting_get_termparam_tlv(par, rlen);
}

int32_t emv_clr_aidparam(void)
{
    if (emvporting_clr_aidparam())
    {
        return EMV_FAIL;
    }
//    EMV_Init(&g_emv_porting);
    return EMV_OK;
}

int32_t emv_add_aidparam(uint8_t* par, int32_t len)
{
    if (emvporting_add_aidparam(par, len))
    {
        return EMV_FAIL;
    }
//    EMV_Init(&g_emv_porting);
    return EMV_OK;
}

int32_t emv_get_aidparam(int32_t inx, uint8_t* par, int32_t *rlen)
{
    return emvporting_get_aidparam_tlv(inx, par, rlen);
}

int32_t emv_clr_capkparam(void)
{
    if (emvporting_clr_capkparam())
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_add_capkparam(uint8_t* par, int32_t len)
{
    if (emvporting_add_capkparam(par, len))
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_get_capkparam(int32_t inx, uint8_t* par, int32_t *rlen)
{
    return emvporting_get_capkparam_tlv(inx, par, rlen);
}

int32_t emv_clr_cardblack(void)
{
    if (emvporting_clr_cardblack())
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_add_cardblack(void* blk)
{
    if (emvporting_add_cardblack((EMV_CARDBLACK*)blk))
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_clr_certblack(void)
{
    if (emvporting_clr_certblack())
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

int32_t emv_add_certblack(void* blk)
{
    if (emvporting_add_certblack((EMV_CERTBLACK*)blk))
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}



int32_t emv_clr_translog(void)
{
    if (emvporting_clr_translog())
    {
        return EMV_FAIL;
    }
    return EMV_OK;

}

int32_t emv_clr_transwater(void)
{
    if (emvporting_clr_transwater())
    {
        return EMV_FAIL;
    }
    return EMV_OK;

}

int32_t emv_save_transwater(uint8_t* buf, int32_t len)
{
    if (emvporting_save_transwater(buf, len))
    {
        return EMV_FAIL;
    }
    return EMV_OK;

}

int32_t emv_get_transwatername(char* name)
{
    emvporting_get_transwatername(name);
    return EMV_OK;

}

int32_t emv_get_transwater(int32_t inx, uint8_t* buf, int32_t *len)
{
    return emvporting_get_transwater(inx, buf, len);

}


int32_t emv_get_kernel_index(uint8_t *val)
{
    emvporting_get_kernel_index(val);
    return EMV_OK;

}

int32_t emv_set_kernel_index(uint8_t val)
{
    emvporting_set_kernel_index(val);
    return EMV_OK;

}

int32_t emv_write_file(char *filename, uint32_t filesize, uint8_t *buf, uint32_t buflen, uint32_t flag)
{
    return emvporting_write_file(filename, filesize, buf, buflen, flag);

}

int32_t emv_clr_drl(void)
{
	if (emvporting_clr_drl())
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}


int32_t emv_add_drl(void * par, int32_t len)
{
    if (emvporting_add_drl(par, len))
    {
        return EMV_FAIL;
    }
    return EMV_OK;
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
