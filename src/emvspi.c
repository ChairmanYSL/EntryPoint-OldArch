#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_porting.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015��11��17��
* @brief    EMV�ں���ֲ�ӿ�
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
#include "emvporting.h"
#include "ks_crypt.h"
#include "emvi.h"
#include "emv_port_sdk.h"

/** �����Ƿ�ʹ��SDK2.0��API */
#define CFG_EMV_L2TEST_USE_SDKV2        0


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

#define FMT_NUMTOBCD(hex) ((((hex) / 10) << 4) + ((hex) % 10))


/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/

extern int32_t g_emvapi_messageinx;

////////////////////////////add dual function////////////////////////////


#define EMVSPI_DO	0

#if EMVSPI_DO

#include "emv_porting_ui.h"

// ���Ӧ�ø���
#define APPS_MAX_NUM                32
#define PUI_WAIT_INPUT_TIMEOUT_MS 60*1000       /** �ȴ����볬ʱ */

 /**
  * ѡ���ѡӦ���б�
  *
  * @param	 again								 [IN]�ٴ�ѡ����
  * @param	 list								 [IN]��ѡӦ���б�����ΪNULL��
  * @param	 count								 [IN]��ѡӦ�ø���������0��
  * @return  [0 ~ count-1]						 ѡ�е�Ӧ�ö�Ӧ��ѡӦ���б�������
  * @return  {@link EMV_OPER_CANCEL}			 ȡ��
  * @return  {@link EMV_OPER_TIMEOUT}			 ��ʱ
  * @return  {@link EMV_OPER_REPORT_APP}		 ��������֮���ں˻᷵��{@link EMV_REQ_SEL_CANDIDATES}��{@link EMV_REQ_SEL_CANDIDATES_AGAIN}
  *
  * @note	 �����֧�ָù��ܣ���ô����ֱ�ӷ���0ѡ���һ��Ӧ��
  */
 int32_t sel_candidate_list_do(int32_t again, const EMV_CANDIDATE* list, int32_t count)
{
    int32_t     i, ret = 0;
    char*   title = NULL;
    char*   item[APPS_MAX_NUM] = {NULL};
    char    release_name[APPS_MAX_NUM][33];
	uint8_t tempaid[16];

    if (emvporting_autotest_get_flag())
    {
        if (emvporting_autotest_get_appindex())
        {
            return 0;
        }
    }


    if (again)
    {
        title = CN_EN("������ѡ��Ӧ��:", "Please retry to select application:");
    }
    else
    {
        title = CN_EN("��ѡ��Ӧ��:", "Please select application:");
    }


    for (i = 0; i < count; i++)
    {
	  	API_memcpy(tempaid, list[i].aid, 16);
        if (list[i].preferred_name_len == 0)
        {
            API_Conv_BcdToStr(tempaid/*list[i].aid*/, list[i].aid_len, (char*)(release_name[i]));
        }
        else
        {
            API_memcpy(release_name[i], list[i].preferred_name, list[i].preferred_name_len);
            *(release_name[i] + list[i].preferred_name_len) = 0;
        }
        item[i] = release_name[i];
    }

    ret = PUI_MENU_SELECT(title, (const char**)item, count, 0, PUI_WAIT_INPUT_TIMEOUT_MS);

    PUI_SHOWSTA(NULL, CN_EN("���ڴ���������ο���", "Processing, Dnon't remove card"));
#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
    EMV_ResultSelCandidates(ret);
#endif

    if (ret < 0 || ret >= count)
    {
        return EMV_OPER_CANCEL;
    }

    return ret;
}


/**
 * ѡ���ʻ�����
 *
 * @return  {@link EMV_ACCOUNT_DEFAULT}         ѡ����Ĭ��-δָ����Default��
 * @return  {@link EMV_ACCOUNT_SAVINGS}         ѡ���˴����˻���Savings��
 * @return  {@link EMV_ACCOUNT_CHEQUE_DEBIT}    ѡ����֧Ʊ�˻�/����˻���Cheque/Debit��
 * @return  {@link EMV_ACCOUNT_CREDIT}          ѡ���������˻���Credit��
 * @return  {@link EMV_OPER_CANCEL}             ȡ��
 * @return  {@link EMV_OPER_TIMEOUT}            ��ʱ
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧�ָù���
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_SEL_ACCOUNTTYPE}
 */
int32_t sel_account_type_do(void)
{
    int32_t ret = 0x00;

    // cn PBOC3.0 �ڣ��¡��� A.7 �˻�����
    const char* item[] = {
        CN_EN("Ĭ��", "Default"),
        CN_EN("�����ʺ�", "Savings account"),
        CN_EN("֧Ʊ�˻�/����˻�", "Check account/Debit account"),
        CN_EN("�����˻�", "Credit account"),
    };


    if (!emvporting_para_get_account_type_sel_sw())
    {
        return EMV_OPER_UNSUPPORTED;
    }


    ret = PUI_MENU_SELECT(CN_EN("�ʻ�����ѡ��:", "Account type selection:"), item, 4, 0, PUI_WAIT_INPUT_TIMEOUT_MS);

    // ȷ����ɼ�����ʾIC��������
    PUI_SHOWSTA(NULL, CN_EN("���ڴ���������ο���", "Processing, Dnon't remove card"));

    if (ret == 0) {
        ret = EMV_ACCOUNT_DEFAULT;
    } else if (ret == 1) {
        ret = EMV_ACCOUNT_SAVINGS;
    } else if (ret == 2) {
        ret = EMV_ACCOUNT_CHEQUE_DEBIT;
    } else if (ret == 3) {
        ret = EMV_ACCOUNT_CREDIT;
    } else {
        ret = EMV_OPER_CANCEL;
    }

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
	if(ret>=0 && ret<=4)
	{
		EMV_ResultSelAccountType(ret);
	}
#endif

    return ret;
}


