/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     emvicc.c
* @author   Kingsee Development Team xinyong
* @version  V43
* @date     2015��10��21��
* @brief    IC Card Command-Response APDUs
*
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/

#include "emvi.h"


/*
************************************************************************************************************************
*                                                     LOCAL DEFINES
************************************************************************************************************************
*/

#define LE_EXIST    1
#define LE_NULL     0


/*
************************************************************************************************************************
*                                                   LOCAL DATA TYPES
************************************************************************************************************************
*/

typedef struct
{
    uint8_t  cla, ins, p1, p2, lc, le;
    uint8_t* pdata;
} capdu_t;

bool stop_flag = false;


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

#if SELF_APDU_TEST

	struct TESTLOG
	{

		uint8_t asSendCmd[4*2+1];

		uint8_t asSendData[255*2+1];

		uint8_t asRecvData[300*2+4+1];

	};

	struct TESTLOG log[]=
	{
		//PPSE
		{"00A40400", "325041592E5359532E4444463031","6F23840E325041592E5359532E4444463031A511BF0C0E610C4F07A00000000310108701019000"},
		//Select
		{"00A40400", "A0000000031010", "6F568407A0000000031010A54B500A564953414352454449548701019F38189F66049F02069F03069F1A0295055F2A029A039C019F37045F2D026A619F1101019F120A56495341435245444954BF0C089F5A0500000000019000"},
		//GPO
		{"80A80000", "", "777882022020940C0804040018030300100307005713FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF5F3401009F1007060112039000009F2608DF41EB94C6ACDC4F9F2701409F360200A59F6C023C009F6E04207000009F7C20011E0000278131313439303030303030303030303030303030303030303030309000"},
		//Read Record
		{"00B2040C", "", "700A5F280203929F070200009000"},
		//Read Record
		{"00B2031C", "", "70105A08FFFFFFFFFFFFFFFF5F24032811309000"},
		//Read Record
		{"00B20314", "", "702C9F3201039223DCF19B2392B32ABE8EADE6DF4CDC43037EABEF7E8EF7186462F74C517ABEF5738C0DBF8F01099000"},
		//Read Record
		{"00B20414", "", "7081FB9081F877B9DCB4F9D65C04E8FFCC38338ED9EA694B50004B5A5672B8C4026FE5A84A31360D7A3C27937AA42454E005F480D7548CC57A0F33565E3F8602C828DBB4F6815F92115615CC4F3981FD8CEC214012D0017F9C9BB0D77531D91E7C6AF9BBAEB426C98D011B20D8BCD17DAF552F98A0EB675CB689FA8866EFEFE74453F2B411695796E9AF4E180E370469EF8E8665E75F1FEAD346FC230CB93A4E4B58CBF19F2B9817A7393526BA71DB31558BA4F126EA7C2C3FC1737E85CF1CF1C663FE8BA90D9A59B0B480159B842F235CCFE891BB21F8DF8D3F5705FFFFFFFFFFF4CAAD49416F46A00FA35857DA1F4C29714F1BB1710A0874409E1C8E1C9000"},
		//Read Record
		{"00B20514", "", "7081FB9F4681F769CB8CBCB02F8971106D8A94F20A51BB5B27DF656CF2A770A5F7F9FCB1B4C9ACB63C504BB496186A7C6B484733C0CE2297808BBE1B639BEEE8087817703344D7342692DA0798868A1FBF14F4D2613DC05766414101FD81BA6CE27C525504F718554AEB1BC88715AD4A280215356595D8D8FE2896C6FD48ABC5ABC3303E09D35E0543F7E06DBC60CDA13F4B0D739ED6E029A6B54AD540512BA764FD724EA581CEF8BE7C9FBD1C14D5EB3C6C562B7E462E6DE5FB3393E3D1FED3B19A12175DECF00D8B901335A68A9702378EC560D5A0551A29B71E705374B095AA59F17CABB1B3A682D2ECF8A60EB1FAA7B26402656E995F772632E5D3FA9000"},
		//Read Record
		{"00B20614", "", "7081849F4B8180A829EB9EAE6ADB9F3C9C7BA8F76CEAED5FB38D65E055537E3D19469980B94F159295ACB8BB820C7159E3EDF4E619A4EECD879464EF0679FA0AA2F586CB5D6C7ECD14303C957BDDC17AE1A82C64B6215C89D9878E4F7FF5D5F569F863132707289C4DA15451100CD4EE035A165044A50FBF04858DCE567057E03ACFCCC2D6A32C9000"},
		//Read Record
		{"00B20714", "", "700E9F4701039F6907015A6A9A0E3C009000"},

		//GAC
//		{"80AE5000", "", "7781A79F2701409F360200019F4B8190A9ABB53AB32B382929B3F5C200AD9E73BAC6F6E06F6E093F0771F5B165FECB6B0397716AC231762CD5D1D69FBD3F6E23314484A1B9A65B61B2906EE2981C1D73C6E5AFF893FC1F07DF7790EB19F2DE1B82E2D29395ED0E62B1A8D922BD30DC1EEBAFB4C9ADA8931B8E1872F984FA4F2FAEA9458875D554398295569F1DC65D38FC64B0F54EFACFE84B7717070ABF520B9F1007060101030000009000"},
	};

