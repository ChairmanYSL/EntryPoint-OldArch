/**
************************************************************************************************************************
* @file     express_offline_data_authentication.c
* @brief    Expresspay 内核：离线数据认证（Offline Data Authentication：SDA/DDA/CDA）
*
* 移植来源：美运 V4.2.3 submodule/emvco/src/emv_offline_data_authentication.c（逐行移植，只改符号前缀与共享服务调用）
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
* @file     emv_offline_data_authentication.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015年10月21日
* @brief    脱机数据认证（Offline Data Authentication）
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

#define HASH_ALGORITHM  0x01
#define RSA_ALGORITHM   0x01
#define SM3_ALGORITHM   0x04
#define SM2_ALGORITHM   0x04

#define ISSPK_CERT      0x0002
#define ISSPK_CERT_SM2  0x0012
#define SSAD_CERT       0x0003
#define SSAD_CERT_SM2   0x0013
#define ICCPK_CERT      0x0004
#define ICCPK_CERT_SM2  0x0014
#define PINPK_CERT      0x0104

#define DATA_HEADER     0x6A
#define DATA_TAILER     0xBC


/*
************************************************************************************************************************
*                                                   LOCAL DATA TYPES
************************************************************************************************************************
*/

/* Certificate */
typedef struct {
    u8* _cert;
    int _cert_len;
    u8* _pk_remainder;
    int _pk_remainder_len;
    u8* _pk_exponent;
    int _pk_exponent_len;
    u8* _pan;
    int _pan_len;
}certificate;


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

static int express_retrieve_capk(EMV_PUBLICKEY* capk)
{
    int len;
    EMV_CAPKPARAM cert;
    u8  mesg[300], digest[64];
    u8 *rid = appdata_find(TAG_004F_AID, NULL);
    u8 *pki = appdata_find(TAG_008F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX, NULL);

    if (pki == NULL)
    {
		eTRACE("AMEX-error: miss capk index");
        return (rERR(DA_NOCAPKI),DA_ICCDATAMISSING);
    }

    if (EMVPORT_GetCapkParam(&cert, *pki, rid))
    {
        if (ics_option(DA_Support_CAPKLoad_Fail_Action))
        {
            EMVPORT_Message(EMV_MSG_CAPK_NotFound);
        }
        return rERR(DA_CAPKNOTFOUNT);
    }

    if (ics_option(DA_Support_CAPK_Checksum))
    {
        if (cert.algorithm == EMV_PUBKEY_ALG_SM2)
        {
            eTRACE("[TODO]实现国密算法");
            return rERR(DA_ERR);
        }
        else if (cert.algorithm == EMV_PUBKEY_ALG_RSA)
        {
            emv_memcpy(mesg, cert.rid, 5);
            len = 5;
            *(mesg + len) = cert.index;
            len += 1;
            emv_memcpy(mesg + len, cert.key.rsa.modulus, cert.key.rsa.modulus_len);
            len += cert.key.rsa.modulus_len;
            if (cert.key.rsa.exponent[2] == 0x03)
            {
                *(mesg + len) = cert.key.rsa.exponent[2];
                len += 1;
            }
            else
            {
                emv_memcpy(mesg + len, cert.key.rsa.exponent, 3);
                len += 3;
            }
            EMVPORT_Hash(cert.hash_algorithm, mesg, len, digest);
            if (emv_memcmp(digest, cert.checksum, 20))
            {
                if (ics_option(DA_Support_CAPKLoad_Fail_Action))
                {
                    EMVPORT_Message(EMV_MSG_CAPK_Invalid);
                }
                return rERR(DA_CAPKCHKERR);
            }
        }
    }

    emv_memcpy(capk, &(cert.key), sizeof(EMV_PUBLICKEY));

    return 0;
}

