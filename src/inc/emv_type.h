/**
************************************************************************************************************************
*                         			  COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_type.h
* @brief    EMV�ں˽ӿ� - ���Ͷ���
* @author   Kingsee Development Team - xinyong
* @version  V43
*
************************************************************************************************************************
*/
#ifndef EMV_TYPE_H_
#define EMV_TYPE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * ���׽��
 */
enum EMV_Status
{
    EMV_OK                          = (0),              //!< �ɹ�
    EMV_FAIL                        = (-1),             //!< ʧ��

    /************************** ͨ�ô��������޹أ� **************************/
    EMV_ERR                         = (-100),
    EMV_NULPORTING                  = (EMV_ERR -  1),   //!< ��ֲ�ӿ�NULL
    EMV_GETTERMPARAMERR             = (EMV_ERR -  2),   //!< �����ն����ò���ʧ��
    EMV_GETTERMAPPSERR              = (EMV_ERR -  3),   //!< �����ն�Ӧ���б�ʧ��
    EMV_ALLOCBUFFERERR              = (EMV_ERR -  4),   //!< ������?�ֻ���ռ䲻��?
    EMV_UNINITIALIZED               = (EMV_ERR -  5),   //!< �ں�δ��ʼ��
    EMV_NONIDLE                     = (EMV_ERR -  6),   //!< �ں˷ǿ���״̬
    EMV_AMOUNTOVER                  = (EMV_ERR -  7),   //!< ���׽��?��?    EMV_AMOUNTZERO                  = (EMV_ERR -  8),   //!< ���׽���?��?    EMV_QPBOCUNSUPPORTED            = (EMV_ERR -  9),   //!< �ն˽������Բ�֧��QPBOC
	EMV_AMOUNTZERO					= (EMV_ERR -  8),	//!< ���׽���?��
	EMV_QPBOCUNSUPPORTED			= (EMV_ERR -  9),	//!< �ն˽������Բ�֧��QPBOC
	EMV_CARDTYPEILL                 = (EMV_ERR - 10),   //!< ��ƬͨѶ��ʽ������
    EMV_PROCESSOVER                 = (EMV_ERR - 11),   //!< �����ѽ���
    EMV_UNKNOWNSTATUS               = (EMV_ERR - 12),   //!< δ֪����״̬
    EMV_APPSELUNDONE                = (EMV_ERR - 13),   //!< �ں�δ���Ӧ��ѡ��??    EMV_PBOCLOGSFINOTFOUND          = (EMV_ERR - 14),   //!< ������־���û�ҵ�??    EMV_PBOCLOGFMTNOTFOUND          = (EMV_ERR - 15),   //!< ������־��ʽû�ҵ�
	EMV_PBOCLOGSFINOTFOUND			= (EMV_ERR - 14),	//!< ������־���û�ҵ�?
	EMV_PBOCLOGFMTNOTFOUND			= (EMV_ERR - 15),	//!< ������־��ʽû�ҵ�
	EMV_PBOCLOGRDRECFAILED          = (EMV_ERR - 16),   //!< ������־��¼��ȡʧ��
    EMV_PBOCLOGRECNOINV             = (EMV_ERR - 17),   //!< ������־��¼����Ч
    EMV_SELCANDIDATENOTREQ          = (EMV_ERR - 18),   //!< �ں�δ����ѡ���ѡӦ���б�??    EMV_SELACCOUNTNOTREQ            = (EMV_ERR - 19),   //!< �ں�δ����ѡ���ʻ�����
	EMV_SELACCOUNTNOTREQ			= (EMV_ERR - 19),	//!< �ں�δ����ѡ���ʻ�����
	EMV_CHECKIDNOTREQ               = (EMV_ERR - 20),   //!< �ں�δ�������ֿ���֤��
    EMV_ONLINEPINNOTREQ             = (EMV_ERR - 21),   //!< �ں�δ������������PIN
    EMV_OFFLINEPINNOTREQ            = (EMV_ERR - 22),   //!< �ں�δ���������ѻ�PIN
    EMV_BYPASSPINNOTREQ             = (EMV_ERR - 23),   //!< �ں�δ����ȷ���Ƿ�����PIN����
    EMV_FORCEONLINENOTREQ           = (EMV_ERR - 24),   //!< �ں�δ����ȷ���Ƿ�ǿ������
    EMV_GOONLINENOTREQ              = (EMV_ERR - 25),   //!< �ں�δ����������Ȩ
    EMV_ISSREFERRALNOTREQ           = (EMV_ERR - 26),   //!< �ں�δ���󷢿��вο�
    EMV_SETTERMPARAMERR             = (EMV_ERR - 27),   //!< �����ն����ò���ʧ��
    EMV_PAN_NOTMATCH_TRACK2  		= (EMV_ERR - 28),   // - ���˺�����ŵ�����??    EMV_NOT_STA_APP_SELECTED   		= (EMV_ERR - 29),   // - ����������TAG
	EMV_NOT_STA_APP_SELECTED   		= (EMV_ERR - 29),   // - ����������TAG
	EMV_VALUENULL_LENERR            = (EMV_ERR - 30),   // - ����Ϊ�ջ��߳��ȴ���
    EMV_APDU_TIMEOUT				= (EMV_ERR - 31), 	//-apdu communicate error(L1 driver)
    EMV_DATA_REDUNDANT				= (EMV_ERR - 32), 	//return tag data redundant

