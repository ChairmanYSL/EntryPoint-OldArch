/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emvport.c
* @author   Kingsee Development Team xinyong
* @version  V43
* @date     2015��11��17��
* @brief    TODO
*
************************************************************************************************************************
*/

//#define __DEBUG_MODULE__

/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "sdk.h"
#include "emv.h"
#include "emv_ics.h"
#include "emv_tag.h"
#include "emv_type.h"
#include "emv_utils.h"
#include "emvdbg.h"
#include "emverror.h"
#include "emvapi.h"
#include "emvporting.h"
#include "emvspi.h"
#include "emvi.h"
#include "emv_port_sdk.h"
/*
************************************************************************************************************************
*                                                     LOCAL DEFINES
************************************************************************************************************************
*/
/** ȫ�����ò�����������Ч�� */
typedef struct _apipara_context
{
    uint32_t mask;
    uint32_t tsc;
    uint8_t kernel_index;

} apipara_context;




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

static apipara_context g_apipara_ctx;


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

/* Parameter tag aliases: the AMEX/Expresspay kernel parameter files use 0x1F03
 * (Default TDOL) and 0x1F02 (Status Check Support Indicator), while the PayPass
 * convention (and this file's default macros) use 0xDF8102 / 0xDFC108.
 * Accept both in the Entry Point parameter layer so one file can serve both. */
#ifndef TAG_1F02_STATUS_CHECK_SUPPORT
#define TAG_1F02_STATUS_CHECK_SUPPORT   0x1F02u
#endif
#ifndef TAG_1F03_DEFAULT_TDOL
#define TAG_1F03_DEFAULT_TDOL           0x1F03u
#endif

#define PARA_SAVLEN     (1024)
#define PARA_LENLEN     (4)
#define PARA_MAXLEN     (PARA_SAVLEN-PARA_LENLEN)

/* transwater 单条记录的最大长度（长度字段来自文件/应用，必须限幅） */
#define TRANSWATER_MAXLEN   (1024)

#define CAPKPARAM_SAVLEN     (512)
#define CAPKPARAM_LENLEN     (4)
#define CAPKPARAM_MAXLEN     (CAPKPARAM_SAVLEN-CAPKPARAM_LENLEN)



enum EMV_FileTypes
{
    _PARAM,
    _TERMPARAM,
    _AIDPARAM,
    _CAPKPARAM,
    _CARDBLACK,
    _CERTBLACK,
    _TRANSWATER,
    _TRANSLOG,
    _DRL,
};

static char* EMV_FNAME(int32_t type)
{
    static char fname[64] = {0};
    uint8_t kernel_index = 0;

    /* apipara 文件固定为 emv.par，不能走内核索引分支（否则递归初始化） */
    if (type == _PARAM)
    {
        API_strcpy(fname, "emv.par");
        return fname;
    }

    /* 文件名按当前内核索引区分，索引越界时回退到 0，禁止向 fopen 传 NULL */
    emvporting_get_kernel_index(&kernel_index);
    if (kernel_index >= KERNEL_MAX_INX)
    {
        kernel_index = 0;
    }

    switch (type)
    {
        case _TERMPARAM:
            API_sprintf(fname, "emvterm%d.par", kernel_index);
            break;
        case _AIDPARAM:
            API_sprintf(fname, "emvaids%d.par", kernel_index);
            break;
        case _CAPKPARAM:
            API_sprintf(fname, "emvcapk%d.par", kernel_index);
            break;
        case _CARDBLACK:
            API_sprintf(fname, "emvcard%d.blk", kernel_index);
            break;
        case _CERTBLACK:
            API_sprintf(fname, "emvcert%d.blk", kernel_index);
            break;
        case _TRANSWATER:
            API_sprintf(fname, "emvtrns%d.wtr", kernel_index);
            break;
        case _TRANSLOG:
            API_sprintf(fname, "emvtrns%d.log", kernel_index);
            break;
		case _DRL:
			API_sprintf(fname, "drl%d.par", kernel_index);
			break;
        default:
            return NULL;
    }

    return fname;
}





/*
************************************************************************************************************************
*                                                   LOCAL DATA TYPES
************************************************************************************************************************
*/

typedef struct {
    uint32_t amt;
    uint8_t pan[10];
    uint8_t panlen;
    uint8_t rsv;
} TRANSLOG;


int32_t emvporting_apipara_init(void)
{
    int32_t fd;

    if (g_apipara_ctx.mask == 0x74cfbde6)
    {
        return 0;
    }

    if ((fd = API_fopen(EMV_FNAME(_PARAM), F_RDWR)) <= 0)
    {
        if ((fd = API_fopen(EMV_FNAME(_PARAM), F_RDWR|F_CREAT)) <= 0)
        {
            return rERR(ErFOPEN);
        }

        API_memset(&g_apipara_ctx, 0, sizeof(apipara_context));
        g_apipara_ctx.kernel_index = 0;

        if (API_fwrite(fd, &g_apipara_ctx, sizeof(apipara_context)) != sizeof(apipara_context))
        {
            API_fclose(fd);
            return rERR(ErFWRITE);
        }
    }

    API_fseek(fd, 0, F_SEEK_SET);

    if (API_fread(fd, &g_apipara_ctx, sizeof(apipara_context)) != sizeof(apipara_context))
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    API_fclose(fd);
    g_apipara_ctx.mask = 0x74cfbde6;

    return 0;
}

int32_t emvporting_apipara_save(void)
{
    int32_t fd;

    if ((fd = API_fopen(EMV_FNAME(_PARAM), F_RDWR)) <= 0)
    {
        if ((fd = API_fopen(EMV_FNAME(_PARAM), F_RDWR|F_CREAT)) <= 0)
        {
            return rERR(ErFOPEN);
        }
    }

    API_fseek(fd, 0, F_SEEK_SET);

    if (API_fwrite(fd, &g_apipara_ctx, sizeof(apipara_context)) != sizeof(apipara_context))
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    API_fclose(fd);
    return 0;
}


void emvporting_get_kernel_index(uint8_t *val)
{
    emvporting_apipara_init();
    *val = g_apipara_ctx.kernel_index;
}

void emvporting_set_kernel_index(uint8_t val)
{
	/* 合法内核索引为 0..KERNEL_MAX_INX-1；KERNEL_MAX_INX 是内核个数 */
	if(val >= KERNEL_MAX_INX)
		val = 0;
    g_apipara_ctx.kernel_index = val;
    emvporting_apipara_save();
}

uint32_t emvporting_get_tsc(void)
{
    emvporting_apipara_init();
    return g_apipara_ctx.tsc;
}

void emvporting_set_tsc(uint32_t val)
{
    g_apipara_ctx.tsc = val;
    emvporting_apipara_save();
}

#define N_O_R_M_A_L__P_A_R__C

static void init_param(EMV_TERMPARAM* tpar, EMV_AIDPARAM* apar)
{
    if (apar)
    {
        API_memset(apar, 0, sizeof(EMV_AIDPARAM));
        apar->default_ddol_len = 11;
        API_memcpy(apar->default_ddol,"\x9F\x37\x04\x9F\x47\x01\x8F\x01\x9F\x32\x01", 11);
        apar->default_tdol_len = 3;
        API_memcpy(apar->default_tdol, "\x9F\x08\x02", 3);
		apar->kernel_id = 0x02;
        API_memcpy(apar->app_ver, "\x00\x02", 2);
    }

    if (tpar)
    {
        API_memset(tpar, 0, sizeof(EMV_TERMPARAM));
        API_memcpy(tpar->ics, "\xF4\xF0\xF0\xFC\xAF\xFE\xBC", 7);
    }
}

#define PARAM_LEN_LE(n)         if (valuelen > (n)) { return FALSE; } break
#define PARAM_LEN_EQ(n)         if (valuelen != (n)) { return FALSE; } break

/*
 * Check that a parameter value fits the fixed-size destination field it is
 * copied into.  Values from the host are untrusted: over-long values must be
 * rejected instead of overwriting the destination structure.
 */
