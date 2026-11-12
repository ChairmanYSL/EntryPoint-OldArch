/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     ks_type.h
* @author   Android Development Team
* @version  v2.01
* @date     2015年06月12日
* @author 	wangyun,zhongfuchang
* @brief    The Date Type and Global Macro Definition.
*
************************************************************************************************************************
*/
#ifndef __KS_OS_SDK_API_TYPE_H__
#define __KS_OS_SDK_API_TYPE_H__

#include "stdbool.h"

//-----------------------------------------------------------------------------------------------------------------------
	typedef signed char				s8;     //!< Define s8 编译器决定char为必unsigned，所以须使用signed定义
	typedef unsigned char   		u8;		//!< Define u8

	typedef signed short			s16;	//!< Define s16
	typedef unsigned short  		u16;	//!< Define u16

	typedef signed long				s32;	//!< Define s32
	typedef unsigned long			u32;	//!< Define u32

	typedef unsigned long long      u64;	//!< Define u64

	//! Define NULL pointer value and the offset() macro
	#ifndef NULL
		#define	NULL				(0)
	#endif

	//! Define true
	#ifndef TRUE
		#define	TRUE				(1)
	#endif

	//! Define false
	#ifndef FALSE
		#define	FALSE				(0)
	#endif

	//! Define yes
	#ifndef YES
		#define	YES					(1)
	#endif

	//! Define no
	#ifndef NO
		#define	NO					(0)
	#endif

	//! Define on
	#ifndef ON
		#define	ON					(1)
	#endif

	//! Define off
	#ifndef OFF
		#define	OFF					(0)
	#endif

	//! 掩码定义　n : 掩码位数　s : 掩码偏移位 MASK(3,3) = 0x38 _MASK(3,0) = 0x07
	#define MASK(n,s)  (((1<<(s))-1)<<(n))

	//! a和b 返回小的数值
	#define MIN(a,b) 		(((a) < (b)) ? (a) : (b))

	//! a和b 返回大的数值
	#define MAX(a,b) 		(((a) > (b)) ? (a) : (b))

	//! 无符号16位大小端转换
	#define SWAP_U16(c)		(((c>>8)&0xFF)|((c&0xFF)<<8))

	//! 无符号32位大小端转换
	#define	SWAP_U32(v)		(((((v)>>0)&0xFF)<<24)|((((v)>>8)&0xFF)<<16)|((((v)>>16)&0xFF)<<8)|(((v)>>24)&0xFF))

	//! 初始化BUFFER, st需要支持sizeof判断大小
	#define	CLEAR(st)		API_memset((void*)&(st),0,sizeof(st));
//-----------------------------------------------------------------------------------------------------------------------

	#define RET_OK									(0)							//!< 成功
	#define	RET_ERR									(-1)						//!< 失败

#endif

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
