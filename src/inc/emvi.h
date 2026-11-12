/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     emvi.h
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015��10��21��
* @brief    EMV�ںˣ���������
*
************************************************************************************************************************
*/
#ifndef EMVI_H_
#define EMVI_H_


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "emv_ics.h"
#include "emv_tag.h"
#include "emv_utils.h"
#include "emv.h"
#include "emv_type.h"
#include "ks_hw.h"
#include "emvoutcome.h"
/*
************************************************************************************************************************
*                                                        EXTERNS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                        DEFINES
************************************************************************************************************************
*/
#define DISABLE                         0
#define ENABLE                          1

#define EMV_CFG_ICC_SEND_MAXLEN         262                 /* ��Ƭ����ͳ��� */
#define EMV_CFG_ICC_RECV_MAXLEN         512                 /* ��Ƭ�����ճ��� */

#define EMV_CFG_MAX_APPS                128

#define EMV_CFG_MAX_CANDIDATES          12

#include "emvdbg.h"                 /* ���֧��Ӧ�ø�����������255 */


/*
************************************************************************************************************************
*                                                      ERROR CODES
************************************************************************************************************************
*/

enum _ErrCodes
{
    TLV_ERR                         = (-10100),
    TLV_NOOBJSPACE                  = (TLV_ERR -  1),
    TLV_DETAGLENERR                 = (TLV_ERR -  2),
    TLV_DELENLENERR                 = (TLV_ERR -  3),
    TLV_TAGSTARTILL                 = (TLV_ERR -  4),
    TLV_NOTSINGLEOBJ                = (TLV_ERR -  5),
    TLV_INPUTDATAERR                = (TLV_ERR -  6),

    BUF_ERR                         = (-10200),
    BUF_BUFOVER                     = (BUF_ERR -  1),
    BUF_OBJDUP                      = (BUF_ERR -  2),

	TAG_ERR							= (-10300),
	TAG_LEN_INVALID			        = (TAG_ERR - 1),
	TAG_BELONG_INVALID 				= (TAG_ERR - 2),
	TAG_UPDATE_INVALID				= (TAG_ERR - 3),
	TAG_NOT_IN_DATABASE				= (TAG_ERR - 4),
	TAG_IN_INVALID_TEMPLATE 		= (TAG_ERR - 5),
};


/*
************************************************************************************************************************
*                                                      DATA TYPES
************************************************************************************************************************
*/



enum EMVICC_GAC_ReqTypes
{
    AC_AAC  = 0x00,
    AC_TC   = 0x40,
    AC_ARQC = 0x80,
    _AC_ISSUER_REFERRAL,
};

enum EMVICC_VERIFY_PinTypes
{
    PLAINTEXT_PIN  = 0x80,
    ENCIPHERED_PIN = 0x88,
};

//Book A Table 5-3: Entry Point Pre-Processing Indicators
typedef struct
{
	uint8_t aid[16];
	uint8_t aid_len;
	uint8_t status_check_requested;
	uint8_t contactless_app_not_allowed;
	uint8_t zero_amount;
	uint8_t reader_cvm_limit_exceed;
	uint8_t reader_cl_floor_limit_exceed;
	uint8_t ttq[4];
} EMV_PREPROCESS_RESULT;