    /************************** ����״̬��������أ�??**************************/
    AS_ERR                          = (-1100),          //!< Ӧ��ѡ�����??    AS_SELSW6A81                    = (AS_ERR -  1),    //!<
	AS_SELSW6A81					= (AS_ERR -  1),	//!<
	AS_SELERR                       = (AS_ERR -  2),    //!<
    AS_FCIFMTILL                    = (AS_ERR -  3),    //!<
    AS_FCINO6F                      = (AS_ERR -  4),    //!<
    AS_FCINO84                      = (AS_ERR -  5),    //!<
    AS_FCINOA5                      = (AS_ERR -  6),    //!<
    AS_FCINO88                      = (AS_ERR -  7),    //!<
    AS_FCI88VALILL                  = (AS_ERR -  8),    //!<
    AS_FCINO50                      = (AS_ERR -  9),    //!<
    AS_FCINOBF0C                    = (AS_ERR - 10),    //!<
    AS_FCIBF0CVALILL                = (AS_ERR - 11),    //!<
    AS_POSERR9F38                   = (AS_ERR - 12),    //!<
    AS_POSERR9F11                   = (AS_ERR - 13),    //!<
    AS_POSERR9F12                   = (AS_ERR - 14),    //!<
    AS_SUBDUPBF0C                   = (AS_ERR - 15),    //!<
    AS_RDRECERR                     = (AS_ERR - 16),    //!<
    AS_RECFMTILL                    = (AS_ERR - 17),    //!<
    AS_RECNO70                      = (AS_ERR - 18),    //!<
    AS_REC70HAS9F12                 = (AS_ERR - 19),    //!<
    AS_REC70HAS42                   = (AS_ERR - 20),    //!<
    AS_REC70HAS5F50                 = (AS_ERR - 21),    //!<
    AS_APPTMPLILL                   = (AS_ERR - 22),    //!<
    AS_APPTMPLNO50                  = (AS_ERR - 23),    //!<
    AS_NULCANDIDATE                 = (AS_ERR - 24),    //!<
    AS_CANDIDATESELERR              = (AS_ERR - 25),    //!<
    AS_NO9F5D                       = (AS_ERR - 26),    //!<
    AS_NO9F4D                       = (AS_ERR - 27),    //!<
    AS_AIDPARGETERR                 = (AS_ERR - 28),    //!<
    AS_PREPROCAMTOVER               = (AS_ERR - 29),    //!<
    AS_PREPROCAMTZERO               = (AS_ERR - 30),    //!<
    AS_SELACCOUNTERR                = (AS_ERR - 31),    //!<
    AS_IADTCREQAC                   = (AS_ERR - 32),    //!<
    AS_TERMINATE                    = (AS_ERR - 33),    //!<
	AS_SELSW6283					= (AS_ERR - 34),
	AS_SELSW6A82					= (AS_ERR - 35),
	AS_SWNO9000						= (AS_ERR - 36),	//!<sw of select command is not 9000
	AS_CARDCOMMUERROR				= (AS_ERR - 37),	//!<communicate with card error,something wrong with L1 drive
	AS_CARDBLOCK					= (AS_ERR - 38),
    AS_FFILENFAIL                   = (AS_ERR - 39),    //!< FFI���ȳ��ִ���
    AS_RDLOG_9F4F_ERR 				= (AS_ERR - 39),
    AS_AIPLENFAIL                   = (AS_ERR - 40),    //!< AIP���ȳ��ִ���
    AS_9F4DNOBFOC                   = (AS_ERR - 41),    //!< ����9F4D��������BF0C
    AS_A5NO5F2D                     = (AS_ERR - 42),    //!< 5F2D����A5��

    IA_ERR                          = (-1200),          //!< Ӧ�ó�ʼ������
    IA_NOPDOL                       = (IA_ERR -  1),    //!<
    IA_PDOLNO9F66                   = (IA_ERR -  2),    //!<
    IA_PDOLPACKERR                  = (IA_ERR -  3),    //!<
    IA_GPOSW6985                    = (IA_ERR -  4),    //!<
    IA_GPOSWERR                     = (IA_ERR -  5),    //!<
    IA_GPORSPERR                    = (IA_ERR -  6),    //!<
    IA_GPORSP80                     = (IA_ERR -  7),    //!<
    IA_GPORSP80LENERR               = (IA_ERR -  8),    //!<
    IA_GPORSP77NOAIP                = (IA_ERR -  9),    //!<
    IA_GPORSP77AIPLENERR            = (IA_ERR - 10),    //!<
    IA_GPORSP77NOAFL                = (IA_ERR - 11),    //!<
    IA_GPORSP77AFLLENERR            = (IA_ERR - 12),    //!<
    IA_GPORSPUNEXPECTTAG            = (IA_ERR - 13),    //!<
    IA_NOIAD                        = (IA_ERR - 14),    //!<
    IA_GPORSPNOATC                  = (IA_ERR - 15),    //!<
    IA_GPORSPATCLENERR              = (IA_ERR - 16),    //!<
    IA_GPORSPNOAC                   = (IA_ERR - 17),    //!<
    IA_GPORSPACLENERR               = (IA_ERR - 18),    //!<
    IA_POSERRATC                    = (IA_ERR - 19),    //!<
    IA_NOT2EQU                      = (IA_ERR - 20),    //!<
    IA_NOAFL                        = (IA_ERR - 21),    //!<
    IA_AFLERR                       = (IA_ERR - 22),    //!<
    IA_GPOSW6984                    = (IA_ERR - 23),    //!<
    IA_GPOSW6986                    = (IA_ERR - 24),    //!<
    IA_DATADUPLICATION              = (IA_ERR - 25),    //!<
    IA_PDOLDATAERR                  = (IA_ERR - 26),    //!< PDOL���ݼ�����

    RD_ERR                          = (-1300),          //!< ��Ӧ�����ݴ���
    RD_AFLILL                       = (RD_ERR -  1),    //!<
    RD_READRECORDERR                = (RD_ERR -  2),    //!<
    RD_RECORDILL                    = (RD_ERR -  3),    //!<
    RD_SAVEDATAERR                  = (RD_ERR -  4),    //!<
    RD_NOPAN                        = (RD_ERR -  5),    //!<
    RD_NOCDOL1                      = (RD_ERR -  6),    //!<
    RD_NOCDOL2                      = (RD_ERR -  7),    //!<
    RD_5F24ERR                      = (RD_ERR -  8),    //!<
    RD_5F25ERR                      = (RD_ERR -  9),    //!<
    RD_CHECKMANDATORYDATAERR        = (RD_ERR -  10),    //!<
    RD_5F24DATEEXCEPTION            = (RD_ERR -  11),    //!<
	RD_5F25DATEEXCEPTION			= (RD_ERR -  12),	 //!<

