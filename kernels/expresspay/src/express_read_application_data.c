/**
************************************************************************************************************************
* @file     express_read_application_data.c
* @brief    Expresspay 内核：读应用数据（Read Application Data）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_read_application_data.c（逐行移植，只改符号前缀与共享服务调用）
* 符号约定：全部非 static 入口改名为 express_*（避免与 libentrypoint.a 的 180 个同名全局符号冲突）；
*           共享服务（appdata_* / emvtlv_* / emvicc_* / emv_*）直接引用 Entrypoint 已导出实现。
* 修改点（相对美运源码）：
*   - 平台/持久化调用改由 Entrypoint 的 EMV_PORTING（g_emv_port）与 appdata 服务提供；
*   - 美运私有 outcome_* 改为 express_outcome_*（美运 OUTCOME_FLAG=0 时只返回状态码）；
*   - appdata_save2 → express_appdata_save2（Entrypoint 未导出该函数）；
*   - 上下文 g_emv_cnxt → g_express_cnxt（内核私有 EXPRESS_CONTEXT，方案 §5.3）。
* 逐条差异见 doc/EXPRESSPAY_PORT_STATUS.md。
************************************************************************************************************************
*/
/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     express_read_application_data.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    读应用数据（Read Application Data）
*
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "express_priv.h"


/*
************************************************************************************************************************
*                                                     LOCAL DEFINES
************************************************************************************************************************
*/

#define TLVOBJ_BUFF_LEN 64

/*
************************************************************************************************************************
*                                                   LOCAL DATA TYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                    LOCAL CONSTANTS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                               LOCAL FUNCTION PROTOTYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                   LOCAL FUNCTIONS
************************************************************************************************************************
*/


static int express_date_valid(u8 * pdate)
{
	int year = 0, month = 0, day = 0;
	int i = 0;
	u8 mon[] = {31,28,31,30,31,30,31,31,30,31,30,31} ;

    eTRACE( "Call express_date_valid");
	for (i = 0; i < 3; i ++)
    {
		// check each digit shall be between 0 and 9 (including)
		if ((*(pdate + i) & 0xF0) > 0x90 || 	(*(pdate + i) & 0x0F) > 0x09)
        {
			return -1;
        }
	}

	// check month digits 1 - 12
	month = (*(pdate + 1) >> 4) * 10 + (*(pdate + 1) & 0x0F) ;
	if (month > 12 || month < 1)
    {
		return -1;
	}

	// check day digits 1 - 31
	day = (*(pdate + 2) >> 4 ) * 10 + (*(pdate + 2) & 0x0F) ;
	if (day > 31 || day < 1)
    {
		return -1 ;
	}

	year = (*pdate  >> 4 ) * 10 + (*pdate & 0x0F) ;
	year += ((*pdate >= 0x50 ? 19 : 20) * 100) ;

	if ((month == 2) && (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)))
    {
		if (day > 29)
        {
			return -1;
		}
	}
    else
    {
		if (day > mon[month - 1])
        {
			return -1;
		}
	}
	return 0;
}

static void express_create_un(void){
	u8 randpad[4];
	u8 *ptmp = NULL;

    unsigned char i;
    unsigned char *UnpredictNum, * EffectDate;
    unsigned char AppEffectDateexist;
    unsigned char a,b;

    unsigned char temp[4] = {0};

    unsigned short c,year;
	int len=0;

	if((EffectDate = appdata_find(0x5F25, &len)) != NULL)
	{
		UnpredictNum = appdata_find(TAG_9F37_UNPREDICATABLE_NUMBER, &len);
		a = UnpredictNum[0];
        a = a % 61;
        b = a/12;
        year = emv_bcd2dec(EffectDate,1);
        year -= b;
        b = a%12;
		c = emv_bcd2dec(EffectDate+1,1);
        if(b >= c)
        {
            year --;
            c = (12 + c) - b;
			emv_dec2bcd(c,EffectDate+1,1);
            //EMVBaseU16ToBcd(EffectDate+1, c, 1);
        }
        else
        {
            c -= b;
			emv_dec2bcd(c,EffectDate+1,1);
            //EMVBaseU16ToBcd(EffectDate+1, c, 1);
        }
		emv_dec2bcd(year,EffectDate,1);
        //EMVBaseU16ToBcd(EffectDate, year, 1);
        emv_memset(randpad,0,4);
        randpad[2] = EffectDate[0];
        randpad[3] = EffectDate[1];

		appdata_save(TAG_9F37_UNPREDICATABLE_NUMBER,randpad,4,OVERRIDED);
		//emvbase_avl_createsettagvalue(EMVTAG_UnpredictNum,randpad,4);
    }

}

