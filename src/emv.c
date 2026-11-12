#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     emv.c
* @author   Kingsee Development Team xinyong
* @version  V43
* @date     2015��10��21��
* @brief    EMV�ں�
*
************************************************************************************************************************
*/

/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "emvi.h"
#include "emv_kernel.h"


/*
************************************************************************************************************************
*                                                     LOCAL DEFINES
************************************************************************************************************************
*/

/**
 * �ں˰汾
 *
 * ��ʽ��VX.M.N��
 X��ʾEMV�淶�汾�Ͳ����汾��
 M��ʾ���汾��0-99����
 N��ʾ�Ӱ汾��1-99��
 */
//#define  EMV_VERSION     "KSEMVCore_V43E1.1.5"

#define  EMV_VERSION     "KSEMVCore_V2.2.0"

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

static const uint32_t crc_table[] =
{
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
    0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
    0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
    0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
    0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
    0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
    0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
    0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
    0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
    0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
    0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
    0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
    0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
    0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
    0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
    0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
    0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
    0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
    0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
    0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
    0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
    0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
    0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
    0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
    0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
    0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
    0x2d02ef8dL
};

#define DO1(buf) crc = crc_table[((int32_t)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf) DO1(buf); DO1(buf);
#define DO4(buf) DO2(buf); DO2(buf);
#define DO8(buf) DO4(buf); DO4(buf);
#define TLVBUFFER_SIZE (10*1024)


//const EMV_AIDPARAM k_emv_aidparam =
//{
//    .term.ics = "\xF4\xF0\xF0\xFC\xAF\xFE\xA0",//\xF4\xF0\xF8\xFC\xAF\xFE\xA0
//    .term.term_type = 0x22,
//	.term.term_id = "",
//	.term.term_ifd = "",
//	.term.term_country_code = "\x01\x56",
//	.term.trans_currency_code = "\x01\x56",
//	.term.trans_currency_exponent = 2,
//    .term.status_check_support_indicator = 1,
//	.term.acquirer_id = "",
//	.term.merchant_category_code = "\x00\x00",
//	.term.merchant_id = "",
//	.term.merchant_id_exist = false,
//	.term.merchant_id_len = 0,
//	.term.merchant_name = "",
//
//    .aid = "\xA0\x00\x00\x03\x33",
//    .aid_len = 5,
//    .app_sel_indicator = 0,
//    .app_ver = "\x00\x8C",
//	.term_caps = "\xA0\xF8\xC8",
//	.ttq = "\x76\x00\x00\x80",
//    .target_percent = 20,
//    .max_target_percent = 50,
//    .threshold_value = "\x00\x00\x01\xF4", //5.00
//	.term_add_caps = "\xFF\x80\xF0\xF0\x01",
//    .default_ddol_len = 11,
//    .default_ddol = "\x9F\x37\x04\x9F\x47\x01\x8F\x01\x9F\x32\x01",
//    .default_tdol_len = 3,
//    .default_tdol = "\x9F\x08\x02",
//    .trans_ref_currency_code = "\x01\x56",
//    .trans_ref_currency_exponent = 2,
//    .term_action_code_default_exist = 0,
//    .term_action_code_default = "",
//    .term_action_code_online_exist = 0,
//    .term_action_code_online = "",
//    .term_action_code_denial_exist = 0,
//    .term_action_code_denial = "",
//    .term_risk_mana_data_len = 0,
//    .term_risk_mana_data = "",
//    .online_pin_support_indicator = 1,
//
//    .term_floor_limit = "\x00\x00\x03\xE8", //10.00
//	.cl_trans_limit_exist = 1,
//	.cl_trans_limit = "\x00\x00\x00\x00\x80\x00",
//	.cl_floor_limit_exist = 1,
//	.cl_floor_limit = "\x00\x00\x00\x00\x00\x00",
//	.cvm_limit_exist = 1,
//	.cvm_limit = "\x00\x00\x00\x00\x08\x00",
//	.reader_cl_trans_limit_cdcvm = "\x00\x00\x00\x00\x08\x00",
//	.reader_cl_trans_limit_cdcvm_exist = true,
//};



/*
************************************************************************************************************************
*                                                LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/

	  EMV_CONTEXT* 	g_emv_cnxt	= 0;
const EMV_PORTING* 	g_emv_port	= 0;


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


/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/

static uint32_t emv_crc32(uint32_t crc, uint8_t *buf, int32_t len)
{
    if (buf == NULL)
    {
        return 0;
    }

    crc = crc ^ 0xffffffff;

    while (len >= 8)
    {
        DO8(buf);
        len -= 8;
    }

    if (len)
    {
        do
        {
            DO1(buf);
        } while (--len);
    }

    return crc ^ 0xffffffff;
}

int32_t emv_memcmp(const void *dst, const void *src, uint32_t count)
{
    register int32_t r;
    register const uint8_t *d = dst;
    register const uint8_t *s = src;

    ++count;
    while (--count)
    {
        if ((r = (*d - *s)))
        {
            return r;
        }
        ++d;
        ++s;
    }

    return 0;
}

void *emv_memcpy(void *dst, const void *src, uint32_t n)
{
    void *res = dst;
    uint8_t *c1, *c2;

    c1 = (uint8_t *)dst;
    c2 = (uint8_t *)src;
    while (n--)
    {
        *c1++ = *c2++;
    }

    return (res);
}

void *emv_memset(void *dst, int32_t s, uint32_t count)
{
    register char *a = dst;

    count++; /* this actually creates smaller code than using count-- */
    while (--count)
    {
        *a++ = s;
    }

    return dst;
}

uint32_t emv_strlen(const char *s)
{
    register uint32_t i;
    if (!s) return 0;
    for (i=0; *s; ++s) ++i;
    return i;
}


static uint8_t hex2asc(uint8_t hex)
{
    if (hex > 0xF) {
        return ' ';
    } else if (hex < 10) {
        return hex + '0';
    } else {
        return hex + 'A' - 10;
    }
}

static uint8_t asc2hex(uint8_t asc)
{
    if (asc >= '0' && asc <= '9')
        return (asc - '0');
    else if (asc >= 'a' && asc <= 'f')
        return (asc - 'a' + 10);
    else if (asc >= 'A' && asc <= 'F')
        return (asc - 'A' + 10);
    else
        return 0;
}

/**
 * @brief BCD����ת���ַ���
 * @param bcd       BCD����
 * @param bcdlen    BCD���ݳ���
 * @param str       ����ת������ַ���
 */
void emv_bcd2str(uint8_t *bcd, int32_t bcd_len, uint8_t *str)
{
    int32_t i, j;

    for (i = 0, j = 0; i < bcd_len; i++)
    {
        str[j++] = hex2asc((bcd[i] >> 4) & 0x0F);
        str[j++] = hex2asc((bcd[i]) & 0x0F);
    }
    str[j++] = '\0';
}

/**
 * ASC�ַ���ת��BCD���ݴ�
 * @param str       ASC�ַ���ָ��
 * @param str_len   ASC�ַ�������
 * @param bcd       ����BCD���ݴ��Ļ���ָ��
 * @param bcd_len   ����BCD���ݴ��Ļ��泤��
 * @param ralign    �Ҷ�����
 */
void emv_str2bcd(uint8_t *str, int32_t str_len, uint8_t *bcd, int32_t bcd_len, bool ralign)
{
    int32_t i, sta;

    if (bcd_len < ((str_len + 1) / 2))
    {
        str_len = 2 * bcd_len;
    }

    if (ralign)
    {
        sta = bcd_len - (str_len + 1) / 2;
    }
    else
    {
        sta = 0;
    }

    /* ASC�ַ�������Ϊ�����Ĵ��� */
    if (str_len & 0x01)
    {
        if (ralign)
        {
            bcd[sta] &= 0xF0;
            bcd[sta] += asc2hex(str[0]);
            str += 1;
            sta += 1;
        }
        else
        {
            bcd[sta + str_len / 2] &= 0x0F;
            bcd[sta + str_len / 2] += (asc2hex(str[str_len - 1]) << 4) & 0xF0;
        }
        str_len -= 1;
    }

    for (i = 0; i < str_len; i += 2)
    {
        bcd[i / 2 + sta] = ((asc2hex(str[i]) << 4) | (asc2hex(str[i + 1])));
    }
}

