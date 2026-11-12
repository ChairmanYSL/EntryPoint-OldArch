/**
************************************************************************************************************************
*                                     COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_kernel.c
* @brief    Contactless kernel registry (decoupling seam between Entry Point and kernels)
*
* This file is the ONLY place where the Entry Point core talks to a kernel: it never
* references a concrete kernel symbol, only the registered operation table.
************************************************************************************************************************
*/
#include "emv_kernel.h"
#include "emv_type.h"

static const EMV_KERNEL_OPS* g_kernel_table[EMV_KERNEL_MAX];
static uint8_t               g_kernel_count = 0;
static const EMV_KERNEL_OPS* g_current_kernel = 0;

int32_t EMV_RegisterKernel(const EMV_KERNEL_OPS* ops)
{
    uint8_t i;

    if (ops == 0 || ops->init == 0 || ops->preprocess == 0 || ops->process == 0)
    {
        return -1;
    }

    for (i = 0; i < g_kernel_count; i++)
    {
        if (g_kernel_table[i]->kernel_id == ops->kernel_id)
        {
            g_kernel_table[i] = ops;
            return 0;
        }
    }

    if (g_kernel_count >= EMV_KERNEL_MAX)
    {
        return -1;
    }

    g_kernel_table[g_kernel_count++] = ops;
    return 0;
}

const EMV_KERNEL_OPS* EMV_FindKernel(uint8_t kernel_id)
{
    uint8_t i;

    for (i = 0; i < g_kernel_count; i++)
    {
        if (g_kernel_table[i]->kernel_id == kernel_id)
        {
            return g_kernel_table[i];
        }
    }
    return 0;
}

void EMV_ResetKernels(void)
{
    uint8_t i;

    for (i = 0; i < EMV_KERNEL_MAX; i++)
    {
        g_kernel_table[i] = 0;
    }
    g_kernel_count = 0;
    g_current_kernel = 0;
}

int32_t EMV_KernelProcess(void)
{
    if (g_current_kernel == 0 || g_current_kernel->process == 0)
    {
        return EMV_ERR;
    }
    return g_current_kernel->process();
}

uint8_t EMV_GetSelectedKernel(void)
{
    return (g_current_kernel != 0) ? g_current_kernel->kernel_id : EMV_KERNEL_ID_UNKNOWN;
}

/* Internal: called by emv_select_kernel() after a successful combination selection. */
void EMV_SetSelectedKernel(const EMV_KERNEL_OPS* ops)
{
    g_current_kernel = ops;
}

void EMV_KernelPostResult(int32_t kind, int32_t result, const uint8_t *data, int32_t len)
{
    if (g_current_kernel != 0 && g_current_kernel->post_result != 0)
    {
        g_current_kernel->post_result(kind, result, data, len);
    }
}

void EMV_KernelExit(void)
{
    if (g_current_kernel != 0 && g_current_kernel->exit != 0)
    {
        g_current_kernel->exit();
    }
}

int32_t EMV_GetKernelOutcome(EMV_KERNEL_OUTCOME *out)
{
    if (out == 0)
    {
        return -1;
    }
    if (g_current_kernel == 0 || g_current_kernel->get_outcome == 0)
    {
        return -1;
    }
    return g_current_kernel->get_outcome(out);
}
