/**
************************************************************************************************************************
* @file     stub_platform.c
* @brief    Host stub platform (KOS SDK API_* replacement) for tests/host_e2e.c
*
* Behaviour:
*   - file IO  : in-RAM image file table, API_fopen/... semantics per
*                include/ks_stdlib.h (fd > 0, F_SEEK_* are bit values);
*   - ICC      : one scripted card; SELECT("2PAY.SYS.DDF01") -> PPSE,
*                SELECT(AMEX AID) -> ADF FCI, 80A8 -> GPO, 80AE -> GAC;
*   - RNG/time : deterministic, so assertions are reproducible;
*   - crypto   : reports CRYPT_NOT_SUPPORTED (ODA / PIN crypto is not part of
*                this end-to-end test; see doc/TESTING.md "未覆盖项").
*
* This file is test infrastructure only and is never linked into the product.
************************************************************************************************************************
*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "sdk.h"
#include "ks_crypt.h"
#include "emv_port_sdk.h"

#include "stub_platform.h"

/* ---------------------------------------------------------------------------
 * ICS option / discretionary-data hooks (provided by the application in the
 * real product; declared in src/inc/emvi.h)
 * ------------------------------------------------------------------------- */
extern uint32_t ics_option(uint32_t option_id);
extern int32_t  CreateEMVDiscretionaryData(void);

/* ===========================================================================
 * 1) In-RAM image file system
 * ========================================================================= */
#define STUB_MAX_FILES      12
#define STUB_MAX_FILE_SIZE  4096
#define STUB_MAX_FDS        8

typedef struct
{
    char     name[32];
    int32_t  used;
    int32_t  size;
    uint8_t  data[STUB_MAX_FILE_SIZE];
} STUB_FILE;

typedef struct
{
    int32_t used;
    int32_t file;        /* index into g_files */
    int32_t pos;
    int32_t flags;
} STUB_FD;

static STUB_FILE g_files[STUB_MAX_FILES];
static STUB_FD   g_fds[STUB_MAX_FDS];

