/**
************************************************************************************************************************
*                                     COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv.h
* @brief    EMV�ں˽ӿ�
* @author   Kingsee Development Team - xinyong
* @version  V43
*
************************************************************************************************************************
*/
#ifndef EMV_H_
#define EMV_H_

#define EMV_RELEASE_V   1
#define EMV_QVSDC_FLAG 1
#define EMV_FLASHCARD_FLAG 0
#define EMV_ABC_FLAG   1
#define EMV_KER_IN_K21   0



//�л� emv���Ժ����ð汾
#define EMV_ANDROID_SO   0


#include "emv_type.h"


#if defined(__cplusplus)
extern "C" {
#endif


/**
 * ��ֲ�ӿ�
 */
typedef struct {

    //--------------------------------------------------------------------------
    //���Խӿ�
    //--------------------------------------------------------------------------

    /**
     * ���Խӿ�
     *
     * @param   level                               [IN]���Լ���
     * @param   fmt                                 [IN]�����ʽ
     */
    void (*trace)(uint32_t level, char* fmt, ...);

    /**
     * ���Խӿڣ�HEX��
     *
     * @param   level                               [IN]���Լ���
     * @param   msg                                 [IN]��ʾ��Ϣ
     * @param   hex                                 [IN]��������
     * @param   hexlen                              [IN]�������ݵĳ���
     */
    void (*trace_hex)(uint32_t level, char* msg, const void* hex, uint32_t hexlen);

    //--------------------------------------------------------------------------
    //�û������ӿ�
    //--------------------------------------------------------------------------

    /**
     * ��ʾ��ʾ��Ϣ
     *
     * @param   id                                  [IN]��ʾ��Ϣ�������ο�{@link EMV_MessageIDs}
     */
    int32_t (*message)(int32_t id);

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
    int32_t (*sel_candidate_list)(int32_t again, const EMV_CANDIDATE* list, int32_t count);

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
    int32_t (*sel_account_type)(void);

    /**
     * ��ʾ���ȴ��û��ƿ���QPBOC��
     *
     * @return  {@link EMV_OPER_OK}                 �ɹ�
     * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_DEACTIVATE_CARD}
     */
    int32_t (*deactivate_card)(void);

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
    int32_t (*check_id)(uint8_t type, const uint8_t *num, int32_t numlen);

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
    int32_t (*pin_entry)(uint8_t type, uint8_t* output, int32_t* outlen);

    /**
     * ȷ���Ƿ���������Уɣ�
     *
     * @return  {@link EMV_OPER_OK}                 ����
     * @return  {@link EMV_OPER_CANCEL}             ������
     * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
     * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CONFIRM_BYPASS_PIN}
     */
    int32_t (*confirm_bypass_pin)(void);

    /**
     * ȷ���Ƿ�ǿ������
     *
     * @return  {@link EMV_OPER_OK}                 ǿ������
     * @return  {@link EMV_OPER_CANCEL}             ��ǿ������
     * @return  {@link EMV_OPER_UNSUPPORTED}        ��֧��
     * @return  {@link EMV_OPER_REPORT_APP}         ��������֮���ں˻᷵��{@link EMV_REQ_CONFIRM_FORCE_ONLINE}
     */
    int32_t (*confirm_force_online)(void);

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
    int32_t (*go_online)(uint8_t* resp, int32_t* resplen);

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
    int32_t (*issuer_referral)(uint8_t* pan, int32_t panlen);

    //--------------------------------------------------------------------------

    /**
     * ����ȫ�ֻ��棬���ڴ��Ӧ�����ݣ����鿪7168�ֽ����ϣ�
     *
     * @param   size                                [OU]�����ȫ�ֻ����С
     * @return  ȫ�ֻ���ĵ�ַ
     *
     * @warning ȫ�ֻ����ڽ��������в���������\n
     *          ȫ�ֻ�������������ط����ã��ٴ�ʹ��EMVǰ�������³�ʼ��
     */
    uint8_t* (*alloc_buffer)(int32_t* size);

    //--------------------------------------------------------------------------

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
    int32_t (*icc_rw)(int32_t type, uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen);

    //--------------------------------------------------------------------------

    /**
     * ��Կ����
     *
     * @param   alg                                 [IN]��Կ�㷨��ʶ���ο�{@link EMV_PubKeyAlgorithms}
     * @param   pk                                  [IN]��Կ����ʽΪ{@link EMV_PUBLICKEY}
     * @param   input                               [IN]ԭʼ����
     * @param   output                              [OU]�ָ�����
     * @return  �ָ����ݳ��ȣ����ڹ�Կģ���ȣ���С�ڵ���0��ʾʧ��
     */
    int32_t (*recover)(int32_t alg, EMV_PUBLICKEY* pk, uint8_t* input, uint8_t* output);

    /**
     * ��ϣ����
     *
     * @param   alg                                 [IN]��ϣ�㷨��ʶ���ο�{@link EMV_HashAlgorithms}
     * @param   input                               [IN]��������
     * @param   inlen                               [IN]�������ݳ���
     * @param   output                              [OU]��ϣֵ
     * @return  ��ϣֵ���ȣ����ݹ�ϣ�㷨��������С�ڵ���0��ʾʧ��
     */
    int32_t (*hash)(int32_t alg, uint8_t* input, int32_t inlen, uint8_t* output);

    //--------------------------------------------------------------------------

    /**
     * ��ȡ�����
     *
     * @param   buf                                 [OU]���������
     * @param   len                                 [IN]����������ȡ����
     * @return  0                                   �ɹ�
     * @return -1                                   ʧ��
     */
    int32_t (*get_random)(uint8_t *buf, int32_t len);

    /**
     * ��ȡ��ǰ���ں�ʱ��
     *
     * @param   date                                [OU]���ڻ��棨��ʽBCD��YYYYMMDD, 4�ֽڣ�
     * @param   time                                [OU]ʱ�仺�棨��ʽBCD��hhmmss, 3�ֽڣ�
     * @return  0                                   �ɹ�
     * @return -1                                   ʧ��
     */
    int32_t (*get_trans_date_time)(uint8_t* date, uint8_t* time);

    //--------------------------------------------------------------------------

    /**
     * ��ȡ�������м�������9F41��
     *
     * @param   buf     [OU]������棨BCD��ʽ, 4�ֽڣ�
     * @return  0       �ɹ�
     * @return -1       ʧ��
     */
    int32_t (*get_trans_sequence_counter)(uint8_t* buf);

    /**
     * ��ȡ�ն����ò���
     *
     * @param   par     [OU]�ն����ò������ο�{@link EMV_TERMPARAM}
     * @return  0       �ɹ�
     * @return -1       ʧ��
     */
//    int32_t (*get_termparam)(EMV_TERMPARAM* par);

    /**
     * ��ȡ�ն�Ӧ���б�
     *
     * @param   apps    [OU]�ն�Ӧ���б����飬�ο�{@link EMV_TERMAPP}
     * @return  �ն�֧�ֵ�Ӧ�ø������ں����֧��128��
     */
    int32_t (*get_termapps)(EMV_TERMAPP* apps);

    /**
     * ��ȡӦ�ò���
     *
     * @param   par     [OU]Ӧ�ò������ο�{@link EMV_AIDPARAM}
     * @param   inx     [IN]Ӧ�ö�Ӧ�ն�Ӧ���б�������
     * @return  0       �ɹ�
     * @return -1       ʧ��
     */
    int32_t (*get_aidparam)(EMV_AIDPARAM* par, int32_t inx);

    /**
     * ��ȡCA��Կ֤��
     *
     * @param   crt     [OU]CA��Կ֤�飬�ο�{@link EMV_CAPKPARAM}
     * @param   pki     [IN]CA��Կ����
     * @param   rid     [IN]CA��Կ��Ӧ��RID��5�ֽڣ�
     * @return  0       �ɹ�
     * @return -1       ʧ��
     */
    int32_t (*get_capkparam)(EMV_CAPKPARAM* crt, uint8_t pki, const uint8_t* rid);

    /**
     * ��ȡ�ѻ���׼������־��ָ�����˺ŵĽ��׽��
     *
     * @param   pan     [IN]Ӧ�����˺�
     * @param   panlen  [IN]Ӧ�����˺ų���
     * @param   policy  [IN]��ȡ���ԣ�0-ĩ�ʽ��׽�1-���н����ܽ�
     * @return  ���׽��
     */
    uint32_t (*get_translog_amount)(const uint8_t* pan, uint8_t panlen, uint8_t policy);

    /**
     * �����ѻ���׼������־
     *
     * @return  0       ����ɹ�
     * @return -1       ����ʧ��
     */
    int32_t (*sav_translog)(const uint8_t* pan, uint8_t panlen, uint32_t amount);

    /**
     * �������˺��Ƿ��ڿ�Ƭ���������쳣�ļ�����
     *
     * @param   pan     [IN]Ӧ�����˺ţ�BCD��
     * @param   panlen  [IN]Ӧ�����˺ų��ȣ����10�ֽڣ�
     * @param   seq     [IN]Ӧ�����˺����к�
     * @return  0       �����˺Ų��ں�������
     * @return  1       �����˺����ں�������
     */
    int32_t (*fnd_blackcard)(const uint8_t* pan, uint8_t panlen, uint8_t seq);

    /**
     * ����֤���Ƿ���֤���������֤������б�����
     *
     * @param   rid     [IN]ע���Ӧ���ṩ�̱�ʶ��5�ֽڣ�
     * @param   pki     [IN]��֤���Ĺ�Կ����
     * @param   csn     [IN]��Կ֤�����кţ�3�ֽڣ�
     * @return  0       ��֤�鲻�ں�������
     * @return  1       ��֤�����ں�������
     */
    int32_t (*fnd_blackcert)(const uint8_t* rid, uint8_t pki, const uint8_t* csn);


    /**
     * ��ȡ�ں�����
     *
     * @return  �ں�����
     */
    uint8_t (*get_kernel_index)(void);

    /**
     * ��Ƭ�ϵ�
     *
     * @return  0--�ɹ�  1--����ͻ  <0--ʧ��
     */
    int32_t (*icc_pwron)(void);

} EMV_PORTING;


/**
 * �ں˳�ʼ��
 *
 * @param   pPort                                   [IN]��ֲ�ӿڣ��ο�{@link EMV_PORTING}
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_NULPORTING}                  ��ֲ�ӿ�NULL
 * @return  {@link EMV_GETTERMPARAMERR}             �����ն����ò���ʧ��
 * @return  {@link EMV_GETTERMAPPSERR}              �����ն�Ӧ���б�ʧ��
 * @return  {@link EMV_ALLOCBUFFERERR}              �����ȫ�ֻ���ռ䲻��
 *
 * @note    �ýӿ�ֻ��Ҫ��������һ�μ��ɣ�������������ն����ò������ն�Ӧ���б��������µ��á�
 */
int32_t EMV_Init(const EMV_PORTING* pPort);

/**
 * ���ý�������
 *
 * @param   eType                                   [IN]�������ͣ��ο�{@link EMV_TransTypes}
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_NONIDLE}                     �ں˷ǿ���״̬
 */
int32_t EMV_SetTransType(uint8_t eType);

/**
 * ��ȡ��������
 *
 * @param   eType                                   [IN]�������ͣ��ο�{@link EMV_TransTypes}
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 */
int32_t EMV_GetTransType(uint8_t *eType);

/**
 * ���ý��׽��
 *
 * @param   nAmount                                 [IN]���׽���λ���֣�
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_NONIDLE}                     �ں˷ǿ���״̬
 */
int32_t EMV_SetTransAmount(uint32_t nAmount);

/**
 * �����������
 *
 * @param   nAmount                                 [IN]��������λ���֣�
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_NONIDLE}                     �ں˷ǿ���״̬
 */
int32_t EMV_SetOtherAmount(uint32_t nAmount);

/**
 * ����Ԥ������QPBOCר�ã�
 *
 * @param   bOnline                                 [IN]����Ƿ���������
 * @return  {@link EMV_OK}                          �����ǽӴ�ʽ����·��
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_NONIDLE}                     �ں˷ǿ���״̬
 * @return  {@link EMV_AMOUNTOVER}                  ���׽����ն˷ǽӴ������޶�
 * @return  {@link EMV_AMOUNTZERO}                  ���׽��Ϊ�����ն˲�֧������
 * @return  {@link EMV_QPBOCUNSUPPORTED}            �ն˽������Բ�֧��QPBOC
 */
int32_t EMV_PreProcess(uint8_t bOnline);

/**
 * ���ÿ�Ƭ����
 *
 * @param   eType                                   [IN]��Ƭ���ͣ��ο�{@link EMV_CardTypes}
 *   @arg   {@link EMV_CARD_CONTACT}                �Ӵ�ʽ
 *   @arg   {@link EMV_CARD_CONTACTLESS}            �ǽӴ�ʽ
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_NONIDLE}                     �ں˷ǿ���״̬
 * @return  {@link EMV_CARDTYPEILL}                 ��ƬͨѶ��ʽ������
 */
int32_t EMV_SetCardType(uint8_t eType);

/**
 * ��������
 *
 * @return  [-1000 ~ -9999]                         ����״̬����ֵ������ο�{@link EMV_Status}
 * @return  [    1 ~    99]                         �м�״̬����ֵ������ο�{@link EMV_Status}
 * @return  [  100 ~   199]                         ����״̬����ֵ������ο�{@link EMV_Status}
 */
int32_t EMV_Process(int32_t *msginx);

/**
 * ����ѡ���ѡӦ���б��Ľ��
 *
 * @param   eResult                                 [IN]ѡ��Ӧ�ö�Ӧ��ѡӦ���б���������0��ʼ��
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_SELCANDIDATENOTREQ}          �ں�δ����ѡ���ѡӦ���б�
 */
int32_t EMV_ResultSelCandidates(int32_t eResult);

/**
 * ����ѡ���ʻ����͵Ľ��
 *
 * @param   eResult                                 [IN]ѡ�е��ʻ����ͣ��ο�{@link EMV_AccountTypes}
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_SELACCOUNTNOTREQ}            �ں�δ����ѡ���ʻ�����
 */
int32_t EMV_ResultSelAccountType(int32_t eResult);


/**
 * ���ò���ֿ���֤���Ľ��
 *
 * @param   eResult                                 [IN]��������0-��ͨ����1-ͨ����
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_CHECKIDNOTREQ}               �ں�δ�������ֿ���֤��
 */
int32_t EMV_ResultCheckID(int32_t eResult);

/**
 * ��������PIN������
 *
 * @param   eResult                                 [IN]���������ο�{@link EMV_OperResults}
 *   @arg   {@link EMV_OPER_OK}                     ������������
 *   @arg   {@link EMV_OPER_BYPASS}                 �����루����������ֱ�Ӱ�ȷ�ϼ��������룩
 *   @arg   {@link EMV_OPER_UNSUPPORTED}            ��֧�ֻ����쳣
 *   @arg   {@link EMV_OPER_TIMEOUT}                ��ʱ�˳�
 *   @arg   {@link EMV_OPER_CANCEL}                 ����ȡ��
 *   @arg   {@link EMV_OPER_QUIT}                   �����˳�
 * @param   pData                                   [IN]����PIN�������ݣ�ΪNULL��ʾ����Ҫ�ں˱��棩
 * @param   nDataLen                                [IN]����PIN�������ݳ��ȣ����֧��256�ֽڣ�
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_ONLINEPINNOTREQ}             �ں�δ������������PIN
 *
 * @note    pData��nDatalenֻ����������eResultΪEMV_OPER_OKʱ��Ч��
 *          ��ʽ��Ӧ���Զ��壬�ں�ֻ���𱣴棬����ı�ǩΪ{@link TAG_ONLINE_PIN}��
 */
int32_t EMV_ResultOnlinePIN(int32_t eResult, uint8_t* pData, int32_t nDataLen);

/**
 * �����ѻ�PIN������
 *
 * @param   eResult                                 [IN]���������ο�{@link EMV_OperResults}
 *   @arg   {@link EMV_OPER_OK}                     ������������
 *   @arg   {@link EMV_OPER_BYPASS}                 �����루����������ֱ�Ӱ�ȷ�ϼ��������룩
 *   @arg   {@link EMV_OPER_UNSUPPORTED}            ��֧�ֻ����쳣
 *   @arg   {@link EMV_OPER_TIMEOUT}                ��ʱ�˳�
 *   @arg   {@link EMV_OPER_CANCEL}                 ����ȡ��
 *   @arg   {@link EMV_OPER_QUIT}                   �����˳�
 * @param   pData                                   [IN]�ѻ�PINԭʼ���ݣ���֧��NULL����ʽΪASC��
 * @param   nDataLen                                [IN]�ѻ�PINԭʼ���ݵĳ��ȣ�֧��4 ~ 12���ַ���
 * @return  {@link EMV_OK}                          ���óɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_OFFLINEPINNOTREQ}            �ں�δ���������ѻ�PIN
 *
 * @note    pData��nDatalenֻ����������eResultΪEMV_OPER_OKʱ��Ч��
 *          ��ʽ������PIN�������ַ��������磺����="123456"����ôpData="123456"��nDataLen=6��
 */
int32_t EMV_ResultOfflinePIN(int32_t eResult, uint8_t* pData, int32_t nDataLen);

/**
 * ���óֿ���ȷ���Ƿ�����PIN����Ľ��
 *
 * @param   eResult                                 [IN]������־��0-��������1-������
 * @return  {@link EMV_OK}                          ���óɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_BYPASSPINNOTREQ}             �ں�δ����ȷ���Ƿ�����PIN����
 */
int32_t EMV_ResultBypassPIN(int32_t eResult);

/**
 * �����̻�ȷ���Ƿ�ǿ�������Ľ��
 *
 * @param   eResult                                 [IN]ǿ��������־��0-��ǿ��������1-ǿ��������
 * @return  {@link EMV_OK}                          ���óɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_FORCEONLINENOTREQ}           �ں�δ����ȷ���Ƿ�ǿ������
 */
int32_t EMV_ResultForceOnline(int32_t eResult);

/**
 * �����������
 *
 * @param   eResult                                 [IN]����������ο�{@link EMV_OnlineResults}
 * @param   pResp                                   [IN]��Ȩ��Ӧ���ݣ��൱��55�����ݣ�TLV��ʽ��
 * @param   nRespLen                                [IN]��Ȩ��Ӧ���ݳ���
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_GOONLINENOTREQ}              �ں�δ����������Ȩ
 *
 * @note    �����Ȩ��Ӧ��������ARC(8A)����IAD(91)��10�ֽڣ���ȡIAD��9~10�ֽڵ�ARC��;\n
 *          �����Ȩ��Ӧ��������ARC(8A)����IAD(91)��10�ֽڣ�����������������:\n
 *          - EMV_ONLINE_SUCC_ACCEPTʱ��ARC = "00"\n
 *          - EMV_ONLINE_SUCC_DENIALʱ��ARC = "05"\n
 *          - EMV_ONLINE_SUCC_ISSREFʱ��ARC = "01"\n
 */
int32_t EMV_ResultOnline(int32_t eResult, uint8_t* pResp, int32_t nRespLen);

/**
 * ���÷����вο����
 *
 * @param   eResult                                 [IN]�����вο������0-�ܾ���1-��׼��
 * @return  {@link EMV_OK}                          ���óɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_ISSREFERRALNOTREQ}           �ں�δ���󷢿��вο�
 */
int32_t EMV_ResultIssuerReferral(int32_t eResult);

/**
 * ��ȡָ����ǩ��T���ĳ��ȣ�L����ֵ��V��
 *
 * @param   nTag                                    [IN]��ǩ��T�����ο�{@link emv_tag.h}
 * @param   pLen                                    [OU]���ȣ�L����ΪNULLʱ���������
 * @return  ֵ��V����ָ�룬����NULL��ʾû���ҵ�
 */
uint8_t* EMV_GetData(uint32_t nTag, int32_t* pLen);


/**
 * ����ָ����ǩ�����ݺͳ���(��������д)
 *
 * @param   tag                             [IN]��ǩֵ������: 0x9F02
 * @param   value                           [IN]��ǩ��Ӧ�����ݻ���
 * @param   length                          [IN]��ǩ��Ӧ�����ݳ���
 * @return  {@link EMV_OK}                  �ɹ�
 * @return  {@link EMV_FAIL}                ʧ��
 * @note    ֻ����{@link EMV_Process}����{@link EMV_STA_CANDIDATES_BUILT}ʱ������Ч
 */
int32_t EMV_SetData(uint32_t tag, uint8_t *value, int32_t valuelen);

/**
 * ������������
 *
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 */
int32_t EMV_ProcessExit(void);

/**
 * �ں��˳�
 *
 * @return  {@link EMV_OK}                          �ɹ�
 */
int32_t EMV_Exit(void);


typedef struct
{
	int32_t (*emv_process)(void);
	int32_t (*emv_app_sel_init)(void);
	int32_t (*emv_initiate_application)(void);
	int32_t (*emv_read_application_data)(void);
	int32_t (*emv_offline_data_auth)(void);
	int32_t (*emv_processing_restrictions)(void);
	int32_t (*emv_cardholder_verify)(void);
	int32_t (*emv_terminal_risk_manage)(void);
	int32_t (*emv_action_analyse)(void);
	int32_t (*emv_compelte)(int32_t *msginx);

}emv_kernel_interface;

extern emv_kernel_interface emv_k_interface;


#if defined(__cplusplus)
}
#endif


#endif /* EMV_H_ */
/*
************************************************************************************************************************
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ END OF FILE ~~~
************************************************************************************************************************
*/