/**
 * ��ʾ���ȴ��û��ƿ���QPBOC��
 *
 * @return  {@link EMV_OPER_OK}                 �ɹ�
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_DEACTIVATE_CARD}
 */
int32_t deactivate_card_do(void)
{
//	while(API_ICC_ChkPreSent(ICC_NON_CONTSCT) != CARD_NOT_EXIST)
//	{
//		;
//	}

//	if (API_ICC_PwrOff(ICC_NON_CONTSCT))
//	{
//		return EMV_FAIL;
//	}
//	else
//	{
//		return EMV_OPER_OK;
//	}

	API_ICC_PwrOff(ICC_NON_CONTSCT);
	return EMV_OPER_OK;

}

/**
 * ����ֿ���֤����PBOC��
 *
 * @param   type                                [IN]֤�����ͣ��ο�{@link PBOC_DocumentTypes}
 * @param   num                                 [IN]֤�����루ASC��ʽ��
 * @param   numlen                              [IN]֤�����볤��
 * @return  {@link EMV_OPER_OK}                 ����ͨ��
 * @return  {@link EMV_OPER_CANCEL}             ���鲻ͨ��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CHECK_ID}
 */
int32_t check_id_do(uint8_t type, const uint8_t *num, int32_t numlen)
{
    int32_t ret;
    char typestr[][25] = {CN_EN("����֤", "Identity card"), CN_EN("����֤", "Official card"), CN_EN("����", "Passport"), CN_EN("�뾳֤", "Arrival card"), CN_EN("��ʱ����֤", "Temporary identity card"), CN_EN("����", "Others")};
    char tInfo[256]={0};
    char sID[41] = {0};

    if (numlen > 40)
    {
        numlen = 40;
    }
    API_memcpy(sID, num, numlen);

    API_sprintf(tInfo, "����:%s\n%s\n1.��ȷ��2.����", typestr[type], sID);

    ret = PUI_KEY_SELECT(CN_EN("���ʾ֤��", "Please show your credentials"), tInfo, 1,2, PUI_WAIT_INPUT_TIMEOUT_MS);

    // ����ֵ,����ֵת����emvspi_ack_examine_cred����ֵ
    if (ret == 1)
    {
      ret = 1; // ����
    }
    else
    {
      ret = 0; // ������
    }

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
	EMV_ResultCheckID(ret);//0-��ͨ����1-ͨ��
#endif

    return ret;
}



/**
 * ����PIN
 * @param   type                                [IN]����
 *   @arg   =0                                  ��������
 *   @arg   =1                                  �ѻ����루��ʾ���һ�λ��ᣩ
 *   @arg   >1                                  �ѻ�����
 * @param   output                              [OU]������棨�����������256�ֽڣ��ѻ��������12�ֽڣ�
 * @param   outlen                              [OU]������Ȼ��棨��������<=256��4<=�ѻ�����<=12��
 * @return  {@link EMV_OPER_OK}                 ����ɹ�
 * @return  {@link EMV_OPER_BYPASS}             �����루����������ֱ�Ӱ�ȷ�ϼ��������룩
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧�ֻ����쳣
 * @return  {@link EMV_OPER_TIMEOUT}            ��ʱ�˳�
 * @return  {@link EMV_OPER_CANCEL}             ����ȡ��
 * @return  {@link EMV_OPER_QUIT}               �����˳�
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_ONLINE_PIN}��{@link EMV_REQ_OFFLINE_PIN}��{@link EMV_REQ_OFFLINE_PIN_LAST}
 *
 * @note    ����PIN����ʱ�������Ӧ���Զ��壬�ں�ֻ���𱣴�\n
 *          �ѻ�PIN����ʱ�����������PIN�������ַ��������磺����"123456"����ôoutput="123456"������ֵ6
 */