void emv_dec2hex(uint32_t dec, uint8_t *hex, int32_t hexlen)
{
    int32_t i;

    for (i = hexlen - 1; i >= 0; i--)
    {
        hex[i] = (uint8_t)(dec & 0xFF);

        dec >>= 8;
    }
}

uint32_t emv_hex2dec(const uint8_t *hex, int32_t hexlen)
{
    int32_t i;
    uint32_t dec = 0;

    if (hex == NULL)
    {
        return 0;
    }

    for (i = 0; i < hexlen; i++)
    {
        dec += (uint32_t)(hex[i]) << (8 * (hexlen - 1 - i));
    }

    return dec;
}

void emv_dec2bcd(uint32_t dec, uint8_t *bcd, int32_t bcdlen)
{
    int32_t i;
    int32_t temp;

    for (i = bcdlen - 1; i >= 0; i--)
    {
        temp = dec % 100;
        bcd[i] = (uint8_t)(((temp / 10) << 4) + ((temp % 10) & 0x0F));
        dec /= 100;
    }
}

uint32_t emv_bcd2dec(const uint8_t *bcd, int32_t bcdlen)
{
    int32_t i;
    uint32_t dec = 0;

    if (bcd == NULL)
    {
        return 0;
    }

    for (i = 0; i < bcdlen; i++)
    {
        dec = dec * 10 + ((bcd[i] >> 4) & 0x0f);
        dec = dec * 10 + ((bcd[i]     ) & 0x0f);
    }

    return dec;
}

int32_t emv_bcd2hex(const uint8_t *bcd_data, int32_t bcd_len, uint8_t *hex_data)
{
	int32_t i;
	uint8_t byte,high_nibble,low_nibble;


    if (bcd_data == NULL || hex_data == NULL || bcd_len <= 0)
	{
        return -1;
    }

    for (i = 0; i < bcd_len; i++)
	{
        byte = bcd_data[i];

        high_nibble = (byte >> 4) & 0x0F;
        low_nibble = byte & 0x0F;

        if (high_nibble > 9 || low_nibble > 9)
		{
            return -1;
        }

        hex_data[2 * i]     = high_nibble;
        hex_data[2 * i + 1] = low_nibble;
    }

    return bcd_len * 2;
}


////////////////////////////////////////////////////////////////////////////////

/**
 * @brief �Ա��������ں͵�ǰ����
 * @param date ��������
 * @param len  ���ڳ���
 *      @arg 8  "YYYYMMDD"
 *      @arg 4  YYYYMMDD
 *      @arg 3  YYMMDD
 *      @arg 2  YYMM
 *      @arg 1  YY
 * @return [>0]-���ڵ�ǰ���ڣ� [=0]-���ڵ�ǰ���ڣ�[<0]-С�ڵ�ǰ����
 */
int32_t emv_date_compare(uint8_t *date, int32_t len)
{
    uint8_t temp[4], curr[4];

    if (date == NULL) {
        return 0;
    }
    EMVPORT_GetTransDateTime(curr, NULL);

    eTRACE_HEX("curr date", curr, 4);
    emv_memset(temp, 0x00, 4);
    if (len < 4) {
        //if (*date >= 0x50) {
        //    temp[0] = 0x19;
        //} else
        {
            temp[0] = 0x20;
        }
        temp[1] = *date; //YY
        if (len > 1) {
            temp[2] = *(date + 1); //MM
        }
        if (len > 2) {
            temp[3] = *(date + 2); //DD
        }
    } else if (len == 8) {
        emv_str2bcd(date, 8, temp, 4, false);
    } else {
        emv_memcpy(temp, date, 4);
    }
    eTRACE_HEX("comp date", temp, 4);
    return emv_memcmp(temp, curr, 4);
}

////////////////////////////////////////////////////////////////////////////////

