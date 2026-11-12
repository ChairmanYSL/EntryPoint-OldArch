/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     ks_sdk.h
* @author   Android Development Team
* @version  v2.01
* @date     2015年06月12日
* @author 	zhongfuchang
* @brief    The most important(Event operation) API of KOS-SDK
*
************************************************************************************************************************
*/
#ifndef _KS_SDK_H_
#define _KS_SDK_H_

//◎调试输出
	
	/**
	 * 调试输出类型定义
	 */
	enum DBG_TYPE
	{
		DBG_NONE		=0,					//!< 无调试输出	
		DBG_CON			=1,					//!< 调试输出到控制台	
		DBG_MEM			=2,					//!< 调试输出到内存	
		DBG_UART_A		=3,					//!< 调试输出到串口A		KOS有效
		DBG_UART_B		=4,					//!< 调试输出到串口B		KOS有效
		DBG_USB_CDC		=5,					//!< 调试输出到USB串口
		DBG_NET			=6,					//!< 调试输出到网络
		DBG_FILE		=7,					//!< 调试输出到文件	
		DBG_TFLASH		=8,					//!< 调试输出到存储卡	
		DBG_TYPE_MAX
	};

	/**
	 * 调试及被定义
	 */
	enum DBG_LEVEL
	{
		DBG_APP_INFO		=0x01,				//!< 普通调试信息		
		DBG_APP_WARMING		=0x02,				//!< 警告调试信息		
		DBG_APP_ERROR		=0x04,				//!< 错误调试信息	
		DBG_APP_SECRECY		=0x08,				//!< 保密调试信息	
		DBG_APP_1			=0x10,				//!< 用户调试信息分组1		如UI部分
		DBG_APP_2			=0x20,				//!< 用户调试信息分组2		如流程码处理
		DBG_APP_3			=0x40,				//!< 用户调试信息分组3		如通信部分处理
		DBG_APP_4			=0x80,				//!< 用户调试信息分组4		如电话功能库
		DBG_APP_MASK		=0xFF				//!< 应用调试信息	
	};	//VER:1.1

	//-----------------------------------------------------------------------------------------------------------------------
//◎时间

	typedef struct
	{
		int	  Year;		// The number of years						range 1900 to xxxx
		int	  Month;	// The number of months, 					range 1 to 12
		int	  Day;		// The day of the month, 					range 1 to 31
		int	  Hour;		// The number of hours past midnight, 		range 0 to 23
		int	  Min;		// The number of minutes after the hour, 	range 0 to 59
		int	  Sec;		// The number of seconds after the minute,  range 0 to 59, (60 to allow for leap seconds)
		int	  MSec;		// The number of MSec after the minute, 	range 
		int	  WeekDay; 	// The number of days since Sunday,    		range 0 to 6
		int	  YearDay; 	// The number of days since January 1, 		range 0 to 365
	}DATE_TIME;
	
	
		//<FUN> 获取系统时间
		//<OUP> pTime			:   DATE_TIME
	extern void API_TimeGet(DATE_TIME* pTime);		
	
		//<FUN> 设置系统时间
		//<INP> pTime			:   DATE_TIME
	extern void API_TimeSet(DATE_TIME* pTime);		
	
	
		//<FUN> 获取当前毫秒数
		//<RET> int				:   单位:豪秒
	extern int  API_TimeCurrMS(void);				
	
		//<FUN> 获取当前秒数
		//<RET> int				:   单位:秒
	extern int  API_TimeCurrSec(void);				
	
		//<FUN> 睡眠指定时间
		//<INP> tTime			:   睡眠时间,单位:微秒
	extern void API_TimeSleepUS(u32 tTime);			
	
		//<FUN> 睡眠指定时间
		//<INP> tTime			:   睡眠时间,单位:豪秒
	extern void API_TimeSleepMS(u32 tTime);			
	
		//<FUN> 睡眠指定时间
		//<INP> tTime			:   睡眠时间,单位:秒
	extern void API_TimeSleepS(u32 tTime);			

//-----------------------------------------------------------------------------------------------------------------------
//◎文件系统

	
#endif
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/