int32_t pin_entry_do(uint8_t type, uint8_t* output, int32_t* outlen)
{
    int32_t ret;
    uint8_t *pro;
    uint8_t buf[20] = {0};
    uint8_t key[24] = {"\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"};//{"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"};
	uint8_t* pan;
	int32_t panlen;
	uint8_t panBuff[17], panXor[9], cardLen;
	int32_t i;
	uint8_t cardNo[20] = {0};

	uint8_t keyidx;
	uint8_t minlen;
	uint8_t maxlen;
	uint8_t timeout;
//	uint8_t moneyflag;

    if (type == 0)
    {
        pro = (uint8_t*)CN_EN("�������������룺\r\n(ͨ�������������)", "Please input online PIN: \r\n(Input by pinpad)");
    }
    else if (type == 1)
    {
        pro = (uint8_t*)CN_EN("���һ���ѻ����룺\r\n(ͨ�������������)", "Please input the last offline PIN: \r\n(Input by pinpad)");
    }
    else
    {
        pro = (uint8_t*)CN_EN("�������ѻ����룺\r\n(ͨ�������������)", "Please input offline PIN: \r\n(Input by pinpad)");
    }

    API_memset(buf, 0, sizeof(buf));
/////////////////////////////////////////////////////////////////
	pan = EMV_GetData(TAG_005A_APPLICATION_PAN, &panlen);
	if(pan==NULL || panlen>10)
	{
		return EMV_OPER_UNSUPPORTED;
	}
	pan[panlen] = 0x00;
	TRACE("****panlen:%d****", panlen);
	TRACE_HEX("****pan****", pan, panlen);
	//cardLen = API_strlen(pan);
	for(i=0;i<panlen;i++)
	{
		if(((pan[i]>>4)&0x0F) > 0x09)
		{
			cardLen = i*2;
			break;
		}
		if((pan[i]&0x0F) > 0x09)
		{
			cardLen = i*2 + 1;
			break;
		}

		if(i==(panlen-1))//���һ��
		{
			cardLen = (i+1)*2;
			break;
		}
	}
	API_Conv_BcdToStr(pan, panlen, (char*)cardNo);
	TRACE_HEX("****cardNo1****", cardNo+19, 1);
	cardNo[cardLen] = 0x00;
	TRACE_HEX("****cardNo2****", cardNo+19, 1);
	TRACE("****cardLen:%d****", cardLen);
	TRACE_HEX("****cardNo****", cardNo, cardLen);
	API_memset(panBuff, '0', 16);
	if(cardLen < 13)
	{
		API_memcpy(panBuff+4+13-cardLen, cardNo, 12-(13-cardLen));
	}
	else
	{
		API_memcpy(panBuff+4, cardNo+cardLen-13, 12);
	}
	panBuff[16] = 0x00;
	TRACE_HEX("****panBuff****", panBuff, 16);
	API_Conv_StrToBcd_Left((char*)panBuff, 16, panXor);
	TRACE_HEX("****panXor****", panXor, 8);
/////////////////////////////////////////////////////////////////
    /** �Զ�����֧��,ֱ�ӷ�������
    * ���ڷ��˹���Ԥ�����û�PIN,����֧�����ķ�ʽ�����������̷�ʽ���������������ֱ��Ӧ��
    */
    if (emvporting_autotest_get_flag())
    {
        emvporting_autotest_get_passwd(buf/*, 12*/);
        ret = API_strlen((const char*)buf);
        if (ret > 0)
        {
            TRACE_HEX("buf", buf, ret);

            // ȷ����ɼ�����ʾIC��������
            PUI_SHOWSTA(NULL, CN_EN("���ڴ���������ο���", "Processing, Dnon't remove card"));

            if (type == 0)
            {

                API_memset(output, 0xFF, 8);
                output[0] = ret;
                API_Conv_StrToBcd_Left((char*)buf, ret, output+1);
/////////////////////////////////////////////////////////////////
			for(i=0;i<8;i++)
			{
				buf[i] ^= panXor[i];
			}
/////////////////////////////////////////////////////////////////
            API_Crypt(CRYPT_TYPE_DES, key, API_strlen((const char*)key), NULL, output, 8, output, sizeof(buf), (uint32_t*)&ret, CRYPT_MODE_ENCRYPT);
			*outlen = 8;

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
		EMV_ResultOnlinePIN(EMV_OPER_OK, output, *outlen);
#endif

			return EMV_OPER_OK;
			//return 8;
            }
            else
            {
                API_memcpy(output, buf, ret);
                *(output + ret) = 0;
			*outlen = ret;

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
		EMV_ResultOfflinePIN(EMV_OPER_OK, output, *outlen);
#endif

			return EMV_OPER_OK;
                	//return ret;
            }
        }
    }

    // PUI_INPUT_PASSWD
    // @return >=0�ɹ�����PIN�ַ������������뷵��0
    // -2 CV_PIN_BYPASS
    // -3 CV_PIN_TIMEOUT
    // -4 CV_PIN_FAIL

    // ��������
    //ret = PUI_INPUT_PASSWD(NULL, (char *)pro, (char *)output, PUI_WAIT_INPUT_TIMEOUT_MS);
    PUI_SHOWSTA(NULL, (const char *)pro);
//    g_pe_var.minlen = 4;
//    g_pe_var.maxlen = 12;
//    g_pe_var.timeout = 65;
//    g_pe_var.moneyflag = 1;
//	g_pe_var.keyidx = 0;
    minlen = 4;
    maxlen = 12;
    timeout = 65;
//    moneyflag = 1;
	keyidx = 0;

//	PCI_ParaPwdOtherTest(NULL);
//	PCI_LoadKeyTest(NULL);
//	PCI_PinTest(NULL);

	API_PCI_SetPinblock_Xor_Padding((const char*)cardNo/*"000720002784932N"*/, "FFFFFFFFFFFFFFFF");//cardNo--���˺�panBuff--���˺Ŵ�����ȡ��16�ֽ�
	API_PCI_SetPrompt(NULL, "Pls Input PIN:", NULL, NULL);

	//ret = API_PCI_GetPin(0, PIN_VOICE_NONE, g_pe_var.minlen, g_pe_var.maxlen, TYPE_ANSI98, g_pe_var.keyidx, buf, sizeof(buf), (uint32_t*)outlen, g_pe_var.timeout);
	ret = API_PCI_GetPin(0, PIN_VOICE_NONE, minlen, maxlen, TYPE_ANSI98, keyidx, buf, sizeof(buf), (uint32_t*)outlen, timeout);
    //ret = PinPad_GetPin(g_pe_var.minlen,g_pe_var.maxlen,g_pe_var.timeout,NULL,g_pe_var.moneyflag,g_pe_var.extMoney,buf);
	// ȷ����ɼ�����ʾIC��������
    PUI_SHOWSTA(NULL, CN_EN("���ڴ���������ο���", "Processing, Dnon't remove card"));

    TRACE("PUI_INPUT_PASSWD = %d", ret);

    if (ret == 0)
    {
        TRACE_HEX("****EPIN****", buf, 8);
        if (type == 0)
        {
            API_memcpy(output, buf, 8);
		*outlen = 8;

 TRACE("EMV_ResultOnlinePIN !!! ");

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
		EMV_ResultOnlinePIN(EMV_OPER_OK, output, *outlen);
#endif

		return EMV_OPER_OK;
		//return 8;
        }
        else //FLY ���ܲ�����Ҫ�޸�
        {
            API_Crypt(CRYPT_TYPE_DES, key, API_strlen((const char*)key), NULL, buf, 8, buf, sizeof(buf), (uint32_t*)&ret, CRYPT_MODE_DECRYPT);
/////////////////////////////////////////////////////////////////
		for(i=0;i<8;i++)
		{
			buf[i] ^= panXor[i];
		}
/////////////////////////////////////////////////////////////////
            API_Conv_BcdToStr(buf+1, 6, (char*)output);
            *(output + buf[0]) = 0;
            TRACE_HEX("****PIN****", output, buf[0]);
		*outlen = buf[0];

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
		EMV_ResultOfflinePIN(EMV_OPER_OK, output, *outlen);
#endif

		return EMV_OPER_OK;
            	//return buf[0];
        }
    }
    else if (ret == RET_PCI_PWD_NULL) // û������ֱ�Ӱ�ȷ�ϼ�
    {
        return EMV_OPER_BYPASS;
    }
    else if (ret == RET_PCI_USER_CANCEL)
    {
        return EMV_OPER_CANCEL;
    }
    else if (ret == RET_PCI_USER_TIMEOUT) // CV_PIN_TIMEOUT
    {
        return EMV_OPER_TIMEOUT;
    }
    else
    {
        return EMV_OPER_UNSUPPORTED;
    }
}