typedef struct
{
    int32_t emvbuffer_size;                                     //!< ͨ��<alloc_buffer>�ӿڷ�����ں˵�ȫ�ֻ����С
    uint32_t kernel_checksum;
    uint32_t config_checksum;                                    //!< �ն�����У��ͣ�CRC32��

    EMV_TERMAPP* termapps;                                //!< �ն�Ӧ���б�������
    int32_t termapps_num;                                       //!< �ն�Ӧ�ø���

    uint8_t* tlvbuffer;                                          //!< ���Ӧ�����ݵ�ȫ�ֻ���
    int32_t tlvbuffer_size;                                     //!< ���Ӧ�����ݵ�ȫ�ֻ����С

    EMV_PREPROCESS_RESULT* preprocess_result;
    int preprocess_result_num;

    EMV_CANDIDATE *candidates;                            //!< ��ѡӦ���б�������
    int32_t candidates_num;                                     //!< ��ѡӦ�ø���
    int32_t req_sel_candidates_result;                             //!< ��ǰѡ�е�Ӧ�ö�Ӧ��ѡӦ���б�����

    //----------------------------

    int32_t status;                                             //!< �ں˵�ǰ״̬��@link EMV_Status��

    uint8_t  force_online;                                 //!< �ǽӽ�����������
    uint8_t  card_types_allowed;                                 //!< Ԥ���״������������Ŀ�ƬͨѶ���ͣ�@link EMV_CardTypes��
    uint8_t  card_type;                                          //!< ��ƬͨѶ���ͣ�@link EMV_CardTypes��

    uint8_t  trans_type;                                         //!< �������ͣ�@link EMV_TransTypes��
    uint32_t trans_amount;                                       //!< ���׽��
    uint32_t trans_amount_other;                                 //!< �������׽��
    int32_t req_sel_accounttype_result;                         //!< ��ǰѡ�е��ʻ�����
    int32_t req_check_id_result;
    int32_t req_online_pin_result;

    int32_t req_offline_pin_result;
    uint8_t  offline_pin[12+1];
    uint8_t ec_onlinpin_falg;

    int32_t req_issuer_referral_result;

    int32_t req_confirm_force_online_result;
    int32_t req_confirm_bypass_pin_result;

    //----------------------------
    int32_t online_result; /* the online result */
    //----------------------------
    uint8_t contactless_type;                                           //!< ֧����ʽ��@link EMV_PayModes��

	bool amount_empty;
	bool oth_amount_empty;
	bool amount_absent;
	bool oth_amount_absent;
	bool trans_type_absent;
} EMV_CONTEXT;


/*
************************************************************************************************************************
*                                                   GLOBAL VARIABLES
************************************************************************************************************************
*/

extern const EMV_AIDPARAM   k_emv_aidparam;
extern 		  EMV_CONTEXT*  g_emv_cnxt;
extern const EMV_PORTING* 	g_emv_port;


/*
************************************************************************************************************************
*                                                         MACRO'S
************************************************************************************************************************
*/

#define BIT_GET(bn, data)                   ((data)[(bn) >> 8] &   ((bn) & 0xFF))
#define BIT_SET(bn, data)                   ((data)[(bn) >> 8] |=  ((bn) & 0xFF))
#define BIT_CLR(bn, data)                   ((data)[(bn) >> 8] &= ~((bn) & 0xFF))

#define AUC_For_Domestic_Cash(x)                    (*(x) & 0x80)
#define AUC_For_International_Cash(x)               (*(x) & 0x40)
#define AUC_For_Domestic_Goods(x)                   (*(x) & 0x20)
#define AUC_For_International_Goods(x)              (*(x) & 0x10)
#define AUC_For_Domestic_Services(x)                (*(x) & 0x08)
#define AUC_For_International_Services(x)           (*(x) & 0x04)
#define AUC_For_ATMs(x)                             (*(x) & 0x02)
#define AUC_For_Not_ATMs(x)                         (*(x) & 0x01)
#define AUC_For_Domestic_CashBack(x)                (*((x) + 1) & 0x80)
#define AUC_For_International_CashBack(x)           (*((x) + 1) & 0x40)

#define AUC_For_Domestic_Goods_or_Services(x)       (*(x) & 0x28)
#define AUC_For_International_Goods_or_Services(x)  (*(x) & 0x14)

/**
 * CID  : Cryptogram Information Data - 9F27
 * Terminal Action Analyzing - Generate AC response
 * (x)  : CID
 * Macro:
 * Resp_AAC
 * Resp_TC
 * Resp_ARQC
 * Resp_AAR
 * Resp_Advice
 * Resp_Info
 */
#define Resp_AAC(x)                                 (((x) & 0xC0) == 0x00)
#define Resp_TC(x)                                  (((x) & 0xC0) == 0x40)
#define Resp_ARQC(x)                                (((x) & 0xC0) == 0x80)
#define Resp_AAR(x)                                 (((x) & 0xC0) == 0xC0)
#define Resp_Advice(x)                              ((x) & 0x08)
#define Resp_Info(x)                                ((x) & 0x07)



