/**
************************************************************************************************************************
* @file     express_issuer_script_processing.c
* @brief    Expresspay 内核：发卡行脚本处理（v4.2 明确不支持 → 显式拒绝）
*
* 规范依据：
*   - Expresspay Terminal Technical Specification v4.2, P p76 §16：
*     "Issuer Script Processing — Not supported in this specification."
*   - 连带不支持：§13 Issuer Authentication（p73）、§14 2nd Terminal Action Analysis（p74）、
*     §15 2nd Card Action Analysis（p75）。
*   - 71/72 模板在 v4.2 中无处理要求；授权响应中的脚本由 Entrypoint 用 EP 私有 tag 保存，
*     内核不得执行、也不得改写为内核私有 tag（旧移植的 DF8100/DF8200 重标记已删除）。
*
* 保留本文件的目的：给历史调用点一个显式"不支持"的落点，避免链接期静默消失或误以为脚本已处理。
* 返回 <0 表示拒绝执行；*reslen 恒为 0（不产生任何脚本结果）。
************************************************************************************************************************
*/
#include "express_priv.h"

int express_script_process(int when, u8 *res, int *reslen)
{
    (void)res;

    if (reslen != NULL)
    {
        *reslen = 0;
    }

    eTRACE("AMEX-info: issuer script (when=%d) rejected: not supported by Expresspay v4.2 (P p76 §16)", when);

    return -1;
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