#endif



#if SELF_APDU_TEST

static int32_t icccommand(capdu_t* capdu, uint8_t* output, int32_t* outlen)
{
    int32_t slen = 0, rlen = 0, sw = 0x0000;
    uint8_t  send[EMV_CFG_ICC_SEND_MAXLEN];
    uint8_t  recv[EMV_CFG_ICC_RECV_MAXLEN];
	int32_t k=0;
	uint8_t tempcmd[9]={0},tempdata[255*2+1];
	uint8_t sw_bcd[2]={0};


    emv_memset(send, 0, sizeof(send));
    send[slen++] = capdu->cla;
    send[slen++] = capdu->ins;
    send[slen++] = capdu->p1;
    send[slen++] = capdu->p2;
    if (capdu->lc)
    {
        if (capdu->pdata == NULL)
        {
            return sw;
        }
        send[slen++] = capdu->lc;
        emv_memcpy(send + slen, capdu->pdata, capdu->lc);
        slen += capdu->lc;
    }

    if (capdu->le)
    {
        send[slen++] = 0x00;
    }

	eTRACE("slen = %d", slen);
	eTRACE_HEX("<<<<C_APDU<<<<", send, slen);

    emv_memset(recv, 0, sizeof(recv));

	emv_memset(tempcmd,0,sizeof(tempcmd));

	emv_bcd2str(send, 4, tempcmd);

	emv_memset(tempdata, 0, sizeof(tempdata));

	emv_bcd2str(capdu->pdata, capdu->lc, tempdata);

	eTRACE("lc=%d\r\n",capdu->lc);

	eTRACE("cmd=%s\r\n",tempcmd);

	eTRACE("data=%s\r\n",tempdata);

	for(k=0;k<sizeof(log)/sizeof(log[0]); k++)
	{

		if(!emv_memcmp(tempcmd, log[k].asSendCmd, 8) && !emv_memcmp(tempdata, log[k].asSendData, emv_strlen(log[k].asSendData)))

		{
			eTRACE("start Assignment\r\n");

			rlen = (emv_strlen(log[k].asRecvData)-4)/2;
			emv_str2bcd(log[k].asRecvData, emv_strlen(log[k].asRecvData)-4, recv, (emv_strlen(log[k].asRecvData)-4)/2, false);
			emv_str2bcd(log[k].asRecvData+(emv_strlen(log[k].asRecvData)-4), 4, sw_bcd, 2, false);
			sw = emv_hex2dec(sw_bcd, sizeof(sw_bcd));

			break;
		}
	}


	eTRACE_HEX("<<<<R_APDU<<<<", recv, rlen);
    eTRACE("<<<<SW1SW2<<<<[%04X]", sw);

    if (sw == 0x0000)
    {
        return sw;
    }

    if (rlen > 0)
    {
        if (output != NULL)
        {
            emv_memcpy(output, recv, rlen);
        }
        if (outlen != NULL)
        {
            *outlen = rlen;
        }
    }

    return sw;
}


#else