    DA_ERR                          = (-1400),          //!< �ѻ�������֤����
    DA_READRECORDFAIL               = (DA_ERR -  1),    //!<
    DA_NOCAPKI                      = (DA_ERR -  2),    //!<
    DA_CAPKNOTFOUNT                 = (DA_ERR -  3),    //!<
    DA_CAPKALGERR                   = (DA_ERR -  4),    //!<
    DA_CAPKEXPIRED                  = (DA_ERR -  5),    //!<
    DA_CAPKCHKERR                   = (DA_ERR -  6),    //!<
    DA_NOISSPKCERT                  = (DA_ERR -  7),    //!<
    DA_NOISSPKEXP                   = (DA_ERR -  8),    //!<
    DA_NOICCPKCERT                  = (DA_ERR -  9),    //!<
    DA_NOICCPKEXP                   = (DA_ERR - 10),    //!<
    DA_NODDOL                       = (DA_ERR - 11),    //!<
    DA_NODEFDDOL                    = (DA_ERR - 12),    //!<
    DA_DDOLNO9F37                   = (DA_ERR - 13),    //!<
    DA_DDOLPACKERR                  = (DA_ERR - 14),    //!<
    DA_IAUTHERR                     = (DA_ERR - 15),    //!<
    DA_IAUTHRSPERR                  = (DA_ERR - 16),    //!<
    DA_IAUTHRSP80LENERR             = (DA_ERR - 17),    //!<
    DA_IAUTHRSP77NOSDAD             = (DA_ERR - 18),    //!<
    DA_IAUTHRSP77SDADERR            = (DA_ERR - 19),    //!<
    DA_IAUTHRSPUNEXPECTTAG          = (DA_ERR - 20),    //!<
    DA_SDADLENERR                   = (DA_ERR - 21),    //!<
    DA_SDADRECOVERERR               = (DA_ERR - 22),    //!<
    DA_SDADHEADERERR                = (DA_ERR - 23),    //!<
    DA_SDADTAILERERR                = (DA_ERR - 24),    //!<
    DA_SDADFMTERR                   = (DA_ERR - 25),    //!<
    DA_SDADHASHVERR                 = (DA_ERR - 26),    //!<
    DA_NOSDAD                       = (DA_ERR - 27),    //!<
    DA_9F69ERR                      = (DA_ERR - 28),    //!<
    DA_FDDAVERUNSUP                 = (DA_ERR - 29),    //!<
    DA_NO9F37                       = (DA_ERR - 30),    //!<
    DA_FDDAVERERR                   = (DA_ERR - 31),    //!<
    DA_NOSSAD                       = (DA_ERR - 32),    //!<
    DA_SSADLENERR                   = (DA_ERR - 33),    //!<
    DA_SSADRECOVERERR               = (DA_ERR - 34),    //!<
    DA_SSADHEADERERR                = (DA_ERR - 35),    //!<
    DA_SSADTAILERERR                = (DA_ERR - 36),    //!<
    DA_SSADFMTERR                   = (DA_ERR - 37),    //!<
    DA_SSADHASHVERR                 = (DA_ERR - 38),    //!<
    DA_9F4AERR                      = (DA_ERR - 39),    //!<
    DA_CIDNOTMATCHED                = (DA_ERR - 40),    //!<
    DA_CDAHASHV1ERR                 = (DA_ERR - 41),    //!<
    DA_CDAHASHV2ERR                 = (DA_ERR - 42),    //!<
    DA_ICCDATAMISSING               = (DA_ERR - 43),    //!<
    DA_CERTLENERR                   = (DA_ERR - 44),    //!<
    DA_CERTRECOVERERR               = (DA_ERR - 45),    //!<
    DA_CERTHEADERERR                = (DA_ERR - 46),    //!<
    DA_CERTTAILERERR                = (DA_ERR - 47),    //!<
    DA_CERTFMTERR                   = (DA_ERR - 48),    //!<
    DA_CERTHASHVERR                 = (DA_ERR - 49),    //!<
    DA_PANNOTMATCHED                = (DA_ERR - 50),    //!<
    DA_CERTEXPIRED                  = (DA_ERR - 51),    //!<
    DA_CERTBLOCKED                  = (DA_ERR - 52),    //!<
    DA_CERTALGERR                   = (DA_ERR - 53),    //!<
    DA_PKMODLENERR                  = (DA_ERR - 54),    //!<
    DA_NOPKREM                      = (DA_ERR - 55),    //!<
    DA_NOISSPK                      = (DA_ERR - 56),    //!<
    DA_NOICCPK                      = (DA_ERR - 57),    //!<
    DA_NOPINPKCERT                  = (DA_ERR - 58),    //!<
    DA_NOPINPKEXP                   = (DA_ERR - 59),    //!<
    DA_GETCHALLENGEERR              = (DA_ERR - 60),    //!<
    DA_ENCPINRECOVERERR             = (DA_ERR - 61),    //!<
	DA_SIGNED_DYNAMIC_DATA_LABLE	= (DA_ERR - 62),	//!<

    RE_ERR                          = (-1500),          //!< �������ƴ���

    CV_ERR                          = (-1600),          //!< �ֿ�����֤����
    CV_CVMLISTERR                   = (CV_ERR -  1),    //!<
    CV_ONLINEPINFAIL                = (CV_ERR -  2),    //!<
    CV_OFFLINEPINFAIL               = (CV_ERR -  3),    //!<

    RM_ERR                          = (-1700),          //!< �ն˷��չ�������

    AA_ERR                          = (-1800),          //!< �ն���Ϊ��������
    AA_CDOLPACKERR                  = (AA_ERR -  1),    //!<
    AA_GACERR                       = (AA_ERR -  2),    //!<
    AA_GACRSPERR                    = (AA_ERR -  3),    //!<
    AA_GACRSP80LENERR               = (AA_ERR -  4),    //!<
    AA_GACRSP80VALERR               = (AA_ERR -  5),    //!<
    AA_GACRSP77NOCID                = (AA_ERR -  6),    //!<
    AA_GACRSP77NOATC                = (AA_ERR -  7),    //!<
    AA_GACRSP77NOSDAD               = (AA_ERR -  8),    //!<
    AA_GACRSP77NO9F26               = (AA_ERR -  9),    //!<
    AA_GACRSPUNEXPECTTAG            = (AA_ERR - 10),    //!<
    AA_CIDAAR                       = (AA_ERR - 11),    //!<
    AA_CDAREQUIRE77                 = (AA_ERR - 12),    //!<
    AA_CIDERR                       = (AA_ERR - 13),    //!<
    AA_CIDTERMINATE                 = (AA_ERR - 14),    //!<

    CO_ERR                          = (-1900),          //!< ���׽�������
    CO_NOTONLINE                    = (CO_ERR -  1),    //!<

    CL_ERR                          = (-2000),          //!< �ѣУ£ϣô���
    CL_DAFAIL                       = (CL_ERR -  1),    //!<

	EMV_API_ERROR				= (-9997),
    EMV_UNRECOVER_ERR           = (-9988),
    EMV_TERMINATE                   = (-9999),
    EMV_UNRECOVER_SW           = (0x0000),//(0x9988),

    /************************** ?��????????????? **************************/
    EMV_STA_IDLE = 1,
    EMV_STA_CANDIDATES_BUILT = 2,
    EMV_REQ_SEL_CANDIDATES = 3,
    EMV_REQ_SEL_CANDIDATES_AGAIN = 4,
    EMV_STA_APP_SELECTED = 5,
    EMV_REQ_SEL_ACCOUNTTYPE = 6,
    EMV_STA_APP_INITIALIZED = 7,
    EMV_REQ_DEACTIVATE_CARD = 8,
    EMV_STA_CARD_DEACTIVATED = 9,
    EMV_STA_READ_APP_DATA_COMPLETED = 10,
    EMV_STA_DATA_AUTH_COMPLETED = 11,
    EMV_STA_PROCESS_RESTRICT_COMPLETED = 12,
    EMV_REQ_CHECK_ID = 13,
    EMV_REQ_ONLINE_PIN = 14,
    EMV_REQ_OFFLINE_PIN = 15,
    EMV_REQ_OFFLINE_PIN_LAST = 16,
    EMV_REQ_CONFIRM_BYPASS_PIN = 17,
    EMV_STA_CARDHOLDER_VERIFY_COMPLETED = 18,
    EMV_REQ_CONFIRM_FORCE_ONLINE = 19,
    EMV_STA_RISK_MANAGEMENT_COMPLETED = 20,
    EMV_STA_ACTION_ANALYSE_COMPLETED = 21,
    EMV_REQ_GO_ONLINE = 22,
    EMV_REQ_ISSUER_REFERRAL = 23,
	EMV_REQ_CARDINFO_CONFIRM = 24,						 //!< 请求卡号信息�?�?
	EMV_REQ_SELECT_NEXT = 26,
	EMV_REQ_READCAARD_AGAIN = 27,
	EMV_REQ_SET_BEFOREGPO = 29,
	EMV_REQ_SET_EXCEPTIONFILE = 30,
	EMV_REQ_SET_TAPCARD_AGAIN = 31,
	EMV_REQ_SET_REVOCATIONKEY_CHECK = 32,
	EMV_STA_RRP_COMPLETED = 33,
	EMV_STA_PREGACBAlANCEREAD_COMPLETED = 34,
	EMV_STA_POSTGACPUTDATA_COMPLETED = 35,
	EMV_STA_PERFORM_RRP = 36,
	EMV_REQ_ISSUER_UPDATE = 39,
	EMV_STA_DATAEXCHANGE_COMPLETED = 46,
	EMV_STA_PREGAC_BALANCE_READ = 47,
	EMV_STA_PREGAC_PUTDATA_COMPLETED = 48,
	EMV_STA_GAC_RESPONSE_PROCESS_COMPLETED = 49,
	EMV_STA_AFTERGAC_PUTDATA_COMPLETED = 50,
	EMV_STA_CCC_COMPLETED = 51,
	EMV_STA_READ_CARD_OK = 52,
    EMV_REQ_QVSDC_ISSUER_UPDATE = 52,                     //!< QVSDC���󷢿��и���
    EMV_REQ_QVSDC_ISSUER_UPDATE_AGAIN = 53,                     //!< QVSDC�ٴ����󷢿��и���
    EMV_REQ_QVSDC_MULTICARDCONFLICT = 54,                     //!< QVSDC�࿨��ͻ
	EMV_REQ_SENDDEK = 55,
	EMV_REQ_WAITPTFWF = 56,
	EMV_REQ_SENDDEK_POSTREAD = 57,
	EMV_REQ_SECOND_TAP = 58,
	EMV_STA_BEEP_OK = 59,

    /************************** ???????????????? **************/
    EMV_COMPLETED = 100,
    EMV_ACCEPTED_OFFLINE = 101,
    EMV_DENIALED_OFFLINE = 102,
    EMV_ACCEPTED_ONLINE = 103,
    EMV_DENIALED_ONLINE = 104,
    EMV_FALLBACK = 105,
	EMV_SWITCH_INTERFACE = 106,
    EMV_SEEPHONE = 107,
    EMV_ENDAPPLICATION = 111,
	EMV_USE_ANOTHER_CARD = 112,
    EMV_REQ_RETAP_AGAIN = 114,
    EMV_USE_OTHERCARD = 117,
};

/**
 * ��������
 *
 * 00-19 - ���\n
 * 20-29 - ����\n
 * 30-29 - ��ѯ����\n
 * 40-49 - ת�˷���\n
 * 50-99 - ����\n
 * F0-FF - �ں��Զ���\n
 */