static int express_retrieve_pk_from_icc(certificate *cert, int certtype, EMV_PUBLICKEY *pkin, EMV_PUBLICKEY *pkout)
{
    int pos = 0, cert_cap_len, i, ilen, tlen;
    u8* ptmp;
    u8 buf[256];
    u8 calc[2048]; /* hash source, plus 42/36 remainder + 3 exponent */
    u8 digest[20];
    u8 hash[32];
    u8 date[2];
    u8 tmp[10];
	int ret;

    u8* rid = appdata_find(TAG_004F_AID, NULL);
    u8* pki = appdata_find(TAG_008F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX, NULL);
    if (!pki)
    {
        tvr_set(ICC_DATA_MISSING);
        return -1;
    }
//    eTRACE("cert_len = [%d], modulus_len = [%d]", cert->_cert_len, pkin->rsa.modulus_len);

    if (cert->_cert_len != pkin->rsa.modulus_len)
    {
        return rERR(DA_CERTLENERR);
    }

	ret = EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, pkin, cert->_cert, buf);
    if (ret <= 0)
    {
        return rERR(DA_CERTRECOVERERR);
    }

    if (certtype == ICCPK_CERT)
    {
        if ((0x6A != buf[0]) || (0x04 != buf[1]) || (0xBC != buf[cert->_cert_len - 1])) return rERR(DA_CERTFMTERR);
    }

    if (certtype == ISSPK_CERT)
    {
        if ((0x6A != buf[0]) || (0x02 != buf[1]) || (0xBC != buf[cert->_cert_len - 1])) return rERR(DA_CERTFMTERR);
        pos = 6; /* for issuer public key certificate, Certificate Expiration Date starts from buf[6] */
    }
    else
    {
        pos = 12; /* for the other certificate, it starts from buf[12] */
    }

    /* the part of the modulus that can be stored in the recovered certificate, is the lefmost (modulus len -  42/36) bytes */
    cert_cap_len = cert->_cert_len - (pos + 30);
    /* see whether the public key remainder shall exis */
    if (buf[pos + 7] > cert_cap_len)
    {
        if (cert->_pk_remainder == NULL)
        {
            return (rERR(DA_NOPKREM),DA_ICCDATAMISSING);
        }
        if (cert->_pk_remainder_len + cert_cap_len != buf[pos + 7])
        {
            return rERR(DA_PKMODLENERR);
        }
    }

    /* hash value checking */
    emv_memcpy(calc, buf + 1, cert->_cert_len - 22);
    emv_memcpy(calc + cert->_cert_len - 22, cert->_pk_remainder, cert->_pk_remainder_len);
    emv_memcpy(calc + cert->_cert_len - 22 + cert->_pk_remainder_len, cert->_pk_exponent, buf[pos + 8]);
    tlen = cert->_cert_len - 22 + cert->_pk_remainder_len + buf[pos + 8];
    if (certtype == ICCPK_CERT)
    {
        /* only for ICC PK Certificate */
        if ((ptmp = appdata_find(TAG_AUTH_DATA, &ilen)))
        {
            emv_memcpy(calc + tlen, ptmp, ilen);
        }
        tlen += ilen;

        if ((ptmp = appdata_find(TAG_9F4A_SDA_TAG_LIST, &ilen)))
        {
            /* if Static Data Authentication Tag List is present */
            if (ilen == 1 && *ptmp == TAG_0082_AIP)
            {
                emv_memcpy(calc + tlen, appdata_find(TAG_0082_AIP, NULL ), 2);
                tlen += 2;
            }
            else
            {
                return rERR(DA_9F4AERR);
            }
        }
    }
    if (certtype == ISSPK_CERT)
    {
        EMVPORT_Hash(EMV_HASH_ALG_SHA1, calc, cert->_cert_len - 22 + cert->_pk_remainder_len + buf[pos + 8], hash);
        if (emv_memcmp(hash, buf + cert->_cert_len - 21, 20))
		{
			return rERR(DA_CERTHASHVERR);
		}
         // check expdata
         date[0] = buf[pos +1];
         date[1] = buf[pos];
        if (emv_date_compare(date, 2) < 0)
		{
			return rERR(DA_CERTEXPIRED);
		}
        // check revocList
        if(g_express_cnxt->term.is_support_revoke_capk)
        {
			if (EMVPORT_FndBlackCert(rid, pki[0], buf + 8))
			{
				return rERR(DA_CERTBLOCKED);
			}
		}

        if ((buf[11] != 0x01) || (buf[12] != 0x01))
		{
			return rERR(DA_CERTALGERR);
		}

    }
    else if (certtype == ICCPK_CERT)
    {
        EMVPORT_Hash(EMV_HASH_ALG_SHA1, calc, tlen, hash);
        if (emv_memcmp(hash, buf + cert->_cert_len - 21, 20)) return rERR(DA_CERTHASHVERR);
        date[0] = buf[pos + 1];
        date[1] = buf[pos];
        if (emv_date_compare(date, 2) < 0) return rERR(DA_CERTEXPIRED);
        if (buf[17] != 0x01 || buf[18] != 0x01) return rERR(DA_CERTALGERR);

        emv_memcpy(tmp, cert->_pan, cert->_pan_len);
        if (cert->_pan_len < 10)
        {
            emv_memset(tmp + cert->_pan_len, 0xFF, 10 - cert->_pan_len);
        }
        if (emv_memcmp(buf + 2, tmp, 10)) return rERR(DA_PANNOTMATCHED);
    }

    /* Verify the Issuer Identifier or PAN */ //360705FF
    ilen = 0;
    for (i = 0; i < pos - 2; i++)
    {
        if ((certtype == ICCPK_CERT || certtype == PINPK_CERT) && i == cert->_pan_len) // EMV4.3C
        {
            break;
        }

        if (buf[i + 2] != *(cert->_pan + i))
        {
            if ((buf[i + 2] & 0xF0) == 0xF0)
            {
                ilen = i * 2;
            }
            else if ((buf[i + 2] & 0x0F) == 0x0F)
            {
                ilen = i * 2 + 1;
            }

            if (certtype == ISSPK_CERT)  // EMV4.3C
            {
                if (ilen < 3)
                {
                    return rERR(DA_PANNOTMATCHED);
                }
            }
            else // EMV4.3C
            {
                return rERR(DA_PANNOTMATCHED);
            }
        }
    }

    /* now generate the outputing public key */
    emv_memset(pkout, 0x00, sizeof(EMV_PUBLICKEY));
    pkout->rsa.modulus_len = buf[pos + 7];
    if (pkout->rsa.modulus_len > cert_cap_len)
    {
        /* the remainder is existing */
        emv_memcpy(pkout->rsa.modulus, buf + pos + 9, cert_cap_len);
        emv_memcpy(pkout->rsa.modulus + cert_cap_len, cert->_pk_remainder, cert->_pk_remainder_len);
    }
    else
    {
        emv_memcpy(pkout->rsa.modulus, buf + pos + 9, pkout->rsa.modulus_len);
    }

    if (buf[pos + 8] == 3)
    {
        /* the exponent is 2^16 + 1 */
        emv_memcpy(pkout->rsa.exponent, cert->_pk_exponent, 3);
    } else {
        /* otherwise it is 3 */
        pkout->rsa.exponent[2] = *(cert->_pk_exponent);
    }
    return 0;
}


