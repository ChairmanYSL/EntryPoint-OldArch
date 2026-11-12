/**
************************************************************************************************************************
*                                     COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_combination_Selection.c
* @brief    Combination selection (Entry Point Book B) - AID / Kernel Identifier -> kernel
*
* Rule (EMVCo Entry Point v2.12, combination selection):
*   1. If the candidate carries a Kernel Identifier [9F2A], its first byte selects the kernel.
*   2. Otherwise the kernel is derived from the AID (RID) table (combination selection by AID).
*   3. No kernel match -> unsupported application -> caller ends the application (no crash).
*
* NOTE: this file must not reference any concrete kernel symbol (registry only).
************************************************************************************************************************
*/
#include "emvi.h"
#include "emv_kernel.h"

#define ADF_MASTERCARD "\xA0\x00\x00\x00\x04"
#define ADF_PURE1      "\xA0\x00\x00\x02\x80"
#define ADF_PURE2      "\xA0\x00\x00\x02\x28"
#define ADF_PURE3      "\xD9\x99\x99\x99\x99"
#define ADF_PURE4      "\xD6\x66\x66\x66\x66"
#define ADF_VISA       "\xA0\x00\x00\x00\x03"
#define ADF_PBOC       "\xA0\x00\x00\x03\x33"
#define ADF_JCB        "\xA0\x00\x00\x00\x65"
#define ADF_AMEX       "\xA0\x00\x00\x00\x25"
#define ADF_DISV       "\xA0\x00\x00\x01\x52"

static uint8_t aid_to_kernel_id(const uint8_t* aid, uint8_t aid_len)
{
    if (aid == 0 || aid_len < 5)
    {
        return EMV_KERNEL_ID_UNKNOWN;
    }

    if (emv_memcmp(aid, (const uint8_t*)ADF_MASTERCARD, 5) == 0) return EMV_KERNEL_ID_MASTERCARD;
    if (emv_memcmp(aid, (const uint8_t*)ADF_VISA, 5) == 0)       return EMV_KERNEL_ID_VISA;
    if (emv_memcmp(aid, (const uint8_t*)ADF_AMEX, 5) == 0)       return EMV_KERNEL_ID_AMEX;
    if (emv_memcmp(aid, (const uint8_t*)ADF_JCB, 5) == 0)        return EMV_KERNEL_ID_JCB;
    if (emv_memcmp(aid, (const uint8_t*)ADF_DISV, 5) == 0)       return EMV_KERNEL_ID_DISCOVER;
    if (emv_memcmp(aid, (const uint8_t*)ADF_PBOC, 5) == 0)       return EMV_KERNEL_ID_PBOC;

    if (emv_memcmp(aid, (const uint8_t*)ADF_PURE1, 5) == 0 ||
        emv_memcmp(aid, (const uint8_t*)ADF_PURE2, 5) == 0 ||
        emv_memcmp(aid, (const uint8_t*)ADF_PURE3, 5) == 0 ||
        emv_memcmp(aid, (const uint8_t*)ADF_PURE4, 5) == 0)
    {
        return EMV_KERNEL_ID_PURE;
    }

    return EMV_KERNEL_ID_UNKNOWN;
}

/* Kernel Identifier [9F2A]: first byte is the kernel id, the rest is kernel data. */
static uint8_t kernel_identifier_to_kernel_id(const EMV_CANDIDATE* candidate)
{
    uint8_t kernel_id;

    if (candidate->kernel_id_len == 0)
    {
        return EMV_KERNEL_ID_UNKNOWN;
    }

    kernel_id = candidate->kernel_id[0];
    switch (kernel_id)
    {
        case EMV_KERNEL_ID_MASTERCARD:
        case EMV_KERNEL_ID_VISA:
        case EMV_KERNEL_ID_AMEX:
        case EMV_KERNEL_ID_JCB:
        case EMV_KERNEL_ID_DISCOVER:
        case EMV_KERNEL_ID_PBOC:
        case EMV_KERNEL_ID_PURE:
            return kernel_id;
        default:
            return EMV_KERNEL_ID_UNKNOWN;
    }
}

int32_t emv_combination_Selection(void)
{
    const EMV_CANDIDATE* candidate;
    uint8_t kernel_id;
    int32_t index;

    if (g_emv_cnxt == 0)
    {
        return EMV_UNINITIALIZED;
    }

    index = g_emv_cnxt->req_sel_candidates_result;
    if (index < 0 || index >= g_emv_cnxt->candidates_num || g_emv_cnxt->candidates == 0)
    {
        return EMV_ERR;
    }

    candidate = &g_emv_cnxt->candidates[index];

    /* 1) Kernel Identifier takes precedence (EMVCo EP Book B, combination selection) */
    kernel_id = kernel_identifier_to_kernel_id(candidate);
    if (kernel_id == EMV_KERNEL_ID_UNKNOWN)
    {
        /* 2) fall back to AID based combination selection */
        kernel_id = aid_to_kernel_id(candidate->aid, candidate->aid_len);
    }

    g_emv_cnxt->contactless_type = emv_kernel_id_to_contactless(kernel_id);
    if (kernel_id == EMV_KERNEL_ID_UNKNOWN)
    {
        /* Unsupported application: Entry Point reports "invalid kernel", no dispatch */
        return EMV_ERR;
    }

    return emv_select_kernel(g_emv_cnxt->card_type, g_emv_cnxt->contactless_type);
}
