/**
************************************************************************************************************************
*                         			  COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_utils.h
* @brief    EMV�ں˽ӿ� - ʵ�ù���
* @author   Kingsee Development Team - xinyong
* @version  V43
*
************************************************************************************************************************
*/
#ifndef EMV_UTILS_H_
#define EMV_UTILS_H_


/*
************************************************************************************************************************
* INCLUDE FILES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
* EXTERNS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
* DEFINES
************************************************************************************************************************
*/

/**
 * TLV�������
 */
enum EMV_TLVDecodeFlags
{
    SINGLE_TLVOBJ = 0x01,                           //!< ����������TLV����
    STRING_TLVOBJ = 0x02,                           //!< ���������TLV����
    DECODE_LEVEL1 = 0x10,                           //!< ������һ��
};

/**
 * TLV���Ҽ���
 */
enum EMV_TLVFindLevels
{
    SEARCH_ALL_DESC = 0x01,                         //!< ����ָ�������¹ҵ�����TLV
    SEARCH_ONLY_SON = 0x02,                         //!< ����ָ�������¹ҵ���TLV
};



/*
************************************************************************************************************************
* ERROR CODES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
* DATA TYPES
************************************************************************************************************************
*/

/**
 * TLV�������ݸ�ʽ
 */
typedef struct {
    uint32_t tagname;                                    //!< ��ǩ��
    int32_t parent;                                     //!< ��Ԫ�ؼ��𣨴�0��ʼ��
    int32_t childnum;                                   //!< ��Ԫ�ظ���
    int32_t valuelen;                                   //!< Ԫ�����ݳ���
    uint8_t* pvalue;                                     //!< Ԫ������
} EMV_TLVOBJ;


/*
************************************************************************************************************************
* GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
* MACRO'S
************************************************************************************************************************
*/


/*
************************************************************************************************************************
* FUNCTION PROTOTYPES
************************************************************************************************************************
*/

/**
 * TLV���ݽ���
 *
 * @param   pData                                   [IN]TLV���ݻ���
 * @param   nDataLen                                [IN]TLV���ݻ���ĳ���
 * @param   pTLVs                                   [OU]TLV���������������
 * @param   nTLVs                                   [IN]TLV���������������ĸ��������֧�ֵ�TLV������
 * @param   bFlags                                  [IN]������ǣ��ο�{@link EMV_TLVDecodeFlags}
 * @return  {@link EMV_OK}                          �����ɹ�
 * @return  {@link EMV_FAIL}                        ����ʧ��
 */
int32_t EMV_TLV_Decode(uint8_t *pData, int32_t nDataLen, EMV_TLVOBJ *pTLVs, int32_t nTLVs, int32_t bFlags);
int32_t EMV_TLV_Decode_Paypass(uint8_t *pData, int32_t nDataLen, EMV_TLVOBJ *pTLVs, int32_t nTLVs, int32_t bFlags);


/**
 * ����ָ��TAG��ӦTLV�������ݣ�ͨ��{@link EMV_TLV_Decode}�������е�����
 *
 * @param   nParent                                 [IN]����λ����������ӦpTLVs������±�
 * @param   nTag                                    [IN]���ұ�ǩ
 * @param   pTLVs                                   [IN]TLV�������ݻ���
 * @param   nLevel                                  [IN]���Ҽ��𣬲ο�{@link EMV_TLVFindLevels}
 * @return  =0                                      û�ҵ�
 * @return  >0                                      nTag��pTLVs�����ж�Ӧ���±�
 */
int32_t EMV_TLV_Find(int32_t nParent, uint32_t nTag, EMV_TLVOBJ *pTLVs, int32_t nLevel);

/**
 * ʹ��DOL�Ͷ�Ӧ���ݴ����TLV����
 *
 * @param   pDOL                                    [IN]���ݶ����б�����TAG��LEN��
 * @param   nDOLLen                                 [IN]���ݶ����б��ĳ���
 * @param   pVal                                    [IN]DOL��Ӧ���ݣ�VAL��
 * @param   nValLen                                 [IN]DOL��Ӧ���ݵĳ���
 * @param   pOut                                    [OU]TLV�����������
 * @param   pOutLen                                 [OU]TLV����������Ȼ���
 * @param   nOutMaxLen                              [IN]TLV��������������󳤶�
 * @return  {@link EMV_OK}                          ����ɹ�
 * @return  {@link EMV_FAIL}                        ���ʧ��
 */
int32_t EMV_TLV_PacketByDOL(uint8_t *pDOL, int32_t nDOLLen, uint8_t *pVal, int32_t nValLen, uint8_t *pOut, int32_t *pOutLen, int32_t nOutMaxLen);

