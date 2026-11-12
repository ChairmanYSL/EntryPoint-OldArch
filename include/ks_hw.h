/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     ks_hw.h
* @author   Android Development Team
* @version  v2.01
* @date     2015��06��15��
* @author 	zhongfuchang
* @brief    The standard hardware API of KOS-SDK
*
************************************************************************************************************************
*/


#ifndef _KS_HW_H_
#define _KS_HW_H_

//-----------------------------------------------------------------------------------------------------------------------
//SDKV2.0 �����ӿڣ�ö��Ӳ����Ϣ


//-----------------------------------------------------------------------------------------------------------------------
//��ſ�����
	#define MAG_DATA_PRESENTS	0x01	//!< �ôŵ�������
	#define MAG_DECODE_SUCCUSS	0x02	//!< �ôŵ����ݽ���ɹ�
	#define	MAG_TRK_LEN			120		//!< �ŵ�������󳤶�

	/**
	 * �ŵ����ݽṹ����
	 */
	typedef struct
	{
		unsigned char	Type;						//!< �������� MAGPAR_TRK or MAGPAR_IC
		unsigned char	Flag[3];					//!< �ŵ����KS_MAG_TRK_FLAG
		unsigned char	Len[3];						//!< �ŵ����ݳ���
		unsigned char	Trk[3][MAG_TRK_LEN];		//!< �ŵ�����
	}MAGTRK;

	/**
	 * ����Trk�ṹ�����ڻ�ȡ�ŵ���Ϣ
	 *
	 * @param pTrk {@link MAGTRK}�ṹָ��
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 */
	extern int  API_SetTrkBuf(MAGTRK* pTrk);



//-----------------------------------------------------------------------------------------------------------------------
//��������ӿ�
	/**
	 * ����Beep
	 *
	 * @param tFreq 	Ƶ��	,��λ:����
	 * @param tTimeMs 	����ʱ��,��λ:����
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 */
	extern int API_Beep(unsigned int tFreq,unsigned int tTimeMs);
//-----------------------------------------------------------------------------------------------------------------------
//��LED�ӿ�
	/**
	 * LEDλ����
	 */
	enum LED_MASK
    {
        LED_QPBOC_1 = 0x00000001,    //!< QPBOCָʾ��1
        LED_QPBOC_2 = 0x00000002,    //!< QPBOCָʾ��2
        LED_QPBOC_3 = 0x00000004,    //!< QPBOCָʾ��3
        LED_QPBOC_4 = 0x00000008,    //!< QPBOCָʾ��4
        LED_BLUE    = 0x00000001,    //!< ��
        LED_ORANGE  = 0x00000002,    //!< ��
        LED_GREEN   = 0x00000004,    //!< ��
        LED_RED     = 0x00000008,    //!< ��
        LED_INFO    = 0x00000010,    //!< ��Ϣָʾ��
        LED_ERR     = 0x00000020,    //!< ����ָʾ��
        LED_MAG     = 0x00000040,    //!< ˢ��ָʾ��
        LED_ALL     = 0xFFFFFFFF,    //!< ���е�
    };

    /**
     * ����LED�ƿ���
     *
     * @param tMask 	Ϊ{@link LED_MASK}����
     * @param isOn 		[1]�� [0]��
     * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
     */
	extern int API_SetLedSwitch(unsigned int tMask,int isOn);

//-----------------------------------------------------------------------------------------------------------------------
//������ӿ�
    /**
     * ��������ģ�鿪��, ����ʼ����ɨ��
     *
     * @param isOn 	[1]�� [0]��
     * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
     */
	extern int API_SetBarCodeBacklight(int isOn);



