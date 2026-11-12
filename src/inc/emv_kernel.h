/**
************************************************************************************************************************
*                                     COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emv_kernel.h
* @brief    Contactless kernel registration interface (Entry Point  <->  kernels)
*
* Decoupling contract:
*   - libentrypoint.a MUST NOT reference any kernel symbol.
*   - Every kernel library (AMEX/Expresspay, Mastercard, Visa, JCB, Discover,
*     PBOC, PURE ...) links libentrypoint.a and registers its ops at startup
*     (application calls EMV_RegisterKernel() before EMV_Init()/EMV_Process()).
*   - The Entry Point core dispatches init/preprocess/process through the
*     registered table only.
*
* ASCII-only header on purpose (source tree mixes GBK and damaged UTF-8).
************************************************************************************************************************
*/
#ifndef EMV_KERNEL_H_
#define EMV_KERNEL_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* EMVCo kernel identifiers ([9F2A] first byte / Entry Point combination selection) */
#define EMV_KERNEL_ID_UNKNOWN          0x00
#define EMV_KERNEL_ID_MASTERCARD       0x02
#define EMV_KERNEL_ID_VISA             0x03
#define EMV_KERNEL_ID_AMEX             0x04
#define EMV_KERNEL_ID_JCB              0x05
#define EMV_KERNEL_ID_DISCOVER         0x06
#define EMV_KERNEL_ID_PBOC             0x07
#define EMV_KERNEL_ID_PURE             0x2D

#define EMV_KERNEL_MAX                 8

/**
 * @brief Kernel operation table, provided by each kernel library.
 * @note  All function pointers are mandatory except 'name'.
 */
typedef struct
{
    uint8_t     kernel_id;                       /* one of EMV_KERNEL_ID_xxx */
    const char* name;                            /* diagnostic name */
    int32_t (*init)(void);                       /* kernel init (reset context) */
    int32_t (*preprocess)(bool force_online);    /* Entry Point pre-processing hand-off */
    int32_t (*process)(void);                    /* kernel state machine step */
} EMV_KERNEL_OPS;

/**
 * @brief  Register a kernel. Idempotent for the same kernel_id (replaces entry).
 * @return 0 on success, -1 on invalid ops / table full.
 */
int32_t EMV_RegisterKernel(const EMV_KERNEL_OPS* ops);

/**
 * @brief  Look up a registered kernel (NULL when not registered).
 */
const EMV_KERNEL_OPS* EMV_FindKernel(uint8_t kernel_id);

/**
 * @brief  Unregister all kernels (used by EMV_Exit / test harness).
 */
void EMV_ResetKernels(void);

/**
 * @brief  Execute one step of the currently selected kernel.
 * @return kernel status code, or EMV_ERR when no kernel is selected/registered.
 */
int32_t EMV_KernelProcess(void);

/**
 * @brief  Kernel selected by the last successful combination selection.
 */
uint8_t EMV_GetSelectedKernel(void);

/* Internal: called by emv_select_kernel() after a successful selection. */
void EMV_SetSelectedKernel(const EMV_KERNEL_OPS* ops);

#ifdef __cplusplus
}
#endif

#endif /* EMV_KERNEL_H_ */
