/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emvdbg.h
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015Äê11ÔÂ17ÈÕ
* @brief    TODO
*
************************************************************************************************************************
*/
#ifndef EMVDBG_H_
#define EMVDBG_H_

#define __DEBUG_MODULE__

#define EMV_CFG_DEBUG                   ENABLE              /* ï¿½ï¿½ï¿½Ô¿ï¿½ï¿½ï¿½ */
#define EMV_CFG_DEBUG_ERRNO             DISABLE              /* ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ô¿ï¿½ï¿½ï¿½ */
#define EMV_CFG_MAX_CANDIDATES			12					//maxium of candidate list
#define EMV_APDU_DEBUG					DISABLE
#define EMV_RETURNVALUE_DEBUG			DISABLE
#define EMV_HASHTABLE_DEBUG				DISABLE
#define EMV_TIME_DEBUG					DISABLE

#if (defined KS_DEBUG) || (EMV_CFG_DEBUG == ENABLE)
#define eTRACE(fmt,...) \
		API_Trace(0x01,"[EMVLIB][%-32s - %-4d] "fmt"",__FUNCTION__,__LINE__,##__VA_ARGS__)
#define eTRACE_HEX(msg, hex, hexlen) \
		API_TraceHex(0x01, (msg), (hex), (hexlen)).
#define TRACE(fmt,...) \
							API_Trace(DBG_APP_INFO,"[EMVAPI][%-32s - %-4d] "fmt"",__FUNCTION__,__LINE__,##__VA_ARGS__)
#define TRACE_HEX(msg,hex,hexlen) \
			do { \
				API_Trace(DBG_APP_INFO,"[EMVAPI][%-32s - %-4d]",__FUNCTION__,__LINE__); \
				API_TraceHex(DBG_APP_INFO,(msg),(hex),(hexlen)); \
			} while(0)
#else
#define eTRACE(...)
#define eTRACE_HEX(msg, hex, hexlen)
#endif

#if EMV_CFG_DEBUG_ERRNO
#define rNXT(n) \
		(API_Trace(0x01,"[EMVLIB][%-32s - %-4d] NXT(%d)",__FUNCTION__,__LINE__,n),n)
#define rERR(n) \
		(API_Trace(0x01,"[EMVLIB][%-32s - %-4d] ERR(%d)",__FUNCTION__,__LINE__,n),n)
#define rEND(n) \
		(API_Trace(0x01,"[EMVLIB][%-32s - %-4d] END(%d)",__FUNCTION__,__LINE__,n),n)
#else
#define rNXT(n) (n)
#define rERR(n) (n)
#define rEND(n) (n)
#endif

#if(EMV_APDU_DEBUG == ENABLE)
#define apduTRACE(fmt,...) \
		API_Trace(0x01,"[EMVLIB][%-32s - %-4d] "fmt"",__FUNCTION__,__LINE__,##__VA_ARGS__)
#define apduTRACE_HEX(msg, hex, hexlen) \
		API_TraceHex(0x01, (msg), (hex), (hexlen))
#else
#define apduTRACE(fmt,...)
#define apduTRACE_HEX(msg, hex, hexlen)
#endif

#if(EMV_HASHTABLE_DEBUG == ENABLE)
#define htTRACE(fmt,...) \
			API_Trace(0x01,"[EMVLIB][%-32s - %-4d] "fmt"",__FUNCTION__,__LINE__,##__VA_ARGS__)
#define htTRACE_HEX(msg, hex, hexlen) \
			API_TraceHex(0x01, (msg), (hex), (hexlen))
#else
#define htTRACE(fmt,...)
#define htTRACE_HEX(msg, hex, hexlen)
#endif

#if(EMV_TIME_DEBUG == ENABLE)
	//#define tmTRACE(fmt,...) \
	//			API_Trace(0x01, "[PAYPASSLIB][%-32s - %-4d] "fmt"",__FUNCTION__,__LINE__,##__VA_ARGS__)
	//#define tmTRACE_HEX(msg, hex, hexlen) \
	//			API_TraceHex(0x01, (msg), (hex), (hexlen))

#define tmTRACE(fmt,...) \
				printf("[EMVLIB][%-32s - %-4d] "fmt"\r\n", __FUNCTION__,__LINE__,##__VA_ARGS__)
	//			printf("[EMVLIB][%-32s - %-4d] "fmt"\r\n", __FUNCTION__,__LINE__,##__VA_ARGS__)

#define tmTRACE_HEX(msg, hex, hexlen) \
				API_TraceHex(0x01, (msg), (hex), (hexlen))

#else
#define tmTRACE(fmt,...)
#define tmTRACE_HEX(msg, hex, hexlen)
#endif


#endif /* EMVDBG_H_ */
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