/** [9F10] - Issuer Application Data (IAD) | ������Ӧ������ */
#define IAD_AAC(x)                                  ((*((x) + 4) & 0x30) == 0x00)
#define IAD_TC(x)                                   ((*((x) + 4) & 0x30) == 0x10)
#define IAD_ARQC(x)                                 ((*((x) + 4) & 0x30) == 0x20)
#define IAD_FCHECK(x)                               ((*((x) + 5) & 0x20) == 0x20)

/** [9F6C] - Card Transaction Qualifiers (CTQ) | ��Ƭ�������� */
#define CTQ_REQ_ONLINE_PIN(x)                       BIT_GET(BN_CTQ_REQ_ONLINE_PIN, x)
#define CTQ_REQ_SIGNATURE(x)                        BIT_GET(BN_CTQ_REQ_SIGNATURE, x)
#define CTQ_GO_ONLINE_IF_ODA_FAIL(x)                BIT_GET(BN_CTQ_GO_ONLINE_IF_ODA_FAIL, x)
#define CTQ_TERMINATE_IF_ODA_FAIL_AND_SUP_PBOC(x)   BIT_GET(BN_CTQ_TERMINATE_IF_ODA_FAIL_AND_SUP_PBOC, x)


#define EMVPORT_Message                             g_emv_port->message
#define EMVPORT_SelCandidateList                    g_emv_port->sel_candidate_list
#define EMVPORT_SelAccountType                      g_emv_port->sel_account_type
#define EMVPORT_DeactivateCard                      g_emv_port->deactivate_card
#define EMVPORT_CheckID                             g_emv_port->check_id
#define EMVPORT_PinEntry                            g_emv_port->pin_entry
#define EMVPORT_ConfirmBypassPIN                    g_emv_port->confirm_bypass_pin
#define EMVPORT_ConfirmForceOnline                  g_emv_port->confirm_force_online
#define EMVPORT_GoOnline                            g_emv_port->go_online
#define EMVPORT_IssuerReferral                      g_emv_port->issuer_referral
#define EMVPORT_AllocBuffer                         g_emv_port->alloc_buffer
#define EMVPORT_IccRW                               g_emv_port->icc_rw
#define EMVPORT_Recover                             g_emv_port->recover
#define EMVPORT_Hash                                g_emv_port->hash
#define EMVPORT_GetTransDateTime                    g_emv_port->get_trans_date_time
#define EMVPORT_GetTransSequenceCounter             g_emv_port->get_trans_sequence_counter
#define EMVPORT_GetRandom                           g_emv_port->get_random
//#define EMVPORT_GetTermParam                        g_emv_port->get_termparam
#define EMVPORT_GetTermApps                         g_emv_port->get_termapps
#define EMVPORT_GetAppParam                         g_emv_port->get_aidparam
#define EMVPORT_GetCapkParam                        g_emv_port->get_capkparam
#define EMVPORT_GetTransLogAmount                   g_emv_port->get_translog_amount
#define EMVPORT_SavTransLog                         g_emv_port->sav_translog
#define EMVPORT_FndBlackCard                        g_emv_port->fnd_blackcard
#define EMVPORT_FndBlackCert                        g_emv_port->fnd_blackcert
#define EMVPORT_GetKernelIndex                      g_emv_port->get_kernel_index
#define EMVPORT_IccPwron                            g_emv_port->icc_pwron


/*
************************************************************************************************************************
*                                                  FUNCTION PROTOTYPES
************************************************************************************************************************
*/
int32_t   emv_memcmp(const void *dst, const void *src, uint32_t count);
void *emv_memcpy(void *dst, const void *src, uint32_t n);
void *emv_memset(void *dst, int32_t s, uint32_t count);
uint32_t emv_strlen(const char *s);

void emv_bcd2str(uint8_t *bcd, int32_t bcd_len, uint8_t *str);
void emv_str2bcd(uint8_t *str, int32_t str_len, uint8_t *bcd, int32_t bcd_len, bool ralign);

void emv_dec2hex(uint32_t dec, uint8_t *hex, int32_t hexlen);
uint32_t emv_hex2dec(const uint8_t *hex, int32_t hexlen);

void emv_dec2bcd(uint32_t dec, uint8_t *bcd, int32_t bcdlen);
uint32_t emv_bcd2dec(const uint8_t *bcd, int32_t bcdlen);