/**
 * ȷ���Ƿ���������Уɣ�
 *
 * @return  {@link EMV_OPER_OK}                 ����
 * @return  {@link EMV_OPER_CANCEL}             ������
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CONFIRM_BYPASS_PIN}
 */
int32_t confirm_bypass_pin_do(void)
{

	int32_t ret, flag;

	if (emvporting_para_get_pin_pass_sw())
	{
		if(PUI_SHOWINFO(CN_EN("��ȷ��:", "Please confirm"), CN_EN("�Ƿ���������PIN\r\n��(ȷ�ϼ�) ��(ESC/CANCEL)", "Bypass PIN ?\r\nYes(ENTER) No(ESC/CANCEL)"), 60 * 1000))
		{
			ret = EMV_OPER_OK;
			flag = 1;
		}
		else
		{
			ret = EMV_OPER_CANCEL;
			flag = 0;
		}

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
		EMV_ResultBypassPIN(flag);//0--������ 1--����
#endif

	}
	else
	{
		ret = EMV_OPER_UNSUPPORTED;
	}

	return ret;

}

/**
 * ȷ���Ƿ�ǿ������
 *
 * @return  {@link EMV_OPER_OK}                 ǿ������
 * @return  {@link EMV_OPER_CANCEL}             ��ǿ������
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CONFIRM_FORCE_ONLINE}
 */
int32_t confirm_force_online_do(void)
{
	int32_t ret, flag;

	if (emvporting_para_get_force_online_sw())
	{
		if(PUI_SHOWINFO(CN_EN("��ȷ��:", "Please confirm"), CN_EN("�Ƿ�ǿ������\r\n��(ȷ�ϼ�) ��(ESC/CANCEL)", "Force online ?\r\nYes(ENTER) No(ESC/CANCEL)"), 60 * 1000))
		{
			ret = EMV_OPER_OK;
			flag = 1;
		}
		else
		{
			ret = EMV_OPER_CANCEL;
			flag = 0;
		}
#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
		EMV_ResultForceOnline(flag);//0-��ǿ��������1-ǿ������
#endif
	}
	else
	{
		ret = EMV_OPER_UNSUPPORTED;
	}

	return ret;

}


/**
 * ����������Ȩ
 *
 * @param   resp                                [OU]��Ȩ��Ӧ���ݣ��൱��55�����ݣ�TLV��ʽ��������Ȩ��Ӧ�룩
 * @param   resplen                             [OU]��Ȩ��Ӧ���ݳ���
 * @return  {@link EMV_ONLINE_NOT}              δ������������ֹ
 * @return  {@link EMV_ONLINE_FAIL}             ����ʧ��
 * @return  {@link EMV_ONLINE_SUCC_ACCEPT}      �����ɹ�����������׼
 * @return  {@link EMV_ONLINE_SUCC_DENIAL}      �����ɹ��������оܾ�
 * @return  {@link EMV_ONLINE_SUCC_ISSREF}      �����ɹ���������ָ��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_GO_ONLINE}
 */
int32_t go_online_do(uint8_t* resp, int32_t* resplen)
{
    return EMV_OPER_REPORT_APP;
}


/**
 * �����вο���������ȷ�Ͻ����Ƿ�������ܣ�
 *
 * @param   pan                                 [IN]Ӧ�����˺�
 * @param   panlen                              [IN]Ӧ�����˺ų���
 * @return  {@link EMV_OPER_OK}                 �������
 * @return  {@link EMV_OPER_CANCEL}             ����ܾ�
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_ISSUER_REFERRAL}
 */
int32_t issuer_referral_do(uint8_t* pan, int32_t panlen)
{
    int32_t i, ret;
    char buf[32]={0};
    char tInfo[256]={0};

    for (i = 0; i < panlen; i++) {
        API_sprintf(buf + i * 2, "%02x", *(pan + i));
    }

    API_sprintf(tInfo, "Ӧ�����ʺ�:%s\n1.��׼  2.�ܾ�", buf);

    // ����ֵ,����ֵת����emvspi_ack_examine_cred����ֵ
    ret = PUI_KEY_SELECT(CN_EN("����ϵ�������", "Please contact your bank"), tInfo, 1,2, PUI_WAIT_INPUT_TIMEOUT_MS);

    if(ret == 1)
    {
      ret = 1; // ����
    }
    else
    {
      ret = 0; // ������
    }

#if !EMVSPI_DO //����ں˲�����,��Ҫ�������ϱ��������������һ������
	EMV_ResultIssuerReferral(ret);//0-�ܾ���1-��׼
#endif

    return ret;
}

#endif

////////////////////////////////////////////////////////////////////////////////////