enum EMV_TransTypes
{
	/* ���??*/
	EMV_TRANSTYPE_GOODS_SERVICES = 0x00,			//!< ��Ʒ�ͷ���
	EMV_TRANSTYPE_CASH = 0x01,						//!< �ֽ�
	EMV_TRANSTYPE_ADJUSTMENT_DEBIT = 0x02,			//!< ��������ǣ�??	EMV_TRANSTYPE_CHEQUE_GUARANTEE = 0x03,			//!< ֧Ʊ�������ʽ�ɵ�����??	  EMV_TRANSTYPE_CHEQUE_VERIFICATION = 0x04, 	  //!< ֧Ʊ��ʵ���ʽ���õ���������??	  EMV_TRANSTYPE_EUROCHEQUE = 0x05,				  //!< ŷ��֧Ʊ
	EMV_TRANSTYPE_TRAVELLER_CHEQUE = 0x06,			//!< ����֧Ʊ
	EMV_TRANSTYPE_LETTER_OF_CREDIT = 0x07,			//!< ����֤
	EMV_TRANSTYPE_GLRO = 0x08,						//!< ֱ��ת�ˣ���������ҵ��
	EMV_TRANSTYPE_GOODS_SERVICES_WITH_CASH = 0x09,	//!< �����ֽ�֧������Ʒ�ͷ���
	EMV_TRANSTYPE_CASHBACK = 0x09,					//!< ���� FIXME
	EMV_TRANSTYPE_CASH_DISBURSEMENT = 0x17, 		//!< ���� FIXME

	/* ���� */
	EMV_TRANSTYPE_RETURNS = 0x20,					//!< �˿�
	EMV_TRANSTYPE_DEPOSITS = 0x21,					//!< ���??	EMV_TRANSTYPE_ADJUSTMENT_CREDIT = 0x22, 		//!< ���������ǣ�
	EMV_TRANSTYPE_ADJUSTMENT_CREDIT = 0x22, 		//!< 调整（贷记）
	EMV_TRANSTYPE_CHEQUE_DEPOSIT_GUARANTEE = 0x23,	//!< ֧Ʊ����??	  EMV_TRANSTYPE_CHEQUE_DEPOSIT = 0x24,			  //!< ֧Ʊ���??	  /* ��ѯ���� */
	EMV_TRANSTYPE_CHEQUE_DEPOSIT = 0x24,			//!< 支票存款

	EMV_TRANSTYPE_AVAILABLE_FUNDS_INQUIRY = 0x30,	//!< �����ʽ���?	EMV_TRANSTYPE_BALANCE_INQUIRY = 0x31,			//!< ��ѯ���??	/* ת�˷��� */
	EMV_TRANSTYPE_BALANCE_INQUIRY = 0x31,			//!< 查询余额

	EMV_TRANSTYPE_TRANFER = 0x40,					//!< �ֿ����˻�ת��
	EMV_TRANSTYPE_PAYMENT = 0x50,					//!< ����
	EMV_TRANSTYPE_ADMINISTRATIVE = 0x66,	   //!< ����

	/* �ں��Զ��� */
	_EMV_TRANSTYPE_PBOCLOG = 0xF0,					//!< ��ȡ������־
	_EMV_TRANSTYPE_READ_PAN = 0xF1, 				//!< �����˺�
	_EMV_TRANSTYPE_READ_TRACK2 = 0xF2,				//!< �����ŵ���Ч����
	_EMV_TRANSTYPE_CL_INQUIRY = 0xF3,				//!< QPBOC����ѯ
	_EMV_TRANSTYPE_EC_INQUIRY = 0xF4,				//!< �����ֽ�����ѯ
	_EMV_TRANSTYPE_LITE_FLOW = 0xF5,				//!< �����̽��ף���Ӧ��������ɾͽ�����??
	_EMV_TRANSTYPE_PBOC_LOAD_LOG = 0xF6,					//!< ��ȡȦ����־
	_EMV_TRANSTYPE_UNKNOWN = 0xFF,					//!< δ֪
};


/**
 * ��Ƭ����
 */
enum EMV_CardTypes
{
    EMV_CARD_UNKNOWN = 0x00,                        //!< δ֪
    EMV_CARD_CONTACT = 0x01,                        //!< �Ӵ�ʽ
    EMV_CARD_CONTACTLESS = 0x02,                    //!< �ǽӴ�ʽ
};

/**
 * ֧����ʽ
 */
enum EMV_PayModes
{
    EMV_PAYMODE_UNKNOWN,                            //!< δ֪
    EMV_PAYMODE_PBOC,                               //!< ��׼����ǣ��Ӵ�ʽ��ǽӴ�ʽ��
    EMV_PAYMODE_EC,                                 //!< �����ֽ𣨽Ӵ�ʽ��
    EMV_PAYMODE_MSD,                                //!< MSD���ǽӴ�ʽ���ѷϳ���
    EMV_PAYMODE_QPBOC,                              //!< ���ٽ����PBOC���ǽӴ�ʽ��
    EMV_PAYMODE_QUICS,                              //!< ���ٽ����UNIONPAY���ǽӴ�ʽ��
    EMV_PAYMODE_QVSDC,                              //!< ���ٽ����VISA PAYWAVE���ǽӴ�ʽ��
    EMV_PAYMODE_QMSDC,                              //!< ���ٽ����MASTER PAYPASS���ǽӴ�ʽ��
EMV_PAYMODE_QPURE,                              //!< Pure

};


enum EMV_ContactlessType
{
    EMV_CONTACTLESSTYPE_UNKNOWN, //!< δ֪
    EMV_CONTACTLESSTYPE_QPBOC, //!< uninon pay
    EMV_CONTACTLESSTYPE_QVSDC, //!< paywave
    EMV_CONTACTLESSTYPE_QMSDC, //!< paypass
    EMV_CLTYPE_QMSDC_MASTERCARD, //!< paypass
    EMV_CONTACTLESSTYPE_JCB,
    EMV_CONTACTLESSTYPE_AMEX,
    EMV_CONTACTLESSTYPE_DISV,
    EMV_CONTACTLESSTYPE_QPURE,        //!< pure
};
/**
 * ��������뷽ʽ
 */