static int32_t icccommand(capdu_t* capdu, uint8_t* output, int32_t* outlen)
{
    int32_t slen = 0, rlen = 0, sw = 0x0000;
    uint8_t  send[EMV_CFG_ICC_SEND_MAXLEN];
    uint8_t  recv[EMV_CFG_ICC_RECV_MAXLEN];
//	uint8_t  recv_copy[EMV_CFG_ICC_RECV_MAXLEN];
//    uint8_t  send_copy[EMV_CFG_ICC_SEND_MAXLEN];
	int32_t mask_len,i=0;

    emv_memset(send, 0, sizeof(send));
    send[slen++] = capdu->cla;
    send[slen++] = capdu->ins;
    send[slen++] = capdu->p1;
    send[slen++] = capdu->p2;
    if (capdu->lc)
    {
        if (capdu->pdata == NULL)
        {
            return sw;
        }
        send[slen++] = capdu->lc;
        emv_memcpy(send + slen, capdu->pdata, capdu->lc);
        slen += capdu->lc;
    }
    if (capdu->le)
    {
        send[slen++] = 0x00;
    }

//	emv_memset(send_copy, 0, sizeof(send_copy));
//	emv_memcpy(send_copy, send, slen);

//	while (i < slen)
//	{
//		if(0x57 == send_copy[i])
//		{
//			mask_len = send_copy[i+1];
//			if(mask_len > 0 &&  mask_len <= 19)
//			{
//				emv_memset(&(send_copy[i+2]), 0xFF, mask_len);
//				i++;
//				i += mask_len;
//			}
//			else
//			{
//				i++;
//			}
//		}
//		else if(0x5A == send_copy[i])
//		{
//			mask_len = send_copy[i+1];
//			if(mask_len > 0 &&  mask_len <= 10)
//			{
//				emv_memset(&(send_copy[i+2]), 0xFF, mask_len);
//				i++;
//				i += mask_len;
//			}
//			else
//			{
//				i++;
//			}
//		}
//		else
//		{
//			i++;
//		}
//	}
	apduTRACE("slen = %d", slen);
	apduTRACE_HEX("<<<<Mask_C_APDU<<<<", send, slen);

    emv_memset(recv, 0, sizeof(recv));

	eTRACE("stop_flag = %d", stop_flag);

	if(stop_flag)
	{
		return -106;
	}

    sw = EMVPORT_IccRW(g_emv_cnxt->card_type, send, slen, recv, &rlen);

//	emv_memset(recv_copy, 0, sizeof(recv_copy));
//	emv_memcpy(recv_copy, recv, rlen);
//
//	i = 0;
//	while (i < rlen)
//	{
//		if(0x57 == recv_copy[i])
//		{
//			mask_len = recv_copy[i+1];
//			if(mask_len > 0 &&  mask_len <= 19)
//			{
//				emv_memset(&(recv_copy[i+2]), 0xFF, mask_len);
//				i++;
//				i += mask_len;
//			}
//			else
//			{
//				i++;
//			}
//		}
//		else if(0x5A == recv_copy[i])
//		{
//			mask_len = recv_copy[i+1];
//			if(mask_len > 0 &&  mask_len <= 10)
//			{
//				emv_memset(&(recv_copy[i+2]), 0xFF, mask_len);
//				i++;
//				i += mask_len;
//			}
//			else
//			{
//				i++;
//			}
//		}
//		else
//		{
//			i++;
//		}
//	}

	eTRACE("stop_flag = %d", stop_flag);
	if(stop_flag)
	{
		return -106;
	}
	apduTRACE_HEX("<<<<Mask_R_APDU<<<<", recv, rlen);
    apduTRACE("<<<<SW1SW2<<<<[%04X]", sw);

    if (sw == 0x0000)
    {
        return sw;
    }

    if (rlen > 0)
    {
        if (output != NULL)
        {
            emv_memcpy(output, recv, rlen);
        }
        if (outlen != NULL)
        {
            *outlen = rlen;
        }
    }

    return sw;
}

#endif


/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/

void emvicc_set_stop_flag(bool flag)
{
	eTRACE("app set stop flag = %d", flag);
	stop_flag = flag;
}

bool emvicc_get_stop_flag()
{
	return stop_flag;
}

int32_t emvicc_apdu(uint8_t* input, int32_t inlen, uint8_t* output, int32_t* outlen)
{
    return EMVPORT_IccRW(g_emv_cnxt->card_type, input, inlen, output, outlen);
}

int32_t emvicc_external_authenticate(uint8_t *data, int32_t datalen)
{
    capdu_t capdu;
    capdu.cla   = 0x00;
    capdu.ins   = 0x82;
    capdu.p1    = 0x00;
    capdu.p2    = 0x00;
    capdu.lc    = (uint8_t)datalen;
    capdu.pdata = data;
    capdu.le    = LE_NULL;
    return icccommand(&capdu, NULL, NULL);
}