int32_t message(int32_t id)
{
    if(id>=EMV_MSG_CAPK_NotFound && id<=(EMV_MSG_MAX-1))
    {
        g_emvapi_messageinx = id;
    }
    else
    {
        g_emvapi_messageinx = -1;
    }
    return EMV_OPER_OK;

}

/**
 * ѡ���ѡӦ���б�
 *
 * @param   again                               [IN]�ٴ�ѡ����
 * @param   list                                [IN]��ѡӦ���б�����ΪNULL��
 * @param   count                               [IN]��ѡӦ�ø���������0��
 * @return  [0 ~ count-1]                       ѡ�е�Ӧ�ö�Ӧ��ѡӦ���б�������
 * @return  {@link EMV_OPER_CANCEL}             ȡ��
 * @return  {@link EMV_OPER_TIMEOUT}            ��ʱ
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_SEL_CANDIDATES}��{@link EMV_REQ_SEL_CANDIDATES_AGAIN}
 *
 * @note    �����֧�ָù��ܣ���ô����ֱ�ӷ���0ѡ���һ��Ӧ��
 */
int32_t sel_candidate_list(int32_t again, const EMV_CANDIDATE* list, int32_t count)
{
#if EMVSPI_DO
    return sel_candidate_list_do(again, list, count);
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ѡ���ʻ�����
 *
 * @return  {@link EMV_ACCOUNT_DEFAULT}         ѡ����Ĭ��-δָ����Default��
 * @return  {@link EMV_ACCOUNT_SAVINGS}         ѡ���˴����˻���Savings��
 * @return  {@link EMV_ACCOUNT_CHEQUE_DEBIT}    ѡ����֧Ʊ�˻�/����˻���Cheque/Debit��
 * @return  {@link EMV_ACCOUNT_CREDIT}          ѡ���������˻���Credit��
 * @return  {@link EMV_OPER_CANCEL}             ȡ��
 * @return  {@link EMV_OPER_TIMEOUT}            ��ʱ
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧�ָù���
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_SEL_ACCOUNTTYPE}
 */
int32_t sel_account_type(void)
{
#if EMVSPI_DO
    return sel_account_type_do();
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ��ʾ���ȴ��û��ƿ���QPBOC��
 *
 * @return  {@link EMV_OPER_OK}                 �ɹ�
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_DEACTIVATE_CARD}
 */
int32_t deactivate_card(void)
{
#if EMVSPI_DO
    return deactivate_card_do();
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ����ֿ���֤����PBOC��
 *
 * @param   type                                [IN]֤�����ͣ��ο�{@link PBOC_DocumentTypes}
 * @param   num                                 [IN]֤�����루ASC��ʽ��
 * @param   numlen                              [IN]֤�����볤��
 * @return  {@link EMV_OPER_OK}                 ����ͨ��
 * @return  {@link EMV_OPER_CANCEL}             ���鲻ͨ��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CHECK_ID}
 */
int32_t check_id(uint8_t type, const uint8_t *num, int32_t numlen)
{
#if EMVSPI_DO
    return check_id_do(type, num, numlen);
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ����PIN
 * @param   type                                [IN]����
 *   @arg   =0                                  ��������
 *   @arg   =1                                  �ѻ����루��ʾ���һ�λ��ᣩ
 *   @arg   >1                                  �ѻ�����
 * @param   output                              [OU]������棨�����������256�ֽڣ��ѻ��������12�ֽڣ�
 * @param   outlen                              [OU]������Ȼ��棨��������<=256��4<=�ѻ�����<=12��
 * @return  {@link EMV_OPER_OK}                 ����ɹ�
 * @return  {@link EMV_OPER_BYPASS}             �����루����������ֱ�Ӱ�ȷ�ϼ��������룩
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧�ֻ����쳣
 * @return  {@link EMV_OPER_TIMEOUT}            ��ʱ�˳�
 * @return  {@link EMV_OPER_CANCEL}             ����ȡ��
 * @return  {@link EMV_OPER_QUIT}               �����˳�
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_ONLINE_PIN}��{@link EMV_REQ_OFFLINE_PIN}��{@link EMV_REQ_OFFLINE_PIN_LAST}
 *
 * @note    ����PIN����ʱ�������Ӧ���Զ��壬�ں�ֻ���𱣴�\n
 *          �ѻ�PIN����ʱ�����������PIN�������ַ��������磺����"123456"����ôoutput="123456"������ֵ6
 */
int32_t pin_entry(uint8_t type, uint8_t* output, int32_t* outlen)
{
#if EMVSPI_DO
    return pin_entry_do(type, output, outlen);
#endif
    return EMV_OPER_REPORT_APP;
}

/**
 * ȷ���Ƿ���������Уɣ�
 *
 * @return  {@link EMV_OPER_OK}                 ����
 * @return  {@link EMV_OPER_CANCEL}             ������
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CONFIRM_BYPASS_PIN}
 */
int32_t confirm_bypass_pin(void)
{
#if EMVSPI_DO
    return confirm_bypass_pin_do();
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ȷ���Ƿ�ǿ������
 *
 * @return  {@link EMV_OPER_OK}                 ǿ������
 * @return  {@link EMV_OPER_CANCEL}             ��ǿ������
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CONFIRM_FORCE_ONLINE}
 */
int32_t confirm_force_online(void)
{
#if EMVSPI_DO
    return confirm_force_online_do();
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ����������Ȩ
 *
 * @param   resp                                [OU]��Ȩ��Ӧ���ݣ��൱��55�����ݣ�TLV��ʽ��������Ȩ��Ӧ�룩
 * @param   resplen                             [OU]��Ȩ��Ӧ���ݳ���
 * @return  {@link EMV_ONLINE_NOT}              δ������������ֹ
 * @return  {@link EMV_ONLINE_FAIL}             ����ʧ��
 * @return  {@link EMV_ONLINE_SUCC_ACCEPT}      �����ɹ�����������׼
 * @return  {@link EMV_ONLINE_SUCC_DENIAL}      �����ɹ��������оܾ�
 * @return  {@link EMV_ONLINE_SUCC_ISSREF}      �����ɹ���������ָ��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_GO_ONLINE}
 */