static bool param_len_ok(uint32_t tag, int32_t valuelen)
{
    if (valuelen < 0)
    {
        return FALSE;
    }

    switch (tag)
    {
    case TAG_ICS:                                   PARAM_LEN_EQ(7);
    case TAG_STATUS_CHECK_SUPPORT_INDICATOR:        PARAM_LEN_EQ(1);
    case TAG_1F02_STATUS_CHECK_SUPPORT:             PARAM_LEN_EQ(1);
    case TAG_1F03_DEFAULT_TDOL:                     PARAM_LEN_LE(252);
    case TAG_9F01_ACQUIRER_IDENTIFIER:              PARAM_LEN_LE(6);
    case TAG_9F1E_IFD_SERIAL_NUMBER:                PARAM_LEN_LE(8);
    case TAG_9F15_MERCHANT_CATEGORY_CODE:           PARAM_LEN_LE(2);
    case TAG_9F16_MERCHANT_IDENTIFIER:              PARAM_LEN_LE(15);
    case TAG_9F4E_MERCHANT_NAME:                    PARAM_LEN_LE(128);
    case TAG_9F1A_TERMINAL_COUNTRY_CODE:            PARAM_LEN_EQ(2);
    case TAG_9F1C_TERMINAL_IDENTIFICATION:          PARAM_LEN_EQ(8);
    case TAG_9F7C_MERCHANT_CUSTOM_DATA:             PARAM_LEN_LE(20);
    case TAG_5F2A_TRANSACTION_CURRENCY_CODE:        PARAM_LEN_EQ(2);
    case TAG_5F36_TRANSACTION_CURRENCY_EXPONENT:    PARAM_LEN_EQ(1);
    case TAG_008B_POI_INFOMATION:                   PARAM_LEN_LE(128);
    case TAG_9F05_APPLICATION_DISCRETIONARY_DATA:   PARAM_LEN_LE(128);
    case TAG_9F06_AID_TERMINAL:                     if (valuelen < 1 || valuelen > 16) { return FALSE; } break;
    case TAG_9F35_TERMINAL_TYPE:                    PARAM_LEN_EQ(1);
    case TAG_DF01_ASI:                              PARAM_LEN_EQ(1);
    case TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL: PARAM_LEN_EQ(2);
    case TAG_DF17_TARGET_PERCENTAGE:                PARAM_LEN_EQ(1);
    case TAG_DF16_MAX_TARGET_PERCENTAGE:            PARAM_LEN_EQ(1);
    case TAG_DF15_THRESHOLD_VALUE:                  PARAM_LEN_EQ(4);
    case TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY:   PARAM_LEN_EQ(2);
    case TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT: PARAM_LEN_EQ(1);
    case TAG_DF11_TAC_DEFAULT:                      PARAM_LEN_EQ(5);
    case TAG_DF12_TAC_ONLINE:                       PARAM_LEN_EQ(5);
    case TAG_DF13_TAC_DENIAL:                       PARAM_LEN_EQ(5);
    case TAG_9F1D_TERMINAL_RISK_MANGEMENT_DATA:     PARAM_LEN_LE(8);
    case TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR:     PARAM_LEN_EQ(1);
    case TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT:    PARAM_LEN_EQ(6);
    case TAG_DF21_CVM_REQUIRED_LIMIT:               PARAM_LEN_EQ(6);
    case TAG_DF19_CONTACTLESS_FLOOR_LIMIT:          PARAM_LEN_EQ(6);
    case TAG_9F1B_TERMINAL_FLOOR_LIMIT:             if (valuelen != 4 && valuelen != 6) { return FALSE; } break;
    case TAG_DF811E_MSD_CVMCAP_CVMREQ:              PARAM_LEN_EQ(1);
    case TAG_DF8118_CVMCAP_CVMREQ:                  PARAM_LEN_EQ(1);
    case TAG_DF8119_CVMCAP_NOCVM:                   PARAM_LEN_EQ(1);
    case TAG_DF811B_KERNEL_CONFIG:                  PARAM_LEN_EQ(1);
    case TAG_DF8124_READER_CTL_TRX_LIMIT_NOCDCVM:   PARAM_LEN_LE(6);
    case TAG_DF8125_READER_CTL_TRX_LIMIT_CDCVM:     PARAM_LEN_LE(6);
    case TAG_DF812C_MSD_CVMCAP_NOCVM:               PARAM_LEN_EQ(1);
    case TAG_9F7E_MOBILE_SUPPORT_INDICATOR:         PARAM_LEN_EQ(1);
    case TAG_DF811F_SECURITY_CAP:                   PARAM_LEN_EQ(1);
    case TAG_9F33_TERMINAL_CAPABILITIES:            PARAM_LEN_EQ(3);
    case TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES: PARAM_LEN_EQ(5);
    case TAG_9F2A_KERNEL_ID:                        if (valuelen < 1 || valuelen > 8) { return FALSE; } break;
    default:
        break;
    }

    return TRUE;
}

#undef PARAM_LEN_LE
#undef PARAM_LEN_EQ

/*
 * Validate all parameter values of an already decoded TLV list.  Returns 0
 * when every known field fits, otherwise records the offending tag and
 * returns an error.
 */
static int32_t param_validate(EMV_TLVOBJ* tlvs)
{
    int32_t i;
    int32_t ret = 0;

    for (i = 1; i <= tlvs[0].childnum; ++i)
    {
        if (!param_len_ok(tlvs[i].tagname, tlvs[i].valuelen))
        {
            TRACE("emvport-error: param tag[%08X] len[%d] rejected",
                  (unsigned int)tlvs[i].tagname, tlvs[i].valuelen);
            ret = rERR(ILL_TERMPARAM);
        }
    }

    return ret;
}