static int express_retrieve_pk_from_icc_temp(certificate *cert, int certtype, EMV_PUBLICKEY *pkin, EMV_PUBLICKEY *pkout)
{
    int pos = 0, cert_cap_len, i, ilen, tlen;
    u8* ptmp,*AIP;
    u8 buf[256];
    u8 calc[2048]; /* hash source, plus 42/36 remainder + 3 exponent */
    u8 digest[20];

    u8* rid = appdata_find(TAG_004F_AID, NULL);
    u8* pki = appdata_find(TAG_008F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX, NULL);

    eTRACE("cert_len = [%d], modulus_len = [%d]", cert->_cert_len, pkin->rsa.modulus_len);
    eTRACE("AMEX-info: cert type = %d", certtype);

    if (cert->_cert_len != pkin->rsa.modulus_len)
    {
		eTRACE("AMEX-error: IPK Cert len != ICCPK Modul len");
        return rERR(DA_CERTLENERR);
    }

	tlen = EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, pkin, cert->_cert, buf);
    eTRACE("AMEX-info: EMVPORT_Recover ret = %d", tlen);
    if (tlen <= 0)
    {
        return rERR(DA_CERTRECOVERERR);
    }

	eTRACE_HEX("AMEX-info: Recover data from PK Cert", buf, tlen);

    if (certtype == ICCPK_CERT)
    {
        if ((0x6A != buf[0]) || (0x04 != buf[1]) || (0xBC != buf[cert->_cert_len - 1]))
		{
			eTRACE("AMEX-error: ICCPK Cert format invalid");
			return rERR(DA_CERTFMTERR);
		}
    }


    if (certtype == ISSPK_CERT)
    {
        if ((0x6A != buf[0]) || (0x02 != buf[1]) || (0xBC != buf[cert->_cert_len - 1]))
		{
			eTRACE("AMEX-error: IPK Cert format invalid");
			return rERR(DA_CERTFMTERR);
		}
        pos = 6; /* for issuer public key certificate, Certificate Expiration Date starts from buf[6] */
    }
    else
    {
        pos = 12; /* for the other certificate, it starts from buf[12] */
    }

    /* the part of the modulus that can be stored in the recovered certificate, is the lefmost (modulus len -  42/36) bytes */
    cert_cap_len = cert->_cert_len - (pos + 30);
    /* see whether the public key remainder shall exis */
    if (buf[pos + 7] > cert_cap_len)
    {
        if (cert->_pk_remainder == NULL)
        {
			if(ISSPK_CERT == certtype)
			{
				eTRACE("AMEX-error: IPK Reminder miss");
			}
			else if(ICCPK_CERT == certtype)
			{
				eTRACE("AMEX-error: ICCPK Reminder miss");
			}
            return (rERR(DA_NOPKREM),DA_ICCDATAMISSING);
        }
        if (cert->_pk_remainder_len + cert_cap_len != buf[pos + 7])
        {
			if(ISSPK_CERT == certtype)
			{
				eTRACE("AMEX-error: IPK Reminder len invalid");
			}
			else if(ICCPK_CERT == certtype)
			{
				eTRACE("AMEX-error: ICCPK Reminder len invalid");
			}
            return rERR(DA_PKMODLENERR);
        }
    }

    /* hash value checking */
    emv_memcpy(calc, buf + 1, cert->_cert_len - 22);
    emv_memcpy(calc + cert->_cert_len - 22, cert->_pk_remainder, cert->_pk_remainder_len);
    emv_memcpy(calc + cert->_cert_len - 22 + cert->_pk_remainder_len, cert->_pk_exponent, buf[pos + 8]);
    tlen = cert->_cert_len - 22 + cert->_pk_remainder_len + buf[pos + 8];
    if (certtype == ICCPK_CERT)
    {
        /* only for ICC PK Certificate */
        if ((ptmp = appdata_find(TAG_AUTH_DATA, &ilen)))
        {
			eTRACE_HEX("AMEX-info: Auth Data", AIP, ilen);
            emv_memcpy(calc + tlen, ptmp, ilen);
        }
        tlen += ilen;

        if ((ptmp = appdata_find(TAG_9F4A_SDA_TAG_LIST, &ilen)))
        {
			eTRACE_HEX("AMEX-info: 9F4A", ptmp, ilen);
            /* if Static Data Authentication Tag List is present */
            if (ilen == 1 && *ptmp == TAG_0082_AIP)
            {
				AIP = appdata_find(TAG_0082_AIP, &ilen);
				eTRACE_HEX("AMEX-info: AIP", AIP, ilen);
                emv_memcpy(calc + tlen, appdata_find(TAG_0082_AIP, NULL ), 2);
                tlen += 2;
            }
            else
            {
				eTRACE("AMEX-error: Static Data Authentication Tag List invalid");
                return rERR(DA_9F4AERR);
            }
        }
    }

    EMVPORT_Hash(EMV_HASH_ALG_SHA1, calc, tlen, digest);
    if (emv_memcmp(digest, buf + cert->_cert_len - 21, 20) != 0)
    {
		if(ISSPK_CERT == certtype)
		{
			eTRACE("AMEX-error: Hash from IPK Cert and Retrieval ICCPK is different");
		}
		else if(ICCPK_CERT == certtype)
		{
			eTRACE("AMEX-error: Hash from ICCPK Cert and Retrieval ICCPK is different");
		}
        return rERR(DA_CERTHASHVERR);
    }

    /* Verify the Issuer Identifier or PAN */
    ilen = 0;
    for (i = 0; i < pos - 2; i++)
    {
        if ((certtype == ICCPK_CERT || certtype == PINPK_CERT) && i == cert->_pan_len) // EMV4.3C
        {
            break;
        }

        if (buf[i + 2] != *(cert->_pan + i))
        {
            if ((buf[i + 2] & 0xF0) == 0xF0)
            {
                ilen = i * 2;
            }
            else if ((buf[i + 2] & 0x0F) == 0x0F)
            {
                ilen = i * 2 + 1;
            }

            if (certtype == ISSPK_CERT)  // EMV4.3C
            {
                if (ilen < 3)
                {
                    return rERR(DA_PANNOTMATCHED);
                }
            }
            else // EMV4.3C
            {
                return rERR(DA_PANNOTMATCHED);
            }
        }
    }

    /* Verify the Certificate Expiration Date */
    digest[0] = *(buf + pos + 1);
    digest[1] = *(buf + pos);
    if (emv_date_compare(digest, 2) < 0)
    {
		eTRACE("AMEX-error: PK Cert Expired");
        return rERR(DA_CERTEXPIRED);
    }

    if (certtype == ISSPK_CERT && ics_option(DA_Support_IPKC_Revoc_Check))
    {
        /* verify that the concatenation of RID, Certification Authority Public Key
         Index, and Certificate Serial Number is valid.
         this step is optional and is to allow the revocation of the Issuer Public
         Key Certificate against a list that may be kept by the terminal */
        if (EMVPORT_FndBlackCert(rid, *pki, buf + pos + 2))
        {
			eTRACE("AMEX-error: PK Cert In Revocation List");
            return rERR(DA_CERTBLOCKED);
        }
    }

    /* check public key pk_algorithm */
    if (buf[pos + 6] != RSA_ALGORITHM)
    {
        return rERR(DA_CERTALGERR);
    }

    /* now generate the outputing public key */
    emv_memset(pkout, 0x00, sizeof(EMV_PUBLICKEY));
    pkout->rsa.modulus_len = buf[pos + 7];
    if (pkout->rsa.modulus_len > cert_cap_len)
    {
        /* the remainder is existing */
        emv_memcpy(pkout->rsa.modulus, buf + pos + 9, cert_cap_len);
        emv_memcpy(pkout->rsa.modulus + cert_cap_len, cert->_pk_remainder, cert->_pk_remainder_len);
    }
    else
    {
        emv_memcpy(pkout->rsa.modulus, buf + pos + 9, pkout->rsa.modulus_len);
    }

    if (buf[pos + 8] == 3)
    {
        /* the exponent is 2^16 + 1 */
        emv_memcpy(pkout->rsa.exponent, cert->_pk_exponent, 3);
    } else {
        /* otherwise it is 3 */
        pkout->rsa.exponent[2] = *(cert->_pk_exponent);
    }

    return 0;
}