int32_t go_online(uint8_t* resp, int32_t* resplen)
{
#if EMVSPI_DO
    return go_online_do(resp, resplen);
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * �����вο���������ȷ�Ͻ����Ƿ�������ܣ�
 *
 * @param   pan                                 [IN]Ӧ�����˺�
 * @param   panlen                              [IN]Ӧ�����˺ų���
 * @return  {@link EMV_OPER_OK}                 �������
 * @return  {@link EMV_OPER_CANCEL}             ����ܾ�
 * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
 * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_ISSUER_REFERRAL}
 */
int32_t issuer_referral(uint8_t* pan, int32_t panlen)
{
#if EMVSPI_DO
    return issuer_referral_do(pan, panlen);
#endif
    return EMV_OPER_REPORT_APP;

}

/**
 * ����ȫ�ֻ��棬���ڴ��Ӧ�����ݣ����鿪7168�ֽ����ϣ�
 *
 * @param   size                                [OU]�����ȫ�ֻ����С
 * @return  ȫ�ֻ���ĵ�ַ
 *
 * @warning ȫ�ֻ����ڽ��������в���������\n
 *          ȫ�ֻ�������������ط����ã��ٴ�ʹ��EMVǰ�������³�ʼ��
 */

//extern uint8_t * weakbuf;

uint8_t* weak_alloc_buffer(int32_t* size)
{
    static uint8_t kernel_global_buffer[14*1024+256];

	eTRACE("kernel_global_buffer[%p]", &kernel_global_buffer);

    API_memset(kernel_global_buffer, 0, sizeof(kernel_global_buffer));
    *size = sizeof(kernel_global_buffer);
    return kernel_global_buffer;
}


uint8_t* emv_alloc_buffer(int32_t* size) {return weak_alloc_buffer(size);}


/**
 * ��Ƭ��д
 *
 * @param   type                                [IN]��Ƭ���ͣ��ο�{@link EMV_CardTypes}
 * @param   input                               [IN]�������ݣ�C-APDU��
 * @param   inlen                               [IN]�������ݳ���
 * @param   output                              [OU]������ݻ��棨����SW1��SW2��
 * @param   outlen                              [OU]������ݳ��Ȼ���
 * @return  ״̬��(SW)����0x9000,0x6A82,0x0000
 */
int32_t icc_rw(int32_t type, uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen)
{
    uint32_t sw = 0;
    int32_t ret = -1;
 	int32_t retlen = 0;
    TRACE("type[%d]", type);
    if (type == EMV_CARD_CONTACT)
    {
		return API_ICC_Apdu(ICC_CONTSCT, input, inlen, output, outlen);
    }
    else if (type == EMV_CARD_CONTACTLESS)
    {
        return API_ICC_Apdu(ICC_NON_CONTSCT, input, inlen, output, outlen);
    }
    else
    {

    }
    TRACE("ret:%d", ret);
    return 0x0000;
}


/**
 * ��Կ����
 *
 * @param   alg                                 [IN]��Կ�㷨��ʶ���ο�{@link EMV_PubKeyAlgorithms}
 * @param   pk                                  [IN]��Կ����ʽΪ{@link EMV_PUBLICKEY}
 * @param   input                               [IN]ԭʼ����
 * @param   output                              [OU]�ָ�����
 * @return  �ָ����ݳ��ȣ����ڹ�Կģ���ȣ���С�ڵ���0��ʾʧ��
 */
int32_t recover(int32_t alg, EMV_PUBLICKEY* pk, uint8_t* input, uint8_t* output)
{
    int32_t outlen = 0;
    int32_t ret;

    if (pk == NULL || input == NULL || output == NULL)
    {
        return -1;
    }

    if (alg == EMV_PUBKEY_ALG_RSA)
    {
        RSA_PUBKEY pubkey;

        if (pk->rsa.modulus_len == 0 ||
                (int32_t)pk->rsa.modulus_len > (int32_t)sizeof(pubkey.modulus))
        {
            TRACE("modulus_len Err[%d]", (int)pk->rsa.modulus_len);
            return -1;
        }
        /* 指数只允许 EMV 的 1 字节或 3 字节编码，且不能全 0 */
        if ((pk->rsa.exponent[0] | pk->rsa.exponent[1] | pk->rsa.exponent[2]) == 0)
        {
            TRACE("exponent Err");
            return -1;
        }

        CLEAR(pubkey);
        pubkey.length = pk->rsa.modulus_len;
        API_memcpy(pubkey.modulus, pk->rsa.modulus, pk->rsa.modulus_len);
        if (pk->rsa.exponent[0] == 0x00 && pk->rsa.exponent[1] == 0x00)
        {
            /* 1-byte exponent, stored at exponent[2] */
            pubkey.exponent[3] = pk->rsa.exponent[2];
        }
        else
        {
            /* 3-byte exponent, left padded to 4 bytes */
            API_memcpy(pubkey.exponent+1, pk->rsa.exponent, 3);
        }
//        TRACE_HEX("####PK####", (uint8_t*)&pubkey, sizeof(RSA_PUBKEY));
//        TRACE_HEX("####IN####", input, pk->rsa.modulus_len);
        ret = API_Crypt(CRYPT_TYPE_RSA, (uint8_t*)&pubkey, sizeof(pubkey),
                NULL, input, pk->rsa.modulus_len, output, &outlen, 0);
        if (ret != CRYPT_OK)
        {
            TRACE("API_Crypt Err[%d]", ret);
            return -1;
        }
        if (outlen != (int32_t)pk->rsa.modulus_len)
        {
//            TRACE("outlen Err[%d != %d]", outlen, pk->rsa.modulus_len);
//            TRACE_HEX("####OU####", output, pk->rsa.modulus_len);
            return -1;
        }
    }
    else
    {
        TRACE("alg Err[%d]", alg);
        return -1;
    }
//    TRACE_HEX("####OU####", output, outlen);
    return outlen;
}