static int32_t conv_param(EMV_TERMPARAM* tpar, EMV_AIDPARAM* apar, u8* in, int inlen)
{
    EMV_TLVOBJ tlvs[64];
    int i, tlvnum;
    int32_t ret = 0;
	unsigned char term_floor_lmt[4]={0};

    if (EMV_TLV_Decode(in, inlen, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), STRING_TLVOBJ))
    {
        TRACE("emvport-error: input param is not valid TLV format");
        API_memset(tlvs, 0, sizeof(tlvs));
        return rERR(ILL_TERMPARAM);
    }

    tlvnum = tlvs[0].childnum;

    for (i = 1; i <= tlvnum; ++i)
    {
        if (!param_len_ok(tlvs[i].tagname, tlvs[i].valuelen))
        {
            TRACE("emvport-error: param tag[%08X] len[%d] rejected",
                  (unsigned int)tlvs[i].tagname, tlvs[i].valuelen);
            ret = rERR(ILL_TERMPARAM);
            continue;
        }

        if (tpar)
        {
            switch (tlvs[i].tagname)
            {
            case TAG_ICS:
                API_memcpy(tpar->ics, tlvs[i].pvalue, 7);
                break;
            case TAG_STATUS_CHECK_SUPPORT_INDICATOR:
				if(tlvs[i].valuelen)
				{
					tpar->status_check_support_indicator = *(tlvs[i].pvalue);
				}
                break;
			//for authticate only
			case TAG_9F01_ACQUIRER_IDENTIFIER:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->acquirer_id, tlvs[i].pvalue, tlvs[i].valuelen);
				}
                break;
			case TAG_9F1E_IFD_SERIAL_NUMBER:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->term_ifd, tlvs[i].pvalue, tlvs[i].valuelen);
				}
				break;
            case TAG_9F15_MERCHANT_CATEGORY_CODE:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->merchant_category_code, tlvs[i].pvalue, tlvs[i].valuelen);
				}
                break;
            case TAG_9F16_MERCHANT_IDENTIFIER:
				if(tlvs[i].valuelen > 0)
				{
					API_memcpy(tpar->merchant_id, tlvs[i].pvalue, tlvs[i].valuelen);
				}
                break;
			case TAG_9F4E_MERCHANT_NAME:
				if(tlvs[i].valuelen > 0)
				{
					if(tlvs[i].valuelen > sizeof(tpar->merchant_name))
					{
						tpar->merchant_name_len = sizeof(tpar->merchant_name);
					}
					else
					{
						tpar->merchant_name_len = tlvs[i].valuelen;
					}
					API_memcpy(tpar->merchant_name, tlvs[i].pvalue, tpar->merchant_name_len);
				}
				break;
			case TAG_9F1A_TERMINAL_COUNTRY_CODE:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->term_country_code, tlvs[i].pvalue, sizeof(tpar->term_country_code));
				}
				break;
            case TAG_9F1C_TERMINAL_IDENTIFICATION:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->term_id, tlvs[i].pvalue, sizeof(tpar->term_id));
				}
                break;
			case TAG_9F7C_MERCHANT_CUSTOM_DATA:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->merch_custom_data, tlvs[i].pvalue, tlvs[i].valuelen);
				}
                break;
            case TAG_5F2A_TRANSACTION_CURRENCY_CODE:
				if(tlvs[i].valuelen)
				{
					API_memcpy(tpar->trans_currency_code, tlvs[i].pvalue, sizeof(tpar->trans_currency_code));
				}
                break;
			case TAG_5F36_TRANSACTION_CURRENCY_EXPONENT:
				if(tlvs[i].valuelen)
				{
					tpar->trans_currency_exponent = *(tlvs[i].pvalue);
				}
				break;
			case TAG_008B_POI_INFOMATION:
				if(tlvs[i].valuelen > sizeof(tpar->poi_info))
				{
					tpar->poi_info_len = sizeof(tpar->poi_info);
				}
				else
				{
					tpar->poi_info_len = tlvs[i].valuelen;
				}
				if(tpar->poi_info_len)
				{
					API_memcpy(tpar->poi_info, tlvs[i].pvalue, tpar->poi_info_len);
				}
				break;
			case TAG_9F05_APPLICATION_DISCRETIONARY_DATA:
				TRACE("emvport-info: set 9F05");
				if(tlvs[i].valuelen > sizeof(tpar->proprietary_tag))
				{
					tpar->proprietary_tag_len = sizeof(tpar->proprietary_tag);
				}
				else
				{
					tpar->proprietary_tag_len = tlvs[i].valuelen;
				}

				TRACE("emvport-info: tpar->proprietary_tag_len = %d", tpar->proprietary_tag_len);
				if(tpar->proprietary_tag_len)
				{
					API_memcpy(tpar->proprietary_tag, tlvs[i].pvalue, tpar->proprietary_tag_len);
				}
				break;
				tpar->trans_currency_exponent = *(tlvs[i].pvalue);

			default:
				break;
            }
        }

        if (apar)
        {
            switch (tlvs[i].tagname)
            {
            case TAG_9F06_AID_TERMINAL:
                apar->aid_len = tlvs[i].valuelen;
                API_memcpy(apar->aid, tlvs[i].pvalue, tlvs[i].valuelen);
                break;
			case TAG_9F35_TERMINAL_TYPE:
				if(tlvs[i].valuelen)
				{
					apar->term_type = *(tlvs[i].pvalue);
				}
				break;
            case TAG_DF01_ASI:
				if(tlvs[i].valuelen)
				{
					apar->app_sel_indicator = *(tlvs[i].pvalue);
				}
                break;
            case TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL:
				if(tlvs[i].valuelen)
				{
					API_memcpy(apar->app_ver, tlvs[i].pvalue, 2);
				}
                break;
            case TAG_DF17_TARGET_PERCENTAGE:
				if(tlvs[i].valuelen)
				{
					apar->target_percent = *(tlvs[i].pvalue);
				}
                break;
            case TAG_DF16_MAX_TARGET_PERCENTAGE:
				if(tlvs[i].valuelen)
				{
					apar->max_target_percent = *(tlvs[i].pvalue);
				}
                break;
            case TAG_DF15_THRESHOLD_VALUE:
				if(tlvs[i].valuelen)
				{
					API_memcpy(apar->threshold_value, tlvs[i].pvalue, 4);
				}
                break;
            case TAG_DF14_DEFAULT_DDOL:
                if (tlvs[i].valuelen < sizeof(apar->default_ddol) && tlvs[i].valuelen >= 0)
                {
                    apar->default_ddol_len = tlvs[i].valuelen;
                    API_memcpy(apar->default_ddol, tlvs[i].pvalue, tlvs[i].valuelen);
                }
                break;
            case TAG_DEFAULT_TDOL:
                if (tlvs[i].valuelen < sizeof(apar->default_tdol) && tlvs[i].valuelen >= 0)
                {
                    apar->default_tdol_len = tlvs[i].valuelen;
                    API_memcpy(apar->default_tdol, tlvs[i].pvalue, tlvs[i].valuelen);
                }
                break;
            case TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY:
				if(tlvs[i].valuelen)
				{
					API_memcpy(apar->trans_ref_currency_code, tlvs[i].pvalue, 2);
				}
                break;
            case TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT:
				if(tlvs[i].valuelen)
				{
					apar->trans_ref_currency_exponent = *(tlvs[i].pvalue);
				}
                break;
            case TAG_DF11_TAC_DEFAULT:
				if(tlvs[i].valuelen)
				{
					apar->term_action_code_default_exist = 1;
					API_memcpy(apar->term_action_code_default, tlvs[i].pvalue, 5);
				}
                break;
            case TAG_DF12_TAC_ONLINE:
				if(tlvs[i].valuelen)
				{
					apar->term_action_code_online_exist = 1;
					API_memcpy(apar->term_action_code_online, tlvs[i].pvalue, 5);
				}
                break;
            case TAG_DF13_TAC_DENIAL:
				if(tlvs[i].valuelen)
				{
					apar->term_action_code_denial_exist = 1;
					API_memcpy(apar->term_action_code_denial, tlvs[i].pvalue, 5);
				}
                break;
            case TAG_9F1D_TERMINAL_RISK_MANGEMENT_DATA:
                if (tlvs[i].valuelen <= sizeof(apar->term_risk_mana_data) && tlvs[i].valuelen >= 0)
                {
                    apar->term_risk_mana_data_len = tlvs[i].valuelen;
                    API_memcpy(apar->term_risk_mana_data, tlvs[i].pvalue, tlvs[i].valuelen);
                }
                break;
            case TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR:
                apar->online_pin_support_indicator = *(tlvs[i].pvalue);
                break;
            case TAG_STATUS_CHECK_SUPPORT_INDICATOR:
                apar->status_check_support_indicator = *(tlvs[i].pvalue);
                break;
            case TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT:
				if(tlvs[i].valuelen)
				{
					apar->cl_trans_limit_exist = 1;
					API_memcpy(apar->cl_trans_limit, tlvs[i].pvalue, 6);
				}
                break;
            case TAG_DF21_CVM_REQUIRED_LIMIT:
				if(tlvs[i].valuelen)
				{
					apar->cvm_limit_exist = 1;
					API_memcpy(apar->cvm_limit, tlvs[i].pvalue, 6);
				}
                break;
            case TAG_DF19_CONTACTLESS_FLOOR_LIMIT:
				if(tlvs[i].valuelen)
				{
					apar->cl_floor_limit_exist = 1;
					API_memcpy(apar->cl_floor_limit, tlvs[i].pvalue, 6);
				}
                break;
			//for authenticate only
            case TAG_9F1B_TERMINAL_FLOOR_LIMIT:
				if(tlvs[i].valuelen == 4)
				{
					apar->term_floor_limit_exist = 1;
					API_memcpy(apar->term_floor_limit, tlvs[i].pvalue, 4);
				}
				else if(tlvs[i].valuelen == 6)
				{
					/* 6-byte BCD (n12) -> 4-byte binary, big endian */
					uint64_t floor_dec = 0;
					bool floor_ok = TRUE;
					int32_t k;

					for (k = 0; k < 6; k++)
					{
						uint8_t hi = (uint8_t)((tlvs[i].pvalue[k] >> 4) & 0x0F);
						uint8_t lo = (uint8_t)(tlvs[i].pvalue[k] & 0x0F);

						if (hi > 9 || lo > 9)
						{
							floor_ok = FALSE;
							break;
						}
						floor_dec = floor_dec * 10 + hi;
						if (floor_dec > 0xFFFFFFFFULL)
						{
							floor_ok = FALSE;
							break;
						}
						floor_dec = floor_dec * 10 + lo;
						if (floor_dec > 0xFFFFFFFFULL)
						{
							floor_ok = FALSE;
							break;
						}
					}

					if (floor_ok)
					{
						emv_dec2hex((uint32_t)floor_dec, term_floor_lmt, sizeof(term_floor_lmt));
						apar->term_floor_limit_exist = 1;
						API_memcpy(apar->term_floor_limit, term_floor_lmt, sizeof(apar->term_floor_limit));
					}
					else
					{
						TRACE("emvport-error: 9F1B value invalid, rejected");
						ret = rERR(ILL_TERMPARAM);
					}
				}
				else
				{
					TRACE("emvport-error: 9F1B len[%d] rejected", tlvs[i].valuelen);
					ret = rERR(ILL_TERMPARAM);
				}
                break;
			case TAG_DF811E_MSD_CVMCAP_CVMREQ:
				if(tlvs[i].valuelen)
				{
					apar->msd_cvmcap_cvmreq = *(tlvs[i].pvalue);
				}
				break;
			case TAG_DF8118_CVMCAP_CVMREQ:
				if(tlvs[i].valuelen)
				{
					apar->cvmcap_cvmreq = *(tlvs[i].pvalue);
				}
				break;
			case TAG_DF8119_CVMCAP_NOCVM:
				if(tlvs[i].valuelen)
				{
					apar->cvmcap_nocvm = *(tlvs[i].pvalue);
				}
				break;
			case TAG_DF811B_KERNEL_CONFIG:
				if(tlvs[i].valuelen)
				{
					apar->paypass_kernel_config = *(tlvs[i].pvalue);
				}
				break;
			case TAG_DF8124_READER_CTL_TRX_LIMIT_NOCDCVM:
				if(tlvs[i].valuelen)
				{
					apar->reader_cl_trans_limit_nocdcvm_exist = 1;
					emv_memcpy(apar->reader_cl_trans_limit_nocdcvm, tlvs[i].pvalue, tlvs[i].valuelen);
				}
				break;
			case TAG_DF8125_READER_CTL_TRX_LIMIT_CDCVM:
				if(tlvs[i].valuelen)
				{
					apar->reader_cl_trans_limit_cdcvm_exist = 1;
					emv_memcpy(apar->reader_cl_trans_limit_cdcvm, tlvs[i].pvalue, tlvs[i].valuelen);
				}
				break;
			case TAG_DF812C_MSD_CVMCAP_NOCVM:
				if(tlvs[i].valuelen)
				{
					apar->msd_cvmcap_nocvm = *(tlvs[i].pvalue);
				}
				break;
			case TAG_9F7E_MOBILE_SUPPORT_INDICATOR:
				if(tlvs[i].valuelen > 0)
				{
					apar->mobile_sup_indicator = *(tlvs[i].pvalue);
				}
				break;
			case TAG_DF811F_SECURITY_CAP:
				if(tlvs[i].valuelen > 0)
				{
					apar->secure_cap = *(tlvs[i].pvalue);
				}
				break;
			case TAG_9F33_TERMINAL_CAPABILITIES:
				if(tlvs[i].valuelen)
				{
					emv_memcpy(apar->term_caps, tlvs[i].pvalue, tlvs[i].valuelen);
				}
				break;
			case TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES:
				if(tlvs[i].valuelen)
				{
					emv_memcpy(apar->term_add_caps, tlvs[i].pvalue, tlvs[i].valuelen);
				}
				break;
			case TAG_9F2A_KERNEL_ID:
				if(tlvs[i].valuelen)
				{
					apar->kernel_id = *(tlvs[i].pvalue);
				}
				break;

			default:
				break;
            }
        }
    }

    return ret;
}

