#include "emvi.h"
#include "emvoutcome.h"

#define OUTCOME_FLAG	1

EMV_OUTCOME_DATA g_outcome = {0};
EMV_UIREQ_DATA g_uireq = {0};
EMV_OUTCOME_CALLBACK g_send_outcome_cb = {0};
EMV_ERROR_INDICATION g_err_indicator = {0};
unsigned char g_discretionary_data[1200] = {0};
int g_discretionary_data_len = 0;
unsigned char g_data_record[1200] = {0};
int g_data_record_len = 0;

int EMV_SetOutcome_Callback(void(*func_ptr)(EMV_OUTCOME_DATA data))
{
	g_send_outcome_cb.pSendOutcome = func_ptr;
	return EMV_OK;
}

int EMV_SetUIRequest_Callback(void(*func_ptr)(EMV_UIREQ_DATA data, unsigned char type))
{
	g_send_outcome_cb.pSendUIRequest = func_ptr;
	return EMV_OK;
}

void EMV_InitErrorIndication()
{
	emv_memset((void *)&g_err_indicator, 0, sizeof(EMV_ERROR_INDICATION));
	g_err_indicator.MsgOnError = UIREQUEST_MESSAGEID_NA;
//	appdata_save(TAG_DF8115_ERROR_INDICATION, (unsigned char *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
}

void EMV_InitDataRecord()
{
	emv_memset((void *)&g_data_record, 0, sizeof(g_data_record));
	g_data_record_len = 0;
}

void EMV_InitDiscData()
{
	emv_memset((void *)&g_discretionary_data, 0, sizeof(g_discretionary_data));
	g_discretionary_data_len = 0;
}

void EMV_InitOutCome()
{
	emv_memset((void *)&g_outcome, 0, sizeof(EMV_OUTCOME_DATA));
	g_outcome.Status = OUTCOME_STATUS_NA;
	g_outcome.Start = OUTCOME_START_NA;
	g_outcome.OnlineRespData = OUTCOME_ONLINE_RESP_DATA_NA;
	g_outcome.CVM = OUTCOME_CVM_NA;
	g_outcome.PresentFlag = 0x10;
	g_outcome.InterfacePrefer = OUTCOME_AIP_NA;
	g_outcome.FieldoffReq = OUTCOME_FIELDOFF_NA;
	emv_memset(&g_outcome.RemovalTimeout, 0, sizeof(g_outcome.RemovalTimeout));
}

void EMV_SetOutCome(unsigned char status, unsigned char start, unsigned char online_data, unsigned char cvm, unsigned char ui_on_outcome, unsigned char ui_on_restart, unsigned char data_record_pre, unsigned char disc_data_pre, unsigned char receipt, unsigned char aip, unsigned char field_off_time, unsigned char removal_timeout)
{
	unsigned char flag = 0x00;

	g_outcome.Status = status;
	g_outcome.Start = start;
	g_outcome.OnlineRespData = online_data;
	g_outcome.CVM = cvm;

	if(ui_on_outcome)
	{
		flag |= 0x80;
	}
	if(ui_on_restart)
	{
		flag |= 0x40;
	}
	if(data_record_pre)
	{
		flag |= 0x20;
	}
	if(disc_data_pre)
	{
		flag |= 0x10;
	}
	if(receipt)
	{
		flag |= 0x08;
	}

	g_outcome.PresentFlag = flag;
	g_outcome.InterfacePrefer = aip;
	g_outcome.FieldoffReq = field_off_time;
	g_outcome.RemovalTimeout = removal_timeout;
}

void EMV_InitUIRequest()
{
	g_uireq.MessageID = 0x00;
	g_uireq.Status = UIREQUEST_STATUS_NA;

	emv_memset(&g_uireq.HoldTime, 0, sizeof(g_uireq.HoldTime));
	emv_memset(&g_uireq.LanguagePrefer, 0, sizeof(g_uireq.LanguagePrefer));

//	g_uireq.ValueQualifier = 0x00;
//
//	emv_memset(&g_uireq.Value, 0, sizeof(g_uireq.Value));
//	emv_memset(&g_uireq.CurrencyCode, 0, sizeof(g_uireq.CurrencyCode));
}

