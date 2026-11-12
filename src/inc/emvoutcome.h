#ifndef __EMV_OUTCOME_H__
#define __EMV_OUTCOME_H__

#define OUTCOME_STATUS_NA 													0xF0
#define OUTCOME_STATUS_APPROVED 											0x10
#define OUTCOME_STATUS_DECLINED 											0x20
#define OUTCOME_STATUS_ONLINE_REQUEST										0x30
#define OUTCOME_STATUS_END_APPLICATION 										0x40
#define OUTCOME_STATUS_SELECT_NEXT 											0x50
#define OUTCOME_STATUS_TRY_ANOTHER_INTERFACE 								0x60
#define OUTCOME_STATUS_TRY_ANOTHER_AGAIN 									0x70


#define OUTCOME_START_NA 	0xF0
#define OUTCOME_START_A 	0x00
#define OUTCOME_START_B 	0x10
#define OUTCOME_START_C 	0x20
#define OUTCOME_START_D 	0x30

#define OUTCOME_ONLINE_RESP_DATA_NA 		0xF0
#define OUTCOME_ONLINE_RESP_DATA_EMVDATA 	0x10
#define OUTCOME_ONLINE_RESP_DATA_ANY 		0x20

#define OUTCOME_CVM_NA 					0xF0
#define OUTCOME_CVM_NOCVM 				0x00
#define OUTCOME_CVM_OBTAIN_SIGNATURE 	0x10
#define OUTCOME_CVM_ONLINE_PIN 			0x20
#define OUTCOME_CVM_CDCVM 				0x30

#define OUTCOME_AIP_NA 					0xF0
#define OUTCOME_AIP_CONTACT_CHIP 		0x10

#define OUTCOME_FIELDOFF_NA 			0xFF

#define UIREQUEST_MESSAGEID_CARD_READ_OK			0x17
#define UIREQUEST_MESSAGEID_TRY_AGAIN				0x21
#define UIREQUEST_MESSAGEID_APPROVED				0x03
#define UIREQUEST_MESSAGEID_APPROVED_AND_SIGN		0x1A
#define UIREQUEST_MESSAGEID_DECLINED				0x07
#define UIREQUEST_MESSAGEID_ERROR_OTHER_CARD		0x1C
#define UIREQUEST_MESSAGEID_INSERT_CARD				0x1D
#define UIREQUEST_MESSAGEID_SEEPHONE				0x20
#define UIREQUEST_MESSAGEID_AUTHORISING_AND_WAIT	0x1B
#define UIREQUEST_MESSAGEID_CLEAR_DISPLAY			0x1E
#define UIREQUEST_MESSAGEID_NA						0xFF


#define UIREQUEST_STATUS_NOT_READY						0x00
#define UIREQUEST_STATUS_IDLE							0x01
#define UIREQUEST_STATUS_READY_TO_READ					0x02
#define UIREQUEST_STATUS_PROCESSING						0x03
#define UIREQUEST_STATUS_CARD_READ_SUCCESS				0x04
#define UIREQUEST_STATUS_PROCESSING_ERROR				0x05
#define UIREQUEST_STATUS_CONTACTLESS_COLLISION_DETECT	0x06
#define UIREQUEST_STATUS_NA								0xFF

#define UIREQUEST_VALUE_QUALIFIER_BALANCE				0x20

#define UI_TYPE_ON_OUTCOME		0x01
#define UI_TYPE_ON_RESTART 		0x02
#define UI_TYPE_ON_MSG			0x03

#define ERR_INFO_L1_OK						0x00
#define ERR_INFO_L1_TIME_OUT_ERROR			0x01
#define ERR_INFO_L1_TRANSMISSION_ERROR		0x02
#define ERR_INFO_L1_PROTOCOL_ERROR			0x03