static int express_retrieve_pk_from_icc_sm2(certificate *cert, int certtype, EMV_PUBLICKEY *pkin, EMV_PUBLICKEY *pkout)//xinyong 160220
{
    eTRACE("[TODO]实现国密算法");
    return rERR(DA_ERR);

}

static int express_retrieve_issuer_pk(EMV_PUBLICKEY *capk, EMV_PUBLICKEY* isspk)
{
    certificate cert;

    emv_memset(&cert, 0x00, sizeof(cert));
    /* prepare the issuer public key certificate related data objects */
    if ((cert._cert = appdata_find(TAG_0090_ISSUER_PUBLIC_KEY_CERTIFICATE, &(cert._cert_len))) == NULL)
    {
        return (rERR(DA_NOISSPKCERT),DA_ICCDATAMISSING);
    }
    if ((cert._pk_exponent = appdata_find(TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT, &(cert._pk_exponent_len))) == NULL)
    {
        return (rERR(DA_NOISSPKEXP),DA_ICCDATAMISSING);
    }
    cert._pk_remainder = appdata_find(TAG_0092_ISSUER_PUBLIC_KEY_REMAINDER, &(cert._pk_remainder_len)); /* optional */
    cert._pan = appdata_find(TAG_005A_APPLICATION_PAN, &(cert._pan_len)); /* PAN must be existing from readrec.c */
    if (!cert._pan)
	{
		eTRACE("AMEX-info: IPK Cert miss pan");
		return rERR(DA_IPKCERTNOPAN);
    }

//	eTRACE_HEX("AMEX-info: IPK Cert", cert._cert, cert._cert_len);
//	eTRACE_HEX("AMEX-info: IPK Reminder", cert._pk_remainder, cert._pk_remainder_len);

	/* retrieve the issuer public key from ICC which is encrypted by CAPK */
//	return express_retrieve_pk_from_icc_temp(&cert, ISSPK_CERT, capk, isspk);
    return express_retrieve_pk_from_icc(&cert, ISSPK_CERT, capk, isspk);
}

static int express_retrieve_issuer_pk_sm2(EMV_PUBLICKEY *capk, EMV_PUBLICKEY* isspk)
{
    eTRACE("[TODO]实现国密算法");
    return rERR(DA_ERR);
}


static int express_retrieve_icc_pk(EMV_PUBLICKEY *isspk, EMV_PUBLICKEY *iccpk)
{
    certificate cert;

    emv_memset(&cert, 0x00, sizeof(cert));
    /* prepare the icc public key certificate related data objects */
    if ((cert._cert = appdata_find(TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE, &(cert._cert_len))) == NULL)
    {
        return (rERR(DA_NOICCPKCERT),DA_ICCDATAMISSING);
    }
    if ((cert._pk_exponent = appdata_find(TAG_9F47_ICC_PUBLIC_KEY_EXPONENT, &(cert._pk_exponent_len))) == NULL)
    {
        return (rERR(DA_NOICCPKEXP),DA_ICCDATAMISSING);
    }
    cert._pk_remainder = appdata_find(TAG_9F48_ICC_PUBLIC_KEY_REMAINDER, &(cert._pk_remainder_len)); /* optional */
    cert._pan = appdata_find(TAG_005A_APPLICATION_PAN, &(cert._pan_len)); /* PAN must be existing from readrec.c */

	eTRACE_HEX("AMEX-info: ICCPK Cert", cert._cert, cert._cert_len);
	eTRACE_HEX("AMEX-info: ICCPK Reminder", cert._pk_remainder, cert._pk_remainder_len);
    /* retrieve the icc public key from ICC which is encrypted by issuer pk */
    return express_retrieve_pk_from_icc(&cert, ICCPK_CERT, isspk, iccpk);
}

static int express_retrieve_icc_pk_sm2(EMV_PUBLICKEY *isspk, EMV_PUBLICKEY *iccpk)
{
    eTRACE("[TODO]实现国密算法");
    return rERR(DA_ERR);
}

static int express_retrieve_pin_pk(EMV_PUBLICKEY *isspk, EMV_PUBLICKEY *pinpk)
{
    certificate cert;

    emv_memset(&cert, 0x00, sizeof(cert));
    /* prepare the pin encipherment public key certificate related data objects */
    if ((cert._cert = appdata_find(TAG_9F2D_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_CERTIFICATE, &(cert._cert_len))) == NULL)
    {
        return (rERR(DA_NOPINPKCERT),DA_ICCDATAMISSING);
    }
    if ((cert._pk_exponent = appdata_find(TAG_9F2E_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_EXPONENT, &(cert._pk_exponent_len))) == NULL)
    {
        return (rERR(DA_NOPINPKEXP),DA_ICCDATAMISSING);
    }
    cert._pk_remainder = appdata_find(TAG_9F2F_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_REMAINDER, &(cert._pk_remainder_len)); /* optional */
    cert._pan = appdata_find(TAG_005A_APPLICATION_PAN, &(cert._pan_len)); /* PAN must be existing from readrec.c */

    /* retrieve the icc public key from ICC which is encrypted by issuer pk */
    return express_retrieve_pk_from_icc(&cert, PINPK_CERT, isspk, pinpk);
}


static int express_verify_ssad(EMV_PUBLICKEY* isspk)
{
    u8* ptmp;
    u8* hashv;
    int ilen, tlen;
    u8 digest[20];
    u8 buf[256];
    u8 calc[2048];

    if (!(ptmp = appdata_find(TAG_0093_SIGNED_STATIC_APPLICATION_DATA, &ilen)))
    {
        tvr_set(ICC_DATA_MISSING);
        return (rERR(DA_NOSSAD),DA_ICCDATAMISSING);
    }

    if (ilen != isspk->rsa.modulus_len) return rERR(DA_SSADLENERR);

    if (EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, isspk, ptmp, buf) <= 0)
    {
        return rERR(DA_SSADRECOVERERR);
    }
    if ((0x6A != buf[0]) || (0x03 != buf[1]) || (0xBC != buf[ilen - 1])) return rERR(DA_SSADFMTERR);

    hashv = buf + ilen - 21;

    emv_memcpy(calc, buf + 1, ilen - 22);
    tlen = (ilen - 22);
    if ((ptmp = appdata_find(TAG_AUTH_DATA, &ilen)))
    {

        emv_memcpy(calc + tlen, ptmp, ilen);
        tlen += ilen;
    }
    if ((ptmp = appdata_find(TAG_9F4A_SDA_TAG_LIST, &ilen)))
    {
        if (ilen == 1 && *ptmp == TAG_0082_AIP)
        {
            emv_memcpy(calc + tlen, appdata_find(TAG_0082_AIP, NULL), 2);
            tlen += 2;
        }
        else
        {
            return rERR(DA_9F4AERR);
        }
    }

    /* calculate the sha1 value */
    EMVPORT_Hash(EMV_HASH_ALG_SHA1, calc, tlen, digest);
    if (emv_memcmp(digest, hashv, 20) != 0)
    {
        return rERR(DA_SSADHASHVERR);
    }

    appdata_save(TAG_9F45_DATA_AUTHENTICATION_CODE, buf + 3, 2, UNIQUE);
    return 0;
}