#define T_E_R_M__P_A_R_A_M__C
int32_t emvporting_clr_termparam(void)
{
    TRACE("clr term");

    if (API_fremove(EMV_FNAME(_TERMPARAM)))
    {
        return rERR(ErFREMOVE);
    }
    return 0;
}
/*
���� emvporting_set_termparam(uint8_t* par, int32_t len)
���� par �ն˲���ֵ
	 len �ն˲�������
ע:
�ն˲�������һ�飬�ն˲����洢�������ն˵ĳ���(4���ֽ�)+�ն�����(ʵ�ʳ���)
*/

int32_t emvporting_set_termparam(uint8_t* par, int32_t len)
{
    int32_t fd;
    EMV_TLVOBJ tlvs[64];

    if (par == NULL || len <= 0 || len > PARA_MAXLEN)
    {
        return rERR(ErFUNCPAR);
    }

    /* 落盘前先做结构 + 字段长度校验，坏参数不得进入 Flash */
    if (EMV_TLV_Decode(par, len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), STRING_TLVOBJ))
    {
        TRACE("emvport-error: termparam is not valid TLV");
        return rERR(ILL_TERMPARAM);
    }
    if (param_validate(tlvs))
    {
        TRACE("emvport-error: termparam field length invalid");
        return rERR(ILL_TERMPARAM);
    }

    if ((fd = API_fopen(EMV_FNAME(_TERMPARAM), F_WRONLY|F_CREAT)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fwrite(fd, (void*)&len, PARA_LENLEN) != PARA_LENLEN)
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    if (API_fwrite(fd, par, len) != len)
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }
    TRACE_HEX(" w-terpara# ", par, len);

    API_fclose(fd);
    return 0;
}

int32_t emvporting_get_termparam(EMV_TERMPARAM* par)
{
    int32_t fd;
    uint8_t  buf[PARA_SAVLEN];
    int32_t len;
    uint8_t* ptr;

    if ((fd = API_fopen(EMV_FNAME(_TERMPARAM), F_RDONLY)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, 0, F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, buf, PARA_SAVLEN) < PARA_LENLEN)
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    API_fclose(fd);

    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN)
    {
        return rERR(ILL_TERMPARAMLEN);
    }
    TRACE_HEX(" r-terpara# ", buf + PARA_LENLEN, len);
    ptr = buf + PARA_LENLEN;
    init_param(par, NULL);
    conv_param(par, NULL, ptr, len);

    return 0;
}

int32_t emvporting_get_aidstr(uint8_t * par,int32_t inx)
{
	int32_t len;
	uint8_t buf[1024] = {0};
	{
		int32_t fd;
	    if ((fd = API_fopen(EMV_FNAME(_AIDPARAM), F_RDONLY)) <= 0)
	    {
	        return rERR(ErFOPEN);
	    }
	    if (API_fseek(fd, inx * PARA_SAVLEN, F_SEEK_SET) < 0)
	    {
	        API_fclose(fd);
	        return rERR(ErFSEEK);
	    }
	    if (API_fread(fd, buf, PARA_SAVLEN) != PARA_SAVLEN)
	    {
	        API_fclose(fd);
	        return rERR(ErFREAD);
	    }
	    API_fclose(fd);
	    API_memcpy(&len, buf, PARA_LENLEN);
	    if (len <= 0 || len > PARA_MAXLEN)
	    {
	        return rERR(ILL_AIDPARAMLEN);
	    }
	    TRACE_HEX("paypass aid buf", buf, len+4);

	    API_memcpy(par, buf+PARA_LENLEN, len);
	}
	return len;
}


int32_t emvporting_get_termparam_tlv(uint8_t* par, int32_t *rlen)
{
#if EMV_LINUX_SO
    uint32_t fd;
#else
    int32_t fd;
#endif
    uint8_t  buf[PARA_SAVLEN];
    int32_t len;
    uint8_t* ptr;

    TRACE("get term start");

    if ((fd = API_fopen(EMV_FNAME(_TERMPARAM), F_RDONLY)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, 0, F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, buf, PARA_SAVLEN) < PARA_LENLEN)
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    API_fclose(fd);

    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN)
    {
        return rERR(ILL_TERMPARAMLEN);
    }
    //TRACE_HEX(" r-terpara# ", buf + PARA_LENLEN, len);
//    ptr = buf + PARA_LENLEN;
//    init_param(par, NULL);
//    conv_param(par, NULL, ptr, len);
    API_memcpy(par, buf + PARA_LENLEN, len);
    *rlen = len;

    TRACE("rlen:%d", *rlen);
    TRACE_HEX("par", par, len);
    TRACE("get term success");

    return 0;
}
#define A_I_D__P_A_R_A_M__C

int32_t emvporting_clr_aidparam(void)
{
    TRACE("clr aid");

    if (API_fremove(EMV_FNAME(_AIDPARAM)))
    {
        return rERR(ErFREMOVE);
    }
    return 0;
}
/*
���� emvporting_add_aidparam(uint8_t* par, int32_t len)
���� par aidֵ
	 len aid����
ע:
aid����д��ͬһ���ļ�
ÿ��aid����ռ��1024���ֽ�
*/
int32_t emvporting_add_aidparam(uint8_t* par, int32_t len)
{
    int fd, ret, i=0;
    u8 buf[PARA_SAVLEN];
    EMV_TLVOBJ tlvs[64];
    int pos,pos_type = 0,pos_kernel_id=0;
    u8 aid[16];
    u8 aidlen;
	u8 trans_type,kernel_id=0x00;
	u8 *tmpbuf=NULL;
	bool trans_type_exist=TRUE;

    ////////
//    TRACE_HEX("input aid", par, len);

    if (par == NULL || len <= 0 || len > PARA_MAXLEN)
    {
        return rERR(ErFUNCPAR);
    }

	ret = EMV_TLV_Decode(par, len, &tlvs[0], sizeof(tlvs)/sizeof(tlvs[0]), STRING_TLVOBJ);
	if(ret != EMV_OK)
	{
		TRACE("emvport-error: input aid format is not invalid TLV");
		return rERR(ILL_AIDPARAM);
	}

	/* 落盘前校验所有已知字段长度（含 9C/9F2A，避免后续无长度保护解引用） */
	if (param_validate(&tlvs[0]))
	{
		TRACE("emvport-error: aid param field length invalid");
		return rERR(ILL_AIDPARAM);
	}

	pos = EMV_TLV_Find(0, TAG_9F06_AID_TERMINAL, &tlvs[0], SEARCH_ALL_DESC);
	if(0 == pos)
	{
		TRACE("emvport-error: input aid don't contain 9F06");
        return rERR(ILL_AIDPARAM);
	}

	//fot authenticate only
	pos_type = EMV_TLV_Find(0, TAG_009C_TRANSACTION_TYPE, tlvs, SEARCH_ALL_DESC);
	if(0 == pos_type)
	{
		TRACE("emvport-error: input aid don't contain 9C");
        return rERR(ILL_AIDPARAM);
	}

	trans_type = *(tlvs[pos_type].pvalue);

	pos_kernel_id = EMV_TLV_Find(0, TAG_9F2A_KERNEL_ID, tlvs, SEARCH_ALL_DESC);
	if(0 == pos_kernel_id)
	{
		TRACE("emvport-error: input aid don't contain 9F2A");
        return rERR(ILL_AIDPARAM);
	}
	else
	{
		kernel_id = *(tlvs[pos_kernel_id].pvalue);
	}


//	TRACE("trans_type = %02X", trans_type);
//	TRACE("kernel_id = %02X", kernel_id);

    if (tlvs[pos].valuelen > 16)
    {
        return rERR(ILL_AIDLEN);
    }

    aidlen = tlvs[pos].valuelen;
    API_memcpy(aid, tlvs[pos].pvalue, aidlen);

    ret = API_fsize(EMV_FNAME(_AIDPARAM));
    if (ret > 0 && (ret % PARA_SAVLEN) != 0)
    {
        TRACE("emvport-error: aid param len invalid");
        API_fremove(EMV_FNAME(_AIDPARAM));
    }

    if ((fd = API_fopen(EMV_FNAME(_AIDPARAM), F_RDWR|F_CREAT)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_SET);

    while (1)
    {
        ret = API_fread(fd, buf, PARA_SAVLEN);
        if (ret == PARA_SAVLEN)
        {
            API_memcpy(&ret, buf, PARA_LENLEN);
            if (ret <= 0 || ret > PARA_MAXLEN)
            {
				TRACE("emvport-error: read aid param len invalid");
                continue;
            }
            if (EMV_TLV_Decode(buf+PARA_LENLEN, ret, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), STRING_TLVOBJ))
            {
				TRACE("emvport-error: read aid param format invalid");
                continue;
            }
            if (!(pos = EMV_TLV_Find(0, TAG_9F06_AID_TERMINAL, tlvs, SEARCH_ALL_DESC)))
            {
				TRACE("emvport-error: read aid param contain no aid name");
                continue;
            }
            if (!(pos_type = EMV_TLV_Find(0, TAG_009C_TRANSACTION_TYPE, tlvs, SEARCH_ALL_DESC)))
            {
				TRACE("emvport-error: read aid param contain no trans type");
                continue;
            }
            if (!(pos_kernel_id = EMV_TLV_Find(0, TAG_9F2A_KERNEL_ID, tlvs, SEARCH_ALL_DESC)))
            {
				TRACE("emvport-error: read aid param contain no kernel id");
                continue;
            }

            if ((aidlen == tlvs[pos].valuelen) && \
				(API_memcmp(aid, tlvs[pos].pvalue, aidlen) == 0) && \
				((trans_type == *(tlvs[pos_type].pvalue)) || (trans_type == 0xFF)) && \
				((kernel_id == *(tlvs[pos_kernel_id].pvalue)) || (kernel_id == 0x00)))
            {
                API_fseek(fd, -(PARA_SAVLEN), F_SEEK_CUR);
                break;
            }
            else
            {
                continue;
            }
        }
        else if (ret == 0)
        {
            break;
        }
        else
        {
            API_fclose(fd);
            return rERR(ErFREAD);
        }
    }

    ////////

	API_memset(buf, 0, sizeof(buf));
	API_memcpy(buf, &len, PARA_LENLEN);
	API_memcpy(buf+PARA_LENLEN, par, len);
	ret = API_fwrite(fd, buf, PARA_SAVLEN);
//	TRACE("API_fwrite ret = %d", ret);
	if (ret != PARA_SAVLEN)
	{
		API_fclose(fd);
		return rERR(ErFWRITE);
	}

    API_fclose(fd);
    return 0;
}


