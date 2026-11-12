#include "emvi.h"
#include "emv_kernel.h"
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
	outcome_reset_final_flag();
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
		if(lang_pre_len > sizeof(lang_pre))
		{
			lang_pre_len = sizeof(lang_pre);
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
		/* platform UI request goes through g_send_outcome_cb only */
	}

	//S52.19
	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_NA, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 1, 0, data_record, 1, receipt, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x0D);
	if(g_send_outcome_cb.pSendOutcome != NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{

		/* platform outcome goes through g_send_outcome_cb only */
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
		/* platform outcome goes through g_send_outcome_cb only */
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
		/* platform outcome goes through g_send_outcome_cb only */
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
		if(lang_pre_len > sizeof(lang_pre))
		{
			lang_pre_len = sizeof(lang_pre);
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
		/* platform UI request goes through g_send_outcome_cb only */
	}

	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 1, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		/* platform outcome goes through g_send_outcome_cb only */
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
		/* platform outcome goes through g_send_outcome_cb only */
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
		/* platform outcome goes through g_send_outcome_cb only */
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
		if(lang_pre_len > sizeof(lang_pre))
		{
			lang_pre_len = sizeof(lang_pre);
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
		/* platform UI request goes through g_send_outcome_cb only */
	}


	EMV_SetOutCome(OUTCOME_STATUS_END_APPLICATION, OUTCOME_START_B, OUTCOME_ONLINE_RESP_DATA_NA, OUTCOME_CVM_NA, 0, 0, 0, 1, 0, OUTCOME_AIP_NA, OUTCOME_FIELDOFF_NA, 0x00);
	if(g_send_outcome_cb.pSendOutcome
!= NULL)
	{
		g_send_outcome_cb.pSendOutcome(g_outcome);
	}
	else
	{
		/* platform outcome goes through g_send_outcome_cb only */
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
		/* platform outcome goes through g_send_outcome_cb only */
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
		/* platform UI request goes through g_send_outcome_cb only */
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
//		/* platform UI request goes through g_send_outcome_cb only */
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
//		/* platform UI request goes through g_send_outcome_cb only */
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

/* ---------------------------------------------------------------------------
 * G5: final Outcome assembly (EMVCo Book B 3.5 / Book A Table 6-2, 11 params).
 * Merges the scheme-specific outcome of the selected kernel when available;
 * otherwise the Book B status is derived from the Entry Point status code.
 * Only the three fields the EP must act on are interpreted (UI request, Hold/
 * Field-off, Start); the rest are passed through untouched to the reader.
 * ------------------------------------------------------------------------- */
static int s_final_outcome_sent = 0;

void outcome_reset_final_flag(void)
{
    s_final_outcome_sent = 0;
}

int outcome_final_transaction(int32_t ep_status)
{
    EMV_KERNEL_OUTCOME ko;
    unsigned char status = OUTCOME_STATUS_NA;
    unsigned char start = OUTCOME_START_NA;
    unsigned char online_data = OUTCOME_ONLINE_RESP_DATA_NA;
    unsigned char cvm = OUTCOME_CVM_NA;
    unsigned char ui_msg = UIREQUEST_MESSAGEID_NA;
    unsigned char ui_on_outcome = 0;
    unsigned char ui_on_restart = 0;
    unsigned char data_record = 0;
    unsigned char disc_data = 0;
    unsigned char receipt = 0;
    unsigned char aip = OUTCOME_AIP_NA;
    unsigned char field_off = OUTCOME_FIELDOFF_NA;
    unsigned char removal = 0;
    unsigned char hold_time[3] = {0x00, 0x00, 0x13};
    unsigned char lang_pre[8] = {0};
    int lang_pre_len = 0;
    unsigned char *lang_pre_ptr;

    if (s_final_outcome_sent)
    {
        return EMV_OK;
    }

    switch (ep_status)
    {
        case EMV_ACCEPTED_OFFLINE:
        case EMV_ACCEPTED_ONLINE:
            status = OUTCOME_STATUS_APPROVED;
            ui_msg = UIREQUEST_MESSAGEID_APPROVED;
            break;
        case EMV_DENIALED_OFFLINE:
        case EMV_DENIALED_ONLINE:
            status = OUTCOME_STATUS_DECLINED;
            ui_msg = UIREQUEST_MESSAGEID_DECLINED;
            break;
        case EMV_SEEPHONE:
            status = OUTCOME_STATUS_TRY_ANOTHER_INTERFACE;
            ui_msg = UIREQUEST_MESSAGEID_SEEPHONE;
            break;
        case EMV_ENDAPPLICATION:
        case EMV_USE_ANOTHER_CARD:
        case EMV_USE_OTHERCARD:
            status = OUTCOME_STATUS_END_APPLICATION;
            ui_msg = UIREQUEST_MESSAGEID_ERROR_OTHER_CARD;
            break;
        default:
            return EMV_OK;      /* intermediate request, not a final outcome */
    }

    if (EMV_GetKernelOutcome(&ko) == 0 &&
        ko.status >= OUTCOME_STATUS_APPROVED &&
        ko.status <= OUTCOME_STATUS_TRY_ANOTHER_AGAIN)
    {
        status = ko.status;
        start = ko.start;
        online_data = ko.online_resp_data;
        cvm = ko.cvm;
        ui_on_outcome = (ko.present_flag & 0x80) ? 1 : 0;
        ui_on_restart = (ko.present_flag & 0x40) ? 1 : 0;
        data_record = (ko.present_flag & 0x20) ? 1 : 0;
        disc_data = (ko.present_flag & 0x10) ? 1 : 0;
        receipt = (ko.present_flag & 0x08) ? 1 : 0;
        aip = ko.interface_preference;
        field_off = ko.field_off_req;
        removal = ko.removal_timeout[0];
    }
    else
    {
        if (status == OUTCOME_STATUS_APPROVED || status == OUTCOME_STATUS_DECLINED)
        {
            ui_on_outcome = 1;
            data_record = (g_data_record_len > 0) ? 1 : 0;
        }
        else if (status == OUTCOME_STATUS_TRY_ANOTHER_INTERFACE)
        {
            ui_on_outcome = 1;
        }
    }

    if (ui_on_outcome && ui_msg != UIREQUEST_MESSAGEID_NA)
    {
        lang_pre_ptr = appdata_find(TAG_5F2D_LANGUAGE_PREFERENCE, &lang_pre_len);
        if (lang_pre_ptr && lang_pre_len > 0)
        {
            if (lang_pre_len > (int)sizeof(lang_pre))
            {
                lang_pre_len = (int)sizeof(lang_pre);
            }
            emv_memcpy(lang_pre, lang_pre_ptr, lang_pre_len);
        }
        EMV_SetUIRequest(ui_msg, UIREQUEST_STATUS_READY_TO_READ, hold_time, lang_pre, 0, NULL, NULL);
        if (g_send_outcome_cb.pSendUIRequest != NULL)
        {
            g_send_outcome_cb.pSendUIRequest(g_uireq, UI_TYPE_ON_OUTCOME);
        }
    }

    EMV_SetOutCome(status, start, online_data, cvm, ui_on_outcome, ui_on_restart,
                   data_record, disc_data, receipt, aip, field_off, removal);

    if (g_send_outcome_cb.pSendOutcome != NULL)
    {
        g_send_outcome_cb.pSendOutcome(g_outcome);
    }

    s_final_outcome_sent = 1;
    return EMV_OK;
}