int32_t emvicc_generate_ac(uint8_t type, bool cda, uint8_t *cdolv, int32_t cdolvlen, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x80;
    capdu.ins   = 0xAE;
    capdu.p1    = (uint8_t)(type | ((cda) ? (0x10) : (0x00)));
    capdu.p2    = 0x00;
    capdu.lc    = (uint8_t)cdolvlen;
    capdu.pdata = cdolv;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_get_challenge(uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x00;
    capdu.ins   = 0x84;
    capdu.p1    = 0x00;
    capdu.p2    = 0x00;
    capdu.lc    = 0x00;
    capdu.pdata = NULL;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_get_data(uint32_t tag, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x80;
    capdu.ins   = 0xCA;
    capdu.p1    = (uint8_t)((tag >> 8) & 0xFF);
    capdu.p2    = (uint8_t)((tag     ) & 0xFF);
    capdu.lc    = 0x00;
    capdu.pdata = NULL;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_get_processing_options(uint8_t *pdolv, int32_t pdolvlen, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x80;
    capdu.ins   = 0xA8;
    capdu.p1    = 0x00;
    capdu.p2    = 0x00;
    capdu.lc    = (uint8_t)pdolvlen;
    capdu.pdata = pdolv;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_internal_authenticate(uint8_t *ddolv, int32_t ddolvlen, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x00;
    capdu.ins   = 0x88;
    capdu.p1    = 0x00;
    capdu.p2    = 0x00;
    capdu.lc    = (uint8_t)ddolvlen;
    capdu.pdata = ddolv;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_read_record(uint8_t sfi, uint8_t recno, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x00;
    capdu.ins   = 0xB2;
    capdu.p1    = recno;
    capdu.p2    = ((sfi & 0x1F) << 3) | 0x04;
    capdu.lc    = 0x00;
    capdu.pdata = NULL;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_select(uint8_t *name, uint8_t namelen, bool next, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x00;
    capdu.ins   = 0xA4;
    capdu.p1    = 0x04;
    capdu.p2    = (next) ? (0x02) : (0x00);
    capdu.lc    = namelen;
    capdu.pdata = name;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_verify(uint8_t type, uint8_t *pin, uint8_t pinlen)
{
    capdu_t capdu;
    capdu.cla   = 0x00;
    capdu.ins   = 0x20;
    capdu.p1    = 0x00;
    capdu.p2    = type;
    capdu.lc    = pinlen;
    capdu.pdata = pin;
    capdu.le    = LE_NULL;
    return icccommand(&capdu, NULL, NULL);
}

int32_t emvicc_spi(uint8_t *pdolv, int32_t pdolvlen, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x80;
    capdu.ins   = 0x1A;
    capdu.p1    = 0x00;
    capdu.p2    = 0x00;
    capdu.lc    = (unsigned char)pdolvlen;
    capdu.pdata = pdolv;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_relay_resistance_protocol(uint8_t *term_rr_entropy, int32_t len, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x80;
    capdu.ins   = 0xEA;
    capdu.p1    = 0x00;
    capdu.p2    = 0x00;
    capdu.lc    = (uint8_t)len;
    capdu.pdata = term_rr_entropy;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_compute_crypyographic_checksum(uint8_t *udolv, int32_t udolvlen, uint8_t *out, int32_t *outlen)
{
    capdu_t capdu;
    capdu.cla   = 0x80;
    capdu.ins   = 0x2A;
    capdu.p1    = 0x8E;
    capdu.p2    = 0x80;
    capdu.lc    = (uint8_t)udolvlen;
    capdu.pdata = udolv;
    capdu.le    = LE_EXIST;
    return icccommand(&capdu, out, outlen);
}

int32_t emvicc_put_data(uint32_t tag, uint8_t *in, int32_t inlen)
{
    capdu_t capdu;
	uint8_t out[4]={0};
	int32_t outlen;


    capdu.cla   = 0x80;
    capdu.ins   = 0xDA;
    capdu.p1    = (uint8_t)((tag >> 8) & 0xFF);
    capdu.p2    = (uint8_t)((tag     ) & 0xFF);
    capdu.lc    = (inlen);
    capdu.pdata = in;
    capdu.le    = LE_NULL;
    return icccommand(&capdu, out, &outlen);
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