/*
���� emvporting_get_aidparam(uint8_t* apar, int32_t inx)
���� par aidֵ
	 inx aid��ȡ������
ע:
aid����д��ͬһ���ļ�
ÿ��aid����ռ��1024���ֽ�
��ȡ��inx��Ŀ��aid
*/

int32_t emvporting_get_aidparam(EMV_AIDPARAM* apar, int32_t inx)
{
    int32_t fd,ret;
    uint8_t  buf[PARA_SAVLEN];
    int32_t len;
    uint8_t* ptr;
    EMV_TERMPARAM  tpar_buf;   /* scratch: term-level tags found in the AID record are not persisted (raw TLV read-back is used by kernels) */
    EMV_TERMPARAM* tpar = &tpar_buf;

    init_param(tpar, apar);

for(;;)
{
    if ((fd = API_fopen(EMV_FNAME(_TERMPARAM), F_RDONLY)) <= 0)
    {
        rERR(ErFOPEN);break;
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, 0, F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        rERR(ErFSEEK);break;
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, buf, PARA_SAVLEN) < PARA_LENLEN)
    {
        API_fclose(fd);
        rERR(ErFREAD);break;
        return rERR(ErFREAD);
    }

    ret = API_fclose(fd);
//    TRACE("colse read termpar API_fclose = %d", ret);

    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN)
    {
        rERR(ILL_AIDPARAMLEN);break;
        return rERR(ILL_AIDPARAMLEN);
    }

//    TRACE("len:%d", len);
//    TRACE_HEX("buf", buf, len+4);
    ptr = buf + PARA_LENLEN;
    conv_param(tpar, apar, ptr, len);
    break;
}

{
    if ((fd = API_fopen(EMV_FNAME(_AIDPARAM), F_RDONLY)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, inx * PARA_SAVLEN, F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, buf, PARA_SAVLEN) != PARA_SAVLEN)
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    ret = API_fclose(fd);
//    TRACE("colse read aid API_fclose = %d", ret);

    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN)
    {
        return rERR(ILL_AIDPARAMLEN);
    }
//    TRACE("len:%d", len);
//    TRACE_HEX("buf", buf, len+4);

    ptr = buf + PARA_LENLEN;
    conv_param(tpar, apar, ptr, len);
}

    return 0;
}

int32_t emvporting_get_aidparam_tlv(int32_t inx, uint8_t* par, int32_t *rlen)
{
#if EMV_LINUX_SO
    uint32_t fd;
#else
    int32_t fd;
#endif
    uint8_t  buf[PARA_SAVLEN];
    int32_t len;
    uint8_t* ptr;

    //TRACE("get aid start inx:%d", inx);

    if ((fd = API_fopen(EMV_FNAME(_AIDPARAM), F_RDONLY)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, inx * PARA_SAVLEN, F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, buf, PARA_SAVLEN) != PARA_SAVLEN)
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    API_fclose(fd);

    API_memcpy(&len, buf, PARA_LENLEN);
    if (len <= 0 || len > PARA_MAXLEN)
    {
        return rERR(ILL_AIDPARAMLEN);
    }

    API_memcpy(par, buf + PARA_LENLEN, len);
    *rlen = len;

    return 0;
}
/**
 * @brief ����Ӧ��AID��Ϣ
 * @param _core_index �ں���������
 * @param apps ���Ӧ��AID��Ϣ
 * @return [>=0]-�ɹ�����Ӧ�ü��� [-1]-ʧ��
 */
int32_t emvporting_get_termapps(EMV_TERMAPP* apps)
{
    int fd, ret;
    u8 apps_num = 0;
    EMV_TERMAPP* tmp = apps;
    u8 buf[PARA_SAVLEN];
    u8* par;
    int parlen;
    EMV_TLVOBJ tlvs[64];
    int pos;

    fd = API_fopen(EMV_FNAME(_AIDPARAM), F_RDONLY);
    if(fd <= 0)
    {
        return rERR(ErFOPEN);
    }

    while (1)
    {
        if (API_fread(fd, buf, PARA_SAVLEN) != PARA_SAVLEN)
        {
            if (apps_num > 0)
            {
                ret = apps_num;
            }
            else
            {
                ret = rERR(ErFREAD);
            }
            break;
        }

        API_memcpy(&parlen, buf, PARA_LENLEN);
        if (parlen <= 0 || parlen > PARA_MAXLEN)
        {
            TRACE("FIXME");
            parlen = 0;
        }

        par = buf + PARA_LENLEN;
        if (EMV_TLV_Decode(par, parlen, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), STRING_TLVOBJ))
        {
            TRACE("emvport-error: get aid param is not invalid TLV format");
			continue;
        }

//		TRACE_HEX("emvport-info: get aid param buf", par, parlen);

        /* 条目数达到约定上限后停止写入，避免越过 termapps 区域 */
        if (apps_num >= EMV_CFG_MAX_APPS)
        {
            TRACE("emvport-error: termapps exceeds max[%d]", EMV_CFG_MAX_APPS);
            ret = rERR(EMV_GETTERMAPPSERR);
            break;
        }

        /* 清零 */
        API_memset(tmp, 0, sizeof(EMV_TERMAPP));

        if ((pos = EMV_TLV_Find(0, TAG_9F06_AID_TERMINAL, tlvs, SEARCH_ALL_DESC)))
        {
            if (tlvs[pos].valuelen <= 16)//if (tlvs[pos].valuelen < 16)//xinyong 160218  �������16���ȵ�AIDѡ�񲻵�������
            {
                tmp->aid_len = tlvs[pos].valuelen;
                API_memcpy(tmp->aid, tlvs[pos].pvalue, tmp->aid_len);
                tmp->enable = 1;
            tmp->rsv = 1;   /* default: extended selection (9F29) supported; AID-level override TBD */
            }

            if ((pos = EMV_TLV_Find(0, TAG_DF01_ASI, tlvs, SEARCH_ALL_DESC)))
            {
                tmp->asi = *(tlvs[pos].pvalue);
            }
            else
            {
                tmp->asi = 0;
            }

			pos = EMV_TLV_Find(0, TAG_009C_TRANSACTION_TYPE, tlvs, SEARCH_ALL_DESC);
			if(pos != 0)
			{
				tmp->trans_type = *(tlvs[pos].pvalue);
			}
			else
			{
				tmp->trans_type = 0xFF;
			}

			pos = EMV_TLV_Find(0, TAG_9F2A_KERNEL_ID, tlvs, SEARCH_ALL_DESC);
			if(pos != 0)
			{
				tmp->kernel_id = *(tlvs[pos].pvalue);
			}

//			TRACE_HEX("emvport-info: get aid name", tmp->aid, tmp->aid_len);
//			TRACE("emvport-info: get aid trans type: %02X", tmp->trans_type);
//			TRACE("emvport-info: get aid kernrl id: %02X", tmp->kernel_id);

        }
		else
		{
            TRACE("emvport-error: get aid param doesn't contain 9F06");
			continue;
		}

        apps_num++;
        tmp++;
    }

    API_fclose(fd);
    return ret;
}


#define C_A_P_K__P_A_R_A_M__C