static int express_verify_ssad_sm2(EMV_PUBLICKEY* isspk)
{
    eTRACE("[TODO]实现国密算法");
    return rERR(DA_ERR);

}


static int express_verify_dynamic_signature(EMV_PUBLICKEY* iccpk)//标准动态数据认证
{
    u8 *ptmp;
    int ilen, i, ddolvlen;
    u8 ddolv[EMV_CFG_ICC_SEND_MAXLEN];
    u8 buffer[256];
    u8 result[256 + 256]; /* 256 bytes for non-9F4B data objects */
    u8 digest[20];
    EMV_TLVOBJ tlvs[10];

    /* Dynamic Signature Generation */
    if ((ptmp = appdata_find(TAG_9F49_DDOL, &ilen)) == NULL)
    {
        if (ics_option(DA_Support_Default_DDOL))
        {
            if ((ptmp = appdata_find(TAG_DF14_DEFAULT_DDOL, &ilen)) == NULL)
            {
                return rERR(DA_NODEFDDOL);
            }
        }
        else
        {
            return rERR(DA_NODDOL);
        }
    }

    /* packet the  ddol data buffer */
    if ((ddolvlen = emvdol_packet(ptmp, ilen, ddolv, sizeof(ddolv))) < 0)
    {
        return rERR(DA_DDOLPACKERR);
    }

    /* issue the internal authentication command */
    if ((i = emvicc_internal_authenticate(ddolv, ddolvlen, result, &ilen)) != 0x9000)
    {
		return (rERR(DA_IAUTHERR), DA_IAUTHERR);
    }
    if (emvtlv_decode(result, ilen, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ) < 0)
    {
		return (rERR(DA_IAUTHRSPERR), DA_IAUTHERR);
    }


    if (tlvs[1].tagname == TAG_0080_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1)
    {
        if (tlvs[1].valuelen == 0)
        {
            return (rERR(DA_IAUTHRSP80LENERR), DA_IAUTHERR);
        }
        i = 1;
    }
    else if (tlvs[1].tagname == TAG_0077_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2)
    {
        if ((i = emvtlv_get(1, TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA, tlvs, SEARCH_ONLY_SON)) == 0)
        {
            return (rERR(DA_IAUTHRSP77NOSDAD),DA_IAUTHERR);
        }
        if (tlvs[i].valuelen == 0)
        {
            return (rERR(DA_IAUTHRSP77SDADERR),DA_IAUTHERR);
        }
    }
    else
    {
        return (rERR(DA_IAUTHRSPUNEXPECTTAG),DA_IAUTHERR);
    }

    ptmp = tlvs[i].pvalue; /* the signed dynamic application data */
    ilen = tlvs[i].valuelen;


    /* recover the sdad */
    if (EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, iccpk, ptmp, buffer) <= 0)
    {
        return rERR(DA_SDADRECOVERERR);
    }


    /* generate hash source */
    emv_memcpy(result, buffer + 1, ilen - 22);
    emv_memcpy(result + ilen - 22, ddolv, ddolvlen);
    i = ilen - 22 + ddolvlen;
    EMVPORT_Hash(EMV_HASH_ALG_SHA1, result, i, digest);
    if (emv_memcmp(digest, buffer + ilen - 21, 20) != 0)
    {
        return rERR(DA_SDADHASHVERR);
    }
    /* ICC Dynamic Number */
    appdata_save(TAG_9F4C_ICC_DYNAMIC_NUMBER, buffer+5, *(buffer+4), UNIQUE);
    return 0;
}



static int express_verify_dynamic_signature_sm2(EMV_PUBLICKEY* iccpk)//xinyong 160220
{
    eTRACE("[TODO]实现国密算法");
    return rERR(DA_ERR);
}