void EMV_SetUIRequest(unsigned char message_id, unsigned char status, unsigned char *hold_time, unsigned char *language_pre, unsigned char value_qualifier, unsigned char *value, unsigned char *currency_code)
{
	g_uireq.MessageID = message_id;
	g_uireq.Status = status;

	if(hold_time != NULL)
	{
		emv_memcpy(&g_uireq.HoldTime, hold_time, sizeof(g_uireq.HoldTime));
	}

	if(language_pre != NULL)
	{
		emv_memcpy(&g_uireq.LanguagePrefer, language_pre, sizeof(g_uireq.LanguagePrefer));
	}

//	g_uireq.ValueQualifier = value_qualifier;
//
//	if(value != NULL)
//	{
//		emv_memcpy(&g_uireq.Value, value, sizeof(g_uireq.Value));
//	}
//
//	if(currency_code != NULL)
//	{
//		emv_memcpy(&g_uireq.CurrencyCode, currency_code, sizeof(g_uireq.CurrencyCode));
//	}
}

int outcome_other_card(unsigned char data_record, unsigned char receipt)
{
	unsigned char hold_time[3]={0x00,0x00,0x13},lang_pre[8]={0};
	int hold_time_len,lang_pre_len;
	unsigned char *hold_time_ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &hold_time_len);
	unsigned char *lang_pre_ptr = appdata_find(TAG_5F2D_LANGUAGE_PREFERENCE, &lang_pre_len);

	eTRACE("paypass-info: outcome_other_card");

#if OUTCOME_FLAG
	if(hold_time_ptr && hold_time_len > 0)
	{
		if(hold_time_len > sizeof(hold_time))
		{
			hold_time_len = sizeof(hold_time);
		}
		emv_memcpy(hold_time, hold_time_ptr, hold_time_len);
	}

	if(lang_pre_ptr && lang_pre_len > 0)
	{
		if(lang_pre_len > sizeof(lang_pre_ptr))
		{
			lang_pre_len = sizeof(lang_pre_ptr);
		}
		emv_memcpy(lang_pre, lang_pre_ptr, lang_pre_len);
	}

	//S52.18
	EMV_SetUIRequest(UIREQUEST_MESSAGEID_ERROR_OTHER_CARD, UIREQUEST_STATUS_NOT_READY, hold_time, lang_pre, 0, NULL, NULL);

	if(g_send_outcome_cb.pSendUIRequest != NULL)
	{
		g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_OUTCOME);
	}
	else
	{
		JNISendUIReq(g_uireq, UI_TYPE_ON_OUTCOME);
	}

	//S52.19
	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_NA, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 1, 0, data_record, 1, receipt, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x0D);
	if(g_send_outcome_cb.pSendOutcome != NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{

		JNISendOutcome(g_outcome);
	}
#endif

	return EMV_ENDAPPLICATION;
}

int outcome_select_next()
{
	eTRACE("paypass-info: outcome_select_next");

#if OUTCOME_FLAG

	EMV_SetOutCome(OUTCOME_STATUS_SELECT_NEXT, OUTCOME_START_C, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 0, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome != NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}

#endif

	return EMV_REQ_SELECT_NEXT;
}

int outcome_try_again_without_restart()
{
	unsigned char hold_time[3]={0x00,0x00,0x13};
	int len=0,i=0;
	unsigned char *ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &len);

	eTRACE("paypass-info: outcome_try_again_without_restart");

#if OUTCOME_FLAG
	emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
	g_discretionary_data_len = 0;

	emv_memcpy(g_discretionary_data+i, "\xDF\x81\x15\x06", 4);
	i += 4;
	appdata_save(TAG_DF8115_ERROR_INDICATION, (unsigned char *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
	emv_memcpy(g_discretionary_data+i, (void *)&g_err_indicator, sizeof(g_err_indicator));
	i += sizeof(g_err_indicator);

	g_discretionary_data_len = i;

	appdata_save(TAG_FF8106_DISCRETIONARY_DATA, g_discretionary_data, g_discretionary_data_len, OVERRIDED);

	EMV_SetOutCome(OUTCOME_STATUS_TRY_ANOTHER_AGAIN, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 0, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}
#endif
	return EMV_REQ_RETAP_AGAIN;
}

int outcome_try_again_end_application()
{
	unsigned char hold_time[3]={0x00,0x00,0x13},lang_pre[8]={0};
	int hold_time_len,lang_pre_len;
	unsigned char *hold_time_ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &hold_time_len);
	unsigned char *lang_pre_ptr = appdata_find(TAG_5F2D_LANGUAGE_PREFERENCE, &lang_pre_len);

	eTRACE("paypass-info: outcome_try_again_end_application");

#if OUTCOME_FLAG

	if(hold_time_ptr && hold_time_len > 0)
	{
		if(hold_time_len > sizeof(hold_time))
		{
			hold_time_len = sizeof(hold_time);
		}
		emv_memcpy(hold_time, hold_time_ptr, hold_time_len);
	}

	if(lang_pre_ptr && lang_pre_len > 0)
	{
		if(lang_pre_len > sizeof(lang_pre_ptr))
		{
			lang_pre_len = sizeof(lang_pre_ptr);
		}
		emv_memcpy(lang_pre, lang_pre_ptr, lang_pre_len);
	}

	EMV_SetUIRequest(UIREQUEST_MESSAGEID_TRY_AGAIN, UIREQUEST_STATUS_READY_TO_READ, hold_time, lang_pre, 0, NULL, NULL);

	if(g_send_outcome_cb.pSendUIRequest != NULL)
	{
		g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_RESTART);
	}
	else
	{
		JNISendUIReq(g_uireq, UI_TYPE_ON_RESTART);
	}

	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 1, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}