int32_t checkCAPKhash(EMV_CAPKPARAM* out)//xinyong 160304 ���ӹ�Կ���²���У�鹦��
{
    int32_t len;
    EMV_CAPKPARAM cert;
    uint8_t  mesg[300], digest[64];
#if 1
    API_memcpy((void*)&cert, (void*)out, sizeof(EMV_CAPKPARAM));

    if (cert.algorithm == EMV_PUBKEY_ALG_SM2)
    {
        API_memcpy(mesg, cert.rid, 5);
        len = 5;
        *(mesg + len) = cert.index;
        len += 1;

        API_memcpy(mesg + len, cert.key.sm2.r, 32);
        len += 32;
        API_memcpy(mesg + len, cert.key.sm2.s, 32);
        len += 32;
//ʹ��SH1У��hashֵ
        TRACE(" sm2 cert.algorithm:%02x cert.hash_algorithm:%02x ", cert.algorithm, cert.hash_algorithm);
        hash(EMV_HASH_ALG_SHA1, mesg, len, digest);
        if (API_memcmp(digest, cert.checksum, 20))
        {
//            if (ics_option(DA_Support_CAPKLoad_Fail_Action))
//            {
//                message(EMV_MSG_CAPK_Invalid);
//            }
            return rERR(DA_CAPKCHKERR);
        }

    }
    else if (cert.algorithm == EMV_PUBKEY_ALG_RSA)
    {
        if (cert.key.rsa.modulus_len == 0 ||
                (int32_t)cert.key.rsa.modulus_len > (int32_t)sizeof(cert.key.rsa.modulus))
        {
            return -1;
        }
        API_memcpy(mesg, cert.rid, 5);
        len = 5;
        *(mesg + len) = cert.index;
        len += 1;
        API_memcpy(mesg + len, cert.key.rsa.modulus, cert.key.rsa.modulus_len);
        len += cert.key.rsa.modulus_len;
        if (cert.key.rsa.exponent[0] == 0x00 &&
                cert.key.rsa.exponent[1] == 0x00 &&
                cert.key.rsa.exponent[2] == 0x03)
        {
            /* exponent 3 is encoded as a single byte */
            *(mesg + len) = cert.key.rsa.exponent[2];
            len += 1;
        }
        else
        {
            API_memcpy(mesg + len, cert.key.rsa.exponent, 3);
            len += 3;
        }
        /* EMV Book 2: RSA CAPK checksum is always SHA-1 over RID||Index||Modulus||Exponent */
        hash(EMV_HASH_ALG_SHA1, mesg, len, digest);
        if (API_memcmp(digest, cert.checksum, 20))
        {
            return rERR(DA_CAPKCHKERR);
        }
    }
    else
    {
        return -1;
    }
#endif

    return 0;
}

int32_t conv_capkparam(EMV_CAPKPARAM* out, uint8_t* in, int32_t inlen)
{
    EMV_TLVOBJ tlvs[20];
    int32_t pos;

    if (EMV_TLV_Decode(in, inlen, tlvs, sizeof(tlvs) / sizeof(tlvs[0]), STRING_TLVOBJ))
    {
        TRACE("CAPK TLV format invalid");
        return -1;
    }

    API_memset(out, 0, sizeof(EMV_CAPKPARAM));

    /* RID */
    if (!(pos = EMV_TLV_Find(0, TAG_9F06_AID_TERMINAL, tlvs, SEARCH_ALL_DESC)))
    {
        if (!(pos = EMV_TLV_Find(0, TAG_004F_AID, tlvs, SEARCH_ALL_DESC)))
        {
            return -2;
        }
    }
    if (tlvs[pos].valuelen != 5)
    {
        return -21;
    }
    API_memcpy(out->rid, tlvs[pos].pvalue, 5);

    /* INDEX */
    if (!(pos = EMV_TLV_Find(0, TAG_9F22_CAPK_INDEX, tlvs, SEARCH_ALL_DESC)))
    {
        return -3;
    }
    if (tlvs[pos].valuelen != 1)
    {
        return -31;
    }
    out->index = *(tlvs[pos].pvalue);

    /* ALGORITHM */
    if (!(pos = EMV_TLV_Find(0, TAG_DF07_CAPK_ALGORITHM_INDICATOR, tlvs, SEARCH_ALL_DESC)))
    {
        out->algorithm = EMV_PUBKEY_ALG_RSA;
//        return -4;
    }
    else
    {
        if (tlvs[pos].valuelen != 1)
        {
            return -41;
        }
        out->algorithm = *(tlvs[pos].pvalue);
    }

    /* HASH ALGORITHM */
    if (!(pos = EMV_TLV_Find(0, TAG_DF06_CAPK_HASH_ALGORITHM_INDICATOR, tlvs, SEARCH_ALL_DESC)))
    {
        out->hash_algorithm = EMV_HASH_ALG_SHA1;
//        return -5;
    }
    else
    {
        if (tlvs[pos].valuelen != 1)
        {
            return -51;
        }
        out->hash_algorithm = *(tlvs[pos].pvalue);
    }

    /* MODULUS */
    if (!(pos = EMV_TLV_Find(0, TAG_DF02_CAPK_MODULUS, tlvs, SEARCH_ALL_DESC)))
    {
        return -6;
    }
    if (tlvs[pos].valuelen > sizeof(out->key.rsa.modulus) || tlvs[pos].valuelen > 255)
    {
        return -61;
    }
    out->key.rsa.modulus_len = tlvs[pos].valuelen;
    API_memcpy(out->key.rsa.modulus, tlvs[pos].pvalue, tlvs[pos].valuelen);

    /* EXPONENT */
    if (!(pos = EMV_TLV_Find(0, TAG_DF04_CAPK_EXPONENT, tlvs, SEARCH_ALL_DESC)))
    {
        return -7;
    }
    if (tlvs[pos].valuelen == 1)
    {
        API_memcpy(out->key.rsa.exponent+2, tlvs[pos].pvalue, 1);
    }
    else if (tlvs[pos].valuelen == 3)
    {
        API_memcpy(out->key.rsa.exponent, tlvs[pos].pvalue, 3);
    }
    else
    {
        return -71;
    }

    /* EXPIRATION DATE */
    if (!(pos = EMV_TLV_Find(0, TAG_DF05_CAPK_EXPIRATION_DATE, tlvs, SEARCH_ALL_DESC)))
    {
        return -8;
    }

	TRACE("conv_capkparam: tlvs[pos].valuelen = %d", tlvs[pos].valuelen);
	if (tlvs[pos].valuelen == 3)
	{
        API_memcpy(out->expiration_date+1, tlvs[pos].pvalue, 3);
		out->expiration_date[0] = 0x20;
	}
    else if (tlvs[pos].valuelen == 4)
    {
        API_memcpy(out->expiration_date, tlvs[pos].pvalue, 4);
    }
    else if (tlvs[pos].valuelen == 8)
    {
        API_Conv_StrToBcd_Left((char*)tlvs[pos].pvalue, 8, out->expiration_date);
    }
    else
    {
        return -81;
    }

    /* CHECKSUM */
    if (!(pos = EMV_TLV_Find(0, TAG_DF03_CAPK_CHECKSUM, tlvs, SEARCH_ALL_DESC)))
    {
        API_memset(out->checksum, 0x00, 20);//
        out->hash_algorithm = 0x00;//��У��

//        return -9;
    }
    else
    {
        /* 结构体 checksum 只有 20 字节；EMV RSA/SM2 CAPK 校验值为 SHA-1 20 字节 */
        if (tlvs[pos].valuelen != 20)
        {
            return -91;
        }
        API_memcpy(out->checksum, tlvs[pos].pvalue, 20);

        if (checkCAPKhash(out))
        {
            TRACE("CAPK checksum verify failed");
            return rERR(DA_CAPKCHKERR);
        }
    }
    return 0;
}

static int32_t conv_tag_to_buf(uint32_t tag, uint8_t len, uint8_t* out, int32_t *rlen)
{
    int32_t i, j=0;
    uint8_t tagval;

    for(i=0;i<4;i++)
    {
        tagval = ((tag>>(24-8*i)) & 0xFF);
        if(tagval)
        {
            out[j++] = tagval;
        }
    }
    if(len>=0x80)
    {
        out[j++] = 0x81;
    }
    out[j++] = len;
    (*rlen) += j;


    return j;
}

static int32_t conv_capk_to_tlv(EMV_CAPKPARAM* in, uint8_t *out, int32_t *rlen)
{

    *rlen = 0;

    /* RID */
    conv_tag_to_buf(TAG_9F06_AID_TERMINAL, 0x05, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), in->rid, 5);
    (*rlen) += 5;

    /* INDEX */
    conv_tag_to_buf(TAG_9F22_CAPK_INDEX, 0x01, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), &in->index, 1);
    (*rlen) += 1;

    /* ALGORITHM */
    conv_tag_to_buf(TAG_DF07_CAPK_ALGORITHM_INDICATOR, 0x01, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), &in->algorithm, 1);
    (*rlen) += 1;

    /* HASH ALGORITHM */
    conv_tag_to_buf(TAG_DF06_CAPK_HASH_ALGORITHM_INDICATOR, 0x01, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), &in->hash_algorithm, 1);
    (*rlen) += 1;

    /* MODULUS */
    conv_tag_to_buf(TAG_DF02_CAPK_MODULUS, in->key.rsa.modulus_len, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), in->key.rsa.modulus, in->key.rsa.modulus_len);
    (*rlen) += in->key.rsa.modulus_len;

    /* EXPONENT */
    if(0 == API_memcmp(in->key.rsa.exponent, "\x00\x00", 2))
    {
        conv_tag_to_buf(TAG_DF04_CAPK_EXPONENT, 0x01, out+(*rlen), rlen);
        API_memcpy(out+(*rlen), &in->key.rsa.exponent[2], 1);
        (*rlen) += 1;
    }
    else
    {
        conv_tag_to_buf(TAG_DF04_CAPK_EXPONENT, 0x03, out+(*rlen), rlen);
        API_memcpy(out+(*rlen), in->key.rsa.exponent, 3);
        (*rlen) += 3;
    }

    /* EXPIRATION DATE */
    conv_tag_to_buf(TAG_DF05_CAPK_EXPIRATION_DATE, 0x04, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), in->expiration_date, 4);
    (*rlen) += 4;

    /* CHECKSUM */
    conv_tag_to_buf(TAG_DF03_CAPK_CHECKSUM, 0x14, out+(*rlen), rlen);
    API_memcpy(out+(*rlen), in->checksum, 20);
    (*rlen) += 20;

    return 0;
}