static int express_verify_fast_dynamic_signature(EMV_PUBLICKEY * icc_key)//QPBOC
{
    u8 * ptmp;
    u32 appver = 0x0020; //默认旧卡
    u8 buffer[256];
    int len, bufferLen, ptmpLen;
    u8 result[256 + 256]; /* 256 bytes for non-9F4B data objects */
    u8 digest[64];

    ptmp = appdata_find(TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA, &len);
    if(ptmp == NULL || len == 0)
    {
        return rERR(DA_NOSDAD);
    }

    if (len != icc_key->rsa.modulus_len)
    {
        return rERR(DA_SDADLENERR);
    }

    /* recover the sdad */
    if (EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, icc_key, ptmp, buffer) <= 0)
    {
        return rERR(DA_SDADRECOVERERR);
    }

    /* generate hash source */
    emv_memcpy(result, buffer + 1, len - 22);
    bufferLen = len -22;


    ptmp = appdata_find(TAG_9F08_APPLICATION_VERSION_NUMBER_CARD, NULL);
    if (ptmp != NULL)
    {
        appver = ptmp[0] * 256 + ptmp[1];
    }

    ptmp = appdata_find(TAG_9F69_CARD_AUTHENTICATION_RELATED_DATA, &ptmpLen);
    if (ptmp)
    {
        if (ptmpLen < 8 || ptmpLen > 16)
        {
            return rERR(DA_9F69ERR);
        }

        if (*ptmp == 0x00)
        {
            if (appver >= 0x0030)
            {
                return rERR(DA_FDDAVERUNSUP);
            }

            /* "00"版本 */
            ptmp = appdata_find(TAG_9F37_UNPREDICATABLE_NUMBER, &ptmpLen);
            if( ptmp == NULL || ptmpLen == 0)
            {
                return rERR(DA_NO9F37);
            }
            emv_memcpy(result + bufferLen, ptmp , ptmpLen);
            bufferLen += ptmpLen;
        }
        else if (*ptmp == 0x01)
        {
            /* "01"版本 */
            ptmp = appdata_find(TAG_9F37_UNPREDICATABLE_NUMBER, &ptmpLen);
            if( ptmp == NULL || ptmpLen == 0)
            {
                return rERR(DA_NO9F37);
            }
            emv_memcpy(result + bufferLen, ptmp , ptmpLen);
            bufferLen += ptmpLen;

            ptmp = appdata_find(TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, &ptmpLen);
            emv_memcpy(result + bufferLen, ptmp , ptmpLen);
            bufferLen += ptmpLen;

            ptmp = appdata_find(TAG_5F2A_TRANSACTION_CURRENCY_CODE, &ptmpLen);
            emv_memcpy(result + bufferLen, ptmp , ptmpLen);
            bufferLen += ptmpLen;

            ptmp = appdata_find(TAG_9F69_CARD_AUTHENTICATION_RELATED_DATA, &ptmpLen);
            emv_memcpy(result + bufferLen, ptmp , ptmpLen);
            bufferLen += ptmpLen;
        }
        else
        {
            return rERR(DA_FDDAVERERR);
        }
    }
    else
    {
        if (appver >= 0x0030)
        {
            return rERR(DA_FDDAVERUNSUP);
        }

        /* "00"版本 */
        ptmp = appdata_find(TAG_9F37_UNPREDICATABLE_NUMBER, &ptmpLen);
        if( ptmp == NULL || ptmpLen == 0 )
        {
            return rERR(DA_NO9F37);
        }
        emv_memcpy(result + bufferLen, ptmp , ptmpLen);
        bufferLen += ptmpLen;
    }

    EMVPORT_Hash(EMV_HASH_ALG_SHA1, result, bufferLen, digest);

    if (emv_memcmp(digest, buffer + len - 21, 20) != 0) {
        return rERR(DA_SDADHASHVERR);
    }

    appdata_save(TAG_9F4C_ICC_DYNAMIC_NUMBER, buffer+5, *(buffer+4), UNIQUE);
    return 0;
}


static int express_verify_fast_dynamic_signature_sm2(EMV_PUBLICKEY * icc_key)//fDDA
{
    eTRACE("[TODO]实现国密算法");
    return rERR(DA_ERR);

}

int express_process_sda(void)
{
    int ret;
    EMV_PUBLICKEY pkx, pky;
    EMV_PUBLICKEY *capk, *isspk;

    if (g_express_cnxt->da_readrec_failed == TRUE)
    {
        return rERR(DA_READRECORDFAIL);
    }

    capk  = (EMV_PUBLICKEY*)&pkx;
    isspk = (EMV_PUBLICKEY*)&pky;
    if ((ret = express_retrieve_capk(capk)) < 0)
    {
		if(ret == DA_ICCDATAMISSING)
		{
			tvr_set(ICC_DATA_MISSING);
		}
        return ret;
    }

    if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)
    {
        if ((ret = express_retrieve_issuer_pk_sm2(capk, isspk)) < 0)
        {
            return ret;
        }
    }
    else
    {
        if ((ret = express_retrieve_issuer_pk(capk, isspk)) < 0)
        {
			if(ret == DA_ICCDATAMISSING)
			{
				tvr_set(ICC_DATA_MISSING);
			}
            return ret;
        }
    }

    /* Saved for PIN Encipherment usage */
    appdata_save(TAG_ISSUER_PK, (u8*)isspk, sizeof(EMV_PUBLICKEY), UNIQUE);

    /* verify the signed static applicaiton data (ssad), the main function for SDA */
    if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)
    {
        eTRACE("[TODO]实现国密算法");
        if ((ret = express_verify_ssad_sm2(isspk)) < 0)//xinyong 160220
        {
            return ret;
        }
    }
    else
    {
        if ((ret = express_verify_ssad(isspk)) < 0)
        {
			if(ret == DA_ICCDATAMISSING)
			{
				tvr_set(ICC_DATA_MISSING);
			}
            return ret;
        }
    }

    return 0;
}

//forcda=0 Standard Dynamic Data Authentication
//forcda=2 Dynamic Data Authentication For QPBOC
//forcda=1(or other value) Only for retrive CAPK/IPK/ICCPK
int express_process_dda(int forcda)
{
    int ret;
    EMV_PUBLICKEY pkx, pky;
    EMV_PUBLICKEY *capk, *isspk, *iccpk;

    if (g_express_cnxt->da_readrec_failed == TRUE)	//not read all auth data from SFI 11-30
    {
		eTRACE("AMEX-error: Auth Data from Card Read error");
        return rERR(DA_READRECORDFAIL);
    }

    capk = (EMV_PUBLICKEY*)&pkx;
    isspk = (EMV_PUBLICKEY*)&pky;

	ret = express_retrieve_capk(capk);
	eTRACE("AMEX-info: express_retrieve_capk ret = %d", ret);
//	eTRACE_HEX("AMEX-info: CAPK Modul", capk->rsa.modulus, capk->rsa.modulus_len);
//	eTRACE_HEX("AMEX-info: CAPK Exponent", capk->rsa.exponent, 3);

	if(ret < 0)
    {
		if(ret == DA_ICCDATAMISSING)
		{
			tvr_set(ICC_DATA_MISSING);
		}
        return ret;
    }

    if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)
    {
        if ((ret = express_retrieve_issuer_pk_sm2(capk, isspk)) < 0)
        {
            return ret;
        }
    }
    else
    {
		ret = express_retrieve_issuer_pk(capk, isspk);
		eTRACE("AMEX-info: express_retrieve_issuer_pk ret = %d", ret);
//		eTRACE_HEX("AMEX-info: IPK Modul", isspk->rsa.modulus, isspk->rsa.modulus_len);
//		eTRACE_HEX("AMEX-info: IPK Exponent", isspk->rsa.exponent, 3);
        if (ret < 0)
        {
			if(ret == DA_ICCDATAMISSING)
			{
				tvr_set(ICC_DATA_MISSING);
			}
            return ret;
        }
    }
    /* Saved for PIN Encipherment usage */
    appdata_save(TAG_ISSUER_PK, (u8*)isspk, sizeof(EMV_PUBLICKEY), UNIQUE);

    iccpk = (EMV_PUBLICKEY*)&pkx;
    if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)
    {
        if ((ret = express_retrieve_icc_pk_sm2(isspk, iccpk)) < 0)
        {
            return ret;
        }
    }
    else
    {
		ret = express_retrieve_icc_pk(isspk, iccpk);
		eTRACE("AMEX-info: express_retrieve_icc_pk ret = %d", ret);
        if (ret < 0)
        {
			if(ret == DA_ICCDATAMISSING)
			{
				tvr_set(ICC_DATA_MISSING);
			}
            return ret;
        }
    }
    /* Saved for PIN Encipherment usage */
    appdata_save(TAG_ICCARD_PK, (u8*)iccpk, sizeof(EMV_PUBLICKEY), UNIQUE);

    if(forcda == 0)
    {
        /* this is a dda only, not for cda preprocessing */
        if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)
        {
            eTRACE("[TODO]实现国密算法");
            if ((ret = express_verify_dynamic_signature_sm2(iccpk)) < 0)//xinyong 160220
            {
                return ret;
            }
        }
        else
        {
            if ((ret = express_verify_dynamic_signature(iccpk)) < 0)
            {
                return ret;
            }
        }
    }
    else if (forcda == 2)
    {
        if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)//xinyong 160221
        {
            eTRACE("[TODO]实现国密算法");
            if ((ret = express_verify_fast_dynamic_signature_sm2(iccpk)) < 0)
            {
                eTRACE("express_verify_fast_dynamic_signature ret = %d", ret);
                return ret;
            }
        }
        else
        {
            if ((ret = express_verify_fast_dynamic_signature(iccpk)) < 0)
            {
                eTRACE("express_verify_fast_dynamic_signature ret = %d", ret);
                return ret;
            }
        }
    }

    return 0;
}