int express_check_read_afl(void)
{
    unsigned short t, i, j;
    unsigned char AFL_Num;
    unsigned char *AFL;
	u8 * ptr = NULL;
	int len = 0;

	AFL = appdata_find(TAG_0094_AFL, &len);

    AFL_Num = len >> 2;

    if(AFL_Num == 0)
    {
		return rERR(IA_AFLERR);
    }

    for(i = 0; i < AFL_Num; i++)
    {
        j = i << 2;

//		GPO_PROC_71 request
//		if(AFL[j] & 0x07)
//		{
//			return rERR(IA_AFLFMTERROR);
//		}

        t = AFL[j];
        t >>= 3;

        if(t == 0 || t >= 31)
        {
			eTRACE("AMEX-error: SFI num invalid");
			return rERR(IA_RECORDNUMINVALID);
        }

        if(AFL[j + 1] < 1)
        {
			eTRACE("AMEX-error: first record num invalid");
			return rERR(IA_RECORDNUMINVALID);
        }

        if(AFL[j + 2] < AFL[j + 1])
        {
			eTRACE("AMEX-error: last record num is less than first record num");
			return rERR(IA_RECORDNUMINVALID);
        }

        if(AFL[j + 3] > (AFL[j + 2] - AFL[j + 1] + 1))
        {
			eTRACE("AMEX-error: oda data record num overflow");
			return rERR(IA_RECORDNUMINVALID);
        }
    }

    return EMV_OK;
}

static bool express_is_valid_date(u8 *date) /* YYMMDD(BCD) */
{
    int i;
    int year, month, day;
    u8  mon[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    for (i = 0; i < 3; i++)
    {
        /* check each digit shall be between 0 and 9 (including)*/
        if ((*(date + i) & 0xF0) > 0x90 || (*(date + i) & 0x0F) > 0x09)
        {
            return FALSE;
        }
    }

    /* check month digits 1 - 12 */
    month = (*(date + 1) >> 4) * 10 + (*(date + 1) & 0x0F);
    if (month > 12 || month < 1)
    {
        return FALSE;
    }

    /* check day digits 1 - 31 */
    day = (*(date + 2) >> 4) * 10 + (*(date + 2) & 0x0F);
    if (day > 31 || day < 1)
    {
        return FALSE;
    }

    year = (*date >> 4) * 10 + (*date & 0x0F);
    year += ((*date >= 0x50 ? 19 : 20) * 100);

    if ((month == 2) && (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0)))
    {
        if (day > 29)
        {
            return FALSE;
        }
    }
    else
    {
        if (day > mon[month - 1])
        {
            return FALSE;
        }
    }
    return TRUE;
}


static int express_check_data_validation(void)
{
	u8 *ptr = NULL;
	int len;

	ptr = appdata_find(TAG_5F24_APPLICATION_EXPIRATION_DATE, &len);
	if(len != 3)
	{
		eTRACE("AMEX-info: app expire date(5F24) len invalid");
		return EMV_FAIL;
	}

	if(express_is_valid_date(ptr) == FALSE)
	{
		eTRACE("AMEX-info: app expire date(5F24) format invalid");
		return EMV_FAIL;
	}

	ptr = appdata_find(TAG_5F25_APPLICATION_EFFECTIVE_DATE, &len);
	if(ptr != NULL)
	{
		if(len != 3)
		{
			eTRACE("AMEX-info: app effect date(5F25) len invalid");
			return EMV_FAIL;
		}

		if(express_is_valid_date(ptr) == FALSE)
		{
			eTRACE("AMEX-info: app effect date(5F25) format invalid");
			return EMV_FAIL;
		}
	}

	return EMV_OK;
}