/**
 * ��ϣ����
 *
 * @param   alg                                 [IN]��ϣ�㷨��ʶ���ο�{@link EMV_HashAlgorithms}
 * @param   input                               [IN]��������
 * @param   inlen                               [IN]�������ݳ���
 * @param   output                              [OU]��ϣֵ
 * @return  ��ϣֵ���ȣ����ݹ�ϣ�㷨��������С�ڵ���0��ʾʧ��
 */
int32_t hash(int32_t alg, uint8_t* input, int32_t inlen, uint8_t* output)
{
    int32_t outlen = 0;

    switch (alg)
    {
        case EMV_HASH_ALG_SHA1:
	        if (CRYPT_OK == API_Hash(HASH_TYPE_SHA1, input, inlen, output, HASH_DIRECT))
	        {
	            outlen = 20;
	        }
        break;
        case EMV_HASH_ALG_SHA256:

	        if (CRYPT_OK == API_Hash(HASH_TYPE_SHA256, input, inlen, output, HASH_DIRECT))
	        {
	            outlen = 32; //FIXME
	        }
        break;
        case EMV_HASH_ALG_SHA512:

	        if (CRYPT_OK == API_Hash(HASH_TYPE_SHA512, input, inlen, output, HASH_DIRECT))
	        {
	            outlen = 64; //FIXME
	        }
        break;
        case EMV_HASH_ALG_SM3:
	        if (CRYPT_OK == API_Hash(HASH_TYPE_SM3, input, inlen, output, HASH_DIRECT))
	        {
	            outlen = 32; //FIXME
	        }
        break;
    }
//    TRACE("outlen:%d", outlen);
//    TRACE_HEX("HASHVAL", output, outlen);
    return outlen;
}


/**
 * ��ȡ�����
 *
 * @param   buf                                 [OU]���������
 * @param   len                                 [IN]����������ȡ����
 * @return  0                                   �ɹ�
 * @return -1                                   ʧ��
 */
int32_t get_random(uint8_t *buf, int32_t len)
{
	uint8_t buf1[256] = {0};
	int32_t len1 = 0;
	int32_t ret;

	if (buf == NULL || len <= 0 || len > (int32_t)sizeof(buf1))
	{
		TRACE("random len Err[%d]", (int)len);
		return -1;
	}

	len1 = ((len + 3) / 4) * 4;
	if (len1 > (int32_t)sizeof(buf1))
	{
		len1 = (int32_t)sizeof(buf1);
	}

	ret = API_Rng(buf1, len1);
	if (ret != CRYPT_OK)
	{
		TRACE("API_Rng Err[%d]", ret);
		return -1;
	}

	API_memcpy(buf, buf1, len);
    TRACE_HEX("random", buf, len);
    return 0;
}


/**
 * ��ȡ��ǰ���ں�ʱ��
 *
 * @param   date                                [OU]���ڻ��棨��ʽBCD��YYYYMMDD, 4�ֽڣ�
 * @param   time                                [OU]ʱ�仺�棨��ʽBCD��hhmmss, 3�ֽڣ�
 * @return  0                                   �ɹ�
 * @return -1                                   ʧ��
 */
int32_t get_trans_date_time(uint8_t* date, uint8_t* time)
{
    DATE_TIME curDateTime;

    API_TimeGet(&curDateTime);

    if (date != NULL)
    {
        *(date    ) = FMT_NUMTOBCD((curDateTime.Year / 100) & 0xFF);
        *(date + 1) = FMT_NUMTOBCD((curDateTime.Year % 100) & 0xFF);
        *(date + 2) = FMT_NUMTOBCD((curDateTime.Month));
        *(date + 3) = FMT_NUMTOBCD((curDateTime.Day));
    }

    if (time != NULL)
    {
        *(time    ) = FMT_NUMTOBCD((curDateTime.Hour));
        *(time + 1) = FMT_NUMTOBCD((curDateTime.Min));
        *(time + 2) = FMT_NUMTOBCD((curDateTime.Sec));
    }

    return 0;
}


/**
 * ��ȡ�������м�������9F41��
 *
 * @param   buf                                 [OU]������棨BCD��ʽ, 4�ֽڣ�
 * @return  0                                   �ɹ�
 * @return -1                                   ʧ��
 */
int32_t get_trans_sequence_counter(uint8_t* buf)
{
    int32_t  tsc;
    char tempbuf[8+1] = {0};

    tsc = emvporting_get_tsc();
    tsc++;
    if (tsc > 99999999 || tsc <= 0)
    {
        tsc = 1;
    }
    emvporting_set_tsc(tsc);

    // ת����ʽ
    API_sprintf((char *)tempbuf, "%08d", tsc);
    API_Conv_StrToBcd_Left(tempbuf, 8, buf);

    return 0;
}



/**
 * ��ȡ�ն����ò���
 *
 * @param   par     [OU]�ն����ò������ο�{@link EMV_TERMPARAM}
 * @return  =0      �ɹ�
 * @return  <0      ʧ��
 * @note    Only the minimal retained field set is returned (see emv_type.h);
 *          use emv_get_termparam() for the complete parameter file as raw TLV.
 */
int32_t get_termparam(EMV_TERMPARAM* par)
{
#if !EMV_RELEASE_V
    return emvparanew_get_termparam(par);
#else
    return emvporting_get_termparam(par);
#endif

}