/**
 * ʹ��TAG����ȥ��ȡӦ�����ݴ����TLV����
 *
 * @param   pTags                                   [IN]TAG�б�
 * @param   nTagNum                                 [IN]TAG����
 * @param   pOut                                    [OU]TLV���ݻ���
 * @param   pOutLen                                 [OU]TLV���ݳ��Ȼ���
 * @param   nOutMaxLen                              [IN]TLV���ݻ�����󳤶�
 * @return  {@link EMV_OK}                          ����ɹ�
 * @return  {@link EMV_FAIL}                        ���ʧ��
 */
int32_t EMV_TLV_PacketByTAG(uint32_t *pTags, int32_t nTagNum, uint8_t *pOut, int32_t *pOutLen, int32_t nOutMaxLen);


/**
 * �ɣÿ�����ӿ�
 *
 * @param   pInput                                  [IN]�������ݣ�C-APDU��
 * @param   nInlen                                  [IN]�������ݳ���
 * @param   pOutput                                 [OU]������ݻ���
 * @param   pOutlen                                 [OU]������ݳ��Ȼ��棨����SW1��SW2��
 * @return  ״̬��(SW)����0x9000,0x6A82,0x0000
 */
int32_t EMV_ICC_Apdu(uint8_t *pInput, int32_t nInlen, uint8_t *pOutput, int32_t *pOutlen);

/**
 * �ɣÿ�ȡ�������GET DATA��
 * @param   nTag                                    [IN]��ǩ���ο�PBOC��5���ֱ�B.10
 * @param   pOut                                    [OU]��ǩ����
 * @param   pOutLen                                 [OU]��ǩ���ݳ���
 * @return  [>=0]                                   ״̬��(SW)����0x9000,0x6A88,0x0000
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_APPSELUNDONE}                �ں�δ���Ӧ��ѡ��
 */
int32_t EMV_ICC_GetData(uint32_t nTag, uint8_t *pOut, int32_t *pOutLen);

/**
 * �ɣÿ�����¼���READ RECORD��
 * @param   nSFI                                    [IN]SFI�����ļ���ʶ��
 * @param   nRecNo                                  [IN]��¼��
 * @param   pOut                                    [OU]��¼����
 * @param   pOutLen                                 [OU]��¼���ݳ���
 * @return  [>=0]                                   ״̬��(SW)����0x9000,0x6A83,0x0000
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_APPSELUNDONE}                �ں�δ���Ӧ��ѡ��
 */
int32_t EMV_ICC_ReadRecord(uint8_t nSFI, uint8_t nRecNo, uint8_t *pOut, int32_t *pOutLen);


/**
 * ��ȡ��Ƭ���׼�¼��PBOC��
 *
 * @param   nRecNo                                  [IN]��¼��
 *   @arg   [0x00~0xff]                             ������־��¼��
 *   @arg   {@link PBOC_TRANSREC_SFI}               ������־���
 *   @arg   {@link PBOC_TRANSREC_NUM}               ������־����
 *   @arg   {@link PBOC_TRANSREC_FMT}               ������־��ʽ
 * @param   pOut                                    [OU]��¼����
 * @param   pOutLen                                 [OU]��¼���ݳ���
 * @return  {@link EMV_OK}                          �ɹ�
 * @return  {@link EMV_UNINITIALIZED}               �ں�δ��ʼ��
 * @return  {@link EMV_APPSELUNDONE}                �ں�δ���Ӧ��ѡ��
 * @return  {@link EMV_PBOCLOGSFINOTFOUND}          ������־���û�ҵ�
 * @return  {@link EMV_PBOCLOGFMTNOTFOUND}          ������־��ʽû�ҵ�
 * @return  {@link EMV_PBOCLOGRDRECFAILED}          ������־��¼��ȡʧ��
 * @return  {@link EMV_PBOCLOGRECNOINV}             ������־��¼����Ч
 */
int32_t EMV_GetTransLog(int32_t nRecNo, uint8_t *pOut, int32_t *pOutLen);

int32_t EMV_GetLoadLog(int32_t nRecNo, uint8_t *pOut, int32_t *pOutLen);

/**
 * ��ȡ�ǽӿ���
 *
 * @param   buff                                   [IN]�������ݻ���
 * @param   buffsize                                [IN]�������ݻ���Ĵ�С
 * @param   bufflen                                   [OU]�������������С
 * @return  {@link EMV_OK}                          ��ȡ���ųɹ�
 * @return  {@link EMV_FAIL}                        ��ȡ����ʧ��
 */
int32_t EMV_GetRfidCardNo(uint8_t *buff, uint8_t buffsize, uint8_t *bufflen);

#endif /* EMV_UTILS_H_ */
/*
************************************************************************************************************************
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ END OF FILE ~~~
************************************************************************************************************************
*/
