/**
************************************************************************************************************************
* @file     host_e2e.c
* @brief    Host end-to-end test: libentrypoint.a + libexpresspay.a + stub platform.
*
* This is the runtime acceptance gate that complements the compile/symbol audit:
*   a) link gate          : both static libraries + the platform stub link with no
*                           unresolved symbols, and the AMEX kernel registers;
*   b) AMEX registration  : amex_kernel_register() -> EMV_RegisterKernel -> candidate
*      and dispatch        list -> combination selection -> final SELECT -> APP_SELECTED;
*   c) malicious SDOL     : card-controlled 9F3F SDOL with an oversized request must
*                           make EMV_Process return an error state, never crash;
*   d) candidate cap      : >12 PPSE 61 templates must stop at EMV_CFG_MAX_CANDIDATES;
*   e) DOL packing bound  : EMV_TLV_PacketByDOL must reject over-long DOLs and keep
*                           the caller buffer (canary) untouched;
*   f) appdata reset      : EMV_ProcessExit + a new transaction must work again.
*
* No real hardware / real card is used.  See doc/TESTING.md.
************************************************************************************************************************
*/
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "sdk.h"
#include "ks_crypt.h"           /* API_Rng / API_Hash / API_Crypt                     */
#include "emvi.h"               /* emv.h + emv_type.h + emv_tag.h + emv_utils.h + emvoutcome.h */
#include "emv_kernel.h"         /* EMV_KERNEL_OPS / EMV_FindKernel / EMV_GetSelectedKernel */
#include "emvapi.h"             /* emv_set_termparam / emv_add_aidparam               */
#include "emvporting.h"         /* emvporting_get_termapps / emvporting_get_aidparam  */
#include "stub_platform.h"

#include "express_api.h"        /* amex_kernel_register()                              */

/* ---------------------------------------------------------------------------
 * Test harness
 * ------------------------------------------------------------------------- */