//static int32_t load_term_param()
//{
//	int32_t ret,i;
//	eTRACE("paypass-info: load_term_param");
//
//	//for spi process sdol,need to load term param before ppse
//	if(g_emv_cnxt->config.poi_info_len)
//	{
//		appdata_save(TAG_008B_POI_INFOMATION, g_emv_cnxt->config.poi_info, g_emv_cnxt->config.poi_info_len, OVERRIDED);
//	}
//	if(g_emv_cnxt->config.proprietary_tag_len)
//	{
//		EMV_TLVOBJ tlvs[4]={0};
//
//		ret = emvtlv_decode(g_emv_cnxt->config.proprietary_tag, g_emv_cnxt->config.proprietary_tag_len, tlvs, sizeof(tlvs)/sizeof(EMV_TLVOBJ), STRING_TLVOBJ);
//		eTRACE("paypass-info: emvtlv_decode ret = %d", ret);
//		if(ret == EMV_OK)
//		{
//			for(i = 1; i < tlvs[0].childnum+1; i++)
//			{
//				if(tlvs[i].valuelen == 0)
//				{
//					appdata_save(tlvs[i].tagname, NULL, 0, OVERRIDED);
//				}
//				else
//				{
//					appdata_save(tlvs[i].tagname, tlvs[i].pvalue, tlvs[i].valuelen, OVERRIDED);
//				}
//				emv_taglib_add_proprietary_tag(tlvs[i].tagname);
//			}
//		}
//	}
//
//	appdata_save_singlebyte(TAG_STATUS_CHECK_SUPPORT_INDICATOR, g_emv_cnxt->config.status_check_support_indicator, OVERRIDED);
//	if(!emv_memcmp(g_emv_cnxt->config.term_id, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", sizeof(g_emv_cnxt->config.term_id)))
//	{
//		appdata_save(TAG_9F1C_TERMINAL_IDENTIFICATION, NULL, 0, OVERRIDED);
//	}
//	else
//	{
//		appdata_save(TAG_9F1C_TERMINAL_IDENTIFICATION, g_emv_cnxt->config.term_id, sizeof(g_emv_cnxt->config.term_id), OVERRIDED);
//	}
//    appdata_save(TAG_9F1E_IFD_SERIAL_NUMBER, g_emv_cnxt->config.term_ifd, sizeof(g_emv_cnxt->config.term_ifd), OVERRIDED);
//    appdata_save(TAG_9F1A_TERMINAL_COUNTRY_CODE, g_emv_cnxt->config.term_country_code, sizeof(g_emv_cnxt->config.term_country_code), OVERRIDED);
//    appdata_save(TAG_5F2A_TRANSACTION_CURRENCY_CODE, g_emv_cnxt->config.trans_currency_code, sizeof(g_emv_cnxt->config.trans_currency_code), OVERRIDED);
//	appdata_save_singlebyte(TAG_5F36_TRANSACTION_CURRENCY_EXPONENT, g_emv_cnxt->config.trans_currency_exponent, OVERRIDED);
//
//	if(g_emv_cnxt->config.acquirer_id_exist)
//	{
//		if(g_emv_cnxt->config.acquirer_id_len)
//		{
//			appdata_save(TAG_9F01_ACQUIRER_IDENTIFIER, g_emv_cnxt->config.acquirer_id, g_emv_cnxt->config.acquirer_id_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F01_ACQUIRER_IDENTIFIER, NULL, 0, OVERRIDED);
//		}
//	}
//
//    appdata_save(TAG_9F15_MERCHANT_CATEGORY_CODE, g_emv_cnxt->config.merchant_category_code, sizeof(g_emv_cnxt->config.merchant_category_code), OVERRIDED);
//
//	if(g_emv_cnxt->config.merchant_id_exist)
//	{
//		if(g_emv_cnxt->config.merchant_id_len)
//		{
//			appdata_save(TAG_9F16_MERCHANT_IDENTIFIER, g_emv_cnxt->config.merchant_id, g_emv_cnxt->config.merchant_id_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F16_MERCHANT_IDENTIFIER, NULL, 0, OVERRIDED);
//		}
//	}
//
//	if(g_emv_cnxt->config.merchant_name_exist)
//	{
//		if(g_emv_cnxt->config.merchant_name_len)
//		{
//			appdata_save(TAG_9F4E_MERCHANT_NAME, g_emv_cnxt->config.merchant_name, g_emv_cnxt->config.merchant_name_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F4E_MERCHANT_NAME, NULL, 0, OVERRIDED);
//		}
//	}
//
//	appdata_save_singlebyte(TAG_DF8117_CARDDATA_INPUT_CAP, g_emv_cnxt->config.card_data_input_cap, OVERRIDED);
//
//	appdata_save(TAG_9F6D_MSD_APPVER_NUM, g_emv_cnxt->config.msd_app_ver, sizeof(g_emv_cnxt->config.msd_app_ver), OVERRIDED);
//
//	if(emv_memcmp(g_emv_cnxt->config.rrp_min_grace, "\x00\x00", sizeof(g_emv_cnxt->config.rrp_min_grace)))
//	{
//		eTRACE_HEX("paypass-info: g_emv_cnxt->config.rrp_min_grace", g_emv_cnxt->config.rrp_min_grace, sizeof(g_emv_cnxt->config.rrp_min_grace));
//		appdata_save(TAG_DF8132_MIN_RR_GRACE_PERIOD, g_emv_cnxt->config.rrp_min_grace, sizeof(g_emv_cnxt->config.rrp_min_grace), OVERRIDED);
//	}
//
//	if(emv_memcmp(g_emv_cnxt->config.rrp_max_grace, "\x00\x00", sizeof(g_emv_cnxt->config.rrp_max_grace)))
//	{
//		eTRACE_HEX("paypass-info: g_emv_cnxt->config.rrp_max_grace", g_emv_cnxt->config.rrp_max_grace, sizeof(g_emv_cnxt->config.rrp_max_grace));
//		appdata_save(TAG_DF8133_MAX_RR_GRACE_PERIOD, g_emv_cnxt->config.rrp_max_grace, sizeof(g_emv_cnxt->config.rrp_max_grace), OVERRIDED);
//	}
//
//	if(emv_memcmp(g_emv_cnxt->config.rrp_expect_c_apdu, "\x00\x00", sizeof(g_emv_cnxt->config.rrp_expect_c_apdu)))
//	{
//		eTRACE_HEX("paypass-info: g_emv_cnxt->config.rrp_expect_c_apdu", g_emv_cnxt->config.rrp_expect_c_apdu, sizeof(g_emv_cnxt->config.rrp_expect_c_apdu));
//		appdata_save(TAG_DF8134_TERMEXPECTTRANSTIME_FOR_RRP_CAPDU, g_emv_cnxt->config.rrp_expect_c_apdu, sizeof(g_emv_cnxt->config.rrp_expect_c_apdu), OVERRIDED);
//	}
//
//	if(emv_memcmp(g_emv_cnxt->config.rrp_expect_r_apdu, "\x00\x00", sizeof(g_emv_cnxt->config.rrp_expect_r_apdu)))
//	{
//		eTRACE_HEX("paypass-info: g_emv_cnxt->config.rrp_expect_r_apdu", g_emv_cnxt->config.rrp_expect_r_apdu, sizeof(g_emv_cnxt->config.rrp_expect_r_apdu));
//		appdata_save(TAG_DF8135_TERMEXPECTTRANSTIME_FOR_RRP_RAPDU, g_emv_cnxt->config.rrp_expect_r_apdu, sizeof(g_emv_cnxt->config.rrp_expect_r_apdu), OVERRIDED);
//	}
//
//	if(emv_memcmp(g_emv_cnxt->config.rrp_accuracy_threshold, "\x00\x00", sizeof(g_emv_cnxt->config.rrp_accuracy_threshold)))
//	{
//		eTRACE_HEX("paypass-info: g_emv_cnxt->config.rrp_accuracy_threshold", g_emv_cnxt->config.rrp_accuracy_threshold, sizeof(g_emv_cnxt->config.rrp_accuracy_threshold));
//		appdata_save(TAG_DF8136_RR_ACCURACY_THRESHOLD, g_emv_cnxt->config.rrp_accuracy_threshold, sizeof(g_emv_cnxt->config.rrp_accuracy_threshold), OVERRIDED);
//	}
//
//	eTRACE("paypass-info: g_emv_cnxt->config.tags_to_read_exist = %s", g_emv_cnxt->config.tags_to_read_exist?"TRUE":"FALSE");
//	if(g_emv_cnxt->config.tags_to_read_exist)
//	{
//		if(g_emv_cnxt->config.tags_to_read_len > 0)
//		{
//			appdata_save(TAG_DF8112_TAGS_TO_READ, g_emv_cnxt->config.tags_to_read, g_emv_cnxt->config.tags_to_read_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_DF8112_TAGS_TO_READ, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.proc2first_write_flag_exist)
//	{
//		if(g_emv_cnxt->config.proc2first_write_flag_len)
//		{
//			appdata_save_singlebyte(TAG_DF8110_PROCEED_TO_FIRST_WRITE_FLAG, g_emv_cnxt->config.proc2first_write_flag, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_DF8110_PROCEED_TO_FIRST_WRITE_FLAG, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.dsvn_term_exist)
//	{
//		if(g_emv_cnxt->config.dsvn_term_len)
//		{
//			appdata_save(TAG_DF810D_DSVN_TERM, g_emv_cnxt->config.dsvn_term, g_emv_cnxt->config.dsvn_term_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_DF810D_DSVN_TERM, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.ds_req_opera_id_exist)
//	{
//		appdata_save(TAG_9F5C_DS_REQ_OPERATOR_ID, g_emv_cnxt->config.ds_req_opera_id, sizeof(g_emv_cnxt->config.ds_req_opera_id), OVERRIDED);
//	}
//	if(g_emv_cnxt->config.protect_envelope1_exist)
//	{
//		if(g_emv_cnxt->config.protect_envelope1_len)
//		{
//			appdata_save(TAG_9F70_PROTECTED_DATA_ENVELOPE1, g_emv_cnxt->config.protect_envelope1, g_emv_cnxt->config.protect_envelope1_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F70_PROTECTED_DATA_ENVELOPE1, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.protect_envelope2_exist)
//	{
//		if(g_emv_cnxt->config.protect_envelope2_len)
//		{
//			appdata_save(TAG_9F71_PROTECTED_DATA_ENVELOPE2, g_emv_cnxt->config.protect_envelope2, g_emv_cnxt->config.protect_envelope2_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F71_PROTECTED_DATA_ENVELOPE2, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.protect_envelope3_exist)
//	{
//		if(g_emv_cnxt->config.protect_envelope3_len)
//		{
//			appdata_save(TAG_9F72_PROTECTED_DATA_ENVELOPE3, g_emv_cnxt->config.protect_envelope3, g_emv_cnxt->config.protect_envelope3_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F72_PROTECTED_DATA_ENVELOPE3, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.protect_envelope4_exist)
//	{
//		if(g_emv_cnxt->config.protect_envelope4_len)
//		{
//			appdata_save(TAG_9F73_PROTECTED_DATA_ENVELOPE4, g_emv_cnxt->config.protect_envelope4, g_emv_cnxt->config.protect_envelope4_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F73_PROTECTED_DATA_ENVELOPE4, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.protect_envelope5_exist)
//	{
//		if(g_emv_cnxt->config.protect_envelope5_len)
//		{
//			appdata_save(TAG_9F74_PROTECTED_DATA_ENVELOPE5, g_emv_cnxt->config.protect_envelope5, g_emv_cnxt->config.protect_envelope5_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F74_PROTECTED_DATA_ENVELOPE5, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.unprotect_envelope1_exist)
//	{
//		if(g_emv_cnxt->config.unprotect_envelope1_len)
//		{
//			appdata_save(TAG_9F75_UNPROTECTED_DATA_ENVELOPE1, g_emv_cnxt->config.unprotect_envelope1, g_emv_cnxt->config.unprotect_envelope1_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F75_UNPROTECTED_DATA_ENVELOPE1, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.unprotect_envelope2_exist)
//	{
//		if(g_emv_cnxt->config.unprotect_envelope2_len)
//		{
//			appdata_save(TAG_9F76_UNPROTECTED_DATA_ENVELOPE2, g_emv_cnxt->config.unprotect_envelope2, g_emv_cnxt->config.unprotect_envelope2_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F76_UNPROTECTED_DATA_ENVELOPE2, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.unprotect_envelope3_exist)
//	{
//		if(g_emv_cnxt->config.unprotect_envelope3_len)
//		{
//			appdata_save(TAG_9F77_UNPROTECTED_DATA_ENVELOPE3, g_emv_cnxt->config.unprotect_envelope3, g_emv_cnxt->config.unprotect_envelope3_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F77_UNPROTECTED_DATA_ENVELOPE3, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.unprotect_envelope4_exist)
//	{
//		if(g_emv_cnxt->config.unprotect_envelope4_len)
//		{
//			appdata_save(TAG_9F78_UNPROTECTED_DATA_ENVELOPE4, g_emv_cnxt->config.unprotect_envelope4, g_emv_cnxt->config.unprotect_envelope4_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F78_UNPROTECTED_DATA_ENVELOPE4, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.unprotect_envelope5_exist)
//	{
//		if(g_emv_cnxt->config.unprotect_envelope5_len)
//		{
//			appdata_save(TAG_9F79_UNPROTECTED_DATA_ENVELOPE5, g_emv_cnxt->config.unprotect_envelope5, g_emv_cnxt->config.unprotect_envelope5_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_9F79_UNPROTECTED_DATA_ENVELOPE5, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.tags_to_write_before_gac_exist)
//	{
//		if(g_emv_cnxt->config.tags_to_write_before_gac_len)
//		{
//			appdata_save(TAG_FF8102_TAGS_TO_WRITE_BEFOREGAC, g_emv_cnxt->config.tags_to_write_before_gac, g_emv_cnxt->config.tags_to_write_before_gac_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_FF8102_TAGS_TO_WRITE_BEFOREGAC, NULL, 0, OVERRIDED);
//		}
//	}
//	if(g_emv_cnxt->config.tags_to_write_after_gac_exist)
//	{
//		if(g_emv_cnxt->config.tags_to_write_after_gac_len)
//		{
//			appdata_save(TAG_FF8103_TAGS_TO_WRITE_AFTERGAC, g_emv_cnxt->config.tags_to_write_after_gac, g_emv_cnxt->config.tags_to_write_after_gac_len, OVERRIDED);
//		}
//		else
//		{
//			appdata_save(TAG_FF8103_TAGS_TO_WRITE_AFTERGAC, NULL, 0, OVERRIDED);
//		}
//	}
//
//	return EMV_OK;
//}