static int express_check_read_record_mandatory_data(void)
{
	u32 tag_list[] = {
		TAG_005A_APPLICATION_PAN,
		TAG_5F24_APPLICATION_EXPIRATION_DATE,
		TAG_008C_CDOL_1,
	};

	int i, len;
	u8 *ptr = NULL;

	for(i = 0; i < sizeof(tag_list)/sizeof(tag_list[0]); i++)
	{
		ptr = appdata_find(tag_list[i], &len);
		if(ptr == NULL || len <= 0)
		{
			return EMV_FAIL;
		}
	}

	return EMV_OK;
}


#define EMV_CFG_AUTH_DATA_MAXLEN        1536                /* the maximum, see 1055 from 4.1e CASE, may be more */

int express_read_application_data(void)
{
    int i = 0, ret = 0, gret = 0;
    u8 *ptmp;
    int ilen;
    u8  sfi, first_recno, last_recno, da_recsum, recno;
    int rep_bitmap[31];
    u8  resp[EMV_CFG_ICC_RECV_MAXLEN];
    int rlen = 0;
    EMV_TLVOBJ tlvs[80];
    int pos;
    /* 静态 scratch（1.5KB）：避免栈帧超过 4KB 触发 __chkstk（host 门禁未放行该符号） */
    static u8 auth_data[EMV_CFG_AUTH_DATA_MAXLEN];
    int offset = 0;

    u8* pExpirationDate = NULL;
    int nExpirationDateLen = 0;
    u8* pPan = NULL;
    int nPanLen = 0;
    u8* pSeq = NULL;

    u8      pan[50];
    int     panlen;
	u8 buf[128] = {0};

//    g_express_cnxt->da_readrec_failed = FALSE;

    emv_memset(rep_bitmap, 0, sizeof(rep_bitmap));

    ptmp = appdata_find(TAG_0094_AFL, &ilen);

    if (ptmp == NULL || ilen == 0)
    {
        return rEND(EMV_ENDAPPLICATION);
    }

	ret = express_check_read_afl();
	if(ret != EMV_OK)
	{
		return ret;
	}

    while (i < ilen)
    {
        sfi = (*(ptmp + (i++)) >> 3) & 0x1F;
        first_recno = *(ptmp + (i++));
        last_recno = *(ptmp + (i++));
        da_recsum = *(ptmp + (i++));

        if (sfi < 1 || sfi > 30)
        {
            return rERR(RD_AFLILL);
        }
        if (first_recno == 0)
        {
            return rERR(RD_AFLILL);
        }
        if (first_recno > last_recno || da_recsum > (last_recno - first_recno + 1))
        {
            return rERR(RD_AFLILL);
        }
        for (recno = first_recno; recno <= last_recno; recno++)
        {
            ret = emvicc_read_record(sfi, recno, resp, &rlen);
			if(ret == 0x0000)
			{
				return EMV_APDU_TIMEOUT;
			}
		    else if(ret != 0x9000)
            {
                return express_outcome_request_another_payment();
            }

            if (sfi <= 10)
            {
                ret = emvtlv_decode(resp, rlen, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ);
                if (ret < 0 || resp[0] != TAG_0070_AEF_DATA_TEMPLATE)
                {
					return express_outcome_request_another_payment();
                    return rERR(RD_RECORDILL);
                }
                if (!(rep_bitmap[sfi] & (1 << (recno - 1))))
                {
                    rep_bitmap[sfi] |= (1 << (recno - 1));

                    for (pos = 2; pos <= tlvs[0].childnum; pos++)
                    {
                        if (tlvs[pos].childnum >= 0
                                || tlvs[pos].tagname == 0x9F06 || tlvs[pos].tagname == 0x9F34
                                || tlvs[pos].tagname == 0x9F15 || tlvs[pos].tagname == 0x9F16
                                || tlvs[pos].tagname == 0x9F4E || tlvs[pos].tagname == 0x9F39
                                || tlvs[pos].tagname == 0x9F1C || tlvs[pos].tagname == 0x9F33
                                || tlvs[pos].tagname == 0x95   || tlvs[pos].tagname == 0x9A
                                || tlvs[pos].tagname == 0x9B   || tlvs[pos].tagname == 0x9C
                                || tlvs[pos].tagname == 0x9F37 || tlvs[pos].tagname == 0x91
                                || tlvs[pos].tagname == 0x8A   || tlvs[pos].tagname == 0x4F
                                || tlvs[pos].tagname == 0x50   || tlvs[pos].tagname == 0x87
                                || tlvs[pos].tagname == 0x9F12 || tlvs[pos].tagname == 0x9F02
                                || tlvs[pos].tagname == 0x81
                                || tlvs[pos].tagname == 0x9F66 || tlvs[pos].tagname == 0x9F7A || tlvs[pos].tagname == 0x9F7B
                                || (tlvs[pos].tagname & 0xFFF0) == 0x00C0
                                || (tlvs[pos].tagname & 0xFF00) == 0xDF00)
                        {
                            continue;
                        }
                        if (express_appdata_save2(tlvs + pos, UNIQUE) < 0)
                        {
							eTRACE("AMEX-error: read dumplicate data");
							return express_outcome_request_another_payment();
                        }
                        else
                        {

                        }
                    }
                }
            }

            if (da_recsum)
            {
                eTRACE("g_express_cnxt->da_readrec_failed:%d resp[0]:%02x ", g_express_cnxt->da_readrec_failed, resp[0]);
                if (g_express_cnxt->da_readrec_failed == FALSE)
                {
                    if (resp[0] != 0x70)
                    {
                        g_express_cnxt->da_readrec_failed = TRUE;
                    }
                    else
                    {
                        if (sfi <= 10)
                        {
                            /* For files with SFI in the range 1 to 10, the record tag ('70') and the record length
                             are excluded from the offline data authentication process. */
                            emv_memcpy(auth_data + offset, tlvs[1].pvalue, tlvs[1].valuelen);
                            offset += tlvs[1].valuelen;
                        }
                        else
                        {
                            /* For files with SFI in the range 11 to 30, the record tag ('70') and the record length
                             are not excluded from the offline data authentication process. */
                            emv_memcpy(auth_data + offset, resp, rlen);
                            //offset += (rlen-1);
                            offset += rlen;
                        }
                    }
                }
                da_recsum--;
            }
        } /* for (recno = first_recno; recno <= last_recno; recno++) */
    } /* while (i < ilen) */

	if(EMV_FAIL == express_check_read_record_mandatory_data())
	{
		return express_outcome_request_another_payment();
	}

	if(EMV_FAIL == express_check_data_validation())
	{
		return express_outcome_request_another_payment();
	}

//	eTRACE_HEX("AMEX-info: auth_data in read record", auth_data, offset);
	eTRACE("AMEX-info:(for Debug only) g_express_cnxt->da_readrec_failed = %d", g_express_cnxt->da_readrec_failed);
    if (g_express_cnxt->da_readrec_failed == FALSE)
    {
        appdata_save(TAG_AUTH_DATA, auth_data, offset, UNIQUE);
    }

    if(g_express_cnxt->msdoremv == MSD_MODE)
    {
    	if (!appdata_find(TAG_0057_TRACK2_EQUIVALENT_DATA, &ilen) || ilen == 0 ||
			!appdata_find(TAG_5F25_APPLICATION_EFFECTIVE_DATE, &ilen) || ilen == 0 ||
			!appdata_find(TAG_5F20_CARDHOLDER_NAME, &ilen) || ilen == 0)
    	{
        	eTRACE("AMEX-info: MSD miss 57|5F25|5F20");
			return rERR(RD_ERR);
    	}
		ret = emvicc_get_data(TAG_9F36_ATC, buf, &ilen);

        if(ret == 0)
        {
        	eTRACE("AMEX-info: MSD get ATC communicate error");
			return rERR(RD_ERR);
        }
        if(ret == 0x9000)
        {
        	eTRACE("AMEX-info: MSD get ATC command ret = %d", ret);
            if(!emv_memcmp(buf,"\x9F\x36",2) && buf[2]==2)// ATC found
            {
                appdata_save(TAG_9F36_ATC,(unsigned char*)&buf[3],2,OVERRIDED);
            }
            else
            {
            	eTRACE("AMEX-error: MSD get ATC len invliad");
				return rERR(RD_MSD_ATC_LENERR);
            }
        }
        else
        {
        	eTRACE("AMEX-error: MSD get ATC SW != 9000");
			return rERR(RD_ERR);
        }

		express_create_un();
    }


    return rNXT(EMV_STA_READ_APP_DATA_COMPLETED);
}


/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