enum EMV_PosEntryModes
{
    EMV_POSENTRYMODE_UNKNOWN = 0x00,                //!< δ֪
    EMV_POSENTRYMODE_MANUAL = 0x01,                 //!< �ֹ�����
    EMV_POSENTRYMODE_QPBOC_CL = 0x07,               //!< ���ٽ����IC�����루�ǽӴ�ʽ��
    EMV_POSENTRYMODE_MAG_FALLBACK = 0x95,//0x80,           //!< ��������ȡ��FALLBACK��
    EMV_POSENTRYMODE_MAG = 0x02,//0x90,                    //!< ��������ȡ
    EMV_POSENTRYMODE_MSD_CL = 0x91,                 //!< MSD���ǽӴ�ʽ���ѷϳ���
    EMV_POSENTRYMODE_PBOC = 0x05,//0x95,                   //!< ��׼�����IC�����루�Ӵ�ʽ��
    EMV_POSENTRYMODE_PBOC_CL = 0x98,                //!< ��׼�����IC�����루�ǽӴ�ʽ��
};

/**
 * �ʻ�����
 */
enum EMV_AccountTypes
{
    EMV_ACCOUNT_DEFAULT = 0x00,                     //!< Ĭ��-δָ����Default��
    EMV_ACCOUNT_SAVINGS = 0x10,                     //!< �����˻���Savings��
    EMV_ACCOUNT_CHEQUE_DEBIT = 0x20,                //!< ֧Ʊ�˻�/����˻���Cheque/Debit��
    EMV_ACCOUNT_CREDIT = 0x30,                      //!< �����˻���Credit��
};

/**
 * ��ʾ��Ϣ����
 */
enum EMV_MessageIDs
{
    EMV_MSG_CAPK_NotFound,                         	//!< ��ʾCA��Կδ�ҵ�
    EMV_MSG_CAPK_Expired,                          	//!< ��ʾCA��Կ�ѹ���
    EMV_MSG_CAPK_Invalid,                          	//!< ��ʾCA��Կ������

    EMV_MSG_ServiceNotAllowed,                     	//!< ��ʾ����������
    EMV_MSG_PinTryLimitExceeded,                   	//!< ��ʾPIN���Դ�������
    EMV_MSG_IssuerAuthenticationFailed,            	//!< ��ʾ��������֤ʧ��

    EMV_MSG_BlacklistCard,                         	//!< ��ʾ��������
    EMV_MSG_AppExpired,                            	//!< ��ʾӦ����ʧЧ
    EMV_MSG_PwdErr,                                 //!< ��ʾ�ѻ��������
    EMV_MSG_CDCVM,                                 //!< ��ʾ�û��豸CVM
    EMV_MSG_SEEPHONE,                                 //!< ��ʾ�û��豸CVM

    EMV_MSG_MAX,
};

/**
 *  �û���������ֵ
 */
enum EMV_OperResults
{
    EMV_OPER_OK = 0,                                //!< ȷ�� / ��
    EMV_OPER_CANCEL = -1,                           //!< ȡ�� / ��
    EMV_OPER_BACK = -2,                             //!< ����
    EMV_OPER_QUIT = -3,                             //!< �˳�
    EMV_OPER_TIMEOUT = -4,                          //!< ��ʱ
    EMV_OPER_BYPASS = -5,                           //!< ������û������ֱ��ȷ�ϣ�
    EMV_OPER_UNSUPPORTED = -6,                      //!< ��֧��
    EMV_OPER_REPORT_APP = -7,                       //!< ��������ֱ���ϱ�Ӧ��
};

/**
 * ��Կ�㷨��ʶ
 */
enum EMV_PubKeyAlgorithms
{
    EMV_PUBKEY_ALG_RSA = 0x01,                      //!< ǩ���㷨��RSA��       �����㷨��RSA��       ��ϣ�㷨��SHA-1
    EMV_PUBKEY_ALG_ECC2 = 0x02,                     //!< ǩ���㷨��ECC(ECDSA)�������㷨��ECC(ECIES)����ϣ�㷨��SHA-256
    EMV_PUBKEY_ALG_ECC4 = 0x03,                     //!< ǩ���㷨��ECC(ECDSA)�������㷨��ECC(ECIES)����ϣ�㷨��SHA-512
    EMV_PUBKEY_ALG_SM2 = 0x04,                      //!< ǩ���㷨��SM2��       �����㷨��SM2��       ��ϣ�㷨��SM3
};

/**
 * ��ϣ�㷨��ʶ
 */
enum EMV_HashAlgorithms
{
    EMV_HASH_ALG_SHA1 = 0x01,                       //!< SHA-1
    EMV_HASH_ALG_SHA256 = 0x03,                     //!< SHA-256
    EMV_HASH_ALG_SHA512 = 0x05,                     //!< SHA-512
    EMV_HASH_ALG_SM3 = 0x07,                        //!< SM3
};

/**
 * �������
 */
enum EMV_OnlineResults
{
    EMV_ONLINE_NOT = 0,                             //!< δ����
    EMV_ONLINE_FAIL,                                //!< ����ʧ��
    EMV_ONLINE_SUCC_ACCEPT,                         //!< �����ɹ�����������׼
    EMV_ONLINE_SUCC_DENIAL,                         //!< �����ɹ��������оܾ�
    EMV_ONLINE_SUCC_ISSREF,                         //!< �����ɹ���������ָ��
    _EMV_2GAC_AAC,                                  //!< 2GAC����ܾ����ں�ר�ã�
};

/**
 * ֤������
 */