void emv_bcd_add(uint8_t *num1,  uint8_t *num2,  uint8_t *sum)
{
	uint8_t carry = 0;
	int i;

	  // 从最低字节开始（大端存储的第5字节是最低位）
	  for (i = 5; i >= 0; i--)
	  {
		  uint8_t byte1 = num1[i];
		  uint8_t byte2 = num2[i];

		  uint8_t digit1_high = (byte1 >> 4) & 0x0F;
		  uint8_t digit1_low = byte1 & 0x0F;
		  uint8_t digit2_high = (byte2 >> 4) & 0x0F;
		  uint8_t digit2_low = byte2 & 0x0F;

		  // 先加低位数字
		  uint8_t temp_low = digit1_low + digit2_low + carry;
		  carry = temp_low / 10;
		  uint8_t result_low = temp_low % 10;

		  // 再加高位数字
		  uint8_t temp_high = digit1_high + digit2_high + carry;
		  carry = temp_high / 10;
		  uint8_t result_high = temp_high % 10;

		  // 组合结果
		  sum[i] = (result_high << 4) | result_low;
	  }

	  // 检查溢出（不应该发生，因为EMV金额有上限）
	  if (carry > 0)
	  {
		  // 如果发生溢出，设置结果为最大值
		  // 或者根据你的需求处理
		  sum[0] = 0x99;
		  sum[1] = 0x99;
		  sum[2] = 0x99;
		  sum[3] = 0x99;
		  sum[4] = 0x99;
		  sum[5] = 0x99;
	  }
}


static void load_trans_amount(uint32_t amount, uint32_t amount_other)
{
    uint8_t buf[24];
	uint8_t *amt,*amt_oth;
	int amt_len,amt_oth_len;
	uint8_t amt_sum[6]={0};

	eTRACE("ep-info: load_trans_amount");
	eTRACE("ep-info: amount = %u", amount);
	eTRACE("ep-info: amount_other = %u", amount_other);
	eTRACE("ep-info: g_emv_cnxt->oth_amount_absent = %s", g_emv_cnxt->oth_amount_absent?"TRUE":"FALSE");
	eTRACE("ep-info: g_emv_cnxt->amount_absent = %s", g_emv_cnxt->amount_absent?"TRUE":"FALSE");
	eTRACE("ep-info: g_emv_cnxt->oth_amount_empty = %s", g_emv_cnxt->oth_amount_empty?"TRUE":"FALSE");
	eTRACE("ep-info: g_emv_cnxt->amount_empty = %s", g_emv_cnxt->amount_empty?"TRUE":"FALSE");

	if(!g_emv_cnxt->oth_amount_absent)
	{
		if(!g_emv_cnxt->oth_amount_empty)
		{
			emv_memset(buf, 0, sizeof(buf));
			emv_dec2hex(amount_other, buf, 4);
			appdata_save(TAG_9F04_AMOUNT_OTHER_BINARY, buf, 4, UNIQUE);

			emv_memset(buf, 0, sizeof(buf));
			emv_dec2bcd(amount_other, buf, 6);

			appdata_save(TAG_9F03_AMOUNT_OTHER_NUMERIC, buf, 6, OVERRIDED);
		}
		else
		{
			eTRACE("ep-warning: amount(9F03) is Empty");
			appdata_save(TAG_9F03_AMOUNT_OTHER_NUMERIC, NULL, 0, OVERRIDED);
		}
	}
	else
	{
		eTRACE("ep-warning: other amount(9F03) is absent");
	}

    emv_memset(buf, 0, sizeof(buf));
    emv_dec2hex(amount, buf, 4);
    appdata_save(TAG_0081_AMOUNT_AUTHORISED_BINARY, buf, 4, UNIQUE);

    emv_memset(buf, 0, sizeof(buf));
    emv_dec2bcd(amount, buf, 6);

	if(!g_emv_cnxt->amount_absent)
	{
		if(!g_emv_cnxt->amount_empty)
		{
			appdata_save(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, buf, 6, OVERRIDED);
		}
		else
		{
			eTRACE("ep-warning: amount(9F02) is Empty");
			appdata_save(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, NULL, 0, OVERRIDED);
		}
	}
	else
	{
		eTRACE("ep-warning: amount(9F02) is absent");
	}

//	if(!g_emv_cnxt->amount_absent && !g_emv_cnxt->amount_empty && !g_emv_cnxt->oth_amount_absent && !g_emv_cnxt->oth_amount_empty)
//	{
//
//		amt = appdata_find(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, &amt_len);
//		amt_oth = appdata_find(TAG_9F03_AMOUNT_OTHER_NUMERIC, &amt_oth_len);
//		emv_bcd_add(amt, amt_oth, amt_sum);
//		eTRACE_HEX("ep-info: g_emv_cnxt->amt_sum", amt_sum, sizeof(amt_sum));
//
//		appdata_save(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, amt_sum, sizeof(amt_sum), OVERRIDED);
//	}
}


static void EMV_initialize(int32_t flag)
{
    if (g_emv_cnxt->tlvbuffer)
    {
        emv_memset(g_emv_cnxt->tlvbuffer, 0, g_emv_cnxt->tlvbuffer_size);
    }

    g_emv_cnxt->status = EMV_STA_IDLE;
    g_emv_cnxt->trans_type = _EMV_TRANSTYPE_UNKNOWN;
    g_emv_cnxt->trans_amount = 0;
    g_emv_cnxt->trans_amount_other = 0;

    g_emv_cnxt->ec_onlinpin_falg = 0;

	//SS.1
	EMV_InitOutCome();
	EMV_InitUIRequest();
	EMV_InitErrorIndication();

	emvicc_set_stop_flag(false);
}

static int32_t emv_save_translog(void)
{
    uint8_t* amt;
    uint8_t* pan;
    int32_t panlen;

    if ((pan = EMV_GetData(0x5A, &panlen)) == NULL
            || (amt = EMV_GetData(0x81, NULL)) == NULL)
    {
        return -1;
    }

    return EMVPORT_SavTransLog(pan, panlen, emv_hex2dec(amt, 4));
}

extern const EMV_PORTING g_emv_porting;