#endif
	return EMV_REQ_RETAP_AGAIN;
}

int outcome_try_again()
{
	unsigned char hold_time[3]={0x00};
	int len=0,i=0;
	unsigned char *ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &len);

	eTRACE("paypass-info: outcome_try_again");

#if OUTCOME_FLAG
	emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));

	emv_memcpy(g_discretionary_data+i, "\xDF\x81\x15\x06", 4);
	i += 4;
	appdata_save(TAG_DF8115_ERROR_INDICATION, (unsigned char *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
	emv_memcpy(g_discretionary_data+i, (void *)&g_err_indicator, sizeof(g_err_indicator));



	EMV_SetOutCome(OUTCOME_STATUS_TRY_ANOTHER_AGAIN, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 0, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}
#endif
	return EMV_REQ_RETAP_AGAIN;
}


int outcome_end_application()
{
	unsigned char hold_time[3]={0x00};
	int len=0;
	unsigned char *ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &len);

	eTRACE("paypass-info: outcome_end_application");

#if OUTCOME_FLAG
	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_NA, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 0, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}

#endif

	return EMV_ENDAPPLICATION;
}

int outcome_seephone()
{
	unsigned char hold_time[3]={0x00},lang_pre[8]={0};
	int hold_time_len,lang_pre_len;
	unsigned char *hold_time_ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &hold_time_len);
	unsigned char *lang_pre_ptr = appdata_find(TAG_5F2D_LANGUAGE_PREFERENCE, &lang_pre_len);


	eTRACE("paypass-info: outcome_seephone");

#if OUTCOME_FLAG

	if(hold_time_ptr && hold_time_len > 0)
	{
		if(hold_time_len > sizeof(hold_time))
		{
			hold_time_len = sizeof(hold_time);
		}
		emv_memcpy(hold_time, hold_time_ptr, hold_time_len);
	}

	if(lang_pre_ptr && lang_pre_len > 0)
	{
		if(lang_pre_len > sizeof(lang_pre_ptr))
		{
			lang_pre_len = sizeof(lang_pre_ptr);
		}
		emv_memcpy(lang_pre, lang_pre_ptr, lang_pre_len);
	}

	emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
	appdata_save(TAG_DF8115_ERROR_INDICATION, (unsigned char *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
	emv_memcpy(g_discretionary_data, (void *)&g_err_indicator, sizeof(g_err_indicator));

	EMV_SetUIRequest(UIREQUEST_MESSAGEID_SEEPHONE, UIREQUEST_STATUS_READY_TO_READ, hold_time, lang_pre, 0, NULL, NULL);
	if(g_send_outcome_cb.pSendUIRequest != NULL)
	{
		g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_RESTART);
	}
	else
	{
		JNISendUIReq(g_uireq, UI_TYPE_ON_MSG);
	}


	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 0, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}

#endif

	return EMV_SEEPHONE;
}

int outcome_end_application_startB()
{
	unsigned char hold_time[3]={0x00};
	int len=0;
	unsigned char *ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &len);

	eTRACE("paypass-info: outcome_end_application_startB");

#if OUTCOME_FLAG

	emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
	appdata_save(TAG_DF8115_ERROR_INDICATION, (unsigned char *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
	emv_memcpy(g_discretionary_data, (void *)&g_err_indicator, sizeof(g_err_indicator));

	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 1, 1, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		JNISendOutcome(g_outcome);
	}

#endif

	return EMV_ENDAPPLICATION;
}

void msg_read_card_ok()
{
	unsigned char hold_time[3]={0},lang_pre[8]={0};
	int len;
	unsigned char *ptr = appdata_find(TAG_5F2D_LANGUAGE_PREFERENCE, &len);

	API_Beep(2700, 500);

	if(ptr && len > 0)
	{
		if(len > sizeof(lang_pre))
		{
			len = sizeof(lang_pre);
		}
		emv_memcpy(lang_pre, ptr, len);
	}

	eTRACE("paypass-info: msg_read_card_ok");
#if OUTCOME_FLAG
	EMV_SetUIRequest(UIREQUEST_MESSAGEID_CLEAR_DISPLAY, UIREQUEST_STATUS_CARD_READ_SUCCESS, hold_time, lang_pre, 0, NULL, NULL);

	if(g_send_outcome_cb.pSendUIRequest != NULL)
	{
		g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_MSG);
	}
	else
	{
		JNISendUIReq(g_uireq, UI_TYPE_ON_MSG);
	}