enum PBOC_DocumentTypes
{
    PBOC_DOCUMENT_ID_CARD = 0x00,                   //!< ����֤
    PBOC_DOCUMENT_MILITARY_OFFICER = 0x01,          //!< ����֤
    PBOC_DOCUMENT_PASSPORT = 0x02,                  //!< ����
    PBOC_DOCUMENT_ENTRY_PERMIT = 0x03,              //!< �뾳֤
    PBOC_DOCUMENT_INTERIM_ID_CARD = 0x04,           //!< ��ʱ����֤
    PBOC_DOCUMENT_OTHER = 0x05,                     //!< ����
};

/**
 * �ں��Զ���Ľ�����־��¼��
 */
enum PBOC_SepcTransRecIndexs
{
    PBOC_TRANSREC_SFI = 0x100,                      //!< ������־���
    PBOC_TRANSREC_NUM = 0x101,                      //!< ������־����
    PBOC_TRANSREC_FMT = 0x102,                      //!< ������־��ʽ
};

/**
 * �ں��Զ����Ȧ����־��¼��
 */
enum PBOC_SepcLoadRecIndexs
{
    PBOC_LOADREC_SFI = 0x110,                      //!< Ȧ����־���
    PBOC_LOADREC_NUM = 0x111,                      //!< Ȧ����־����
    PBOC_LOADREC_FMT = 0x112,                      //!< Ȧ����־��ʽ
};


enum EMVL2_KernelIndex
{
    KERNEL_INX_NA = 0,      //!< ���ں�
    KERNEL_INX_EMV = 1,     //!< EMV�ں�
    KERNEL_INX_PBOC = 2,    //!< PBOC�ں�
    KERNEL_INX_QPBOC = 3,   //!< QPBOC�ں�
    KERNEL_INX_QUICS = 4,   //!< QPBOC�ں�
    KERNEL_INX_QVSDC = 5,   //!< VISA PAYWAVE�ں�
    KERNEL_INX_QMSDC = 6,   //!< MASTER PAYPASS�ں�
    KERNEL_MAX_INX = 7,     //!< ���֧���ں˸���
};


/*
 * Terminal parameters (EMV_TERMPARAM)
 * NOTE: temporary superset restored from the same-generation source; will be
 *       slimmed to the minimal required field set in the struct-minimization phase.
 */
typedef struct {
    uint8_t ics[7];                                      /* ICS */
    uint8_t term_type;                                   /* [9F35] */
    uint8_t status_check_support_indicator;
    uint8_t cl_trans_limit[6];                           /* [DF20] */
    uint8_t acquirer_id[6];                              /* [9F01] */
    uint8_t acquirer_id_exist;
    uint8_t acquirer_id_len;
    uint8_t term_ifd[8];                                 /* [9F1E] */
    uint8_t merchant_category_code[2];                   /* [9F15] */
    uint8_t merchant_id[15];                             /* [9F16] */
    uint8_t merchant_id_exist;
    uint8_t merchant_id_len;
    uint8_t merchant_name[128];                          /* [9F4E] */
    uint8_t merchant_name_len;
    uint8_t merchant_name_exist;
    uint8_t term_country_code[2];                        /* [9F1A] */
    uint8_t term_id[8];                                  /* [9F1C] */
    uint8_t term_id_exist;
    uint8_t term_id_len;
    uint8_t merch_custom_data[20];                       /* [9F7C] */
    uint8_t trans_currency_code[2];                      /* [5F2A] */
    uint8_t trans_currency_exponent;                     /* [5F36] */
    uint8_t poi_info[128];                               /* [8B] */
    uint8_t poi_info_len;
    uint8_t proprietary_tag[128];                        /* [DF830A] */
    uint8_t proprietary_tag_len;
    uint8_t msd_app_ver[2];
    uint8_t card_data_input_cap;                         /* [DF8117] */
    uint8_t rrp_min_grace[2];                            /* [DF8132] */
    uint8_t rrp_max_grace[2];                            /* [DF8133] */
    uint8_t rrp_expect_c_apdu[2];                        /* [DF8134] */
    uint8_t rrp_expect_r_apdu[2];                        /* [DF8135] */
    uint8_t rrp_accuracy_threshold[2];                   /* [DF8136] */
} EMV_TERMPARAM;
typedef struct {

    uint8_t aid[16];                                     /* [9F06] */
    uint8_t aid_len;

    uint8_t app_sel_indicator;                           /* [DF01] 0-partial, 1-exact */
    uint8_t app_ver[2];                                  /* [9F09] */

    uint8_t target_percent;                              /* [DF17] */
    uint8_t max_target_percent;                          /* [DF16] */
    uint8_t threshold_value[4];                          /* [DF15] */

    uint8_t default_ddol_len;                            /* DDOL */
    uint8_t default_ddol[252];                           /* [DF14] */
    uint8_t default_tdol_len;                            /* TDOL */
    uint8_t default_tdol[252];

    uint8_t trans_ref_currency_code[2];                  /* [9F3C] */
    uint8_t trans_ref_currency_exponent;                 /* [9F3D] */

    uint8_t term_action_code_default_exist;
    uint8_t term_action_code_default[5];                 /* [DF11] */
    uint8_t term_action_code_online_exist;
    uint8_t term_action_code_online[5];                  /* [DF12] */
    uint8_t term_action_code_denial_exist;
    uint8_t term_action_code_denial[5];                  /* [DF13] */

    uint8_t term_risk_mana_data_len;
    uint8_t term_risk_mana_data[8];                      /* [9F1D] */

    uint8_t online_pin_support_indicator;                /* [DF18] */
    uint8_t status_check_support_indicator;              /* [1F02] */
    uint8_t cl_trans_limit_exist;
    uint8_t cl_trans_limit[6];                           /* [DF20] */
    uint8_t cvm_limit_exist;
    uint8_t cvm_limit[6];                                /* [DF21] */
    uint8_t cl_floor_limit_exist;
    uint8_t cl_floor_limit[6];                           /* [DF19] */
    uint8_t term_floor_limit_exist;
    uint8_t term_floor_limit[4];                         /* [9F1B] */
    uint8_t zero_amt_allow_flag;                          /* [1F04] */
    uint8_t cvmcap_cvmreq;                               /* [DF8118] */
    uint8_t cvmcap_nocvm;                                /* [DF8119] */
    uint8_t paypass_kernel_config;                       /* [DF811B] */
    uint8_t reader_cl_trans_limit_cdcvm_exist;
    uint8_t reader_cl_trans_limit_cdcvm[6];              /* [DF8125] */
    uint8_t msd_cvmcap_cvmreq_exist;
    uint8_t msd_cvmcap_cvmreq;                           /* [DF811E] */
    uint8_t msd_cvmcap_nocvm_exist;
    uint8_t msd_cvmcap_nocvm;                            /* [DF812C] */
    uint8_t reader_cl_trans_limit_nocdcvm_exist;
    uint8_t reader_cl_trans_limit_nocdcvm[6];            /* [DF8124] */
    uint8_t mobile_sup_indicator;                        /* [9F7E] */
    uint8_t secure_cap;                                  /* [DF811F] */
    uint8_t term_caps[3];                                /* [9F33] */
    uint8_t term_add_caps[5];                            /* [9F40] */
    uint8_t kernel_id;                                   /* [9F2A] */
    uint8_t term_type;
    uint8_t ttq[4];                                      /* [9F66] */

    /* --- fields required by the entry-point pre-processing (new-gen code) --- */
    uint8_t status_check_support;                        /* status check supported */
    uint8_t currency_exp;                                /* [5F36] cached exponent */
    uint8_t zero_amt_offline_allow;
    uint8_t zero_amt_allow;
} EMV_AIDPARAM;