int32_t EMV_Init(const EMV_PORTING* pPort)
{
    uint8_t *ptr = NULL, *next_ptr=NULL, *current_ptr = NULL;
    int32_t len = 0, ret = EMV_OK, candidates_max_size = 0;

    g_emv_cnxt = NULL;

    if (pPort == NULL)
    {
        return EMV_NULPORTING;
    }

//	eTRACE("g_emv_porting[%p]", &g_emv_porting);
//	eTRACE("pPort[%p]", pPort);

    g_emv_port = pPort;

//	eTRACE("g_emv_port[%p]", g_emv_port);

    ptr = EMVPORT_AllocBuffer(&len);

//	eTRACE("ptr[%p]", ptr);
    if (ptr == NULL || len < (sizeof(EMV_CONTEXT) + 2048))
    {
        return rERR(EMV_ALLOCBUFFERERR);
    }
    emv_memset((void*)ptr, 0, len);

//	eTRACE("entrypoint-info: len = %d", len);

	current_ptr = ptr;

    g_emv_cnxt = (EMV_CONTEXT*)ptr;
//	eTRACE("entrypoint-info: ptr = %p", ptr);
	//four byte align
	current_ptr += ((sizeof(EMV_CONTEXT) + 3) & ~3);
//	eTRACE("entrypoint-info: current_ptr = %p", current_ptr);

    g_emv_cnxt->emvbuffer_size = len;
//	eTRACE("entrypoint-info: g_emv_cnxt->emvbuffer_size = %d", g_emv_cnxt->emvbuffer_size);

    g_emv_cnxt->termapps = (EMV_TERMAPP*)current_ptr;

    g_emv_cnxt->termapps_num = EMVPORT_GetTermApps(g_emv_cnxt->termapps);
	eTRACE("g_emv_cnxt->termapps_num = %d\n", g_emv_cnxt->termapps_num);

    if(g_emv_cnxt->termapps_num < 1 || g_emv_cnxt->termapps_num > EMV_CFG_MAX_APPS)
    {
//        eTRACE("entrypoint-error: get aid error,no aid to use,load default value");
//		g_emv_cnxt->termapps_num = 1;
//        emv_memcpy(g_emv_cnxt->termapps[0].aid, k_emv_aidparam.aid, k_emv_aidparam.aid_len);
//        g_emv_cnxt->termapps[0].aid_len = k_emv_aidparam.aid_len;
//        g_emv_cnxt->termapps[0].asi = k_emv_aidparam.app_sel_indicator;
//        g_emv_cnxt->termapps[0].enable = 1;
        ret = rERR(EMV_GETTERMAPPSERR);
    }

	uint32_t termapps_size = sizeof(EMV_TERMAPP) * g_emv_cnxt->termapps_num;
	current_ptr += ((termapps_size + 3) & ~3);
//	eTRACE("entrypoint-info: after set term app current_ptr = %p", current_ptr);

	g_emv_cnxt->tlvbuffer = current_ptr;

//	eTRACE("g_emv_cnxt->tlvbuffer = %p\n", (void*)g_emv_cnxt->tlvbuffer);
	g_emv_cnxt->tlvbuffer_size = TLVBUFFER_SIZE;
	current_ptr += g_emv_cnxt->tlvbuffer_size;

    g_emv_cnxt->preprocess_result = (EMV_PREPROCESS_RESULT*)current_ptr;
	uint32_t preprocess_size = sizeof(EMV_PREPROCESS_RESULT) * g_emv_cnxt->termapps_num;
	current_ptr += ((preprocess_size+3) & ~3);

	g_emv_cnxt->candidates = (EMV_CANDIDATE*)current_ptr;
	candidates_max_size = sizeof(EMV_CANDIDATE) * EMV_CFG_MAX_CANDIDATES;

	uint32_t candidates_total_size = ((candidates_max_size + 3) & ~3);
//	eTRACE("candidates_total_size = %d\n", candidates_total_size);

	g_emv_cnxt->candidates_num = 0;
	next_ptr = (uint8_t *)g_emv_cnxt->candidates + candidates_total_size;;
//	eTRACE("candidates = %p\n", (void*)g_emv_cnxt->candidates);
//	eTRACE("candidates end = %p\n", next_ptr);
//	eTRACE("candidates size = %d\n", sizeof(EMV_CANDIDATE));

	if(next_ptr > (uint8_t *)(ptr + len))
	{
		eTRACE("PAYPASS-info: Not enough memory for candidates");
		return rERR(EMV_ALLOCBUFFERERR);
	}

    EMV_initialize(0);
	appdata_init();

    return ret;

}

int32_t EMV_SetTransType(uint8_t eType)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    g_emv_cnxt->trans_type = eType;

    return EMV_OK;
}

//xinyong 160308 �ǽӽ��׽�����־��ѯ����Ҫ�����ʾ
int32_t EMV_GetTransType(uint8_t *eType)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
//    if (g_emv_cnxt->status != EMV_STA_IDLE)
//        return rERR(EMV_NONIDLE);

    *eType = g_emv_cnxt->trans_type;

    return EMV_OK;
}

int32_t EMV_SetTransAmountAbsent(bool flag)
{
    if (g_emv_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
	}
    if (g_emv_cnxt->status != EMV_STA_IDLE)
    {
        return rERR(EMV_NONIDLE);
	}

//	eTRACE("PAYPASS-info: input flag = %d", flag);
    g_emv_cnxt->amount_absent = flag;

    return EMV_OK;
}

int32_t EMV_SetTransAmount(uint32_t nAmount)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    g_emv_cnxt->trans_amount = nAmount;

    return EMV_OK;
}

int32_t EMV_SetOtherAmountAbsent(bool flag)
{
    if (g_emv_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
	}
    if (g_emv_cnxt->status != EMV_STA_IDLE)
    {
        return rERR(EMV_NONIDLE);
	}

//	eTRACE("PAYPASS-info: input flag = %d", flag);
    g_emv_cnxt->oth_amount_absent = flag;

    return EMV_OK;
}

int32_t EMV_SetTransAmountEmpty(bool flag)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    g_emv_cnxt->amount_empty = flag;
    return EMV_OK;
}

int32_t EMV_SetOtherAmountEmpty(bool flag)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    g_emv_cnxt->oth_amount_empty = flag;
    return EMV_OK;
}

int32_t EMV_SetTransTypeAbsent(bool flag)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    g_emv_cnxt->trans_type_absent = flag;
    return EMV_OK;
}

int32_t EMV_SetOtherAmount(uint32_t nAmount)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    g_emv_cnxt->trans_amount_other = nAmount;

    return EMV_OK;
}