void calc_tc_hash_value(uint8_t *dol, int32_t len);
int32_t emv_date_compare(uint8_t *date, int32_t len);
int32_t emv_bcd2hex(const uint8_t *bcd_data, int32_t bcd_len, uint8_t *hex_data);

////////////////////////////////////////////////////////////////////////////////
/* emvicc.c */

int32_t emvicc_apdu(uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen);


/**
 * �ⲿ��֤��EXTERNAL AUTHENTICATE��.
 *
 * @param data      ��������֤����.
 * @param datalen   ��������֤���ݵĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_external_authenticate(uint8_t *data, int32_t datalen);


/**
 * ����Ӧ�����ģ�GENERATE APPLICATION CRYPTOGRAM��.
 *
 * @param type      ��������Ӧ�����ĵ����ͣ�@see EMVICC_GAC_ReqTypes��.
 * @param cda       ����Ƿ�����CDA.
 * @param cdolv     CDOLָ��������.
 * @param cdolvlen  CDOLָ�������ݵĳ���.
 * @param out       ��Ӧ��������.
 * @param outlen    ��Ӧ�������ݵĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_generate_ac(uint8_t type, bool cda, uint8_t *cdolv, int32_t cdolvlen, uint8_t *out, int32_t *outlen);


/**
 * ��ȡ�������GET CHALLENGE��.
 *
 * @param out       ���������.
 * @param outlen    ��������ȣ��̶�Ϊ8�ֽڣ�.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_get_challenge(uint8_t *out, int32_t *outlen);


/**
 * ȡ���ݣ�GET DATA��.
 *
 * @param tag       Ҫ�������ݵı�ǩ
 * @param out       ��ǩ��Ӧ�����ݣ�TLV��ʽ��
 * @param outlen    ��ǩ��Ӧ�����ݵĳ���
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_get_data(uint32_t tag, uint8_t *out, int32_t *outlen);
int32_t emvicc_spi(uint8_t *pdolv, int32_t pdolvlen, uint8_t *out, int32_t *outlen);
void emvicc_set_stop_flag(bool flag);
bool emvicc_get_stop_flag(void);


/**
 * ��ȡ����ѡ�GET PROCESSING OPTIONS��.
 *
 * @param pdolv     PDOLָ��������.
 * @param pdolvlen  PDOLָ�������ݵĳ���.
 * @param out       ��Ӧ��������.
 * @param outlen    ��Ӧ�������ݵĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_get_processing_options(uint8_t *pdolv, int32_t pdolvlen, uint8_t *out, int32_t *outlen);


/**
 * �ڲ���֤��INTERNAL AUTHENTICATE��.
 *
 * @param ddolv     DDOLָ��������.
 * @param ddolvlen  DDOLָ�������ݵĳ���.
 * @param out       ��Ӧ��������.
 * @param outlen    ��Ӧ�������ݵĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_internal_authenticate(uint8_t *ddolv, int32_t ddolvlen, uint8_t *out, int32_t *outlen);


/**
 * ����¼��READ RECORD��.
 *
 * @param sfi       SFI.
 * @param recno     ��¼��.
 * @param out       ��¼����.
 * @param outlen    ��¼���ݵĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_read_record(uint8_t sfi, uint8_t recno, uint8_t *out, int32_t *outlen);


/**
 * ѡ��SELECT��.
 *
 * @param name      �ļ�����.
 * @param namelen   �ļ����Ƶĳ���.
 * @param next      ����Ƿ�ѡ����һ��.
 * @param out       FCI.
 * @param outlen    FCI�ĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_select(uint8_t *name, uint8_t namelen, bool next, uint8_t *out, int32_t *outlen);


/**
 * ��֤��VERIFY��.
 *
 * @param type      PIN���ͣ�@see EMVICC_VERIFY_PinTypes��.
 * @param pin       ����PIN����.
 * @param pinlen    ����PIN���ݵĳ���.
 * @return          [>0]-SW1SW2, [<0]-����.
 */
int32_t emvicc_verify(uint8_t type, uint8_t *pin, uint8_t pinlen);

