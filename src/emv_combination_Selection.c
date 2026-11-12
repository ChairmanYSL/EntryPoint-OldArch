
#include "emvi.h"

#define ADF_MASTERCARD "\xA0\x00\x00\x00\x04"
#define ADF_PURE1 "\xA0\x00\x00\x02\x80"
#define ADF_PURE2 "\xA0\x00\x00\x02\x28"
#define ADF_PURE3 "\xD9\x99\x99\x99\x99"
#define ADF_PURE4 "\xD6\x66\x66\x66\x66"
#define ADF_VISA "\xA0\x00\x00\x00\x03"
#define ADF_PBOC "\xA0\x00\x00\x03\x33"
#define ADF_JCB  "\xA0\x00\x00\x00\x65"
#define ADF_AMEX  "\xA0\x00\x00\x00\x25"
#define ADF_DISV  "\xA0\x00\x00\x01\x52"

#define KERNEL_ID_MASTERCARD "\x02\x00\x00\x00\x00\x00\x00\x00"
#define KERNEL_ID_VISA "\x03\x00\x00\x00\x00\x00\x00\x00"
#define KERNEL_ID_AMEX "\x04\x00\x00\x00\x00\x00\x00\x00"
#define KERNEL_ID_JCB "\x05\x00\x00\x00\x00\x00\x00\x00"
#define KERNEL_ID_DISV "\x06\x00\x00\x00\x00\x00\x00\x00"
#define KERNEL_ID_PBOC "\x07\x00\x00\x00\x00\x00\x00\x00"
#define KERNEL_ID_SAMA "\x2D\x00\x00\x00\x00\x00\x00\x00"

int32_t emv_combination_Selection(void)
{
    int32_t ret, len;
    int32_t choice;
    uint8_t  buf[EMV_CFG_ICC_RECV_MAXLEN];
    int32_t again = false;
    uint8_t account_type;
    uint8_t *ptr;
    uint8_t pIAD[128] = {0};

	eTRACE("g_emv_cnxt->card_type = %02X", g_emv_cnxt->card_type);

	if(g_emv_cnxt->card_type == EMV_CARD_CONTACTLESS)
	{
		eTRACE("g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len = %d", g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len);

		if(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len != 0)
		{
			eTRACE_HEX("g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id", g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len);
			if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_MASTERCARD, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QMSDC;
			}
			else if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_VISA, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QVSDC;
			}
			else if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_AMEX, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_AMEX;
			}
			else if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_JCB, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_JCB;
			}
			else if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_DISV, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_DISV;
			}
			else if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_PBOC, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QPBOC;
			}
			else if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id, KERNEL_ID_SAMA, g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].kernel_id_len))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QPURE;
			}
			else
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_UNKNOWN;
			}
		}
		else
		{
			if(!emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_MASTERCARD, 5))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QMSDC;

			}
			else if(0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_PURE1, 5) ||
					0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_PURE2, 5) ||
					0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_PURE3, 5) ||
					0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_PURE4, 5) )
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QPURE;

			}
			else if(0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_VISA, 5))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QVSDC;

			}
			else if(0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_JCB, 5))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_JCB;

			}
			else if(0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_DISV, 5))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_DISV;

			}
			else if(0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_AMEX, 5))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_AMEX;

			}
			else if(0==emv_memcmp(g_emv_cnxt->candidates[g_emv_cnxt->req_sel_candidates_result].aid, ADF_PBOC, 5))
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QPBOC;
			}
			else
			{
				g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_UNKNOWN;
			}
		}
	}

	tmTRACE("entrypoint-info: flag9");

	g_emv_cnxt->contactless_type = EMV_CONTACTLESSTYPE_QMSDC;

	if(g_emv_cnxt->contactless_type == EMV_CONTACTLESSTYPE_UNKNOWN)
	{
		return EMV_ERR;
	}

	ret = emv_select_kernel(g_emv_cnxt->card_type, g_emv_cnxt->contactless_type);
	eTRACE("entrypoint-info: emv_select_kernel ret = %d", ret);
	return ret;
}