int32_t emvporting_clr_capkparam(void)
{
    if (API_fremove(EMV_FNAME(_CAPKPARAM)))
    {
//        return rERR(ErFREMOVE);
    }
    return 0;
}

int32_t emvporting_add_capkparam(uint8_t* par, int32_t len)
{
    int32_t fd, ret;
    EMV_CAPKPARAM capkpar, temp;

    if (par == NULL || len <= 0 || len > CAPKPARAM_MAXLEN)
    {
        return rERR(ErFUNCPAR);
    }

    if (ret=conv_capkparam(&capkpar, par, len))
    {
        TRACE("ret[%d]",ret);
        return rERR(ILL_CAPKPARAM);
    }

    len = API_fsize(EMV_FNAME(_CAPKPARAM));
    if (len > 0 && (len % sizeof(EMV_CAPKPARAM)) != 0)
    {
        TRACE("FIXME");
        API_fremove(EMV_FNAME(_CAPKPARAM));
    }

    fd = API_fopen(EMV_FNAME(_CAPKPARAM), F_RDWR|F_CREAT);
    if (fd <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_SET);

    while (1)
    {
        ret = API_fread(fd, &temp, sizeof(EMV_CAPKPARAM));
        if (ret == sizeof(EMV_CAPKPARAM))
        {
            if (temp.index == capkpar.index
                    && API_memcmp(temp.rid, capkpar.rid, 5) == 0)
            {
                ret = API_fseek(fd, -((int32_t)(sizeof(EMV_CAPKPARAM))), F_SEEK_CUR);
                if (ret < 0)
                {
                    API_fclose(fd);
                    return rERR(ErFSEEK);
                }
                break;
            }
            continue;
        }
        else if (ret == 0)
        {
            break;
        }
        else
        {
            API_fclose(fd);
            return rERR(ErFREAD);
        }
    }

    ret = API_fwrite(fd, &capkpar, sizeof(EMV_CAPKPARAM));
    if (ret != sizeof(EMV_CAPKPARAM))
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    API_fclose(fd);

    TRACE("add capk success");

    return 0;
}

int32_t emvporting_get_capkparam(EMV_CAPKPARAM* capk, uint8_t pki, const uint8_t* rid)
{
    int32_t ret;
    int32_t fd;
    EMV_CAPKPARAM pk;

//	TRACE("start emvporting_get_capkparam");

    fd = API_fopen(EMV_FNAME(_CAPKPARAM), F_RDONLY);
    if (fd <= 0)
    {
        return rERR(ErFOPEN);
    }

    while(1)
    {
        ret = API_fread(fd, &pk, sizeof(EMV_CAPKPARAM));
//		TRACE("API_fread ret = %d", ret);
		if (ret != sizeof(EMV_CAPKPARAM))
        {
            API_fclose(fd);
            return rERR(ErFREAD);
        }
        if (pk.index == pki && API_memcmp(pk.rid, rid, 5) == 0)
        {
//            TRACE("pki = %02x", pki);
//            TRACE_HEX("rid", (uint8_t*)rid, 5);
//            TRACE_HEX("capk", (uint8_t*)&pk, sizeof(EMV_CAPKPARAM));
            API_memcpy((void*)capk, (void*)&pk, sizeof(EMV_CAPKPARAM));
            ret = 0;
            break;
        }
    }
    API_fclose(fd);
    return ret;
}

int32_t emvporting_get_capkparam_tlv(int32_t inx, uint8_t* par, int32_t *rlen)
{
#if EMV_LINUX_SO
    uint32_t fd;
#else
    int32_t fd;
#endif

    uint8_t  buf[PARA_SAVLEN];
    int32_t len=0;
    EMV_CAPKPARAM pk;

    //TRACE("get capk start inx:%d", inx);

    fd = API_fopen(EMV_FNAME(_CAPKPARAM), F_RDONLY);
    if (fd <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, inx * sizeof(EMV_CAPKPARAM), F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, &pk, sizeof(EMV_CAPKPARAM)) != sizeof(EMV_CAPKPARAM))
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    API_fclose(fd);

    conv_capk_to_tlv(&pk, buf, &len);
    API_memcpy(par, buf, len);
    *rlen = len;

    //TRACE("rlen:%d", *rlen);
    //TRACE_HEX("par", par, len);
    //TRACE("get capk success");

    return 0;
}

#define C_A_R_D__B_L_A_C_K__C

int32_t emvporting_clr_cardblack(void)
{
    if (API_fremove(EMV_FNAME(_CARDBLACK)))
    {
        return rERR(ErFREMOVE);
    }
    return 0;
}

int32_t emvporting_add_cardblack(EMV_CARDBLACK* blk)
{
    int32_t fd, ret, len;
    EMV_CARDBLACK temp;

    if (blk == NULL)
    {
        return rERR(ErFUNCPAR);
    }

    len = API_fsize(EMV_FNAME(_CARDBLACK));
    if (len > 0 && (len % sizeof(EMV_CARDBLACK)) != 0)
    {
        API_fremove(EMV_FNAME(_CARDBLACK));
    }

    fd = API_fopen(EMV_FNAME(_CARDBLACK), F_RDWR|F_CREAT);
    if (fd <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_SET);

    while (1)
    {
        ret = API_fread(fd, &temp, sizeof(EMV_CARDBLACK));
        if (ret == sizeof(EMV_CARDBLACK))
        {
            if (blk->pan_len == temp.pan_len
                    && API_memcmp(blk->pan, temp.pan, temp.pan_len) == 0
                    && blk->psn == temp.psn)
            {
                TRACE("EXIST");
                API_fclose(fd);
                return 0;
            }
            continue;
        }
        else if (ret == 0)
        {
            break;
        }
        else
        {
            API_fclose(fd);
            return rERR(ErFREAD);
        }
    }

    ret = API_fwrite(fd, blk, sizeof(EMV_CARDBLACK));
    if (ret != sizeof(EMV_CARDBLACK))
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    API_fclose(fd);
    return 0;
}

int32_t emvporting_fnd_cardblack(const uint8_t* pan, int32_t panlen, uint8_t psn)
{
    int32_t fd, ret;
    EMV_CARDBLACK blk;

    if ((fd = API_fopen(EMV_FNAME(_CARDBLACK), F_RDONLY)) <= 0)
    {
        TRACE("Exception File Not Exist.");
        return 0;
    }

    while(1)
    {
        ret = API_fread(fd, &blk, sizeof(EMV_CARDBLACK));
        if (ret != sizeof(EMV_CARDBLACK))
        {
            ret = 0;
            break;
        }
        if (panlen == blk.pan_len
                && API_memcmp(pan, blk.pan, panlen) == 0
                && psn == blk.psn)
        {
            ret = 1;
            break;
        }
    }

    API_fclose(fd);
    return ret;
}

#define C_E_R_T__B_L_A_C_K__C

int32_t emvporting_clr_certblack(void)
{
    if (API_fremove(EMV_FNAME(_CERTBLACK)))
    {
        return rERR(ErFREMOVE);
    }
    return 0;
}

int32_t emvporting_add_certblack(EMV_CERTBLACK* blk)
{
    int32_t fd, ret, len;
    EMV_CERTBLACK temp;

    if (blk == NULL)
    {
        return rERR(ErFUNCPAR);
    }

    len = API_fsize(EMV_FNAME(_CERTBLACK));
    if (len > 0 && (len % sizeof(EMV_CERTBLACK)) != 0)
    {
        TRACE("FIXME");
        API_fremove(EMV_FNAME(_CERTBLACK));
    }

    fd = API_fopen(EMV_FNAME(_CERTBLACK), F_RDWR|F_CREAT);
    if (fd <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_SET);

    while (1)
    {
        ret = API_fread(fd, &temp, sizeof(EMV_CERTBLACK));
        if (ret == sizeof(EMV_CERTBLACK))
        {
            if (blk->pki == temp.pki
                    && API_memcmp(blk->rid, temp.rid, 5) == 0
                    && API_memcmp(blk->csn, temp.csn, 3) == 0)
            {
                TRACE("EXIST");
                API_fclose(fd);
                return 0;
            }
            continue;
        }
        else if (ret == 0)
        {
            break;
        }
        else
        {
            API_fclose(fd);
            return rERR(ErFREAD);
        }
    }

    ret = API_fwrite(fd, blk, sizeof(EMV_CERTBLACK));
    if (ret != sizeof(EMV_CERTBLACK))
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    API_fclose(fd);
    return 0;
}

int32_t emvporting_fnd_certblack(const uint8_t* rid, uint8_t pki, const uint8_t* csn)
{
    int32_t fd, ret;
    EMV_CERTBLACK blk;

    if ((fd = API_fopen(EMV_FNAME(_CERTBLACK), F_RDONLY)) <= 0)
    {
        TRACE("CRL Not Exist.");
        return 0;
    }

    while(1)
    {
        if (API_fread(fd, &blk, sizeof(EMV_CERTBLACK)) != sizeof(EMV_CERTBLACK))
        {
            ret = 0;
            break;
        }
        if (pki == blk.pki
                && API_memcmp(rid, blk.rid, 5) == 0
                && API_memcmp(csn, blk.csn, 3) == 0)
        {
            ret = 1;
            break;
        }
    }

    API_fclose(fd);
    return ret;
}