static int g_asserts;
static int g_fails;
static int g_known_fails;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        g_asserts++;                                                            \
        if (!(cond)) {                                                          \
            printf("    FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);       \
            g_fails++;                                                          \
        }                                                                       \
    } while (0)

/* A KNOWN-FAIL records behaviour that the integration contract requires but the
 * current code does not implement (real defect, documented with file:line).
 * It does not fail the run: the rest of the suite must still be green. */
#define KNOWN_FAIL(cond, msg, evidence)                                         \
    do {                                                                        \
        g_asserts++;                                                            \
        if (!(cond)) {                                                          \
            printf("    KNOWN-FAIL: %s\n              evidence: %s\n",          \
                   (msg), (evidence));                                          \
            g_known_fails++;                                                    \
        }                                                                       \
    } while (0)

#define CHECK_EQ(actual, expected, msg)                                         \
    do {                                                                        \
        long _a = (long)(actual);                                               \
        long _e = (long)(expected);                                             \
        g_asserts++;                                                            \
        if (_a != _e) {                                                         \
            printf("    FAIL: %s (got %ld, want %ld)  (%s:%d)\n",              \
                   (msg), _a, _e, __FILE__, __LINE__);                          \
            g_fails++;                                                          \
        }                                                                       \
    } while (0)

/* AMEX test AID (A0 00 00 00 25 01) */
static const uint8_t k_amex_aid[6] = { 0xA0, 0x00, 0x00, 0x00, 0x25, 0x01 };
static const uint8_t k_amex_kernel_id[8] = { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/* ---------------------------------------------------------------------------
 * TLV builders
 * ------------------------------------------------------------------------- */
static int der_len(uint8_t* out, int len)
{
    if (len < 0x80)
    {
        out[0] = (uint8_t)len;
        return 1;
    }
    if (len <= 0xFF)
    {
        out[0] = 0x81;
        out[1] = (uint8_t)len;
        return 2;
    }
    out[0] = 0x82;
    out[1] = (uint8_t)(len >> 8);
    out[2] = (uint8_t)len;
    return 3;
}

/* Append one BER-TLV object with a 1..3 byte tag (all tags used here fit). */
static int put_tlv(uint8_t* b, int pos, uint32_t tag, const uint8_t* val, int vlen)
{
    int tl;

    if (tag <= 0xFF)
    {
        b[pos++] = (uint8_t)tag;
        tl = 1;
    }
    else if (tag <= 0xFFFF)
    {
        b[pos++] = (uint8_t)(tag >> 8);
        b[pos++] = (uint8_t)tag;
        tl = 2;
    }
    else
    {
        b[pos++] = (uint8_t)(tag >> 16);
        b[pos++] = (uint8_t)(tag >> 8);
        b[pos++] = (uint8_t)tag;
        tl = 3;
    }
    (void)tl;
    pos += der_len(b + pos, vlen);
    if (vlen > 0 && val != NULL)
    {
        memcpy(b + pos, val, (size_t)vlen);
    }
    return pos + vlen;
}

/* Wrap already-built children into a constructed TLV. */
static int wrap_tlv(uint8_t* b, int pos, uint32_t tag, const uint8_t* child, int clen)
{
    return put_tlv(b, pos, tag, child, clen);
}

/* Build PPSE FCI:
 *   6F { 84 "2PAY.SYS.DDF01"  A5 { BF0C { 61*N [9F3F] } } }
 * n_cand = number of "61" application templates (all AMEX AID).
 * with_sdol != 0 appends a malicious 9F3F SDOL whose requested lengths exceed
 * the ICC send buffer (this is the card-controlled overflow attempt).
 */
static int build_ppse(uint8_t* out, int max, int n_cand, int with_sdol, int full)
{
    uint8_t bf0c[2048];
    uint8_t a5[2048];
    uint8_t f6f[2304];
    uint8_t one61[128];
    int p_bf = 0, p_a5 = 0, p_6f = 0, i;

    for (i = 0; i < n_cand; i++)
    {
        int p = 0;
        uint8_t label[9] = { 'A', 'M', 'E', 'X', ' ', 'T', 'E', 'S', 'T' };
        uint8_t prio = 0x01;
        uint8_t aidbuf[7];
        int aidlen = 6;

        memcpy(aidbuf, k_amex_aid, 6);

        /* The "13 templates" case (full == 0) must stay within the decode pool
         * and exercise the candidate cap, so it reuses one AID (dedup would
         * collapse it anyway; the cap triggers on the 13th add).  The "full"
         * case needs *distinct* candidates: append one byte (partial AID match)
         * so 10 fully-populated templates remain 10 candidates after dedup. */
        if (full && n_cand > 1)
        {
            aidbuf[6] = (uint8_t)(0x10 + i);
            aidlen = 7;
        }
        p = put_tlv(one61, p, 0x4F, aidbuf, aidlen);
        if (full)
        {
            p = put_tlv(one61, p, 0x50, label, 9);
            p = put_tlv(one61, p, 0x87, &prio, 1);
            p = put_tlv(one61, p, 0x9F2A, k_amex_kernel_id, 8);
        }
        p_bf = wrap_tlv(bf0c, p_bf, 0x61, one61, p);
    }

    if (with_sdol)
    {
        /* 9F3F SDOL: 84 x "9F 02 FF" = 252 bytes of DOL entries, each requesting
         * 0xFF bytes (255).  Expansion needs > 20 KB while the ICC send buffer is
         * 262 bytes -> sdol_expand_len() must reject it before packing. */
        uint8_t sdol[252];
        int k;
        for (k = 0; k < (int)sizeof(sdol); k += 3)
        {
            sdol[k] = 0x9F;
            sdol[k + 1] = 0x02;
            sdol[k + 2] = 0xFF;
        }
        p_bf = put_tlv(bf0c, p_bf, 0x9F3F, sdol, (int)sizeof(sdol));
    }

    p_a5 = wrap_tlv(a5, p_a5, 0xBF0C, bf0c, p_bf);
    p_6f = put_tlv(f6f, p_6f, 0x84, (const uint8_t*)"2PAY.SYS.DDF01", 14);
    p_6f = wrap_tlv(f6f, p_6f, 0xA5, a5, p_a5);

    /* out = 6F <len> <6f body> */
    p_6f = wrap_tlv(out, 0, 0x6F, f6f, p_6f);
    return (p_6f <= max) ? p_6f : -1;
}

/* Build the final SELECT FCI for the AMEX AID (6F/84/A5 required). */
static int build_adf(uint8_t* out, int max)
{
    uint8_t f6f[128];
    uint8_t a5[64];
    uint8_t label[8] = { 'A', 'M', 'E', 'X', ' ', 'A', 'D', 'F' };
    uint8_t prio = 0x01;
    int p6 = 0, p5 = 0;

    p5 = put_tlv(a5, p5, 0x50, label, 8);
    p5 = put_tlv(a5, p5, 0x87, &prio, 1);

    p6 = put_tlv(f6f, p6, 0x84, k_amex_aid, 6);
    p6 = wrap_tlv(f6f, p6, 0xA5, a5, p5);

    p6 = wrap_tlv(out, 0, 0x6F, f6f, p6);
    return (p6 <= max) ? p6 : -1;
}

/* Terminal parameter TLV (one valid file; 9F66 carries the QPBOC capability bit
 * the AMEX kernel requires in preprocess()). */
static int build_termparam(uint8_t* out, int max)
{
    uint8_t v[32];
    int p = 0;

    memset(v, 0, sizeof(v));

    v[0] = 0x36; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00;
    p = put_tlv(out, p, 0x9F66, v, 4);

    v[0] = 0xE0; v[1] = 0xF8; v[2] = 0xC8;
    p = put_tlv(out, p, 0x9F33, v, 3);

    v[0] = 0x22;
    p = put_tlv(out, p, 0x9F35, v, 1);

    v[0] = 0xF0; v[1] = 0x00; v[2] = 0xF0; v[3] = 0xF0; v[4] = 0x01;
    p = put_tlv(out, p, 0x9F40, v, 5);

    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00;
    p = put_tlv(out, p, 0x9F1B, v, 4);

    p = put_tlv(out, p, 0x9F1C, (const uint8_t*)"TERM0001", 8);
    p = put_tlv(out, p, 0x9F1E, (const uint8_t*)"IFD00001", 8);

    v[0] = 0x01; v[1] = 0x56;
    p = put_tlv(out, p, 0x9F1A, v, 2);
    p = put_tlv(out, p, 0x5F2A, v, 2);

    v[0] = 0x02;
    p = put_tlv(out, p, 0x5F36, v, 1);

    v[0] = 0x01; v[1] = 0x56;
    p = put_tlv(out, p, 0x9F3C, v, 2);

    v[0] = 0x02;
    p = put_tlv(out, p, 0x9F3D, v, 1);

    memset(v, 0, sizeof(v));
    p = put_tlv(out, p, 0xDF11, v, 5);
    p = put_tlv(out, p, 0xDF12, v, 5);
    p = put_tlv(out, p, 0xDF13, v, 5);

    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00; v[4] = 0x00; v[5] = 0x01;
    p = put_tlv(out, p, 0x9F01, v, 6);

    v[0] = 0x59; v[1] = 0x99;
    p = put_tlv(out, p, 0x9F15, v, 2);

    p = put_tlv(out, p, 0x9F16, (const uint8_t*)"MERCHANT0000001", 15);
    p = put_tlv(out, p, 0x9F4E, (const uint8_t*)"TESTMERCHANT", 12);

    /* floor / limit parameters: 6-byte BCD, 000000000000 / 000000005000 / 000000010000 */
    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00; v[4] = 0x00; v[5] = 0x00;
    p = put_tlv(out, p, 0xDF19, v, 6);

    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00; v[4] = 0x50; v[5] = 0x00;
    p = put_tlv(out, p, 0xDF20, v, 6);

    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00; v[4] = 0x10; v[5] = 0x00;
    p = put_tlv(out, p, 0xDF21, v, 6);

    v[0] = 0x00;
    p = put_tlv(out, p, 0x1F02, v, 1);

    v[0] = 0x9F; v[1] = 0x37; v[2] = 0x04;
    p = put_tlv(out, p, 0x1F03, v, 3);

    return (p <= max) ? p : -1;
}

/* AID parameter record (needs 9F06 + 9C + 9F2A for emvporting_add_aidparam). */
static int build_aidparam(uint8_t* out, int max)
{
    uint8_t v[16];
    int p = 0;

    p = put_tlv(out, p, 0x9F06, k_amex_aid, 6);

    v[0] = 0x00;
    p = put_tlv(out, p, 0x009C, v, 1);

    /* 9F2A = 00: "kernel id not configured at AID level" -> the Entry Point
     * falls back to AID/RID combination selection.  The PPSE candidate still
     * carries an explicit 9F2A in the dispatch test, so kernel selection goes
     * through the Kernel Identifier path there. */
    p = put_tlv(out, p, 0x9F2A, v, 1);

    v[0] = 0x00;
    p = put_tlv(out, p, 0xDF01, v, 1);

    memset(v, 0, sizeof(v));
    p = put_tlv(out, p, 0xDF19, v, 6);
    p = put_tlv(out, p, 0xDF20, v, 6);
    p = put_tlv(out, p, 0xDF21, v, 6);

    v[0] = 0x00; v[1] = 0x00; v[2] = 0x00; v[3] = 0x00;
    p = put_tlv(out, p, 0x9F1B, v, 4);

    return (p <= max) ? p : -1;
}

/* ---------------------------------------------------------------------------
 * EMV_PORTING implementation (platform / application layer)
 * ------------------------------------------------------------------------- */
static uint8_t g_emv_pool[16 * 1024];

static uint8_t* test_alloc_buffer(int32_t* size)
{
    memset(g_emv_pool, 0, sizeof(g_emv_pool));
    if (size != NULL)
    {
        *size = (int32_t)sizeof(g_emv_pool);
    }
    return g_emv_pool;
}

static int32_t test_icc_rw(int32_t type, uint8_t* input, int32_t inlen,
                           uint8_t* output, int32_t* outlen)
{
    int32_t rlen = 0;
    int sw;

    stub_icc_note_type((int)type);
    sw = stub_icc_apdu(input, inlen, output, EMV_CFG_ICC_RECV_MAXLEN, (int*)&rlen);
    if (outlen != NULL)
    {
        *outlen = rlen;
    }
    (void)type;
    return (int32_t)sw;
}

static int32_t test_message(int32_t id)                          { (void)id; return 0; }
static int32_t test_sel_candidate_list(int32_t again, const EMV_CANDIDATE* list, int32_t count)
{
    (void)again; (void)list; (void)count;
    return 0;
}
static int32_t test_sel_account_type(void)                      { return 0; }
static int32_t test_deactivate_card(void)                        { return 0; }
static int32_t test_check_id(uint8_t type, const uint8_t* num, int32_t numlen)
{
    (void)type; (void)num; (void)numlen;
    return 0;
}
static int32_t test_pin_entry(uint8_t type, uint8_t* output, int32_t* outlen)
{
    (void)type; (void)output;
    if (outlen != NULL) { *outlen = 0; }
    return 0;
}
static int32_t test_confirm_bypass_pin(void)                     { return 0; }
static int32_t test_confirm_force_online(void)                    { return 0; }
static int32_t test_go_online(uint8_t* resp, int32_t* resplen)
{
    (void)resp;
    if (resplen != NULL) { *resplen = 0; }
    return 0;
}
static int32_t test_issuer_referral(uint8_t* pan, int32_t panlen)
{
    (void)pan; (void)panlen;
    return 0;
}

static int32_t test_recover(int32_t alg, EMV_PUBLICKEY* pk, uint8_t* input, uint8_t* output)
{
    (void)alg; (void)pk; (void)input; (void)output;
    return -1;
}
static int32_t test_hash(int32_t alg, uint8_t* input, int32_t inlen, uint8_t* output)
{
    (void)alg; (void)input; (void)inlen; (void)output;
    return -1;
}
static int32_t test_get_random(uint8_t* buf, int32_t len)
{
    int32_t i;
    static uint32_t s = 0x0BADF00Du;

    if (buf == NULL || len < 0)
    {
        return -1;
    }
    for (i = 0; i < len; i++)
    {
        s = s * 1664525u + 1013904223u;
        buf[i] = (uint8_t)(s >> 24);
    }
    return 0;
}
static int32_t test_get_trans_date_time(uint8_t* date, uint8_t* time)
{
    /* BCD YYYYMMDD (4 bytes) + hhmmss (3 bytes) */
    static const uint8_t d[4] = { 0x20, 0x26, 0x11, 0x12 };
    static const uint8_t t[3] = { 0x10, 0x30, 0x00 };

    if (date != NULL) { memcpy(date, d, 4); }
    if (time != NULL) { memcpy(time, t, 3); }
    return 0;
}
static int32_t test_get_trans_sequence_counter(uint8_t* buf)
{
    static const uint8_t atc[4] = { 0x00, 0x00, 0x00, 0x01 };

    if (buf != NULL) { memcpy(buf, atc, 4); }
    return 0;
}
static int32_t test_get_termapps(EMV_TERMAPP* apps)
{
    /* Real emvporting path: parses the AID parameter file written by
     * emv_add_aidparam() (exercises the RAM file system). */
    return emvporting_get_termapps(apps);
}
static int32_t test_get_aidparam(EMV_AIDPARAM* par, int32_t inx)
{
    return emvporting_get_aidparam(par, inx);
}
static int32_t test_get_capkparam(EMV_CAPKPARAM* crt, uint8_t pki, const uint8_t* rid)
{
    (void)crt; (void)pki; (void)rid;
    return -1;
}
static uint32_t test_get_translog_amount(const uint8_t* pan, uint8_t panlen, uint8_t policy)
{
    (void)pan; (void)panlen; (void)policy;
    return 0;
}
static int32_t test_sav_translog(const uint8_t* pan, uint8_t panlen, uint32_t amount)
{
    (void)pan; (void)panlen; (void)amount;
    return 0;
}
static int32_t test_fnd_blackcard(const uint8_t* pan, uint8_t panlen, uint8_t seq)
{
    (void)pan; (void)panlen; (void)seq;
    return 0;
}
static int32_t test_fnd_blackcert(const uint8_t* rid, uint8_t pki, const uint8_t* csn)
{
    (void)rid; (void)pki; (void)csn;
    return 0;
}
static uint8_t test_get_kernel_index(void)                        { return 0; }
static int32_t test_icc_pwron(void)                               { return 0; }

static void test_trace(uint32_t level, char* fmt, ...)
{
    (void)level; (void)fmt;
}
static void test_trace_hex(uint32_t level, char* msg, const void* hex, uint32_t hexlen)
{
    (void)level; (void)msg; (void)hex; (void)hexlen;
}

static const EMV_PORTING g_port =
{
    .trace                        = test_trace,
    .trace_hex                    = test_trace_hex,
    .message                      = test_message,
    .sel_candidate_list           = test_sel_candidate_list,
    .sel_account_type             = test_sel_account_type,
    .deactivate_card              = test_deactivate_card,
    .check_id                     = test_check_id,
    .pin_entry                    = test_pin_entry,
    .confirm_bypass_pin           = test_confirm_bypass_pin,
    .confirm_force_online         = test_confirm_force_online,
    .go_online                    = test_go_online,
    .issuer_referral              = test_issuer_referral,
    .alloc_buffer                 = test_alloc_buffer,
    .icc_rw                       = test_icc_rw,
    .recover                      = test_recover,
    .hash                         = test_hash,
    .get_random                   = test_get_random,
    .get_trans_date_time          = test_get_trans_date_time,
    .get_trans_sequence_counter   = test_get_trans_sequence_counter,
    .get_termapps                 = test_get_termapps,
    .get_aidparam                 = test_get_aidparam,
    .get_capkparam                = test_get_capkparam,
    .get_translog_amount          = test_get_translog_amount,
    .sav_translog                 = test_sav_translog,
    .fnd_blackcard                = test_fnd_blackcard,
    .fnd_blackcert                = test_fnd_blackcert,
    .get_kernel_index             = test_get_kernel_index,
    .icc_pwron                    = test_icc_pwron
};

/* ---------------------------------------------------------------------------
 * Transaction helpers
 * ------------------------------------------------------------------------- */
/* EMV_Init -> EMV_PreProcess -> SetCardType/SetTransType/SetTransAmount.
 * Returns 0 on success, otherwise the failing step number (negative). */
static int tx_begin(void)
{
    if (EMV_Init(&g_port) != EMV_OK)                       { return -1; }
    if (EMV_PreProcess(1) != EMV_OK)                       { return -2; }
    if (EMV_SetCardType(EMV_CARD_CONTACTLESS) != EMV_OK)   { return -3; }
    if (EMV_SetTransType(EMV_TRANSTYPE_GOODS_SERVICES) != EMV_OK) { return -4; }
    if (EMV_SetTransAmount(1234) != EMV_OK)                { return -5; }
    return 0;
}

static void icc_reset_with_ppse(int n_cand, int with_sdol, int with_adf)
{
    uint8_t buf[2048];
    int len;
    int full = (n_cand > EMV_CFG_MAX_CANDIDATES) ? 0 : 1;

    stub_icc_reset();

    len = build_ppse(buf, (int)sizeof(buf), n_cand, with_sdol, full);
    if (len > 0)
    {
        stub_icc_set_ppse(buf, len);
    }
    if (with_adf)
    {
        len = build_adf(buf, (int)sizeof(buf));
        if (len > 0)
        {
            stub_icc_set_adf(buf, len);
        }
    }
}

/* ---------------------------------------------------------------------------
 * a) Link gate + registration
 * ------------------------------------------------------------------------- */
static void case_a_link_and_register(void)
{
    const EMV_KERNEL_OPS* ops;

    printf("[a] link gate / kernel registration\n");

    CHECK_EQ(amex_kernel_register(), 0, "amex_kernel_register() must succeed");
    ops = EMV_FindKernel(EMV_KERNEL_ID_AMEX);
    CHECK(ops != NULL, "AMEX kernel must be visible through EMV_FindKernel");
    if (ops != NULL)
    {
        CHECK(ops->kernel_id == EMV_KERNEL_ID_AMEX, "registered kernel_id must be 0x04");
        CHECK(ops->init != NULL && ops->preprocess != NULL && ops->process != NULL,
              "registered init/preprocess/process must not be NULL");
    }

    /* Force every platform stub symbol to be referenced: if any of them were
     * missing this executable would not have linked at all. */
    CHECK((void*)&API_ICC_Apdu != NULL,   "API_ICC_Apdu present");
    CHECK((void*)&API_fopen != NULL,      "API_fopen present");
    CHECK((void*)&API_fread != NULL,      "API_fread present");
    CHECK((void*)&API_fwrite != NULL,     "API_fwrite present");
    CHECK((void*)&API_Hash != NULL,       "API_Hash present");
    CHECK((void*)&API_Crypt != NULL,      "API_Crypt present");
    CHECK((void*)&API_Rng != NULL,        "API_Rng present");
    CHECK((void*)&API_TimeGet != NULL,    "API_TimeGet present");
    CHECK((void*)&ics_option != NULL,     "ics_option present");
    CHECK((void*)&CreateEMVDiscretionaryData != NULL, "CreateEMVDiscretionaryData present");

    /* The platform stub's RAM file system must have been used for real by
     * emvporting: emv.par (apipara) + one terminal + one AID parameter record. */
    CHECK_EQ(stub_fs_exists("emv.par"), 1, "emvporting must persist emv.par via API_fopen");
    CHECK_EQ(stub_fs_exists("emvterm0.par"), 1, "emv_set_termparam must write emvterm0.par");
    CHECK_EQ(stub_fs_exists("emvaids0.par"), 1, "emv_add_aidparam must write emvaids0.par");
    CHECK_EQ(stub_fs_size_of("emvaids0.par"), 1024, "AID parameter record must be 1024 bytes");
    {
        uint8_t payload[1024];
        int plen = stub_fs_read_param_payload("emvterm0.par", payload, (int)sizeof(payload));
        CHECK(plen > 0, "terminal parameter file must contain a length-prefixed TLV payload");
        CHECK_EQ(plen, stub_fs_size_of("emvterm0.par") - 4,
              "terminal parameter payload length must match the file header");
    }
}

/* ---------------------------------------------------------------------------
 * b) AMEX dispatch: candidate list -> APP_SELECTED
 * ------------------------------------------------------------------------- */
static void case_b_amex_dispatch(void)
{
    int32_t msg = 0;
    int32_t ret;
    int32_t len = 0;
    uint8_t* p;
    int n_select;
    const EMV_CANDIDATE* cand;
    const EMV_CANDIDATE* sel;

    printf("[b] AMEX registration / dispatch (candidate list -> APP_SELECTED)\n");

    icc_reset_with_ppse(1, 0, 1);
    CHECK_EQ(tx_begin(), 0, "EMV_Init/PreProcess/SetCardType/SetTrans* must succeed");

    ret = EMV_Process(&msg);
    CHECK_EQ(ret, EMV_STA_CANDIDATES_BUILT, "first EMV_Process must build the candidate list");
    CHECK_EQ(stub_icc_select_count(), 1, "exactly one PPSE SELECT before candidate list");
    CHECK_EQ(stub_icc_last_type(), EMV_CARD_CONTACTLESS, "ICC called with contactless card type");

    p = EMV_GetData(TAG_CANDIDATE_LIST, &len);
    CHECK(p != NULL, "candidate list must be readable via EMV_GetData");
    CHECK_EQ(len, (long)sizeof(EMV_CANDIDATE), "exactly one candidate");
    if (p != NULL && len == (int32_t)sizeof(EMV_CANDIDATE))
    {
        cand = (const EMV_CANDIDATE*)p;
        CHECK(memcmp(cand->aid, k_amex_aid, 6) == 0, "candidate AID must be the AMEX test AID");
        CHECK(cand->kernel_id_len == 8 &&
              memcmp(cand->kernel_id, k_amex_kernel_id, 8) == 0,
              "candidate must carry 9F2A = 04 00 00 00 00 00 00 00");
    }

    ret = EMV_Process(&msg);
    CHECK_EQ(ret, EMV_STA_APP_SELECTED, "second EMV_Process must reach APP_SELECTED");
    CHECK_EQ(EMV_GetSelectedKernel(), EMV_KERNEL_ID_AMEX, "AMEX kernel must be selected");

    n_select = stub_icc_select_count();
    CHECK(n_select >= 2, "PPSE SELECT + final ADF SELECT must both reach the card");

    p = EMV_GetData(TAG_9F06_AID_TERMINAL, &len);
    CHECK(p != NULL && len == 6 && memcmp(p, k_amex_aid, 6) == 0,
          "kernel must save the selected AID (9F06) into appdata");

    p = EMV_GetData(TAG_9F37_UNPREDICATABLE_NUMBER, &len);
    CHECK(p != NULL && len == 4, "kernel must write the 4-byte unpredictable number");

    p = EMV_GetData(TAG_0095_TVR, &len);
    CHECK(p != NULL && len == 5, "kernel must create TVR (5 bytes) before later stages");

    sel = EMV_GetSelectedCandidate();
    CHECK(sel != NULL && sel->aid_len == 6 && memcmp(sel->aid, k_amex_aid, 6) == 0,
          "EMV_GetSelectedCandidate must expose the AMEX candidate to the kernel");

    /* D1 fixed: the kernel takes transaction data from the Entry Point in
     * express_preprocess() before it resets appdata. */
    p = EMV_GetData(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, &len);
    CHECK(p != NULL && len == 6 &&
          p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 &&
          p[3] == 0x00 && p[4] == 0x12 && p[5] == 0x34,
          "kernel must pack the terminal transaction amount (9F02 = 000000001234)");

    p = EMV_GetData(TAG_009C_TRANSACTION_TYPE, &len);
    CHECK(p != NULL && len == 1 && p[0] == EMV_TRANSTYPE_GOODS_SERVICES,
          "kernel must preserve the terminal transaction type (9C = 00)");
}

/* ---------------------------------------------------------------------------
 * c) Malicious card SDOL must not crash
 * ------------------------------------------------------------------------- */
static void case_c_malicious_sdol(void)
{
    int32_t msg = 0;
    int32_t ret;
    int32_t len = 0;
    uint8_t* p;

    printf("[c] malicious card SDOL (9F3F) must be rejected, not crash\n");

    icc_reset_with_ppse(1, 1, 0);
    CHECK_EQ(tx_begin(), 0, "transaction setup must succeed");

    ret = EMV_Process(&msg);

    /* sdol_process() -> sdol_expand_len() rejects the oversized SDOL; the Entry
     * Point converts that into the Book B "other card" outcome (ENDAPPLICATION).
     * The important part is that the process survives and does not report a
     * candidate list. */
    CHECK_EQ(ret, EMV_ENDAPPLICATION, "malicious SDOL must end the application");
    CHECK(ret != EMV_STA_CANDIDATES_BUILT && ret != EMV_STA_APP_SELECTED,
          "malicious SDOL must not be treated as a normal application");
    CHECK_EQ(stub_icc_cmd_count(), 1,
          "oversized SDOL must be rejected before any SPI APDU is sent");

    p = EMV_GetData(TAG_9F3F_SDOL, &len);
    CHECK(p != NULL && len == 252, "the card-supplied SDOL must be stored (252 bytes)");
    CHECK(EMV_GetData(TAG_CANDIDATE_LIST, &len) != NULL || len == 0,
          "candidate list accessor must stay valid");
    CHECK_EQ(len, 0, "no candidate may be added from the malicious PPSE");

    p = EMV_GetData(TAG_DF8115_ERROR_INDICATION, &len);
    CHECK(p != NULL, "Book B error indication must be recorded");
}

/* ---------------------------------------------------------------------------
 * d) Candidate list upper bound
 * ------------------------------------------------------------------------- */
static void case_d_candidate_cap(void)
{
    int32_t msg = 0;
    int32_t ret;
    int32_t len = 0;
    uint8_t* p;
    int n;

    printf("[d] candidate list upper bound (>12 PPSE templates)\n");

    icc_reset_with_ppse(13, 0, 0);
    CHECK_EQ(tx_begin(), 0, "transaction setup must succeed");

    ret = EMV_Process(&msg);

    CHECK_EQ(ret, EMV_ENDAPPLICATION, "13 app templates must end the application (no silent success)");
    CHECK(ret != EMV_STA_CANDIDATES_BUILT, "13 app templates must not build a 13-entry list");

    p = EMV_GetData(TAG_CANDIDATE_LIST, &len);
    CHECK(p != NULL, "candidate buffer must be accessible for bounds check");
    CHECK(len >= 0 && len <= (int32_t)(EMV_CFG_MAX_CANDIDATES * sizeof(EMV_CANDIDATE)),
          "candidate bytes must not exceed EMV_CFG_MAX_CANDIDATES entries");
    n = (int)sizeof(EMV_CANDIDATE);
    CHECK_EQ(len / n, EMV_CFG_MAX_CANDIDATES,
          "candidate count must be capped at EMV_CFG_MAX_CANDIDATES");

    /* d2: a spec-shaped PPSE where every 61 template also carries 50/87/9F2A.
     * D2 fixed: the decoder pool now holds 4 containers + 12*4 template objects. */
    icc_reset_with_ppse(10, 0, 0);
    CHECK_EQ(tx_begin(), 0, "d2 transaction setup must succeed");
    ret = EMV_Process(&msg);
    CHECK(ret == EMV_REQ_SEL_CANDIDATES || ret == EMV_STA_CANDIDATES_BUILT,
          "d2: spec-shaped PPSE must reach candidate selection");

    p = EMV_GetData(TAG_CANDIDATE_LIST, &len);
    CHECK(p != NULL && (len / n) == 10,
          "10 fully-populated app templates must decode and produce 10 candidates");
}

/* ---------------------------------------------------------------------------
 * e) EMV_TLV_PacketByDOL bounds + canary
 * ------------------------------------------------------------------------- */
#define CANARY_BYTE 0xA5
#define CANARY_SIZE 48
#define OUT_MAX     16

static int canary_intact(const uint8_t* out, int from, int size)
{
    int i;
    for (i = from; i < size; i++)
    {
        if (out[i] != CANARY_BYTE)
        {
            return 0;
        }
    }
    return 1;
}

static void case_e_dol_packet_bounds(void)
{
    uint8_t out[OUT_MAX + CANARY_SIZE];
    uint8_t val[64];
    int32_t olen;
    int32_t ret;

    printf("[e] DOL packing boundary (EMV_TLV_PacketByDOL)\n");

    memset(val, 0x11, sizeof(val));

    /* e1: DOL asks for 64 bytes, only 16 available in the output buffer. */
    {
        static const uint8_t dol[] = { 0x9F, 0x02, 0x81, 0x40 };   /* 9F02, length 64 */
        memset(out, CANARY_BYTE, sizeof(out));
        olen = -1234;
        ret = EMV_TLV_PacketByDOL((uint8_t*)dol, (int32_t)sizeof(dol),
                                  val, (int32_t)sizeof(val), out, &olen, OUT_MAX);
        CHECK_EQ(ret, EMV_FAIL, "oversized DOL value must be rejected");
        CHECK_EQ(olen, -1234, "pOutLen must stay untouched on failure");
        CHECK(canary_intact(out, OUT_MAX, (int)sizeof(out)), "canary after output buffer must be intact");
    }

    /* e2: 3-byte DOL length (65536 requested) far beyond the value buffer. */
    {
        static const uint8_t dol[] = { 0x9F, 0x02, 0x83, 0x01, 0x00, 0x00 };
        memset(out, CANARY_BYTE, sizeof(out));
        olen = -1234;
        ret = EMV_TLV_PacketByDOL((uint8_t*)dol, (int32_t)sizeof(dol),
                                  val, 8, out, &olen, OUT_MAX);
        CHECK_EQ(ret, EMV_FAIL, "huge DOL length must be rejected");
        CHECK_EQ(olen, -1234, "pOutLen must stay untouched on failure");
        CHECK(canary_intact(out, OUT_MAX, (int)sizeof(out)), "canary after output buffer must be intact");
    }

    /* e3: positive control - a well-formed DOL still packs correctly. */
    {
        static const uint8_t dol[] = { 0x9F, 0x02, 0x06 };         /* 9F02, length 6 */
        uint8_t good[32];
        memset(good, 0, sizeof(good));
        olen = -1;
        ret = EMV_TLV_PacketByDOL((uint8_t*)dol, (int32_t)sizeof(dol),
                                  val, 6, good, &olen, (int32_t)sizeof(good));
        CHECK_EQ(ret, EMV_OK, "well-formed DOL must pack successfully");
        CHECK_EQ(olen, 9, "packed TLV length must be tag(2)+len(1)+value(6)");
        if (olen == 9)
        {
            CHECK(good[0] == 0x9F && good[1] == 0x02 && good[2] == 0x06,
                  "packed TLV header must be 9F 02 06");
            CHECK(memcmp(good + 3, val, 6) == 0, "packed TLV value must be copied");
        }
    }
}

/* ---------------------------------------------------------------------------
 * f) appdata reset between transactions
 * ------------------------------------------------------------------------- */
static void case_f_appdata_reset(void)
{
    int32_t msg = 0;
    int32_t ret;
    int32_t len = 0;

    printf("[f] appdata reset (EMV_ProcessExit -> new transaction)\n");

    icc_reset_with_ppse(1, 0, 1);
    CHECK_EQ(tx_begin(), 0, "first transaction setup must succeed");
    CHECK_EQ(EMV_Process(&msg), EMV_STA_CANDIDATES_BUILT, "first transaction: candidates built");
    CHECK_EQ(EMV_Process(&msg), EMV_STA_APP_SELECTED, "first transaction: APP_SELECTED");

    CHECK_EQ(EMV_ProcessExit(), EMV_OK, "EMV_ProcessExit must succeed");
    CHECK(EMV_GetData(TAG_009C_TRANSACTION_TYPE, &len) == NULL,
          "appdata must be empty after EMV_ProcessExit");

    /* Second transaction through the same (re-initialized) kernel. */
    icc_reset_with_ppse(1, 0, 1);
    CHECK_EQ(tx_begin(), 0, "second transaction setup must succeed");
    CHECK(EMV_GetData(TAG_009C_TRANSACTION_TYPE, &len) == NULL,
          "appdata must be empty after EMV_Init");
    CHECK_EQ(EMV_Process(&msg), EMV_STA_CANDIDATES_BUILT, "second transaction: candidates built");
    CHECK_EQ(EMV_Process(&msg), EMV_STA_APP_SELECTED, "second transaction: APP_SELECTED");
    CHECK_EQ(EMV_GetSelectedKernel(), EMV_KERNEL_ID_AMEX, "second transaction: AMEX selected again");

    CHECK(EMV_GetData(TAG_009C_TRANSACTION_TYPE, &len) != NULL,
          "second transaction: appdata repopulated by the kernel");
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
static int setup_parameter_files(void)
{
    uint8_t term[512];
    uint8_t aid[512];
    int tlen, alen;

    tlen = build_termparam(term, (int)sizeof(term));
    alen = build_aidparam(aid, (int)sizeof(aid));
    if (tlen <= 0 || alen <= 0)
    {
        printf("  setup: TLV build failed\n");
        return -1;
    }

    if (emv_set_termparam(term, tlen) != EMV_OK)
    {
        printf("  setup: emv_set_termparam failed (RAM file IO / validation)\n");
        return -1;
    }
    if (emv_add_aidparam(aid, alen) != EMV_OK)
    {
        printf("  setup: emv_add_aidparam failed (RAM file IO / validation)\n");
        return -1;
    }
    return 0;
}

int main(void)
{
    printf("== Entrypoint + AMEX Expresspay host E2E test ==\n\n");

    g_asserts = 0;
    g_fails = 0;
    g_known_fails = 0;

    stub_fs_reset();
    stub_ics_reset();

    if (setup_parameter_files() != 0)
    {
        printf("\nRESULT: FAIL (platform stub / parameter file setup)\n");
        return 1;
    }

    case_a_link_and_register();
    case_b_amex_dispatch();
    case_c_malicious_sdol();
    case_d_candidate_cap();
    case_e_dol_packet_bounds();
    case_f_appdata_reset();

    printf("\n--------------------------------------------\n");
    printf("assertions: %d, failures: %d, known-fails (documented gaps): %d\n",
           g_asserts, g_fails, g_known_fails);
    if (g_fails == 0)
    {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