/**
 * �ն�Ӧ���б���ռ20�ֽڣ�
 */
typedef struct {
    uint8_t aid[16];                                     /* [9F06] AID */
    uint8_t aid_len;
    uint8_t asi;                                         /* application selection indicator: 0-partial, 1-exact */
    uint8_t enable;
    uint8_t rsv;                                         /* reserved / extended selection support */
    uint8_t trans_type;
    uint8_t kernel_id;                                   /* [9F2A] kernel identifier (first byte) */
} EMV_TERMAPP;

/**
 * ��ѡӦ���б�
 */
typedef struct {

    uint8_t aid[16];                                     //!< Ӧ�ñ�ʶ��
    uint8_t aid_len;                                     //!< Ӧ�ñ�ʶ������
    uint8_t lable[16];                                   //!< Ӧ�ñ�ǩ
    uint8_t lable_len;                                   //!< Ӧ�ñ�ǩ����
    uint8_t preferred_name[16];                          //!< Ӧ����ѡ����
    uint8_t preferred_name_len;                          //!< Ӧ����ѡ���Ƴ���
    uint8_t priority;                                    //!< Ӧ������ָʾ��
    uint8_t rsv[4];                                      //!< ������rsv[0]���ں��б�������ʾ�ն�Ӧ���б�����,rsv[1] save kernel ID 9F2A��
    uint8_t kernel_id[8];
	uint8_t kernel_id_len;
} EMV_CANDIDATE;

/**
 * �ңӣ���Կ��ʽ
 */
typedef struct {
    uint8_t modulus[248];                                //!< ģ
    uint8_t modulus_len;                                 //!< ģ��
    uint8_t exponent[3];                                 //!< ָ��
} EMV_RSAPUBKEY;

/**
 * �ӣͣ���Կ��ʽ
 */
typedef struct {
    uint8_t r[32];                                       //!< R
    uint8_t s[32];                                       //!< S
} EMV_SM2PUBKEY;

/**
 * ��Կ��ʽ
 */
typedef union {
    EMV_RSAPUBKEY rsa;                              //!< RSA��Կ
    EMV_SM2PUBKEY sm2;                              //!< SM2��Կ
} EMV_PUBLICKEY;

/**
 * �ã���Կ֤���ʽ��ռ512�ֽڣ�
 */
typedef struct {
    uint8_t rid[5];                                      //!< Registered Application Provider Identifier
    uint8_t index;                                       //!< Certification Authority Public Key Index
    uint8_t algorithm;                                   //!< Certification Authority Public Key Algorithm Indicator
    uint8_t hash_algorithm;                              //!< Certification Authority Hash Algorithm Indicator
    EMV_PUBLICKEY key;                              //!< Certification Authority Public Key
    uint8_t expiration_date[4];                          //!< Certificate Expiration Date
    uint8_t checksum[20];                                //!< Certification Authority Public Key Checksum
    uint8_t rsv[228];                                    //!< Reserved
} EMV_CAPKPARAM;

/**
 * ��Ƭ��������¼��ʽ���쳣�ļ���ռ16�ֽڣ�
 */
typedef struct {
    uint8_t pan[10];                                     //!< Application PAN
    uint8_t pan_len;                                     //!< The Length of Application PAN
    uint8_t psn;                                         //!< Application PAN Sequence Number
    uint8_t rsv[4];                                      //!< Reserved
} EMV_CARDBLACK;

/**
 * ֤���������¼��ʽ�������б���ռ16�ֽڣ�
 */
typedef struct {
    uint8_t rid[5];                                      //!< Registered Application Provider Identifier
    uint8_t pki;                                         //!< Certificate Authority Public Key Index
    uint8_t csn[3];                                      //!< Certificate Serial Number
    uint8_t rsv[7];                                      //!< Reserved
} EMV_CERTBLACK;

typedef struct
{
	uint8_t Flag;	//1-Default DRL 0-��ͨ DRL
	uint8_t Index;
	uint8_t CLROFFLimit[6];//Contactless Reader Offline Floor Limit
	uint8_t CLRTLimit[6];//Contactless Reader Trans Limit
	uint8_t CVMLimit[6];//CVM Trans Limit
}BCTC_DRL;

#endif /* EMV_TYPE_H_ */
/*
************************************************************************************************************************
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ END OF FILE ~~~
************************************************************************************************************************
*/