#define T_R_A_N_S__L_O_G__C


int32_t emvporting_clr_translog(void)
{
    if (API_fremove(EMV_FNAME(_TRANSLOG)))
        return rERR(ErFREMOVE);
    return 0;
}

int32_t emvporting_sav_translog(const uint8_t* pan, uint8_t panlen, uint32_t amount)
{
    int32_t fd;
    TRANSLOG log;

    CLEAR(log);
    log.amt = amount;

    if(panlen>10)//V2CL0620010����¼����257�����ݣ�PAN 5A����248��FF������ϵͳ����
    {
        API_memcpy(log.pan, pan, 10);
    }
    else
    {
        API_memcpy(log.pan, pan, panlen);
    }
    log.panlen = panlen;

    if ((fd = API_fopen(EMV_FNAME(_TRANSLOG), F_RDWR|F_CREAT)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_END);

    if (API_fwrite(fd, &log, sizeof(TRANSLOG)) != sizeof(TRANSLOG))
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    API_fclose(fd);
    return 0;
}

uint32_t emvporting_get_translog_amount(const uint8_t* pan, uint8_t panlen, uint8_t policy)
{
    int32_t fd;
    TRANSLOG log;
    uint32_t amount = 0;

    if ((fd = API_fopen(EMV_FNAME(_TRANSLOG), F_RDONLY)) <= 0)
    {
        return amount;
    }

    while (1)
    {
        if (API_fread(fd, &log, sizeof(TRANSLOG)) != sizeof(TRANSLOG))
        {
            break;
        }
        if (panlen == log.panlen
                && API_memcmp(pan, log.pan, panlen) == 0)
        {
            if (policy)
                amount += log.amt;
            else
                amount  = log.amt;
        }
    }

    API_fclose(fd);
    return amount;
}

#define T_R_A_N_S__W_A_T_E_R__C

int32_t emvporting_clr_transwater(void)
{
    if (API_fremove(EMV_FNAME(_TRANSWATER)))
        return rERR(ErFREMOVE);
    return 0;

}

int32_t emvporting_save_transwater(uint8_t* buf, int32_t len)
{
    int32_t fd;

    if (buf == NULL || len <= 0 || len > TRANSWATER_MAXLEN)
    {
        return rERR(ErFUNCPAR);
    }

    if ((fd = API_fopen(EMV_FNAME(_TRANSWATER), F_RDWR|F_CREAT)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_END);

    if (API_fwrite(fd, (uint8_t*)&len, 4) != 4)
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    if (API_fwrite(fd, buf, len) != len)
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    API_fclose(fd);
    return 0;
}

void emvporting_get_transwatername(char *name)
{
	//static char fname[13] = {0};
	API_strcpy(name, EMV_FNAME(_TRANSWATER));
	//return fname;
	//return EMV_FNAME(_TRANSWATER);
}

int32_t emvporting_get_transwater(int32_t inx, uint8_t* buf, int32_t *len)
{
    int32_t fd;
    int32_t length = 0;
    TRACE("size:%d inx:%d", API_fsize(EMV_FNAME(_TRANSWATER)), inx);

    if (buf == NULL || len == NULL || inx < 0)
    {
        return rERR(ErFUNCPAR);
    }

    if ((fd = API_fopen(EMV_FNAME(_TRANSWATER), F_RDONLY)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    if (API_fseek(fd, inx, F_SEEK_SET) < 0)
    {
        API_fclose(fd);
        return rERR(ErFSEEK);
    }

    if (API_fread(fd, (uint8_t*)&length, 4) != 4)
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    if (length <= 0 || length > TRANSWATER_MAXLEN)
    {
        API_fclose(fd);
        return rERR(ILL_TERMPARAMLEN);
    }

    /* 防止文件被截断/半写时按垃圾长度读取 */
    if (API_fsize(EMV_FNAME(_TRANSWATER)) < (inx + 4 + length))
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }

    if (API_fread(fd, buf, length) != length)
    {
        API_fclose(fd);
        return rERR(ErFREAD);
    }
    TRACE("length:%d", length);
    *len = length;
    API_fclose(fd);
    return 0;

}


int32_t emvporting_write_file(char *filename, uint32_t filesize, uint8_t *buf, uint32_t buflen, uint32_t flag)
{
    int32_t fd;
    static int32_t size = 0;
    int32_t filerealsize = 0;

    if(1 == flag)
    {
        size = 0;
        API_fremove(filename);
    }

    filerealsize = API_fsize(filename);
    TRACE("size:%d filesize:%d filerealsize:%d", size, filesize, filerealsize);

    if(size>=filesize || (filerealsize>0&&filerealsize>=filesize))
    {
        TRACE("size:%d filesize:%d filerealsize:%d", size, filesize, filerealsize);
        return rERR(ErWRCOMPLETE);
    }

    if ((fd = API_fopen(filename, F_RDWR|F_CREAT)) <= 0)
    {
        return rERR(ErFOPEN);
    }

    API_fseek(fd, 0, F_SEEK_END);

    if (API_fwrite(fd, buf, buflen) != buflen)
    {
        API_fclose(fd);
        return rERR(ErFWRITE);
    }

    size += buflen;//��д����ļ�����

    API_fclose(fd);
    return 0;
}
static int32_t conv_drlparam(BCTC_DRL* out, uint8_t* in, int32_t inlen)
{
    EMV_TLVOBJ tlvs[10];
    int32_t pos;

    if (EMV_TLV_Decode(in, inlen, tlvs, sizeof(tlvs) / sizeof(tlvs[0]), STRING_TLVOBJ))
    {
        TRACE("drlTLV error");
        return -1;
    }

    API_memset(out, 0, sizeof(BCTC_DRL));

    if (pos = EMV_TLV_Find(0, 0xDF22, tlvs, SEARCH_ALL_DESC))
    {
		if((tlvs[pos].pvalue[0] & 0x80) == 0x80)
		{
        	out->Flag = 1;
			out->Index = tlvs[pos].pvalue[0];//default index is 0xf0;
		}
		else
		{
			out->Flag = 0;

			out->Index = tlvs[pos].pvalue[0] & 0x0F;
		}
    }
    if (pos = EMV_TLV_Find(0, 0xDF19, tlvs, SEARCH_ALL_DESC))
	{
		if(6 == tlvs[pos].valuelen)
		{
			emv_memcpy(out->CLROFFLimit, tlvs[pos].pvalue, 6);
		}
		else
		{
			emv_memcpy(out->CLROFFLimit, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
		}
	}
	else
	{
		emv_memcpy(out->CLROFFLimit, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
	}

	if (pos = EMV_TLV_Find(0, 0xDF20, tlvs, SEARCH_ALL_DESC))
	{
		if(6 == tlvs[pos].valuelen)
		{
			emv_memcpy(out->CLRTLimit, tlvs[pos].pvalue, 6);
		}
		else
		{
			emv_memcpy(out->CLRTLimit, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
		}
	}
	else
	{
		emv_memcpy(out->CLRTLimit, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
	}
	if (pos = EMV_TLV_Find(0, 0xDF21, tlvs, SEARCH_ALL_DESC))
	{
		if(6 == tlvs[pos].valuelen)
		{
			emv_memcpy(out->CVMLimit, tlvs[pos].pvalue, 6);
		}
		else
		{
			emv_memcpy(out->CVMLimit, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
		}
	}
	else
	{
		emv_memcpy(out->CVMLimit, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);
	}
    return 0;
}

int32_t emvporting_clr_drl(void)
{
    if (API_fremove(EMV_FNAME(_DRL)))
    {
        return rERR(ErFREMOVE);
    }
    return 0;
}

int32_t emvporting_add_drl(void * par, int32_t len)
{
	int32_t fd, ret, rlen;
	BCTC_DRL DRL;
	BCTC_DRL rDRL;

	if (par == NULL)
	{
		return rERR(ErFUNCPAR);
	}
	TRACE_HEX("emvporting_add_drl", par,len);

    if (conv_drlparam(&DRL, par, len))
    {
        return rERR(ILL_DRL);
    }

	rlen = API_fsize(EMV_FNAME(_DRL));
	if (rlen > 0 && (rlen % sizeof(BCTC_DRL)) != 0)
	{
		TRACE("FIXME");
		API_fremove(EMV_FNAME(_DRL));
	}

	fd = API_fopen(EMV_FNAME(_DRL), F_RDWR|F_CREAT);
	if (fd <= 0)
	{
		return rERR(ErFOPEN);
	}

	API_fseek(fd, 0, F_SEEK_SET);

	while (1)
	{
		ret = API_fread(fd, &rDRL, sizeof(BCTC_DRL));

		if (ret == sizeof(BCTC_DRL))
		{
			if (rDRL.Index == DRL.Index)
			{
				TRACE("EXIST");
				API_fclose(fd);
				return 0;
			}
			continue;
		}
		else if (ret == 0)
		{
			break;
		}
		else
		{
			API_fclose(fd);
			return rERR(ErFREAD);
		}
	}

	TRACE_HEX("drl write",(void *)&DRL,sizeof(BCTC_DRL));

	ret = API_fwrite(fd, (void *)&DRL, sizeof(BCTC_DRL));
	if (ret != sizeof(BCTC_DRL))
	{
		API_fclose(fd);
		return rERR(ErFWRITE);
	}

	API_fclose(fd);
	return 0;
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
