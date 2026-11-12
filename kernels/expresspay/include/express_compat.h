/**
************************************************************************************************************************
* @file     express_compat.h
* @brief    Expresspay 内核移植兼容层（短类型名 / 编译器属性 / 平台调用收敛点）
*
* 用途：让从美运 V4.2.3 平移过来的 .c 文件尽量少改动即可编译，同时把"必须替换成
*       Entrypoint 服务"的调用集中在本文件暴露的宏里，避免散落各处难以审计。
*
* 只允许放：类型别名、属性宏、纯内存/字符串工具宏。
* 不允许放：文件 IO、参数持久化、SDK 调用（那些必须改成 Entrypoint 的 emv_* / emvporting_* 服务）。
************************************************************************************************************************
*/
#ifndef EXPRESS_COMPAT_H_
#define EXPRESS_COMPAT_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * 短类型名（美运源码 emvi.h 依赖）
 * ------------------------------------------------------------------------- */
#ifndef EXPRESS_COMPAT_TYPES_DEFINED
#define EXPRESS_COMPAT_TYPES_DEFINED
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  s32;
#endif

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef ENABLE
#define ENABLE   1
#endif
#ifndef DISABLE
#define DISABLE  0
#endif
#ifndef UNIQUE
#define UNIQUE    0   /* appdata_save 覆盖标志：不覆盖（emvi.h:581） */
#endif
#ifndef OVERRIDED
#define OVERRIDED 1   /* appdata_save 覆盖标志：覆盖（emvi.h:582） */
#endif

/* ---------------------------------------------------------------------------
 * 编译器/链接属性（原 emvspi.c:285-291）
 * ------------------------------------------------------------------------- */
#if defined(__GNUC__)
#define EXPRESS_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define EXPRESS_WEAK __weak
#else
#define EXPRESS_WEAK
#endif

/* ---------------------------------------------------------------------------
 * 纯内存工具：直接映射到 Entrypoint 已导出的实现（emvi.h:278-281），不得再本地定义
 * ------------------------------------------------------------------------- */
#define API_memcpy   emv_memcpy
#define API_memset   emv_memset
#define API_memcmp   emv_memcmp
#define API_strlen   emv_strlen

/* ---------------------------------------------------------------------------
 * 必须替换（不可直接映射）的美运调用清单 —— 保留宏名但故意映射到不存在的符号，
 * 让编译期立即暴露尚未改写的调用点（移植阶段"红名单"）。
 *
 *   API_fopen/fread/fwrite/fseek/fclose/fsize/fremove  → Entrypoint emvporting_* 参数服务
 *   API_strcpy/API_sprintf/API_Conv_StrToBcd_Left     → libc / emv 工具
 *   API_ICC_Apdu                                       → Entrypoint emvicc_*（经 porting icc_rw）
 *   API_Crypt/API_Hash/API_Rng/API_TimeGet             → Entrypoint porting recover/hash/get_random/get_trans_date_time
 * ------------------------------------------------------------------------- */
#ifdef EXPRESS_AUDIT_UNPORTED
/* 打开 EXPRESS_AUDIT_UNPORTED 后，残留的平台调用会编译失败，便于逐个清零：
 *   clang -DEXPRESS_AUDIT_UNPORTED ...
 * 语法：define 成函数名会因"隐式声明"被 -Werror=implicit-function-declaration 拦截，
 * 这里显式给出错误属性以获得更好的报错信息。 */
#define API_fopen    EXPRESS_PORT_ME_API_fopen
#define API_fread    EXPRESS_PORT_ME_API_fread
#define API_fwrite   EXPRESS_PORT_ME_API_fwrite
#define API_fseek    EXPRESS_PORT_ME_API_fseek
#define API_fclose   EXPRESS_PORT_ME_API_fclose
#define API_fsize    EXPRESS_PORT_ME_API_fsize
#define API_fremove  EXPRESS_PORT_ME_API_fremove
#define API_strcpy   EXPRESS_PORT_ME_API_strcpy
#define API_sprintf  EXPRESS_PORT_ME_API_sprintf
#define API_ICC_Apdu EXPRESS_PORT_ME_API_ICC_Apdu
#define API_Crypt    EXPRESS_PORT_ME_API_Crypt
#define API_Hash     EXPRESS_PORT_ME_API_Hash
#define API_Rng      EXPRESS_PORT_ME_API_Rng
#define API_TimeGet  EXPRESS_PORT_ME_API_TimeGet
#endif /* EXPRESS_AUDIT_UNPORTED */

/* ---------------------------------------------------------------------------
 * 日志：Entrypoint 使用 EMV_PORTING.trace / trace_hex（emv.h:49,59）
 * 移植时把 eTRACE/eTRACE_HEX 映射到 Entrypoint 的 porting 日志，禁止再定义 eTRACE 宏
 * ------------------------------------------------------------------------- */
#define EXPRESS_TRACE(...)      do { } while (0)   /* 由 express_debug.h 按需实现，骨架阶段空实现 */

#endif /* EXPRESS_COMPAT_H_ */