static int stub_file_find(const char* name)
{
    int i;

    if (name == NULL)
    {
        return -1;
    }
    for (i = 0; i < STUB_MAX_FILES; i++)
    {
        if (g_files[i].used && strcmp(g_files[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int stub_file_create(const char* name)
{
    int i;

    for (i = 0; i < STUB_MAX_FILES; i++)
    {
        if (!g_files[i].used)
        {
            memset(&g_files[i], 0, sizeof(g_files[i]));
            strncpy(g_files[i].name, name, sizeof(g_files[i].name) - 1);
            g_files[i].used = 1;
            g_files[i].size = 0;
            return i;
        }
    }
    return -1;
}

void stub_fs_reset(void)
{
    memset(g_files, 0, sizeof(g_files));
    memset(g_fds, 0, sizeof(g_fds));
}

int stub_fs_file_count(void)
{
    int i, n = 0;

    for (i = 0; i < STUB_MAX_FILES; i++)
    {
        if (g_files[i].used)
        {
            n++;
        }
    }
    return n;
}

int stub_fs_exists(const char* name)
{
    return stub_file_find(name) >= 0;
}

int stub_fs_size_of(const char* name)
{
    int f = stub_file_find(name);

    return (f < 0) ? -1 : g_files[f].size;
}

int stub_fs_read_param_payload(const char* name, uint8_t* buf, int maxlen)
{
    int f = stub_file_find(name);
    int32_t len;

    if (f < 0 || buf == NULL || g_files[f].size < 4)
    {
        return -1;
    }
    memcpy(&len, g_files[f].data, 4);
    if (len <= 0 || len > g_files[f].size - 4 || len > maxlen)
    {
        return -1;
    }
    memcpy(buf, g_files[f].data + 4, (size_t)len);
    return (int)len;
}

int API_fopen(const char* filename, int flags)
{
    int f, i;
    int create = ((flags & F_CREAT) != 0);

    f = stub_file_find(filename);
    if (f < 0)
    {
        if (!create)
        {
            return 0;                       /* <=0 means failure */
        }
        f = stub_file_create(filename);
        if (f < 0)
        {
            return 0;
        }
    }
    else if (create)
    {
        /* Opening an existing file with F_CREAT: keep content (POSIX-like). */
    }

    for (i = 0; i < STUB_MAX_FDS; i++)
    {
        if (!g_fds[i].used)
        {
            g_fds[i].used = 1;
            g_fds[i].file = f;
            g_fds[i].pos = ((flags & F_APPEND) != 0) ? g_files[f].size : 0;
            g_fds[i].flags = flags;
            return i + 1;                   /* fd must be > 0 */
        }
    }
    return 0;
}

int API_fclose(int fd)
{
    int ret;

    if (fd <= 0 || fd > STUB_MAX_FDS || !g_fds[fd - 1].used)
    {
        return -1;
    }
    ret = g_fds[fd - 1].pos;
    g_fds[fd - 1].used = 0;
    return ret;
}

int API_fseek(int fd, int offset, int whence)
{
    STUB_FD* h;
    int base;

    if (fd <= 0 || fd > STUB_MAX_FDS || !g_fds[fd - 1].used)
    {
        return -1;
    }
    h = &g_fds[fd - 1];

    switch (whence)
    {
        case F_SEEK_SET: base = 0; break;
        case F_SEEK_CUR: base = h->pos; break;
        case F_SEEK_END: base = g_files[h->file].size; break;
        default: return -1;
    }

    base += offset;
    if (base < 0 || base > STUB_MAX_FILE_SIZE)
    {
        return -1;
    }
    h->pos = base;
    return 0;
}

int API_fread(int fd, void* buf, u32 count)
{
    STUB_FD* h;
    int32_t avail, n;

    if (fd <= 0 || fd > STUB_MAX_FDS || !g_fds[fd - 1].used || buf == NULL)
    {
        return -1;
    }
    h = &g_fds[fd - 1];
    avail = g_files[h->file].size - h->pos;
    if (avail < 0)
    {
        avail = 0;
    }
    n = ((int32_t)count < avail) ? (int32_t)count : avail;
    if (n > 0)
    {
        memcpy(buf, g_files[h->file].data + h->pos, (size_t)n);
    }
    h->pos += n;
    return (int)n;
}

int API_fwrite(int fd, const void* buf, u32 count)
{
    STUB_FD* h;
    int32_t n = (int32_t)count;

    if (fd <= 0 || fd > STUB_MAX_FDS || !g_fds[fd - 1].used || buf == NULL)
    {
        return -1;
    }
    h = &g_fds[fd - 1];
    if (n < 0 || h->pos + n > STUB_MAX_FILE_SIZE)
    {
        return -1;
    }
    if (n > 0)
    {
        memcpy(g_files[h->file].data + h->pos, buf, (size_t)n);
        h->pos += n;
        if (h->pos > g_files[h->file].size)
        {
            g_files[h->file].size = h->pos;
        }
    }
    return (int)n;
}

int API_fremove(const char* filename)
{
    int f = stub_file_find(filename);
    int i;

    if (f < 0)
    {
        return -1;
    }
    for (i = 0; i < STUB_MAX_FDS; i++)
    {
        if (g_fds[i].used && g_fds[i].file == f)
        {
            g_fds[i].used = 0;
        }
    }
    memset(&g_files[f], 0, sizeof(g_files[f]));
    return 0;
}

int API_fsize(const char* filename)
{
    int f = stub_file_find(filename);

    return (f < 0) ? -1 : g_files[f].size;
}

/* ===========================================================================
 * 2) Scripted ICC card
 * ========================================================================= */
#define STUB_ICC_BUF  512

typedef struct
{
    uint8_t ppse[STUB_ICC_BUF];
    int32_t ppse_len;
    uint8_t adf[STUB_ICC_BUF];
    int32_t adf_len;
    uint8_t gpo[STUB_ICC_BUF];
    int32_t gpo_len;
    uint8_t gac[STUB_ICC_BUF];
    int32_t gac_len;

    int32_t select_sw;
    int32_t force_sw;               /* 0 = disabled */

    int32_t sw;
    int32_t cmd_count;
    int32_t select_count;
    int32_t last_type;
    uint8_t last_cmd[STUB_ICC_BUF];
    int32_t last_cmd_len;
} STUB_CARD;

static STUB_CARD g_card;

void stub_icc_reset(void)
{
    memset(&g_card, 0, sizeof(g_card));
    g_card.select_sw = 0x6A82;
}

void stub_icc_set_ppse(const uint8_t* data, int len)
{
    g_card.ppse_len = (len > STUB_ICC_BUF) ? STUB_ICC_BUF : ((len < 0) ? 0 : len);
    memset(g_card.ppse, 0, sizeof(g_card.ppse));
    if (g_card.ppse_len > 0)
    {
        memcpy(g_card.ppse, data, (size_t)g_card.ppse_len);
    }
}

void stub_icc_set_adf(const uint8_t* data, int len)
{
    g_card.adf_len = (len > STUB_ICC_BUF) ? STUB_ICC_BUF : ((len < 0) ? 0 : len);
    memset(g_card.adf, 0, sizeof(g_card.adf));
    if (g_card.adf_len > 0)
    {
        memcpy(g_card.adf, data, (size_t)g_card.adf_len);
    }
}

void stub_icc_set_gpo(const uint8_t* data, int len)
{
    g_card.gpo_len = (len > STUB_ICC_BUF) ? STUB_ICC_BUF : ((len < 0) ? 0 : len);
    memset(g_card.gpo, 0, sizeof(g_card.gpo));
    if (g_card.gpo_len > 0)
    {
        memcpy(g_card.gpo, data, (size_t)g_card.gpo_len);
    }
}

void stub_icc_set_gac(const uint8_t* data, int len)
{
    g_card.gac_len = (len > STUB_ICC_BUF) ? STUB_ICC_BUF : ((len < 0) ? 0 : len);
    memset(g_card.gac, 0, sizeof(g_card.gac));
    if (g_card.gac_len > 0)
    {
        memcpy(g_card.gac, data, (size_t)g_card.gac_len);
    }
}

void stub_icc_set_select_sw(int sw)
{
    g_card.select_sw = sw;
}

void stub_icc_set_force_sw(int sw)
{
    g_card.force_sw = sw;
}

int stub_icc_sw(void)             { return g_card.sw; }
int stub_icc_cmd_count(void)      { return g_card.cmd_count; }
int stub_icc_select_count(void)   { return g_card.select_count; }
int stub_icc_last_type(void)      { return g_card.last_type; }

void stub_icc_note_type(int type)
{
    g_card.last_type = (int32_t)type;
}

const uint8_t* stub_icc_last_cmd(int* len)
{
    if (len != NULL)
    {
        *len = g_card.last_cmd_len;
    }
    return g_card.last_cmd;
}

static int stub_icc_copy_response(const uint8_t* src, int srclen, uint8_t* out, int outmax, int* outlen)
{
    int n = srclen;

    if (n < 0)
    {
        n = 0;
    }
    if (n > outmax)
    {
        n = outmax;
    }
    if (n > 0 && out != NULL)
    {
        memcpy(out, src, (size_t)n);
    }
    if (outlen != NULL)
    {
        *outlen = n;
    }
    return n;
}

static int stub_icc_is_amex_aid(const uint8_t* data, int lc)
{
    static const uint8_t amex_aid[6] = { 0xA0, 0x00, 0x00, 0x00, 0x25, 0x01 };

    return (lc == 6 && memcmp(data, amex_aid, 6) == 0);
}

int stub_icc_apdu(const uint8_t* cmd, int cmdlen, uint8_t* out, int outmax, int* outlen)
{
    int sw = 0x6D00;
    int lc;

    if (outlen != NULL)
    {
        *outlen = 0;
    }
    if (cmd == NULL || cmdlen < 4)
    {
        g_card.sw = 0x6700;
        return g_card.sw;
    }

    g_card.cmd_count++;
    g_card.last_cmd_len = (cmdlen > STUB_ICC_BUF) ? STUB_ICC_BUF : cmdlen;
    memset(g_card.last_cmd, 0, sizeof(g_card.last_cmd));
    memcpy(g_card.last_cmd, cmd, (size_t)g_card.last_cmd_len);

    if (g_card.force_sw != 0)
    {
        g_card.sw = g_card.force_sw;
        return g_card.sw;
    }

    switch (cmd[1])
    {
        case 0xA4:      /* SELECT (00 A4 04 00 Lc <name> 00) */
            g_card.select_count++;
            if (cmdlen >= 6 && cmd[4] > 0 && cmdlen >= 5 + cmd[4])
            {
                lc = cmd[4];
                if (lc == 14 && memcmp(cmd + 5, "2PAY.SYS.DDF01", 14) == 0)
                {
                    if (g_card.ppse_len > 0)
                    {
                        stub_icc_copy_response(g_card.ppse, g_card.ppse_len, out, outmax, outlen);
                        sw = 0x9000;
                    }
                    else
                    {
                        sw = g_card.select_sw;
                    }
                }
                else if (stub_icc_is_amex_aid(cmd + 5, lc))
                {
                    if (g_card.adf_len > 0)
                    {
                        stub_icc_copy_response(g_card.adf, g_card.adf_len, out, outmax, outlen);
                        sw = 0x9000;
                    }
                    else
                    {
                        sw = g_card.select_sw;
                    }
                }
                else
                {
                    sw = g_card.select_sw;
                }
            }
            else
            {
                sw = 0x6700;
            }
            break;

        case 0xA8:      /* GET PROCESSING OPTIONS (80 A8 00 00 Lc <data> 00) */
            if (g_card.gpo_len > 0)
            {
                stub_icc_copy_response(g_card.gpo, g_card.gpo_len, out, outmax, outlen);
                sw = 0x9000;
            }
            else
            {
                sw = 0x6A86;
            }
            break;

        case 0xAE:      /* GENERATE AC (80 AE P1 00 Lc <CDOL> 00) */
            if (g_card.gac_len > 0)
            {
                stub_icc_copy_response(g_card.gac, g_card.gac_len, out, outmax, outlen);
                sw = 0x9000;
            }
            else
            {
                sw = 0x6985;
            }
            break;

        case 0xB2:      /* READ RECORD */
            sw = 0x6A83;
            break;

        case 0xC0:      /* GET RESPONSE */
            sw = 0x6D00;
            break;

        case 0xCA:      /* GET DATA */
            sw = 0x6A88;
            break;

        default:
            sw = 0x6D00;
            break;
    }

    g_card.sw = sw;
    return (int)sw;
}

int API_ICC_Apdu(int tCardIndex, unsigned char* pCmdBuf, int nCmdLen,
                 unsigned char* pOutBuf, int* pOutLen)
{
    (void)tCardIndex;
    return stub_icc_apdu((const uint8_t*)pCmdBuf, nCmdLen,
                         (uint8_t*)pOutBuf, STUB_ICC_BUF, pOutLen);
}

/* ===========================================================================
 * 3) ICS options
 * ========================================================================= */
#define STUB_ICS_MAX 16

typedef struct
{
    uint32_t id;
    uint32_t value;
} STUB_ICS;

static STUB_ICS g_ics[STUB_ICS_MAX];
static int g_ics_num;

void stub_ics_reset(void)
{
    memset(g_ics, 0, sizeof(g_ics));
    g_ics_num = 0;
    /* PPSE SPI processing enabled: lets a card-supplied 9F3F SDOL reach
     * sdol_process() (the malicious-SDOL test needs it).  A normal PPSE without
     * 9F3F/9F3E skips the SPI branch, so candidate-list tests are unaffected. */
    stub_ics_set(0x0608, 1);        /* MISC_Paypass_Support_SPI      */
    stub_ics_set(0x0604, 1);        /* MISC_Paypass_SPI_ACTIVE_FLAG  */
}

void stub_ics_set(uint32_t option_id, uint32_t value)
{
    int i;

    for (i = 0; i < g_ics_num; i++)
    {
        if (g_ics[i].id == option_id)
        {
            g_ics[i].value = value;
            return;
        }
    }
    if (g_ics_num < STUB_ICS_MAX)
    {
        g_ics[g_ics_num].id = option_id;
        g_ics[g_ics_num].value = value;
        g_ics_num++;
    }
}

uint32_t ics_option(uint32_t option_id)
{
    int i;

    for (i = 0; i < g_ics_num; i++)
    {
        if (g_ics[i].id == option_id)
        {
            return g_ics[i].value;
        }
    }
    return 0;
}

int32_t CreateEMVDiscretionaryData(void)
{
    return 0;
}

/* ===========================================================================
 * 4) Trace / misc platform helpers
 * ========================================================================= */
void API_Trace(unsigned int level, char* fmt, ...)
{
    (void)level;
    (void)fmt;
    /* Debug output is intentionally discarded: the scenario is asserted, not
     * the log text.  Enable EMV_CFG_DEBUG in a debug build to see eTRACE. */
}

void API_TraceHex(unsigned int level, char* msg, const void* hex, unsigned int hexlen)
{
    (void)level;
    (void)msg;
    (void)hex;
    (void)hexlen;
}

int API_Beep(unsigned int tFreq, unsigned int tTimeMs)
{
    (void)tFreq;
    (void)tTimeMs;
    return 0;
}

/* ===========================================================================
 * 5) stdlib / string helpers
 * ========================================================================= */
void* API_memcpy(void* dst, const void* src, int n)
{
    if (dst == NULL || src == NULL || n <= 0)
    {
        return dst;
    }
    return memcpy(dst, src, (size_t)n);
}

void* API_memmove(void* dst, const void* src, int n)
{
    if (dst == NULL || src == NULL || n <= 0)
    {
        return dst;
    }
    return memmove(dst, src, (size_t)n);
}

void* API_memset(void* dst, int s, int n)
{
    if (dst == NULL || n <= 0)
    {
        return dst;
    }
    return memset(dst, s, (size_t)n);
}

int API_memcmp(const void* a, const void* b, int n)
{
    if (n <= 0)
    {
        return 0;
    }
    return memcmp(a, b, (size_t)n);
}

int API_strlen(const char* s)
{
    return (s == NULL) ? 0 : (int)strlen(s);
}

char* API_strcpy(char* dst, const char* src)
{
    if (dst == NULL)
    {
        return NULL;
    }
    if (src == NULL)
    {
        dst[0] = '\0';
        return dst;
    }
    return strcpy(dst, src);
}

int API_sprintf(char* str, const char* format, ...)
{
    va_list ap;
    int ret;

    if (str == NULL || format == NULL)
    {
        return 0;
    }
    va_start(ap, format);
    ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

int API_snprintf(char* str, int size, const char* format, ...)
{
    va_list ap;
    int ret;

    if (str == NULL || format == NULL || size <= 0)
    {
        return 0;
    }
    va_start(ap, format);
    ret = vsnprintf(str, (size_t)size, format, ap);
    va_end(ap);
    return ret;
}

void API_Conv_StrToBcd_Left(char* str, int slen, u8* bcd)
{
    int i, blen;

    if (str == NULL || bcd == NULL || slen <= 0)
    {
        return;
    }
    blen = (slen + 1) / 2;
    memset(bcd, 0, (size_t)blen);
    for (i = 0; i < slen; i++)
    {
        int d = (str[i] >= '0' && str[i] <= '9') ? (str[i] - '0') : 0;
        if ((i & 1) == 0)
        {
            bcd[i / 2] |= (u8)(d << 4);
        }
        else
        {
            bcd[i / 2] |= (u8)d;
        }
    }
}

void API_Conv_StrToBcd_Right(char* str, int slen, u8* bcd, int blen)
{
    int i, off;

    if (str == NULL || bcd == NULL || slen <= 0 || blen <= 0)
    {
        return;
    }
    memset(bcd, 0, (size_t)blen);
    off = blen * 2 - slen;
    for (i = 0; i < slen; i++)
    {
        int d = (str[i] >= '0' && str[i] <= '9') ? (str[i] - '0') : 0;
        int nib = off + i;
        if ((nib & 1) == 0)
        {
            bcd[nib / 2] |= (u8)(d << 4);
        }
        else
        {
            bcd[nib / 2] |= (u8)d;
        }
    }
}

/* ===========================================================================
 * 6) RNG / time / crypto placeholders
 * ========================================================================= */
static uint32_t g_rng_state = 0x12345678u;

int API_Rng(u8* buf, int len)
{
    int i;

    if (buf == NULL || len < 0)
    {
        return CRYPT_PAR_ERR;
    }
    for (i = 0; i < len; i++)
    {
        g_rng_state = g_rng_state * 1103515245u + 12345u;
        buf[i] = (u8)((g_rng_state >> 16) & 0xFF);
    }
    return CRYPT_OK;
}

void API_TimeGet(DATE_TIME* pTime)
{
    if (pTime == NULL)
    {
        return;
    }
    pTime->Year = 2026;
    pTime->Month = 11;
    pTime->Day = 12;
    pTime->Hour = 10;
    pTime->Min = 30;
    pTime->Sec = 0;
    pTime->MSec = 0;
    pTime->WeekDay = 4;
    pTime->YearDay = 315;
}

int API_Hash(int type, u8* input, int inlen, u8* output, int flag)
{
    (void)type;
    (void)input;
    (void)inlen;
    (void)output;
    (void)flag;
    /* ODA / certificate hashing is out of scope for the host E2E test.
     * Return an explicit "not supported" instead of a fake digest. */
    return CRYPT_NOT_SUPPORTED;
}

int API_Crypt(int type, const u8* key, int keylen, const u8* iv,
              const u8* input, int inlen, u8* output, int* outlen, int mode)
{
    (void)type;
    (void)key;
    (void)keylen;
    (void)iv;
    (void)input;
    (void)inlen;
    (void)output;
    (void)outlen;
    (void)mode;
    return CRYPT_NOT_SUPPORTED;
}