#define ERR_INFO_L2_OK						0x00
#define ERR_INFO_L2_CARD_DATA_MISSING		0x01
#define ERR_INFO_L2_CAM_FAILED				0x02
#define ERR_INFO_L2_STATUS_BYTES			0x03
#define ERR_INFO_L2_PARSING_ERROR			0x04
#define ERR_INFO_L2_MAX_LIMIT_EXCEEDED		0x05
#define ERR_INFO_L2_CARD_DATA_ERROR			0x06
#define ERR_INFO_L2_MAGSTRIPE_NOT_SUPPORTED	0x07
#define ERR_INFO_L2_NO_PPSE					0x08
#define ERR_INFO_L2_PPSE_FAULT				0x09
#define ERR_INFO_L2_EMPTY_CANDIDATE_LIST	0x0A
#define ERR_INFO_L2_IDS_READ_ERROR			0x0B
#define ERR_INFO_L2_IDS_WRITE_ERROR			0x0C
#define ERR_INFO_L2_IDS_DATA_ERROR			0x0D
#define ERR_INFO_L2_IDS_NO_MATCHING_AC		0x0E
#define ERR_INFO_L2_TERMINAL_DATA_ERROR		0x0F

#define ERR_INFO_L3_OK						0x00
#define ERR_INFO_L3_TIME_OUT				0x01
#define ERR_INFO_L3_STOP					0x02
#define ERR_INFO_L3_AMOUNT_NOT_PRESENT		0x03



typedef struct{
	unsigned char MessageID;
	unsigned char Status;
	unsigned char HoldTime[3];
	unsigned char LanguagePrefer[8];
}EMV_UIREQ_DATA;

typedef struct{
	unsigned char Status;
	unsigned char Start;
	unsigned char OnlineRespData;
	unsigned char CVM;
	unsigned char PresentFlag;
	unsigned char InterfacePrefer;
	unsigned char FieldoffReq;
	unsigned char RemovalTimeout;
}EMV_OUTCOME_DATA;

typedef struct{
	unsigned char L1;
	unsigned char L2;
	unsigned char L3;
	unsigned char SW12[2];
	unsigned char MsgOnError;
}EMV_ERROR_INDICATION;

//typedef struct{
//	void (*pSendUIRequest)(unsigned char MessageID,unsigned char Status,unsigned char *HoldTime, unsigned char *LanguagePrefer, unsigned char ValueQualifier, unsigned char *Value, unsigned char *CurrencyCode, unsigned char type);
//	void (*pSendOutcome)(unsigned char Status, unsigned char Start, unsigned char OnlineRespData, unsigned char CVM, unsigned char PresentFlag, unsigned char InterfacePrefer, unsigned char FieldoffReq, unsigned char *RemovalTimeout);
//}EMV_OUTCOME_CALLBACK;

typedef struct{
	void (*pSendUIRequest)(EMV_UIREQ_DATA data, unsigned char type);
	void (*pSendOutcome)(EMV_OUTCOME_DATA data);
}EMV_OUTCOME_CALLBACK;

extern EMV_OUTCOME_DATA g_outcome;
extern EMV_UIREQ_DATA g_uireq;
extern EMV_ERROR_INDICATION g_err_indicator;
extern unsigned char g_discretionary_data[1200];
extern int g_discretionary_data_len;
extern unsigned char g_data_record[1200];
extern int g_data_record_len;


int EMV_SetOutcome_Callback(void(*func_ptr)(EMV_OUTCOME_DATA data));
int EMV_SetUIRequest_Callback(void(*func_ptr)(EMV_UIREQ_DATA data, unsigned char type));

//int EMV_SetOutcome_Callback(void (*func_ptr)(unsigned char Status, unsigned char Start, unsigned char OnlineRespData, unsigned char CVM, unsigned char PresentFlag, unsigned char InterfacePrefer, unsigned char FieldoffReq, unsigned char *RemovalTimeout));
//int EMV_SetUIRequest_Callback(void (*func_ptr)(unsigned char MessageID,unsigned char Status,unsigned char *HoldTime, unsigned char *LanguagePrefer, unsigned char ValueQualifier, unsigned char *Value, unsigned char *CurrencyCode, unsigned char type));

void EMV_InitOutCome();
void EMV_InitUIRequest();
int outcome_other_card(unsigned char data_record, unsigned char receipt);
int outcome_select_next();
int outcome_try_again();


void EMV_InitErrorIndication(void);
void EMV_InitDataRecord(void);
void EMV_InitDiscData(void);
int outcome_request_another_payment(void);
int outcome_end_application(void);
int outcome_end_application_startB(void);
int outcome_seephone(void);
int outcome_try_again_without_restart(void);
int outcome_try_again_end_application(void);
void msg_read_card_ok(void);
#endif
