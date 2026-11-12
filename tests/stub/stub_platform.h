/**
************************************************************************************************************************
* @file     stub_platform.h
* @brief    Host stub platform for the Entrypoint / Expresspay end-to-end test.
*
* The real product links against the KOS platform SDK (API_*).  This stub gives
* the host build a deterministic, fully controllable implementation of those
* symbols:
*   - an image file system in RAM (emvporting parameter files really work);
*   - a scripted ICC card (PPSE / final SELECT / GPO / GAC responses);
*   - deterministic RNG / time / trace / crypto placeholders.
*
* It is test-only code: never linked into the product libraries.
************************************************************************************************************************
*/
#ifndef STUB_PLATFORM_H_
#define STUB_PLATFORM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Image file system (API_fopen/API_fread/API_fwrite/API_fseek/API_fsize/
 * API_fremove/API_fclose are declared in include/ks_stdlib.h and implemented in
 * stub_platform.c)
 * ------------------------------------------------------------------------- */
void        stub_fs_reset(void);
int         stub_fs_file_count(void);
int         stub_fs_exists(const char* name);
int         stub_fs_size_of(const char* name);
/* Read a whole emvporting parameter file (4-byte little-endian length header +
 * TLV payload) into buf.  Returns payload length, or -1 when absent/invalid. */
int         stub_fs_read_param_payload(const char* name, uint8_t* buf, int maxlen);

/* ---------------------------------------------------------------------------
 * Scripted ICC card
 *
 * The test registers the responses the card should give; stub_icc_apdu() is
 * the single dispatcher used by both EMV_PORTING.icc_rw (through the test) and
 * API_ICC_Apdu (platform SDK path).
 * ------------------------------------------------------------------------- */
void        stub_icc_reset(void);
void        stub_icc_set_ppse(const uint8_t* data, int len);
void        stub_icc_set_adf(const uint8_t* data, int len);
void        stub_icc_set_gpo(const uint8_t* data, int len);
void        stub_icc_set_gac(const uint8_t* data, int len);
void        stub_icc_set_select_sw(int sw);          /* SW for unmatched SELECT   */
void        stub_icc_set_force_sw(int sw);           /* answer every command with SW (for error paths) */

int         stub_icc_sw(void);                       /* last SW returned          */
int         stub_icc_cmd_count(void);                /* total commands received   */
int         stub_icc_select_count(void);             /* SELECT commands received  */
int         stub_icc_last_type(void);                /* last ICC "type" argument  */
void        stub_icc_note_type(int type);            /* record ICC card-type arg  */
const uint8_t* stub_icc_last_cmd(int* len);          /* last C-APDU (for asserts) */

/* Dispatch one C-APDU.  Returns SW1SW2.  Response data (without SW) is written
 * to out/outlen.  outmax is the capacity of out (never exceeded). */
int         stub_icc_apdu(const uint8_t* cmd, int cmdlen, uint8_t* out, int outmax, int* outlen);

/* ---------------------------------------------------------------------------
 * ICS options (ics_option is provided by the application / terminal config)
 * ------------------------------------------------------------------------- */
void        stub_ics_reset(void);
void        stub_ics_set(uint32_t option_id, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* STUB_PLATFORM_H_ */