#endif
}

int outcome_request_another_payment()
{
	return EMV_ENDAPPLICATION;
}

//int outcome_approved_transactions()
//{
//	unsigned char *cvm_res = NULL;
//	unsigned char cvm = g_emv_cnxt->cvm;
//
//	eTRACE("paypass-info: start outcome_approved_transactions");
//#if OUTCOME_FLAG
//	cvm_res = appdata_find(TAG_9F34_CVM_RESULTS, NULL);
//	if(cvm_res != 0x00 && cvm_res != 0x02)
//	{
//		cvm = OUTCOME_CVM_NA;
//	}
//
//	if((OUTCOME_CVM_OBTAIN_SIGNATURE == cvm) && (cvm_res != 0x01))
//	{
//		EMV_SetUIRequest(UIREQUEST_MESSAGEID_APPROVED_AND_SIGN, UIREQUEST_STATUS_CARD_READ_SUCCESS, NULL, NULL, 0, NULL, NULL);
//	}
//	else
//	{
//		EMV_SetUIRequest(UIREQUEST_MESSAGEID_APPROVED, UIREQUEST_STATUS_CARD_READ_SUCCESS, NULL, NULL, 0, NULL, NULL);
//	}
//
//	if(g_send_outcome_cb.pSendUIRequest != NULL)
//	{
//		g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_OUTCOME);
//	}
//	else
//	{
//		JNISendUIReq(g_uireq, UI_TYPE_ON_OUTCOME);
//	}
//
//	EMV_SetOutCome(OUTCOME_STATUS_APPROVED, OUTCOME_START_NA, OUTCOME_ONLINE_RESP_DATA_ANY, cvm, 1, 1, 1, 1, 0, OUTCOME_AIP_NA, 0x13, 0x00);
//	if(g_send_outcome_cb.pSendOutcome != NULL)
//	{
//		g_send_outcome_cb.pSendOutcome(g_outcome);
//	}
//
//#endif
//	return EMV_ACCEPTED_OFFLINE;
//}

//int outcome_online_request()
//{
//	unsigned char *cvm_res = NULL;
//	unsigned char cvm = g_emv_cnxt->cvm;
//	unsigned char hold_time[3]={0x00};
//	int len=0;
//	unsigned char *ptr = appdata_find(TAG_DF812D_MESSAGE_HOLD_TIME, &len);
//
//
//	eTRACE("paypass-info: start outcome_online_request");
//#if OUTCOME_FLAG
//	cvm_res = appdata_find(TAG_9F34_CVM_RESULTS, NULL);
//	if(cvm_res != 0x00 && cvm_res != 0x02)
//	{
//		cvm = OUTCOME_CVM_NA;
//	}
//
//	if((OUTCOME_CVM_OBTAIN_SIGNATURE == cvm) && (cvm_res != 0x01))
//	{
//		EMV_SetUIRequest(UIREQUEST_MESSAGEID_APPROVED_AND_SIGN, UIREQUEST_STATUS_CARD_READ_SUCCESS, NULL, NULL, 0, NULL, NULL);
//		g_emv_cnxt->receipt = 1;
//	}
//	else
//	{
//		EMV_SetUIRequest(UIREQUEST_MESSAGEID_APPROVED, UIREQUEST_STATUS_CARD_READ_SUCCESS, NULL, NULL, 0, NULL, NULL);
//	}
//
//	if(g_send_outcome_cb.pSendUIRequest != NULL)
//	{
//		g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_OUTCOME);
//	}
//	else
//	{
//		JNISendUIReq(g_uireq, UI_TYPE_ON_OUTCOME);
//	}
//
//	EMV_SetOutCome(OUTCOME_STATUS_ONLINE_REQUEST, OUTCOME_START_NA, OUTCOME_ONLINE_RESP_DATA_NA, cvm, 1, 0, 1, 1, g_emv_cnxt->receipt, OUTCOME_AIP_NA, 0x13, 0x00);
//	if(g_send_outcome_cb.pSendOutcome != NULL)
//	{
//		g_send_outcome_cb.pSendOutcome(g_outcome);
//	}
//
//#endif
//	return EMV_ACCEPTED_OFFLINE;
//}