/**
 * ��ȡ�ն�Ӧ���б�
 *
 * @param   apps    [OU]�ն�Ӧ���б����飬�ο�{@link EMV_TERMAPP}
 * @return  �ն�֧�ֵ�Ӧ�ø������ں����֧��128��
 */
int32_t get_termapps(EMV_TERMAPP* apps)
{
#if !EMV_RELEASE_V
    return emvparanew_get_termapps(apps);
#else
    return emvporting_get_termapps(apps);
#endif

}

/**
 * ��ȡӦ�ò���
 *
 * @param   par     [OU]Ӧ�ò������ο�{@link EMV_AIDPARAM}
 * @param   inx     [IN]Ӧ�ö�Ӧ�ն�Ӧ���б�������
 * @return  =0      �ɹ�
 * @return  <0      ʧ��
 * @note    Only the fields EMV_PreProcess() consumes are filled (see
 *          emv_type.h); kernels use emv_get_aidparam()/..._aidparam_tlv().
 */
int32_t get_aidparam(EMV_AIDPARAM* par, int32_t inx)
{
#if !EMV_RELEASE_V
    return emvparanew_get_aidparam(par, inx);
#else
    return emvporting_get_aidparam(par, inx);
#endif

}

/**
 * ��ȡCA��Կ֤��
 *
 * @param   crt     [OU]CA��Կ֤�飬�ο�{@link EMV_CAPKPARAM}
 * @param   pki     [IN]CA��Կ����
 * @param   rid     [IN]CA��Կ��Ӧ��RID��5�ֽڣ�
 * @return  =0      �ɹ�
 * @return  <0      ʧ��
 */
int32_t get_capkparam(EMV_CAPKPARAM* crt, uint8_t pki, const uint8_t* rid)
{
    return emvporting_get_capkparam(crt, pki, rid);

}

/**
 * ��ȡ�ѻ���׼������־��ָ�����˺ŵĽ��׽��
 *
 * @param   pan     [IN]Ӧ�����˺�
 * @param   panlen  [IN]Ӧ�����˺ų���
 * @param   policy  [IN]��ȡ���ԣ�0-ĩ�ʽ��׽�1-���н����ܽ�
 * @return  ĩ�ʽ��׽������н����ܽ��
 */
uint32_t get_translog_amount(const uint8_t* pan, uint8_t panlen, uint8_t policy)
{
    return emvporting_get_translog_amount(pan, panlen, policy);

}

/**
 * �����ѻ���׼������־
 *
 * @return  0       ����ɹ�
 * @return -1       ����ʧ��
 */
int32_t sav_translog(const uint8_t* pan, uint8_t panlen, uint32_t amount)
{
    if (emvporting_sav_translog(pan, panlen, amount))
    {
        return -1;
    }
    return 0;

}

/**
 * �������˺��Ƿ��ڿ�Ƭ���������쳣�ļ�����
 *
 * @param   pan     [IN]Ӧ�����˺ţ�BCD��
 * @param   panlen  [IN]Ӧ�����˺ų��ȣ����10�ֽڣ�
 * @param   seq     [IN]Ӧ�����˺����к�
 * @return  0       �����˺Ų��ں�������
 * @return  1       �����˺����ں�������
 */
int32_t fnd_blackcard(const uint8_t* pan, uint8_t panlen, uint8_t seq)
{
    return emvporting_fnd_cardblack(pan, panlen, seq);

}

/**
 * ����֤���Ƿ���֤���������֤������б�����
 *
 * @param   rid     [IN]ע���Ӧ���ṩ�̱�ʶ��5�ֽڣ�
 * @param   pki     [IN]��֤���Ĺ�Կ����
 * @param   csn     [IN]��Կ֤�����кţ�3�ֽڣ�
 * @return  0       ��֤�鲻�ں�������
 * @return  1       ��֤�����ں�������
 */
int32_t fnd_blackcert(const uint8_t* rid, uint8_t pki, const uint8_t* csn)
{
    return emvporting_fnd_certblack(rid, pki, csn);

}


/**
 * ��ȡ�ں�����
 *
 * @return  �ں�����
 */
uint8_t get_kernel_index(void)
{
    uint8_t val = 0;
    emvporting_get_kernel_index(&val);
    return val;

}

/**
 * ��Ƭ�ϵ�
 *
 * @return  0--�ɹ�  1--����ͻ  <0--ʧ��
 */
int32_t icc_pwron(void)
{
    int32_t ret = -1;

    TRACE("ret:%d", ret);
    return ret;
}

const EMV_PORTING g_emv_porting =
{
    .message = message,
    .sel_candidate_list = sel_candidate_list,
    .sel_account_type = sel_account_type,
    .deactivate_card = deactivate_card,
    .check_id = check_id,
    .pin_entry = pin_entry,
    .confirm_bypass_pin = confirm_bypass_pin,
    .confirm_force_online = confirm_force_online,
    .go_online = go_online,
    .issuer_referral = issuer_referral,

    .alloc_buffer = emv_alloc_buffer,
    .icc_rw = icc_rw,
    .recover = recover,
    .hash = hash,
    .get_random = get_random,
    .get_trans_date_time = get_trans_date_time,

    .get_trans_sequence_counter = get_trans_sequence_counter,
//    .get_termparam = get_termparam,
    .get_termapps = get_termapps,
    .get_aidparam = get_aidparam,
    .get_capkparam = get_capkparam,
    .get_translog_amount = get_translog_amount,
    .sav_translog = sav_translog,
    .fnd_blackcard = fnd_blackcard,
    .fnd_blackcert = fnd_blackcert,


    .get_kernel_index = get_kernel_index,
    .icc_pwron = icc_pwron,

};


/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
