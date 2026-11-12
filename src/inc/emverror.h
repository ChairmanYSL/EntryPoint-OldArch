/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emverror.h
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年11月24日
* @brief    EMV错误接口
*
************************************************************************************************************************
*/
#ifndef EMVAPI_EMVERROR_H_
#define EMVAPI_EMVERROR_H_


enum EMV_ErrCodes
{
    ErOK = 0,
    ErUNKNOWN = -1,
    ErFUNCPAR = -2,
    ErFOPEN = -3,
    ErFSEEK = -4,
    ErFREAD = -5,
    ErFWRITE = -6,
    ErFREMOVE = -7,
    ILL_TERMPARAM = -8,
    ILL_TERMPARAMLEN = -9,
    ILL_AIDPARAM = -10,
    ILL_AIDPARAMLEN = -11,
    ILL_AIDLEN = -12,
    ILL_CAPKPARAM = -13,
    ILL_CAPKPARAMLEN = -14,
    ErFCCTRNULL = -15,//!<闪卡控制参数为空    
    ErFCRECORDNULL = -16,//!<闪卡记录为空
    ErFCRECORDFULL = -17,//!<闪卡记录已满
    ErFCRECORDNO = -18,   //!<闪卡记录号错误
    ErNOTFINDAID = -19,
    ErNOTFINDCAPK = -20,
    ErWRCOMPLETE = -21,//文件已写完
    ErFILESIZE = -22,//文件大小错误
	ILL_DRL = -23,

};



#endif /* EMVAPI_EMVERROR_H_ */
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
