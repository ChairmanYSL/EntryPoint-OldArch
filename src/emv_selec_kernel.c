/**
************************************************************************************************************************
*                                     COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_selec_kernel.c
* @brief    Kernel selection (Entry Point side) - registry based
*
* The Entry Point core does not reference any contactless kernel symbol: kernels
* register an EMV_KERNEL_OPS table (see emv_kernel.h) and this module selects and
* starts the one required by the combination selection result.
************************************************************************************************************************
*/
#include "sdk.h"
#include "emv.h"
#include "emvi.h"
#include "emv_kernel.h"

/* Contactless-type (legacy Entry Point context) <-> EMVCo kernel id mapping */
static uint8_t contactless_to_kernel_id(uint8_t contactless_type)
{
    switch (contactless_type)
    {
        case EMV_CONTACTLESSTYPE_QMSDC: return EMV_KERNEL_ID_MASTERCARD;
        case EMV_CONTACTLESSTYPE_QVSDC: return EMV_KERNEL_ID_VISA;
        case EMV_CONTACTLESSTYPE_AMEX:  return EMV_KERNEL_ID_AMEX;
        case EMV_CONTACTLESSTYPE_JCB:   return EMV_KERNEL_ID_JCB;
        case EMV_CONTACTLESSTYPE_DISV:  return EMV_KERNEL_ID_DISCOVER;
        case EMV_CONTACTLESSTYPE_QPBOC: return EMV_KERNEL_ID_PBOC;
        case EMV_CONTACTLESSTYPE_QPURE: return EMV_KERNEL_ID_PURE;
        default:                        return EMV_KERNEL_ID_UNKNOWN;
    }
}

uint8_t emv_kernel_id_to_contactless(uint8_t kernel_id)
{
    switch (kernel_id)
    {
        case EMV_KERNEL_ID_MASTERCARD: return EMV_CONTACTLESSTYPE_QMSDC;
        case EMV_KERNEL_ID_VISA:       return EMV_CONTACTLESSTYPE_QVSDC;
        case EMV_KERNEL_ID_AMEX:       return EMV_CONTACTLESSTYPE_AMEX;
        case EMV_KERNEL_ID_JCB:        return EMV_CONTACTLESSTYPE_JCB;
        case EMV_KERNEL_ID_DISCOVER:   return EMV_CONTACTLESSTYPE_DISV;
        case EMV_KERNEL_ID_PBOC:       return EMV_CONTACTLESSTYPE_QPBOC;
        case EMV_KERNEL_ID_PURE:       return EMV_CONTACTLESSTYPE_QPURE;
        default:                       return EMV_CONTACTLESSTYPE_UNKNOWN;
    }
}

int32_t emv_select_kernel(uint8_t card_type, uint8_t emv_core)
{
    const EMV_KERNEL_OPS* ops;
    uint8_t kernel_id;
    int32_t ret;

    if (g_emv_cnxt == 0)
    {
        return EMV_UNINITIALIZED;
    }

    if (card_type == EMV_CARD_CONTACT)
    {
        kernel_id = EMV_KERNEL_ID_PBOC;
    }
    else
    {
        kernel_id = contactless_to_kernel_id(emv_core);
    }

    if (kernel_id == EMV_KERNEL_ID_UNKNOWN)
    {
        return EMV_ERR;
    }

    ops = EMV_FindKernel(kernel_id);
    if (ops == 0)
    {
        /* Kernel not linked/registered: Entry Point must report "invalid kernel",
         * never call a NULL function pointer. */
        return EMV_ERR;
    }

    ret = ops->init();
    if (ret != EMV_OK)
    {
        return ret;
    }

    ret = ops->preprocess(g_emv_cnxt->force_online != 0);
    if (ret != EMV_OK)
    {
        return ret;
    }

    EMV_SetSelectedKernel(ops);
    return EMV_OK;
}
