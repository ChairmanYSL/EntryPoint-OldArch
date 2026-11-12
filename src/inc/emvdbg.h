/**
************************************************************************************************************************
*                                     COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emvdbg.h
* @brief    Debug / trace macros for the Entry Point core
* @author   Kingsee Development Team
* @version  V43
*
* NOTE: ASCII-only file on purpose (source tree mixes GBK and damaged UTF-8).
*       EMV_CFG_DEBUG can be overridden by the build system (-DEMV_CFG_DEBUG=ENABLE).
************************************************************************************************************************
*/
#ifndef EMVDBG_H_
#define EMVDBG_H_

#ifndef DISABLE
#define DISABLE                         0
#endif
#ifndef ENABLE
#define ENABLE                          1
#endif

#ifndef EMV_CFG_DEBUG
#define EMV_CFG_DEBUG                   DISABLE             /* production default: trace off */
#endif
#define EMV_CFG_DEBUG_ERRNO             DISABLE
#define EMV_APDU_DEBUG                  DISABLE
#define EMV_RETURNVALUE_DEBUG           DISABLE
#define EMV_HASHTABLE_DEBUG             DISABLE
#define EMV_TIME_DEBUG                  DISABLE

#if (EMV_CFG_DEBUG == ENABLE)
#define eTRACE(fmt, ...) \
        API_Trace(0x01, "[EMVLIB][%-32s - %-4d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define eTRACE_HEX(msg, hex, hexlen) \
        API_TraceHex(0x01, (msg), (hex), (hexlen))
#define TRACE(fmt, ...) \
        API_Trace(DBG_APP_INFO, "[EMVAPI][%-32s - %-4d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define TRACE_HEX(msg, hex, hexlen) \
        do { \
            API_Trace(DBG_APP_INFO, "[EMVAPI][%-32s - %-4d]", __FUNCTION__, __LINE__); \
            API_TraceHex(DBG_APP_INFO, (msg), (hex), (hexlen)); \
        } while (0)
#else
#define eTRACE(...)
#define eTRACE_HEX(msg, hex, hexlen)
#define TRACE(...)
#define TRACE_HEX(msg, hex, hexlen)
#endif

#if EMV_CFG_DEBUG_ERRNO
#define rNXT(n) \
        (API_Trace(0x01, "[EMVLIB][%-32s - %-4d] NXT(%d)", __FUNCTION__, __LINE__, n), n)
#define rERR(n) \
        (API_Trace(0x01, "[EMVLIB][%-32s - %-4d] ERR(%d)", __FUNCTION__, __LINE__, n), n)
#define rEND(n) \
        (API_Trace(0x01, "[EMVLIB][%-32s - %-4d] END(%d)", __FUNCTION__, __LINE__, n), n)
#else
#define rNXT(n) (n)
#define rERR(n) (n)
#define rEND(n) (n)
#endif

#if (EMV_APDU_DEBUG == ENABLE)
#define apduTRACE(fmt, ...) \
        API_Trace(0x01, "[EMVLIB][%-32s - %-4d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define apduTRACE_HEX(msg, hex, hexlen) \
        API_TraceHex(0x01, (msg), (hex), (hexlen))
#else
#define apduTRACE(fmt, ...)
#define apduTRACE_HEX(msg, hex, hexlen)
#endif

#if (EMV_HASHTABLE_DEBUG == ENABLE)
#define htTRACE(fmt, ...) \
        API_Trace(0x01, "[EMVLIB][%-32s - %-4d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define htTRACE_HEX(msg, hex, hexlen) \
        API_TraceHex(0x01, (msg), (hex), (hexlen))
#else
#define htTRACE(fmt, ...)
#define htTRACE_HEX(msg, hex, hexlen)
#endif

#if (EMV_TIME_DEBUG == ENABLE)
#define tmTRACE(fmt, ...) \
        API_Trace(0x01, "[EMVLIB][%-32s - %-4d] " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define tmTRACE_HEX(msg, hex, hexlen) \
        API_TraceHex(0x01, (msg), (hex), (hexlen))
#else
#define tmTRACE(fmt, ...)
#define tmTRACE_HEX(msg, hex, hexlen)
#endif

#endif /* EMVDBG_H_ */