int32_t emvicc_relay_resistance_protocol(uint8_t *term_rr_entropy, int32_t len, uint8_t *out, int32_t *outlen);
int32_t emvicc_compute_crypyographic_checksum(uint8_t *udolv, int32_t udolvlen, uint8_t *out, int32_t *outlen);
int32_t emvicc_put_data(uint32_t tag, uint8_t *in, int32_t inlen);
int32_t emvicc_get_data(uint32_t tag, uint8_t *out, int32_t *outlen);
int32_t emvicc_spi(uint8_t *pdolv, int32_t pdolvlen, uint8_t *out, int32_t *outlen);
void emvicc_set_stop_flag(bool flag);
bool emvicc_get_stop_flag(void);
////////////////////////////////////////////////////////////////////////////////
/* emvtlv.c */
int32_t remove_tag_from_tags2readyet(int32_t tagname);
int32_t add_tag_to_tags2readyet(int32_t tagname);
int32_t remove_tag_from_tags2write_before_gac(int32_t tagname);
int32_t add_tag_to_tags2readyet(int32_t tagname);
int32_t add_tag_to_dataneed(int32_t tagname);
int32_t add_tag_to_tags2write_yet_before_gac(int32_t tagname);
int32_t add_tag_to_tags2write_yet_after_gac(int32_t tagname);
bool data2send_contains_key(int32_t tagname);
int32_t add_tlv_to_tags2write_yet_after_gac(int32_t tagname, int32_t len, uint8_t *value);
int32_t add_tlv_to_tags2write_yet_before_gac(int32_t tagname, int32_t len, uint8_t *value);
int32_t add_tlv_to_data2send(int32_t tagname, int32_t len, uint8_t *value);
int32_t add_tag_to_data2send(int32_t tagname);

/**
 * @brief ����TLV��ʽ���ݰ�
 * @param tlv       TLV��ʽ���ݰ�
 * @param tlvlen    TLV��ʽ���ݰ��ĳ���
 * @param objs      �������TLVOBJ��tlvobj_t��
 * @param objspace  �������TLVOBJ������
 * @param deflag    ������ȱ��
 *      @arg SINGLE_TLVOBJ  ����TLV���ݽ���
 *      @arg STRING_TLVOBJ  ���TLV���ݽ���
 *      @arg DECODE_LEVEL1  ֻ������1����TLV����
 * @return 0-�ɹ�
 */
int32_t emvtlv_decode(uint8_t *data, int32_t data_len, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags);
int32_t emvtlv_get(int32_t parent, uint32_t tag, EMV_TLVOBJ *tlvs, int32_t level);
int32_t emvtlv_find_dup(int32_t pos, EMV_TLVOBJ *tlvs);

int32_t tlv_get_tag(uint8_t* buf, uint32_t* tag);
int32_t tlv_get_len(uint8_t* buf, int32_t* len);
int32_t tlv_set_tag(uint32_t tag, uint8_t* buf);
int32_t tlv_set_len(int32_t len, uint8_t* buf);

////////////////////////////////////////////////////////////////////////////////
/* emvdol.c */
int32_t emvdol_packet(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t valspace);
int32_t emvdol_packet_tlv(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t vallen, uint8_t *tlv, int32_t tlvlmt);
int32_t emvdol_tag_exists(uint32_t tagname, uint8_t *dol, int32_t dollen);
int cdol_packet(uint8_t *dol, int dollen, uint8_t *val, int val_max_len);
int udol_packet(uint8_t *dol, int dollen, uint8_t *val, int val_max_len);

////////////////////////////////////////////////////////////////////////////////
/* emvbuf.c */

//override
#define UNIQUE                  (0)                         //!< ������д
#define OVERRIDED               (1)                         //!< ������д

int32_t appdata_init(void);
int32_t appdata_save(uint32_t tag, uint8_t *value, int32_t valuelen, int32_t override);
int32_t appdata_save2(EMV_TLVOBJ *tlv, int32_t override);
int32_t appdata_save_iccdata(uint32_t tag);
uint8_t* appdata_find(uint32_t tag, int32_t *pvaluelen);
int32_t appdata_save_singlebyte(uint32_t tag, uint8_t value, int32_t override);
int32_t appdata_save_tlv(EMV_TLVOBJ *tlv, int32_t override, uint32_t template);
int32_t appdata_save_or(uint32_t tag, int32_t index, uint8_t value);
int32_t appdata_save_and(uint32_t tag, int32_t index, uint8_t value);
////////////////////////////////////////////////////////////////////////////////
/* emv_appsel.c */
extern int32_t emv_build_candidate_list(void);
extern int32_t emv_app_sel_init(void);
extern int32_t emv_initiate_application(void);
extern int32_t emv_read_application_data(void);