//refer to EMV Book B
int32_t EMV_PreProcess(uint8_t bOnline)
{
	EMV_AIDPARAM  aid_param={0};
	int i, j;
	uint32_t status_check_amt = 1;
	bool endFlag = true;
	EMV_PREPROCESS_RESULT preprocess={0};

	if (g_emv_cnxt == NULL)
	{
		return EMV_UNINITIALIZED;
	}
	if (g_emv_cnxt->status != EMV_STA_IDLE)
	{
		return rERR(EMV_NONIDLE);
	}

	g_emv_cnxt->force_online = bOnline;
	g_emv_cnxt->card_types_allowed = EMV_CARD_UNKNOWN;
	g_emv_cnxt->card_types_allowed |= EMV_CARD_CONTACTLESS;
	eTRACE("ep-info: customer set force_online:%d", g_emv_cnxt->force_online);


	eTRACE("ep-info: g_emv_cnxt->amount_absent:%d", g_emv_cnxt->amount_absent);
	if(!g_emv_cnxt->amount_absent)
	{
		for (i =0; i < g_emv_cnxt->termapps_num; i++)
		{
			emv_memset(&aid_param, 0x00, sizeof(EMV_AIDPARAM));
			emv_memset(&preprocess, 0x00, sizeof(preprocess));

			EMVPORT_GetAppParam(&aid_param, i);
			//3.1.1.2
			emv_memcpy(preprocess.ttq, aid_param.ttq, sizeof(aid_param.ttq));

			//3.1.1.3
			if(aid_param.status_check_support)
			{
				status_check_amt = 1;
				for (j = 0; j < aid_param.currency_exp; j++)
				{
					status_check_amt *= 10;
				}
				eTRACE("ep-info: trans_currency_exponent:%d, status_check_amt:%d", aid_param.currency_exp, status_check_amt);
				eTRACE("ep-info: g_emv_cnxt->trans_amount:%d", g_emv_cnxt->trans_amount);
				if (g_emv_cnxt->trans_amount == status_check_amt)
				{
					eTRACE("ep-info: g_emv_cnxt->trans_amount == status_check_amt");
					preprocess.ttq[1] |= 0x80;
				}
			}
			//3.1.1.4
            if (g_emv_cnxt->trans_amount == 0)
            {
                eTRACE("ep-info: trx amount is 0");
                if(1 == aid_param.zero_amt_offline_allow)
                {
                    //goto 3.1.1.5
                }
                else
                {
                    if (0 == aid_param.zero_amt_allow) // option1
                    {
                        preprocess.contactless_app_not_allowed = 1;
                    }
                    else
                    {
                        preprocess.zero_amount = 1;
                    }

                }
            }
			//3.1.1.5
			if (aid_param.cl_trans_limit_exist)
			{
				if (g_emv_cnxt->trans_amount >= emv_bcd2dec(aid_param.cl_trans_limit, sizeof(aid_param.cl_trans_limit)))
				{
					preprocess.contactless_app_not_allowed = 1;
					eTRACE("ep-warning: aid[%d] exceed Contactless trans limit", i);
				}
			}
			//3.1.1.6
			if (aid_param.cl_floor_limit_exist)
			{
				if (preprocess.ttq[0] & 0x08)
				{
					//For an Offline-only Terminal, the Reader Contactless Floor Limit should not be present
					//as the terminal cannot process transactions online
				}
				else
				{
					if (g_emv_cnxt->trans_amount > emv_bcd2dec(aid_param.cl_floor_limit, sizeof(aid_param.cl_floor_limit)))
					{
						preprocess.reader_cl_floor_limit_exceed = 1;
						eTRACE("ep-warning: trans amount exceed cl_floor_limit");
					}
				}
			}
			else
			{
				//3.1.1.7
				if (preprocess.ttq[0] & 0x08)
				{
					//For an Offline-only Terminal, the Reader Contactless Floor Limit should not be present
					//as the terminal cannot process transactions online
				}
				else
				{
					if(aid_param.term_floor_limit_exist && g_emv_cnxt->trans_amount > emv_hex2dec(aid_param.term_floor_limit, sizeof(aid_param.term_floor_limit)))
					{
						preprocess.reader_cl_floor_limit_exceed = 1;
						eTRACE("ep-warning: trans amount exceed term floor limit");
					}
				}
			}
			//3.1.1.8
			if(aid_param.cvm_limit_exist)
			{
				if (g_emv_cnxt->trans_amount >= emv_bcd2dec(aid_param.cvm_limit, sizeof(aid_param.cvm_limit)))
				{
					preprocess.reader_cvm_limit_exceed = 1;
					eTRACE("ep-warning: trans amount exceed cvm limit");
				}
			}
			//3.1.1.9
			if(preprocess.reader_cl_floor_limit_exceed)
			{
				preprocess.ttq[1] |= 0x80;
			}
			//3.1.1.10
			if(preprocess.status_check_requested)
			{
				preprocess.ttq[1] |= 0x80;
			}
			//3.1.1.11
			if(preprocess.zero_amount)
			{
				if(!(preprocess.ttq[0] & 0x08))
				{
					preprocess.ttq[1] |= 0x80;
				}
				else
				{
					preprocess.contactless_app_not_allowed = 1;
				}
			}
			//3.1.1.12
			if(preprocess.reader_cvm_limit_exceed)
			{
				preprocess.ttq[1] |= 0x40;
			}

            emv_memcpy(g_emv_cnxt->preprocess_result + i, &preprocess, sizeof(EMV_PREPROCESS_RESULT));
            g_emv_cnxt->preprocess_result_num++;
		}

	}

	//TODO: add outcome process

	return EMV_OK;
}


int32_t EMV_SetCardType(uint8_t eType)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;

    eTRACE("g_emv_cnxt->status:%d", g_emv_cnxt->status);

    if (g_emv_cnxt->status != EMV_STA_IDLE)
        return rERR(EMV_NONIDLE);

    eTRACE("eType:%02x g_emv_cnxt->card_types_allowed:%02x", eType, g_emv_cnxt->card_types_allowed);
    if ((eType == EMV_CARD_CONTACT || eType == EMV_CARD_CONTACTLESS)
            && (g_emv_cnxt->card_types_allowed & eType) != 0)
    {
        g_emv_cnxt->card_type = eType;

        return EMV_OK;
    }
    else
    {
        return EMV_CARDTYPEILL;
    }
}

/* EMV_SetRRPTimeDelay removed: RRP is a kernel (PayPass) specific feature, must not be referenced by the Entry Point core. */

int32_t EMV_Process(int32_t *msginx)
{
    int32_t ret;

    if (g_emv_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
    }

    if (g_emv_cnxt->status < EMV_STA_IDLE || g_emv_cnxt->status >= EMV_COMPLETED)
    {
        return rERR(EMV_PROCESSOVER);
    }

    if (g_emv_cnxt->status == EMV_STA_IDLE)
    {
        eTRACE("card_type[%d]", g_emv_cnxt->card_type);
        eTRACE("trans_type[%02X]", g_emv_cnxt->trans_type);
        eTRACE("trans_amount[%d.%02d]", g_emv_cnxt->trans_amount/100, g_emv_cnxt->trans_amount%100);
        eTRACE("trans_amount_other[%d.%02d]", g_emv_cnxt->trans_amount_other/100, g_emv_cnxt->trans_amount_other%100);

		tmTRACE("entrypoint-info: flag1");
        if (g_emv_cnxt->card_type == EMV_CARD_UNKNOWN)
        {
            return rERR(EMV_CARDTYPEILL);
        }
//		load_term_param();
		load_trans_amount(g_emv_cnxt->trans_amount, g_emv_cnxt->trans_amount_other);
		appdata_save(TAG_009C_TRANSACTION_TYPE, &g_emv_cnxt->trans_type, 1, UNIQUE);
		tmTRACE("entrypoint-info: flag2");
	}

	eTRACE("g_emv_cnxt->status = %d", g_emv_cnxt->status);

    switch (g_emv_cnxt->status)
    {
        case EMV_STA_IDLE:
            eTRACE("Build candidate list--------------------------------------------------Start");
            g_emv_cnxt->status = emv_build_candidate_list();
            break;

		case EMV_STA_CANDIDATES_BUILT:
        case EMV_REQ_SEL_CANDIDATES:
        case EMV_REQ_SEL_CANDIDATES_AGAIN:
		case EMV_REQ_SEL_ACCOUNTTYPE:

			ret = emv_combination_Selection();
			eTRACE("emv_combination_Selection ret = %d", ret);
			if(ret != EMV_OK)
			{
				eTRACE("Invalid Kernel");
				g_emv_cnxt->status = EMV_ENDAPPLICATION;
			}
			else
			{
				g_emv_cnxt->status = EMV_KernelProcess();
				if (g_emv_cnxt->status == EMV_ERR)
				{
					g_emv_cnxt->status = EMV_ENDAPPLICATION;
				}
			}
			break;
        default:
			g_emv_cnxt->status = EMV_KernelProcess();
				if (g_emv_cnxt->status == EMV_ERR)
				{
					g_emv_cnxt->status = EMV_ENDAPPLICATION;
				}
			break;
    }


    eTRACE("g_emv_cnxt->status[%d]", g_emv_cnxt->status);
    return g_emv_cnxt->status;
}


int32_t EMV_ResultSelCandidates(int32_t eResult)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_SEL_CANDIDATES && g_emv_cnxt->status != EMV_REQ_SEL_CANDIDATES_AGAIN)
        return rERR(EMV_SELCANDIDATENOTREQ);
    if (eResult < 0 || eResult >= g_emv_cnxt->candidates_num)
        return rERR(EMV_SELCANDIDATENOTREQ);

    g_emv_cnxt->req_sel_candidates_result = eResult;

    return EMV_OK;
}

