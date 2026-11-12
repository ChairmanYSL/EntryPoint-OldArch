/**
 * Legacy SDK helpers used by the porting layer but missing from the local SDK headers.
 * Declared here (ASCII-only header) so the Entry Point core can be compiled standalone.
 */
#ifndef EMV_PORT_SDK_H_
#define EMV_PORT_SDK_H_

#include "ks_type.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void API_Conv_StrToBcd_Left(char* str, int emv_strlen, u8* bcd);
extern void API_Conv_StrToBcd_Right(char* str, int slen, u8* bcd, int blen);

#ifdef __cplusplus
}
#endif

#endif /* EMV_PORT_SDK_H_ */