//-----------------------------------------------------------------------------------------------------------------------
//��Ƭ�����ӿ�

	/**
	 * ICC�ӿ���������
	 */
	enum ICC_INDEX
	{
	    ICC_UNKNOWN     = 0,  	//!< δ֪
	    ICC_CONTSCT     = 1,	//!< �Ӵ�ʽIC��	 		contact
	    ICC_NON_CONTSCT = 2,	//!< �ǽӴ�ʽIC��	 	non-contact
	    ICC_PSAM1       = 3,	//!< PSAM��1
	    ICC_PSAM2       = 4,	//!< PSAM��2
	    ICC_PSAM3       = 5,	//!< PSAM��2
	    ICC_PSAM4       = 6,	//!< PSAM��2
	    ICC_CONTSCT_2   = 7,	//!< �Ӵ�ʽIC��2	 	contact
		ICC_MAX			= 8 	//!< MAX����
	};

	/**
	 * ��ICC
	 *
	 * @param tCardIndex ICC����,�ο�{@link ICC_INDEX}
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 */
	extern int API_ICC_Open(int tCardIndex);

	/**
	 * �ر�ICC
	 *
	 * @param tCardIndex ICC����,�ο�{@link ICC_INDEX}
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 */
	extern int API_ICC_Close(int tCardIndex);

	/**
	 * ��Ƭ�ϵ�
	 *
	 * @param tCardIndex ICC����,�ο�{@link ICC_INDEX}
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 * @return {@link RET_HW_ICC_NOT_OPEN}			�豸δ��
	 * @return {@link RET_HW_ICC_CARD_NOT_EXIST}	��Ƭ������
	 * @return {@link RET_HW_ICC_CARD_MALFUNCTION}	�ϵ�ʧ��
	 */
	extern int  API_ICC_PwrOn(int tCardIndex);

	/**
	 * ���ؿ�ƬATR��Ϣ
	 *
	 * @param tCardIndex 		ICC����,�ο�{@link ICC_INDEX}
	 * @param pAtrBuf [out] 	����IC��ATR�����ݻ�����
	 * @param nAtrSize 			ATR�����ݻ������Ĵ�С
	 * @param pAtrLen [out[  	ATR�ĳ���
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 */
	extern int  API_ICC_GetAtr(int tCardIndex,unsigned char* pAtrBuf,unsigned int nAtrSize, unsigned int* pAtrLen);

	/**
	 * ��Ƭ�µ�
	 *
	 * @param tCardIndex ICC����,�ο�{@link ICC_INDEX}
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 * @return {@link RET_HW_ICC_NOT_OPEN}			ICCδ��
	 */
	extern int  API_ICC_PwrOff(int tCardIndex);

	/**
	 * ��ƬAPDUָ��
	 * ��д�������ݺ���Ϣ
	 *
	 * @param tCardIndex 	ICC����,�ο�{@link ICC_INDEX}
	 * @param pCmdBuf 		���͵�APDU�������ݻ�����
	 * @param nCmdLen 		���͵�APDU�������ݳ���
	 * @param pOutBuf [out]	�������ݻ�����	(Ӧ�ó�����ݿ�Ƭ�ṹ�Լ����ƻ�������С)
	 * @param nOutSize		�������ݻ������Ĵ�С
	 * @param pOutLen [out] �������ݳ���
	 * @param pSW [out]		APDUָ��ִ��״̬, ��0x9000,0x6A82,0x0000 0x0000����
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 * @return {@link RET_HW_ICC_NOT_OPEN}			ICCδ��
	 * @return {@link RET_HW_ICC_CARD_NOT_EXIST}	��Ƭ������
	 * @return {@link RET_HW_ICC_CARD_NOT_POWER}	��Ƭδ�ϵ�
	 */
	 extern int  API_ICC_Apdu(		int tCardIndex,unsigned char* pCmdBuf,int nCmdLen,unsigned char* pOutBuf,int* pOutLen);


	/**
	 * ��д���豸������, IC��˽������
	 *
	 * @param tCardIndex 		ICC����,�ο�{@link ICC_INDEX}
	 * @param pSendBuf 			���͵��������ݻ�����
	 * @param nSendLen 			���͵��������ݳ���
	 * @param pRecvBuf [out]	�������ݻ�����	(Ӧ�ó�����ݿ�Ƭ�ṹ�Լ����ƻ�������С)
	 * @param nRecvSize			�������ݻ�������С
	 * @param pRecvLen [out]	�������ݳ���
	 * @param nTimeOutMs 		��ʱʱ��(����)
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_TIME_OUT}					��ʱ
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 * @return {@link RET_HW_ICC_NOT_OPEN}			ICCδ��
	 */
	extern int  API_ICC_Escape(int tCardIndex,const unsigned char* pSendBuf,unsigned int nSendLen,unsigned char* pRecvBuf,unsigned int nRecvSize, unsigned int* pRecvLen,unsigned int nTimeOutMs);

	/**
	 * ���ýӴ�ʽIC���¼��Ƿ�������루Ĭ���Զ��ϵ磩��
	 *
	 * @param isOnly 	Ϊ0��ʾIC��������Զ��ϵ磬Ӧ��WaitEvent�ȵ��¼���ֱ��API_ICC_GetAtr
	 *					Ϊ1��ʾ�����IC�������¼���Ӧ��WaitEvent�ȵ��¼���ִ��API_ICC_PwrOn+API_ICC_GetAtr
	 * @return {@link RET_OK}  						�ɹ�
	 * @return {@link RET_PERMISSION_DENIED} 		Ȩ�޲���
	 * @return {@link RET_API_PARAMETER_ERROR} 		������������
	 * @return {@link RET_DEVICE_MALFUNCTION}		Ӳ������
	 * @return {@link RET_DEVICE_PARAMETER_ERROR}	Ӳ����������
	 * @return {@link RET_FUNCTION_NOT_SUPPORT} 	���ܲ�֧��
	 * @return {@link RET_SYSTEM_ERROR}				ϵͳ����
	 */
	extern int  API_ICC_OnlyChkPreSent(int isOnly);

	enum CARD_STATUS
	{
		CARD_NOT_EXIST			=0x01,		//!< ��������
		CARD_EXIST				=0x02,		//!< �����ڵ�δ�ϵ�
		CARD_EXIST_POWERED		=0x04		//!< ���������Ѿ��ϵ�
	};

	/**
	 * ��⿨�Ƿ���ڡ�
	 *
	 * @param tCardIndex 		ICC����,�ο�{@link ICC_INDEX}
	 * @return ��Ƭ״̬, �ο�{@link CARD_STATUS}
	 */
	extern int  API_ICC_ChkPreSent(int tCardIndex);


	extern void API_Trace(unsigned int level, char* fmt, ...);


	extern void API_TraceHex(unsigned int level, char* msg, const void* hex, unsigned int hexlen);

#endif

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/