extern int32_t track2_parse(uint8_t *pTrack, int32_t nTrackLen, uint8_t *pPan, int32_t *pPanLen, uint8_t *pExpireDate, uint8_t *pServiceCode);

////////////////////////////////////////////////////////////////////////////////
/* emv_security.c */
//extern int32_t process_sda(void);
//extern int32_t process_dda(int32_t forcda);
//extern int32_t encrypt_pin(uint8_t* pin, uint8_t* epin);
//extern int32_t process_cda(int32_t nstep, uint8_t * gac_resp, int32_t gac_len);
//extern int32_t emv_offline_data_auth(void);

////////////////////////////////////////////////////////////////////////////////
/* emv_restrict.c */
extern int32_t emv_processing_restrictions(void);


////////////////////////////////////////////////////////////////////////////////
/* emv_cv.c */
enum EMV_CV_Results
{
    CV_FAIL                     = 1,                            ///< ʧ��
    CV_NOT_SUPPORTED            = 2,                            ///< ��֧��
    CV_UNRECOGNISED             = 3,                            ///< �޷�ʶ��
    CV_OMIT                     = 4,                            ///< ����
    CV_SUCCESS                  = 5,                            ///< �ɹ�
    CV_ONLINE                   = 6,                            ///< �ɹ�-�ȴ�����
    CV_SIGNATURE                = 7,                            ///< �ɹ�-�ȴ�ǩ��
    CV_SIGNATURE_FAIL           = 8,                            ///< ǩ��-��֧��ǩ��
    CV_CONFIRMCD                = 9,
    _CV_REQ_CHECK_ID            = 0xF1,
    _CV_REQ_ONLINE_ENC_PIN      = 0xF2,
    _CV_REQ_OFFLINE_PIN         = 0xF3,
    _CV_REQ_OFFLINE_PIN_LAST    = 0xF4,
    _CV_REQ_CONFIRM_PIN_BYPASS  = 0xF5,
};

enum EMV_CVM_Results
{
    CVM_RESULT_UNKNOWN      = 0x00,
    CVM_RESULT_FAILED       = 0x01,
    CVM_RESULT_SUCCESSFUL   = 0x02,
};

enum EMV_CVM_PERFORMED
{
	CV_CDCVM				= 0x01,
	CV_NOCVM				= 0x3F,
};

extern int32_t process_online_pin(void);
extern int32_t emv_cardholder_verify(void);

////////////////////////////////////////////////////////////////////////////////
/* emv_trm.c */
extern int32_t emv_terminal_risk_manage(void);


////////////////////////////////////////////////////////////////////////////////
/* emv_taa.c */
extern int32_t cid_msg(uint8_t cid);
extern int32_t check_action_code_denial(void);
extern int32_t check_action_code_default(void);
extern int32_t check_action_code_online(void);
extern int32_t analyze_gac_resp(uint8_t *resp, int32_t *rlen, int32_t cdaflag);

extern int32_t emv_action_analyse(void);

////////////////////////////////////////////////////////////////////////////////
uint8_t emv_online_processing(void);

////////////////////////////////////////////////////////////////////////////////
int32_t script_process(int32_t when, uint8_t* res, int32_t* reslen);



////////////////////////////////////////////////////////////////////////////////
/* emv_complete.c */
extern int32_t emv_compelte(int32_t *msginx);
void candidate_del(int32_t index);



/*
************************************************************************************************************************
*                                                 CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                               CROSS-MODULE PROTOTYPES
************************************************************************************************************************
*/
int32_t emv_select_kernel(uint8_t card_type, uint8_t emv_core);
uint32_t ics_option(uint32_t option_id);      /* provided by the application (ICS options) */
int32_t CreateEMVDiscretionaryData(void);
int32_t emv_combination_Selection(void);
int32_t emvtlv_decode_paypass(uint8_t *data, int32_t data_len, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags);
#endif /* EMVI_H_ */
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