int32_t EMV_ResultSelAccountType(int32_t eResult)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_SEL_ACCOUNTTYPE)
        return rERR(EMV_SELACCOUNTNOTREQ);

    g_emv_cnxt->req_sel_accounttype_result = eResult;

    return EMV_OK;
}

int32_t EMV_ResultCheckID(int32_t eResult)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_CHECK_ID)
        return rERR(EMV_CHECKIDNOTREQ);

    g_emv_cnxt->req_check_id_result = eResult;

    return EMV_OK;
}

int32_t EMV_ResultOnlinePIN(int32_t eResult, uint8_t* pData, int32_t nDataLen)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_ONLINE_PIN)
        return rERR(EMV_ONLINEPINNOTREQ);
    eTRACE("result[%d] len[%d]", eResult, nDataLen);

    g_emv_cnxt->req_online_pin_result = eResult;

    if (eResult == EMV_OPER_OK)
    {
        if (pData != NULL && nDataLen > 0 && nDataLen <= 256)
        {
            appdata_save(TAG_ONLINE_PIN, pData, nDataLen, UNIQUE);
        }
    }

    return EMV_OK;
}

int32_t EMV_ResultOfflinePIN(int32_t eResult, uint8_t* pData, int32_t nDataLen)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_OFFLINE_PIN && g_emv_cnxt->status != EMV_REQ_OFFLINE_PIN_LAST)
        return rERR(EMV_OFFLINEPINNOTREQ);
    eTRACE("result[%d] len[%d]", eResult, nDataLen);

    g_emv_cnxt->req_offline_pin_result = eResult;

    if (eResult == EMV_OPER_OK)
    {
        g_emv_cnxt->offline_pin[0] = '\0';
        if (pData != NULL && nDataLen >= 4 && nDataLen <= 12)
        {
            emv_memcpy(g_emv_cnxt->offline_pin, pData, nDataLen);
            g_emv_cnxt->offline_pin[nDataLen] = '\0';
        }
        else
        {
            g_emv_cnxt->req_offline_pin_result = EMV_OPER_UNSUPPORTED;
        }
    }

    return EMV_OK;
}

int32_t EMV_ResultBypassPIN(int32_t eResult)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_CONFIRM_BYPASS_PIN)
        return rERR(EMV_BYPASSPINNOTREQ);

    eTRACE("result[%d]", eResult);

    g_emv_cnxt->req_confirm_bypass_pin_result = eResult;

    return EMV_OK;
}

int32_t EMV_ResultForceOnline(int32_t eResult)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_CONFIRM_FORCE_ONLINE)
        return rERR(EMV_FORCEONLINENOTREQ);
    eTRACE("result[%d]", eResult);
    g_emv_cnxt->req_confirm_force_online_result = eResult;

    return EMV_OK;
}

int32_t EMV_ResultOnline(int32_t eResult, uint8_t* pResp, int32_t nRespLen)
{
    int32_t i;
    uint32_t tag1 = TAG_ISSUER_SCRIPT_71_BASE;
    uint32_t tag2 = TAG_ISSUER_SCRIPT_72_BASE;
    EMV_TLVOBJ tlvs[64];

    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_GO_ONLINE)
        return rERR(EMV_GOONLINENOTREQ);

    g_emv_cnxt->online_result = eResult;
    eTRACE("online_result:%d RespLen:%d", g_emv_cnxt->online_result, nRespLen);
    if(nRespLen>0)eTRACE_HEX("resp", pResp, nRespLen);

    if (eResult == EMV_ONLINE_SUCC_ACCEPT || eResult == EMV_ONLINE_SUCC_ISSREF  || eResult == EMV_ONLINE_SUCC_DENIAL)
    {
        if (pResp != NULL && nRespLen > 0)
        {
            if (emvtlv_decode((uint8_t*)pResp, nRespLen, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), DECODE_LEVEL1) == 0)
            {
                for (i = 1; i <= tlvs[0].childnum; i++)
                {
                    if (tlvs[i].tagname == TAG_0071_ISSUER_SCRIPT_TEMPLATE_1)
                        tlvs[i].tagname = tag1++;
                    else if (tlvs[i].tagname == TAG_0072_ISSUER_SCRIPT_TEMPLATE_2)
                        tlvs[i].tagname = tag2++;
                    appdata_save_tlv(tlvs + i, OVERRIDED, 0xFFFFFFFF);
                }
            }
        }
    }

    return EMV_OK;
}

int32_t EMV_ResultIssuerReferral(int32_t eResult)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status != EMV_REQ_ISSUER_REFERRAL)
        return rERR(EMV_ISSREFERRALNOTREQ);

    g_emv_cnxt->req_issuer_referral_result = eResult;

    return EMV_OK;
}

const EMV_CANDIDATE* EMV_GetSelectedCandidate(void)
{
    int32_t index;

    if (g_emv_cnxt == 0 || g_emv_cnxt->candidates == 0)
    {
        return 0;
    }

    index = g_emv_cnxt->req_sel_candidates_result;
    if (index < 0 || index >= g_emv_cnxt->candidates_num)
    {
        return 0;
    }

    return &g_emv_cnxt->candidates[index];
}
uint8_t* EMV_GetData(uint32_t nTag, int32_t* pLen)
{
    if (g_emv_cnxt == NULL || g_emv_cnxt->status == EMV_STA_IDLE)
    {
        return NULL;
    }

    if (pLen)
    {
        *pLen = 0;
    }

    if (nTag == TAG_KERNEL_VERSION)
    {
        if (pLen)
        {
            *pLen = (int32_t)emv_strlen(EMV_VERSION);
        }
        return (uint8_t*)EMV_VERSION;
    }

    if ((nTag & TAG_KERNEL_BASE) == TAG_KERNEL_BASE)
    {
        switch (nTag)
        {
            case TAG_KERNEL_CHECKSUM:
                if (pLen)
                {
                    *pLen = 4;
                }
                return (uint8_t*)&g_emv_cnxt->kernel_checksum;

            case TAG_CONFIG_CHECKSUM:
                if (pLen)
                {
                    *pLen = 4;
                }
                return (uint8_t*)&g_emv_cnxt->config_checksum;

            case TAG_CANDIDATE_LIST:
                if (g_emv_cnxt->status == EMV_STA_IDLE)
                {
                    return NULL;
                }
                if (pLen)
                {
                    *pLen = g_emv_cnxt->candidates_num * sizeof(EMV_CANDIDATE);
                }
                return (uint8_t*)g_emv_cnxt->candidates;

            default:
                break;
        }
    }

    return appdata_find(nTag, pLen);
}

int32_t EMV_SetData(uint32_t tag, uint8_t *value, int32_t valuelen)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
//    if (g_emv_cnxt->status < EMV_STA_APP_SELECTED)
//        return rERR(EMV_NOT_STA_APP_SELECTED);

//    if (value == NULL || valuelen < 0)
//    {
//        return rERR(EMV_VALUENULL_LENERR);
//    }

    if (appdata_save(tag, value, valuelen, OVERRIDED))
    {
        return EMV_FAIL;
    }
    else
    {
        return EMV_OK;
    }

}

int32_t EMV_GetTransLog(int32_t nRecNo, uint8_t* pOut, int32_t* pOutLen)
{
    int32_t len, ret;
    uint8_t  buf[256];
    uint8_t *ptmp;

    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status < EMV_STA_APP_SELECTED)
        return rERR(EMV_APPSELUNDONE);

    if ((ptmp = appdata_find(TAG_9F4D_LOG_SFI, NULL)) == NULL)
    {
        return rERR(EMV_PBOCLOGSFINOTFOUND);
    }

    if (nRecNo == PBOC_TRANSREC_SFI)
    {
        if (pOut)
            *pOut = *(ptmp);
        if (pOutLen)
            *pOutLen = 1;
        return EMV_OK;
    }
    else if (nRecNo == PBOC_TRANSREC_NUM)
    {
        if (pOut)
            *pOut = *(ptmp + 1);
        if (pOutLen)
            *pOutLen = 1;
        return EMV_OK;
    }
    else if (nRecNo == PBOC_TRANSREC_FMT)
    {
        ptmp = appdata_find(TAG_9F4F_LOG_FORMAT, &len);
        if (ptmp == NULL)
        {
            if ((ret = emvicc_get_data(TAG_9F4F_LOG_FORMAT, buf, &len)) != 0x9000)
            {
                len = 0;
            }

            appdata_save(TAG_9F4F_LOG_FORMAT, buf, len, UNIQUE);

            ptmp = appdata_find(TAG_9F4F_LOG_FORMAT, &len);
        }

        if (ptmp == NULL || len <= 0)
        {
            return rERR(EMV_PBOCLOGFMTNOTFOUND);
        }

        if (pOut)
            emv_memcpy(pOut, ptmp, len);
        if (pOutLen)
            *pOutLen = len;
        return EMV_OK;
    }
    else if (nRecNo >= 1 && nRecNo <= 255)
    {
        if ((ret = emvicc_read_record(*ptmp, (uint8_t)nRecNo, buf, &len)) != 0x9000)
        {
            if (ret != 0x6A83)
            {
                return rERR(EMV_PBOCLOGRDRECFAILED);
            }
            return 0;
        }
        if (pOut)
            emv_memcpy(pOut, buf, len);
        if (pOutLen)
            *pOutLen = len;
        return EMV_OK;
    }

    return EMV_PBOCLOGRECNOINV;
}