int express_process_cda(int nstep, u8 * gac_resp, int gac_len)//复合动态数据认证
{
    u8 * ptmp, *random;
    int ilen, pos;
    EMV_PUBLICKEY* iccpk;
    u8 buf[256];
    u8 calc[512];
    u8 digest[32];

    /* for the first calling, check ics */
    if (!nstep)
    {
        if (!ics_option(TAA_Support_CDAFail_Detected))
        {
            /* before TAA, we cannot detect CDA failure */
            if ((ilen = express_process_dda(1)) < 0)
            {
				return ilen;
            }
        }
    }

    /* it must be existing for CDA processing, otherwise, the card
       session would be terminated in Generate AC */
    ptmp = appdata_find(TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA, &ilen);
    if (!ptmp)
    {
        tvr_set(ICC_DATA_MISSING);
        return rERR(DA_NOSDAD);
    }
//	eTRACE_HEX("AMEX-info: Signed Dynamic App Data", ptmp, ilen);

    /* also, the icc public key must be extracted from the certificate,
       otherwise, CDA has failed and does not come to here */
    iccpk = (EMV_PUBLICKEY *)appdata_find(TAG_ICCARD_PK, NULL);

    if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)//xinyong 160220
    {
        eTRACE("[TODO]实现国密算法");
    }
    else
    {
        if (ilen != iccpk->rsa.modulus_len)
        {
            return rERR(DA_SDADLENERR);
        }
//		eTRACE("AMEX-info: ICC PK Modul len: %d", iccpk->rsa.modulus_len);
//		eTRACE_HEX("AMEX-info: ICC PK Modul before Recover RSA", iccpk->rsa.modulus, iccpk->rsa.modulus_len);
//		eTRACE_HEX("AMEX-info: ICC PK exponent before Recover RSA", iccpk->rsa.exponent, 3);
        if (EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, iccpk, ptmp, buf) <= 0)
        {
            return rERR(DA_SDADRECOVERERR);
        }

//		eTRACE_HEX("AMEX-info: ICC PK Modul after Recover RSA", iccpk->rsa.modulus, iccpk->rsa.modulus_len);
		eTRACE_HEX("AMEX-info: Recover SDA", buf, 248);

        if ((0x6A != buf[0]) || (0x05 != buf[1]) || (0xBC != buf[ilen - 1]))
		{
			return rERR(DA_SDADFMTERR);
		}

        /* ptmp will point to the ICC Dynamic Data elements */
        ptmp = buf + 4 + 1 + buf[4]; /*fetch cid */
        if (*ptmp != *appdata_find(TAG_9F27_CRYPTOGRAM_INFO_DATA, NULL))
        {
            gac_resp[0] = *ptmp;
            return rERR(DA_CIDNOTMATCHED);
        }
        /* Transaction Data Hash Code */
        emv_memcpy(calc, buf + 1, ilen - 22);

		random = appdata_find(TAG_9F37_UNPREDICATABLE_NUMBER, NULL);
//		eTRACE_HEX("AMEX-info: 9F37", random, 4);

        emv_memcpy(calc + ilen - 22, random, 4);

		if(0x01 != buf[2])
		{
			eTRACE("AMEX-error: recover data from iccpk Hash Ind is not 01");
			return rERR(DA_SDADFMTERR);
		}

        EMVPORT_Hash(EMV_HASH_ALG_SHA1, calc, ilen - 22 + 4, digest);
//		eTRACE_HEX("AMEX-info: Hash from recover ICCPK", digest, 20);

        if (emv_memcmp(digest, buf + ilen - 21, 20) != 0)
        {
            return rERR(DA_CDAHASHV1ERR);
        }

        emv_memset(calc,0,sizeof(calc));
        emv_memset(digest,0,sizeof(digest));

        /* PDOL, CDOL1, CDOL2, GenAC response */
        ptmp = appdata_find(TAG_PDOL_VALUE, &ilen);
//		eTRACE_HEX("AMEX-info: PDOL data", ptmp, ilen);
        emv_memcpy(calc, ptmp, ilen);
        pos = ilen;

        ptmp = appdata_find(TAG_CDOL1_VALUE, &ilen);
//		eTRACE_HEX("AMEX-info: CDOL1 data", ptmp, ilen);
        emv_memcpy(calc + pos, ptmp, ilen);
        pos += ilen;

//		eTRACE("AMEX-info: nstep = %d", nstep);
        if (nstep)
        {
            ptmp = appdata_find(TAG_CDOL2_VALUE, &ilen);
            emv_memcpy(calc + pos, ptmp, ilen);
            pos += ilen;
        }

        emv_memcpy(calc + pos, gac_resp, gac_len);
//		eTRACE_HEX("AMEX-info: GAC Resp", gac_resp, gac_len);

        pos += gac_len;
        EMVPORT_Hash(EMV_HASH_ALG_SHA1, calc, pos, digest);
        ptmp = buf + 4 + 10 + buf[4]; /* Transaction Data Hash Code from ICC Dynamic Data */