int32_t EMV_GetLoadLog(int32_t nRecNo, uint8_t* pOut, int32_t* pOutLen)
{
    int32_t len, ret;
    uint8_t  buf[256];
    uint8_t *ptmp;

    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status < EMV_STA_APP_SELECTED)
        return rERR(EMV_APPSELUNDONE);

    if ((ptmp = appdata_find(TAG_DF4D_LOAD_LOG_SFI, NULL)) == NULL)
    {
        return rERR(EMV_PBOCLOGSFINOTFOUND);
    }

    if (nRecNo == PBOC_LOADREC_SFI)
    {
        if (pOut)
            *pOut = *(ptmp);
        if (pOutLen)
            *pOutLen = 1;
        return EMV_OK;
    }
    else if (nRecNo == PBOC_LOADREC_NUM)
    {
        if (pOut)
            *pOut = *(ptmp + 1);
        if (pOutLen)
            *pOutLen = 1;
        return EMV_OK;
    }
    else if (nRecNo == PBOC_LOADREC_FMT)
    {
        ptmp = appdata_find(TAG_DF4F_LOAD_LOG_FORMAT, &len);
//        if (ptmp == NULL)
//        {
//            if ((ret = emvicc_get_data(TAG_DF4F_LOAD_LOG_FORMAT, buf, &len)) != 0x9000)
//            {
//                len = 0;
//            }

//            appdata_save(TAG_DF4F_LOAD_LOG_FORMAT, buf, len, UNIQUE);

//            ptmp = appdata_find(TAG_DF4F_LOAD_LOG_FORMAT, &len);
//        }

        if (ptmp == NULL || len <= 0)
        {
            return rERR(EMV_PBOCLOGFMTNOTFOUND);
        }

        if (pOut)
            emv_memcpy(pOut, ptmp, len);
        if (pOutLen)
            *pOutLen = len;
        return EMV_OK;
    }
    else if (nRecNo >= 1 && nRecNo <= 255)
    {
        if ((ret = emvicc_read_record(*ptmp, (uint8_t)nRecNo, buf, &len)) != 0x9000)
        {
//            if (ret != 0x6A83)
            {
                return rERR(EMV_PBOCLOGRDRECFAILED);
            }
            return 0;
        }
        if (pOut)
            emv_memcpy(pOut, buf, len);
        if (pOutLen)
            *pOutLen = len;
        return EMV_OK;
    }

    return EMV_PBOCLOGRECNOINV;
}

int32_t EMV_ICC_Apdu(uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen)
{
    return emvicc_apdu(input, inlen, output, outlen);
}

int32_t EMV_ICC_GetData(uint32_t nTag, uint8_t *pOut, int32_t *pOutLen)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status < EMV_STA_APP_SELECTED)
        return rERR(EMV_APPSELUNDONE);

    return emvicc_get_data(nTag, pOut, pOutLen);
}

int32_t EMV_ICC_ReadRecord(uint8_t nSFI, uint8_t nRecNo, uint8_t *pOut, int32_t *pOutLen)
{
    if (g_emv_cnxt == NULL)
        return EMV_UNINITIALIZED;
    if (g_emv_cnxt->status < EMV_STA_APP_SELECTED)
        return rERR(EMV_APPSELUNDONE);

    return emvicc_read_record(nSFI, nRecNo, pOut, pOutLen);
}

int32_t EMV_TLV_Decode(uint8_t *pData, int32_t nDataLen, EMV_TLVOBJ *pTLVs, int32_t nTLVs, int32_t bFlags)
{
    int32_t ret;

    ret = emvtlv_decode(pData, nDataLen, pTLVs, nTLVs, bFlags);
    if (ret != EMV_OK)
    {
        ret = EMV_FAIL;
    }

    return ret;
}

int32_t EMV_TLV_Decode_Paypass(uint8_t *pData, int32_t nDataLen, EMV_TLVOBJ *pTLVs, int32_t nTLVs, int32_t bFlags)
{
    int ret;

	if(pData == NULL || pTLVs == NULL)
	{
		return EMV_FAIL;
	}

//	eTRACE("pData = %p", pData);
//	eTRACE("pTLVs = %p", pTLVs);
//	eTRACE("nTLVs = %d", nTLVs);
//	eTRACE("emvtlv_decode = %p", emvtlv_decode);

    ret = emvtlv_decode_paypass(pData, nDataLen, pTLVs, nTLVs, bFlags);
    if (ret != EMV_OK)
    {
        ret = EMV_FAIL;
    }

    return ret;
}


int32_t EMV_TLV_Find(int32_t nParent, uint32_t nTag, EMV_TLVOBJ *pTLVs, int32_t nLevel)
{
    return emvtlv_get(nParent, nTag, pTLVs, nLevel);
}

int32_t EMV_TLV_PacketByDOL(uint8_t *pDOL, int32_t nDOLLen, uint8_t *pVal, int32_t nValLen, uint8_t *pOut, int32_t *pOutLen, int32_t nOutMaxLen)
{
    int32_t ret;

    ret = emvdol_packet_tlv(pDOL, nDOLLen, pVal, nValLen, pOut, nOutMaxLen);
    if (ret < 0)
    {
        return EMV_FAIL;
    }

    if (pOutLen)
    {
        *pOutLen = ret;
    }

    return EMV_OK;
}

int32_t EMV_TLV_PacketByTAG(uint32_t* pTags, int32_t nTagNum, uint8_t* pOut, int32_t* pOutLen, int32_t nOutMaxLen)
{
    int32_t i, reallen = 0, taglen, lenlen;
    uint8_t* ptmp;
    int32_t ilen;

    if (pTags == NULL || pOut == NULL || pOutLen == NULL)
    {
        return EMV_FAIL;
    }

    for (i = 0; i < nTagNum; ++i)
    {
        if (pTags[i] == 0)
        {
            continue;
        }
        if ((ptmp = EMV_GetData(pTags[i], &ilen)) == NULL)
        {
            continue;
        }

        /* reserve worst-case tag(4)+length(4) before writing anything */
        if (reallen + 4 + 4 > nOutMaxLen)
            return EMV_FAIL;

        taglen = tlv_set_tag(pTags[i], pOut+reallen);
        if (taglen < 0 || reallen + taglen > nOutMaxLen)
            return EMV_FAIL;
        reallen += taglen;

        if (reallen + 4 > nOutMaxLen)
            return EMV_FAIL;
        lenlen = tlv_set_len(ilen, pOut+reallen);
        if (lenlen < 0 || reallen + lenlen > nOutMaxLen)
            return EMV_FAIL;
        reallen += lenlen;

        if (reallen + ilen > nOutMaxLen)
        {
            return EMV_FAIL;
        }
        emv_memcpy(pOut + reallen, ptmp, ilen);
        reallen += ilen;
    }

    *pOutLen = reallen;

    return EMV_OK;
}

int32_t EMV_ProcessExit(void)
{
    if (g_emv_cnxt == NULL)
    {
        return EMV_UNINITIALIZED;
    }

    EMV_initialize(1);

    return EMV_OK;
}

int32_t EMV_Exit(void)
{
    g_emv_cnxt = NULL;

    return EMV_OK;
}


/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