//		eTRACE_HEX("AMEX-info: Hash result from ICC Dynamic Data:", ptmp, 20);
//		eTRACE_HEX("AMEX-info: Hash result from Term Cal:", digest, 20);
        if (emv_memcmp(digest, ptmp, 20) != 0)
        {
            return rERR(DA_CDAHASHV2ERR);
        }

        appdata_save(TAG_9F4C_ICC_DYNAMIC_NUMBER, buf + 5, buf[4], OVERRIDED);
        /* save TC or ARQC */
        appdata_save(TAG_9F26_APPLICATION_CRYPTOGRAM, ptmp - 8, 8, OVERRIDED);
    }

    return 0;
}

int express_encrypt_pin(u8* pin, u8* epin)//PBOC(包括国密)不支持脱机密文PIN
{
    int ret;
    u8 icc_random[8];
    u8 term_random[256];
    EMV_PUBLICKEY  pkx, pky;
    EMV_PUBLICKEY* capk;
    EMV_PUBLICKEY* isspk;
    EMV_PUBLICKEY* iccpk;
    EMV_PUBLICKEY* pinpk;

    /* fetch issuer public key */
    if ((isspk = (EMV_PUBLICKEY*)appdata_find(TAG_ISSUER_PK, NULL)) == NULL)
    {
        if (tvr_get(OFFLINE_DA_NOT_PERFORMED) || (g_express_cnxt->da_readrec_failed == TRUE))
        {
            /* if DA not performed or just failed in read record phase.
               on the latter one, the data from reading record module just effect
               express_verify_ssad and express_retrieve_icc_pk, express_retrieve_issuer_pk and express_retrieve_pin_pk
               still work */
            capk = (EMV_PUBLICKEY*)&pkx;
            isspk = (EMV_PUBLICKEY*)&pky;
            if ((ret = express_retrieve_capk(capk)) < 0)
            {
                return ret;
            }

            if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2)
            {
                if ((ret = express_retrieve_issuer_pk_sm2(capk, isspk)) < 0)
                {
                    return ret;
                }
            }
            else
            {
                if ((ret = express_retrieve_issuer_pk(capk, isspk)) < 0)
                {
                    return ret;
                }
            }
        }
        else
        { /* otherwise, DA Failed in retrieving the issuer pk, it is unnecessary to retry it */
            return rERR(DA_NOISSPK);
        }
    }
    /* fetch pin encipherment public key */
    pinpk = (EMV_PUBLICKEY*)&pkx;
    ret = express_retrieve_pin_pk(isspk, pinpk);
    /* mandatory objects absent including 9F2D,9F2E, 9F2F(if should be present) */
    if (ret == DA_ICCDATAMISSING)
    {
        /* should use ICC public key */
        if ((pinpk = (EMV_PUBLICKEY*)appdata_find(TAG_ICCARD_PK, NULL)) == NULL)
        {
            /* if icc pk does no exist, DDA/CDA may failed in retrieving it or not peformed */
            if (tvr_get(DDA_FAILED) || tvr_get(CDA_FAILED))
            { /* if DDA/CDA performed and DDA/CDA failed */
                return rERR(DA_NOICCPK);
            }
            else
            {
                /* DDA not performed */
                iccpk = (EMV_PUBLICKEY*)&pkx;
                if (g_express_cnxt->req_pk_algorithm == EMV_PUBKEY_ALG_SM2) {
                    if ((ret = express_retrieve_icc_pk_sm2(isspk, iccpk)) < 0) {
                       return ret;
                    }
                } else {
                    if ((ret = express_retrieve_icc_pk(isspk, iccpk)) < 0) {
                        return ret;
                    }
                }
                pinpk = iccpk;
            }
        }
    }
    else if (ret < 0)
    {
        return ret;
    }

    if (emvicc_get_challenge(icc_random, &ret) != 0x9000 || ret != 8)
    {
        return rERR(DA_GETCHALLENGEERR);
    }
    *term_random = 0x7F;
    emv_memcpy(term_random + 1, pin, 8);
    emv_memcpy(term_random + 9, icc_random, 8);
    /* the buffer should be pkey_pin->pk_mod_len length, so, padded with a random pattern */
    EMVPORT_GetRandom(term_random + 17, pinpk->rsa.modulus_len - 17);

    if (EMVPORT_Recover(EMV_PUBKEY_ALG_RSA, pinpk, term_random, epin) <= 0)
    {
        return rERR(DA_ENCPINRECOVERERR);
    }

    return pinpk->rsa.modulus_len;
}


/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/

int express_offline_data_auth(void)
{
    int ret = 0;
	u8 *term_cap;

	eTRACE("AMEX-info: start express_offline_data_auth");

    g_express_cnxt->da_sda_performed = FALSE;
    g_express_cnxt->da_dda_performed = FALSE;
    g_express_cnxt->da_cda_performed = FALSE;

	term_cap = appdata_find(TAG_9F33_TERMINAL_CAPABILITIES, NULL);

    if (AIP_CDA_SUPPORT() && (term_cap[2] & 0x08))
    {
		eTRACE("AMEX-info: Term and Card all support CDA");
        g_express_cnxt->da_cda_performed = TRUE;
        if (ics_option(TAA_Support_CDAFail_Detected))
        {
			ret = express_process_dda(1);
			eTRACE("AMEX-info: express_process_dda ret = %d", ret);
            if (ret < 0)
            {
                tvr_set(CDA_FAILED);
            }

        }
    }
    else if (AIP_SDA_SUPPORT() && (term_cap[2] & 0x80))
    {
		eTRACE("AMEX-info: Term and Card all support SDA");
        g_express_cnxt->da_sda_performed = TRUE;
        tvr_set(SDA_SELECTED);
		ret = express_process_sda();
		eTRACE("AMEX-info: express_process_sda ret = %d", ret);
        if (ret < 0)
        {
            tvr_set(SDA_FAILED);
        }
    }
    else
    {
    	eTRACE("AMEX-info: ODA Not Performed");
        tvr_set(OFFLINE_DA_NOT_PERFORMED);
        return rNXT(EMV_STA_DATA_AUTH_COMPLETED);
    }

    tsi_set(OFFLINE_DA_PERFORMED);

    if (ret == DA_IAUTHERR)
    {
        return ret;
	}

    return rNXT(EMV_STA_DATA_AUTH_COMPLETED);
}


/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
