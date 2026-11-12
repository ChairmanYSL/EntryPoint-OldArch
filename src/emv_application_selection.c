#include <string.h>
#include <stdio.h>
/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     emv_application_selection.c
* @author   Kingsee Development Team - xinyong
* @version  V43
* @date     2015��10��21��
* @brief    Ӧ��ѡ��Application Selection��
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

#define DF_PSE  "1PAY.SYS.DDF01"
#define DF_PPSE "2PAY.SYS.DDF01"
#define NO_9F11 (0xFF)


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

static int app_cmp(unsigned char *caid, int caid_len, EMV_TERMAPP *termapp)
{
    /* Book B 4.2: ADF Name / AID 长度为 5..16 字节；终端 AID 长度非法时不得退化为过宽（含空）匹配 */
    if (caid == NULL || termapp == NULL)
    {
        return -1;
    }
    if (termapp->aid_len < 5 || termapp->aid_len > 16)
    {
        return -1;
    }

    eTRACE_HEX("caid", caid, caid_len);
    eTRACE_HEX("taid", termapp->aid, termapp->aid_len);
    eTRACE("asi = %d", termapp->asi);
    eTRACE("kernel id = %02X", termapp->kernel_id);
    eTRACE("g_emv_cnxt->trans_type = %02X", g_emv_cnxt->trans_type);
    eTRACE("taid trans_type = %02X", termapp->trans_type);

    if (caid_len >= 5 && caid_len <= 16)
    {
		if(g_emv_cnxt->trans_type == termapp->trans_type || termapp->trans_type == 0xFF)
		{
			if ((caid_len == termapp->aid_len) || (caid_len > termapp->aid_len && termapp->asi == 0))
			{
				return emv_memcmp(caid, termapp->aid, termapp->aid_len);
			}
		}
    }

    return -1;
}

static int32_t judge_icti(uint8_t icti)
{
	uint8_t *term_add_caps = appdata_find(TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, NULL);

    if (icti >= 0x01 && icti <= 0x08)
        return (term_add_caps[4] & (1 << (icti - 0x01)));
    else if (icti == 0x09)
        return (term_add_caps[3] & 0x01);
    else if (icti == 0x10)
        return (term_add_caps[3] & 0x02);
    else if (icti == NO_9F11)
        return 0;
    else
        return 0;
}

static void candidate_clr(void)
{
    /* ��λ��ѡӦ���б���������������������ز��� */
//    g_emv_cnxt->candidates = (EMV_CANDIDATE*)((uint8_t*)g_emv_cnxt->termapps
//            + ((sizeof(EMV_TERMAPP) * g_emv_cnxt->termapps_num + 3) / 4) * 4);
	memset(g_emv_cnxt->candidates, 0, sizeof(EMV_CANDIDATE) * EMV_CFG_MAX_CANDIDATES);
    g_emv_cnxt->candidates_num = 0;
}

static void candidate_add(EMV_CANDIDATE* candidate)
{
    /* 候选数组按 EMV_CFG_MAX_CANDIDATES 分配（emv.c:EMV_Init），达到上限后拒绝写入，避免越界写 */
    if (g_emv_cnxt == NULL || candidate == NULL || g_emv_cnxt->candidates == NULL)
    {
        return;
    }
    if (g_emv_cnxt->candidates_num < 0 || g_emv_cnxt->candidates_num >= EMV_CFG_MAX_CANDIDATES)
    {
        return;
    }

    emv_memcpy(&g_emv_cnxt->candidates[g_emv_cnxt->candidates_num++], candidate, sizeof(EMV_CANDIDATE));
}

void candidate_del(int32_t index)
{
    int32_t i;

    if (index < 0 || index >= g_emv_cnxt->candidates_num) return;

    for (i = index + 1; i < g_emv_cnxt->candidates_num; ++i)
    {
        emv_memcpy(&g_emv_cnxt->candidates[i-1], &g_emv_cnxt->candidates[i], sizeof(EMV_CANDIDATE));
    }
    g_emv_cnxt->candidates_num--;
}

int32_t deal_spi_resp(uint8_t *resp, int32_t resp_len)
{
	int32_t i, ret,j;
	int32_t pos_fci, pos_dfname, pos_fci_prop,pos_fci_issuer;
	int32_t pos_aid,pos_label,pos_api,pos_kernelID,pos_extselect;
	int32_t pos_temp;
	EMV_CANDIDATE candidate={0};
	EMV_TLVOBJ tlvs[56]={0};

	if (emvtlv_decode(resp, resp_len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ) < 0)
	{
		eTRACE("paypass-error: SPI response format invalid");
		return rERR(AS_FCIFMTILL);
	}

	if (!(pos_fci = emvtlv_get(0, TAG_006F_FCI_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: SPI response don't contain 6F");
		return rERR(AS_FCINO6F);
	}
	if (!(pos_dfname = emvtlv_get(pos_fci, TAG_0084_DF_NAME, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: SPI response don't contain 84");
		return rERR(AS_FCINO84);
	}
	if (!(pos_fci_prop = emvtlv_get(pos_fci, TAG_00A5_FCI_PROPRIETARY_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: SPI response don't contain A5");
		return rERR(AS_FCINOA5);
	}
	if (!(pos_fci_issuer = emvtlv_get(pos_fci_prop, TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: SPI response don't contain BF0C");
		return rERR(AS_FCINOBF0C);
	}

	if (tlvs[pos_fci_issuer].childnum <= 0)
	{
		eTRACE("paypass-error: BF0C don't contain child tag");
		return rERR(AS_FCIBF0CVALILL);
	}

	//S54.12 Build candidate list
	for (i = pos_fci_issuer + 1; i <= tlvs[pos_fci_issuer].childnum + pos_fci_issuer; i++)
	{
		if (tlvs[i].tagname == TAG_0061_APPLICATION_TEMPLATE)
		{
			if ((pos_aid = emvtlv_get(i, TAG_004F_AID, tlvs, SEARCH_ONLY_SON)))
			{
//				eTRACE_HEX("paypass-info: aid in spi resp", tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);
//
//				eTRACE("paypass-info: g_emv_cnxt->termapps_num = %d", g_emv_cnxt->termapps_num);
				for (j = 0; j < g_emv_cnxt->termapps_num; j++)
				{
					ret = app_cmp(tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen, &g_emv_cnxt->termapps[j]);
//					eTRACE("paypass-info: app_cmp ret = %d", ret);

					if (ret == 0)
					{
//						eTRACE("paypass-info: flag1");
						emv_memset(&candidate, 0x00, sizeof(candidate));

						emv_memcpy(candidate.aid, tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);
						candidate.aid_len = tlvs[pos_aid].valuelen;

						if ((pos_label = emvtlv_get(i, TAG_0050_APPLICATION_LABEL, tlvs, SEARCH_ONLY_SON)))
						{
							if (tlvs[pos_label].valuelen > 16)
							{
								tlvs[pos_label].valuelen = 16;
							}
							emv_memcpy(candidate.lable, tlvs[pos_label].pvalue, tlvs[pos_label].valuelen);
							candidate.lable_len = tlvs[pos_label].valuelen;
						}

						if ((pos_api = emvtlv_get(i, TAG_0087_API, tlvs, SEARCH_ONLY_SON)))
						{
							candidate.priority = *(tlvs[pos_api].pvalue);
						}

						if ((pos_kernelID = emvtlv_get(i, TAG_9F2A_KERNEL_ID, tlvs, SEARCH_ONLY_SON)))
						{
							candidate.kernel_id_len = tlvs[pos_kernelID].valuelen;
							if(candidate.kernel_id_len > sizeof(candidate.kernel_id))
							{
								candidate.kernel_id_len = sizeof(candidate.kernel_id);
							}
							emv_memcpy(candidate.kernel_id, tlvs[pos_kernelID].pvalue, candidate.kernel_id_len);
						}

						if ((pos_extselect = emvtlv_get(i, TAG_9F29_EXTENED_SELECTION, tlvs, SEARCH_ONLY_SON)))
						{
							/* j 才是 termapps 的下标（i 是 tlvs 下标） */
							if(g_emv_cnxt->termapps[j].rsv && tlvs[pos_extselect].valuelen > 0)
							{
								if(candidate.aid_len + tlvs[pos_extselect].valuelen <= 16)
								{
									emv_memcpy(candidate.aid + candidate.aid_len, tlvs[pos_extselect].pvalue, tlvs[pos_extselect].valuelen);
									candidate.aid_len += tlvs[pos_extselect].valuelen;
								}
							}
						}

						if ((pos_temp = emvtlv_get(i, TAG_9F0A_ASRPD, tlvs, SEARCH_ONLY_SON)))
						{
							//TODO: add process of 9F0A
						}

//						eTRACE("paypass-info: pos_kernelID = %d", pos_kernelID);
//						eTRACE("paypass-info: candidate.kernel_id_len = %d", candidate.kernel_id_len);
						if(pos_kernelID == 0 || candidate.kernel_id_len == 0)
						{
							if(g_emv_cnxt->termapps[j].kernel_id == 0x02)
							{
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
								candidate.kernel_id[0] = 0x02;
								candidate.kernel_id_len = 8;
							}
							else
							{
								candidate.kernel_id_len = 8;
							}
						}
						if(pos_kernelID && candidate.kernel_id_len > 0)
						{
							if(candidate.kernel_id[0] == 0x00 || candidate.kernel_id[0] == 0x40)
							{
								if(g_emv_cnxt->termapps[j].kernel_id == 0x02)
								{
									emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
									candidate.kernel_id[0] = 0x02;
									candidate.kernel_id_len = 8;
								}
							}
							else if(candidate.kernel_id[0] == 0x02 || candidate.kernel_id[0] == 0x42)
							{
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
								candidate.kernel_id[0] = 0x02;
								candidate.kernel_id_len = 8;
							}
							else if(candidate.kernel_id[0] == 0x08 || candidate.kernel_id[0] == 0x48)
							{
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
								candidate.kernel_id[0] = 0x08;
								candidate.kernel_id_len = 8;
							}
						}

						if(candidate.kernel_id_len != 0)
						{
//							eTRACE("paypass-info: g_emv_cnxt->termapps[j].kernel_id = %02X", g_emv_cnxt->termapps[j].kernel_id);
//							eTRACE("paypass-info: candidate.kernel_id[0] = %02X", candidate.kernel_id[0]);
							if(g_emv_cnxt->termapps[j].kernel_id != 0 && g_emv_cnxt->termapps[j].kernel_id != candidate.kernel_id[0])
							{
								eTRACE("paypass-info: AID don't match because kernel id invalid");
								continue;
							}
						}

						if (g_emv_cnxt->candidates_num >= EMV_CFG_MAX_CANDIDATES)
						{
							eTRACE("paypass-error: candidate list is full");
							return rERR(AS_ERR);
						}

						candidate.rsv[0] = i;
						candidate_add(&candidate);
//						eTRACE("paypass-info: AID match");
//						eTRACE_HEX("paypass-info: AID name", candidate.aid, candidate.aid_len);
//						eTRACE("paypass-info: kernel id: %02X", candidate.kernel_id[0]);

						continue;
					}
					else
					{
						eTRACE("paypass-info: AID don't match because term don't obtain");
					}
				}
			}
			i += tlvs[i].childnum; /* continue with next '61' */
		}
	}

	return EMV_OK;
}


int32_t parse_spi_resp(uint8_t *resp, int32_t resp_len, int32_t sw)
{
	int32_t ret;
	uint8_t sw_bcd[2]={0};

	//S54.6
	if(sw == 0x9000)
	{
		ret = deal_spi_resp(resp, resp_len);
		eTRACE("paypass-info: deal_spi_resp ret = %d", ret);

		//S54.10
		if(ret != EMV_OK)
		{
			//S54.11
			g_err_indicator.L2 = ERR_INFO_L2_PARSING_ERROR;
			return outcome_other_card(0, 0);
		}

		//S54.15
		return EMV_STA_CANDIDATES_BUILT;
	}
	else
	{
		//S54.7
		g_err_indicator.L2 = ERR_INFO_L2_STATUS_BYTES;
		g_err_indicator.MsgOnError = UIREQUEST_MESSAGEID_ERROR_OTHER_CARD;
		emv_dec2hex(sw, sw_bcd, sizeof(sw_bcd));
		emv_memcpy(g_err_indicator.SW12, sw_bcd, sizeof(g_err_indicator.SW12));

		appdata_save(TAG_DF8115_ERROR_INDICATION, (uint8_t *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);

		g_discretionary_data_len = 0;
		emv_memcpy(g_discretionary_data, "\xDF\x81\x15\x06", 4);
		g_discretionary_data_len += 4;
		emv_memcpy(g_discretionary_data+g_discretionary_data_len, (void *)&g_err_indicator, sizeof(g_err_indicator));
		g_discretionary_data_len += sizeof(g_err_indicator);

		appdata_save(TAG_FF8106_DISCRETIONARY_DATA, g_discretionary_data, g_discretionary_data_len, OVERRIDED);

		return outcome_other_card(0, 0);
	}

	return EMV_OK;
}


/* 按 SDOL 各项请求长度预估展开后的数据长度（不访问 appdata），用于在调用 emvdol_packet 之前
   做容量校验，避免卡下发的超长 SDOL 越界写入 pack。
   >=0：展开长度；<0：SDOL 格式非法或展开长度超过发送缓冲上限。 */
static int32_t sdol_expand_len(const uint8_t *dol, int32_t dollen)
{
	int32_t i = 0, total = 0;

	if (dol == NULL || dollen <= 0)
	{
		return -1;
	}

	while (i < dollen)
	{
		if ((dol[i] & 0x1F) == 0x1F)                /* has_subsequent：复合 tag */
		{
			i++;
			while (i < dollen && (dol[i] & 0x80))   /* another_byte_follow */
			{
				i++;
			}
		}

		i++;                                        /* tag 的最后一个字节 */
		if (i >= dollen)
		{
			return -1;                              /* 缺少长度字节 */
		}

		total += dol[i++];                          /* 该 DOL 项请求的数据长度 */
		if (total > EMV_CFG_ICC_SEND_MAXLEN)
		{
			return -1;
		}
	}

	return total;
}


int32_t sdol_process(uint8_t flag, uint8_t *term_category, uint8_t *poi_info_value, int32_t poi_info_value_len)
{
	uint8_t *sdol=NULL;
	int32_t len,i=0;
	int32_t raw_len=0,hdr_len=0,data_len=0;
	uint8_t temp[EMV_CFG_ICC_SEND_MAXLEN]={0};
	uint8_t pack[EMV_CFG_ICC_SEND_MAXLEN]={0};
    uint8_t resp[EMV_CFG_ICC_RECV_MAXLEN]={0};
	int32_t pack_len=0,ret,resp_len=0;

	eTRACE("paypass-info: input flag = %d", flag);

	//S SPI 4
	if(flag == 1)
	{
		pack[i++] = 0x83;
		pack[i++] = 0x05;
		emv_memcpy(pack+i, "\x00\x01\x02", 3);
		i += 3;
		emv_memcpy(pack+i, term_category, 2);
		i += 2;
		pack_len = i;
	}
	else if(flag == 2 || flag == 3) //S SPI 6 / S SPI 7
	{
		sdol = appdata_find(TAG_9F3F_SDOL, &len);
		if(sdol && len > 0)
		{
			/* 防御：先按 SDOL 各项请求长度估算展开长度，超出发送缓冲即拒绝，不依赖 emvdol_packet 内部校验 */
			raw_len = sdol_expand_len(sdol, len);
			if(raw_len < 0 || raw_len > (int32_t)sizeof(pack))
			{
				eTRACE("paypass-error: sdol request length exceeds send buffer");
				return EMV_ERR;
			}

			raw_len = emvdol_packet(sdol, len, pack, (int32_t)sizeof(pack));
			if(raw_len < 0 || raw_len > (int32_t)sizeof(pack))
			{
				eTRACE("paypass-info: parse sdol error");
				return EMV_ERR;
			}

			/* flag 3 在 SDOL 请求值之后追加 POI Information：ID '0001' + L '02' + 终端类别 2 字节，共 5 字节 */
			data_len = raw_len + ((flag == 3) ? 5 : 0);

			/* Book B Annex C：SPI 命令的 Lc 是单字节。'83' TLV 总长（tag + 长度字节 + 值）不得超过 255，
			   超长必须返回错误：既不能截断长度字节，也不能依赖 ICC 层对 Lc 的截断 */
			if(data_len > 0xFF)
			{
				eTRACE("paypass-error: sdol data length exceeds 255");
				return EMV_ERR;
			}

			/* 83 tag + BER 长度：<=0x7F 用 1 字节；128..255 用 0x81（单字节长度，不截断） */
			if(data_len <= 0x7F)
			{
				hdr_len = 2;
				temp[0] = 0x83;
				temp[1] = (uint8_t)data_len;
			}
			else
			{
				hdr_len = 3;
				temp[0] = 0x83;
				temp[1] = 0x81;
				temp[2] = (uint8_t)data_len;
			}

			if(hdr_len + data_len > 0xFF)
			{
				eTRACE("paypass-error: sdol spi command exceeds single-byte Lc");
				return EMV_ERR;
			}

			/* 防御：写入 temp/pack 前再比较一次缓冲容量（当前 Lc 上限下恒成立，保留以防上限放宽） */
			if(hdr_len + data_len > (int32_t)sizeof(temp))
			{
				eTRACE("paypass-info: sdol pack len greater than max apdu send len");
				return EMV_ERR;
			}

			emv_memcpy(temp+hdr_len, pack, raw_len);
			if(flag == 3)
			{
				emv_memcpy(temp+hdr_len+raw_len, "\x00\x01\x02", 3);
				emv_memcpy(temp+hdr_len+raw_len+3, term_category, 2);
			}

			i = hdr_len + data_len;
			emv_memcpy(pack, temp, i);
			pack_len = i;
			eTRACE_HEX("paypass-info: pack complete", pack, pack_len);
		}
	}
	else
	{
		return EMV_OK;
	}

	if(pack_len <= 0)
	{
		return EMV_OK;
	}

	ret = emvicc_spi(pack, pack_len, resp, &resp_len);
	eTRACE("paypass-info: emvicc_spi ret = %d", ret);

	if(ret < 0)
	{
		//S54.5
		g_err_indicator.L1 = ERR_INFO_L1_TRANSMISSION_ERROR;


		outcome_try_again();
		return EMV_APDU_TIMEOUT;
	}

	/* 卡只返回 9000 而无响应数据时 emvicc_spi 不回写 resp_len：解析前必须校验，避免按未初始化/越界长度解码 */
	if(resp_len < 0 || resp_len > (int32_t)sizeof(resp))
	{
		eTRACE("paypass-error: invalid SPI response length");
		return EMV_ERR;
	}
	if(0x9000 == ret && resp_len <= 0)
	{
		eTRACE("paypass-error: SPI response contains no data");
		return EMV_ERR;
	}

	ret = parse_spi_resp(resp, resp_len, ret);
	eTRACE("paypass-info: parse_spi_resp ret = %d", ret);
	return ret;
}


int32_t spi_process()
{
	bool sdol_exist=false,tc_sup_list_exist=false,tc_match=false,tc_exist=false;
	uint8_t *term_cate_sup_list=NULL,*sdol=NULL,*poi_info;
	int32_t len,tcs_list_len,poi_info_len,poi_info_value_len;
	uint8_t term_category[2]={0},poi_info_value[255]={0};
	int32_t i=0,j=0,ret;

	// S SPI 1

//	eTRACE("paypass-info: kernel support SPI: %d", ics_option(MISC_Paypass_Support_SPI));
//	eTRACE("paypass-info: kernel SPI active flag: %d", ics_option(MISC_Paypass_SPI_ACTIVE_FLAG));

	if(ics_option(MISC_Paypass_Support_SPI) && ics_option(MISC_Paypass_SPI_ACTIVE_FLAG))
	{
		term_cate_sup_list = appdata_find(TAG_9F3E_TERMINAL_CATEGORIES_SUPPORTED_LIST, &tcs_list_len);
		if(term_cate_sup_list != NULL && tcs_list_len > 0)
		{
			tc_sup_list_exist = true;
		}

		sdol = appdata_find(TAG_9F3F_SDOL, &len);
		if(sdol != NULL && len > 0)
		{
			sdol_exist = true;
		}
		// S SPI 2
		if(!tc_sup_list_exist && !sdol_exist)
		{
			//GOTO S52.12
		}
		else
		{
			//S52 SPI 3 4 5 6 7 8
			poi_info = appdata_find(TAG_008B_POI_INFOMATION, &poi_info_len);

			//8B format ID-L-V,ID is constant 2 byte,L is constact 1 byte
			if(poi_info != NULL && poi_info_len > 0)
			{
				if(term_cate_sup_list != NULL)
				{
					if(tcs_list_len % 2)
					{
						eTRACE("paypass-error: 9F3E len invalid");
						return EMV_ERR;
					}
					i = 0;
					while (i < poi_info_len)
					{
						if(!emv_memcmp(poi_info+i, "\x00\x01", 2) && (poi_info[i+2] == 0x02))	//in L2 test,only check match ID=0001 and L = 02
						{
							emv_memset(term_category, 0, sizeof(term_category));
							poi_info_value_len = 0;
							emv_memset(poi_info_value, 0, sizeof(poi_info_value));

							i += 3;
							emv_memcpy(term_category, poi_info+i, 2);
							i += 2;
							tc_exist = true;
						}
						else
						{
							i += 2;
							poi_info_value_len = poi_info[i++];
							i += poi_info_value_len;
							continue;
							tc_exist = false;
						}

						if(tc_exist)
						{
							j=0;
							while (j < tcs_list_len)
							{
								if(!emv_memcmp(term_category, term_cate_sup_list+j, sizeof(term_category)))
								{
									eTRACE("paypass-info: the Terminal Category (POI Information ID '0001') of the terminal is on the TerminalCategories Supported List");
									tc_match = true;
									break;
								}
								j += sizeof(term_category);
							}
						}
						if(tc_match)
						{
							break;
						}
					}
				}
			}

			if(tc_match && !sdol_exist)
			{
				ret = sdol_process(1, term_category, poi_info_value, poi_info_value_len);
			}
			else if(sdol_exist && !tc_match)
			{
				ret = sdol_process(2, term_category, poi_info_value, poi_info_value_len);
			}
			else if(sdol_exist && tc_match)
			{
				ret = sdol_process(3, term_category, poi_info_value, poi_info_value_len);
			}
			else
			{
				return EMV_OK;
			}

			if(ret != EMV_OK)
			{
				return ret;
			}
		}
	}


	return EMV_OK;
}

int32_t deal_pse_resp(uint8_t *resp, int32_t resp_len)
{
    int32_t i, ret,j,sw;
    int32_t pos_fci, pos_dfname, pos_fci_prop,pos_fci_issuer,pos_sfi,pos_aef_data_temp;
	int32_t pos_aid,pos_label,pos_api,pos_ddf_name,pos_perf_name;
    EMV_CANDIDATE candidate={0};
    EMV_TLVOBJ tlvs[56]={0};
	bool mandatory_exist=false;
	uint8_t sfi, recno = 1;

	if (emvtlv_decode(resp, resp_len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ) < 0)
	{
		eTRACE("paypass-error: PSE response format invalid");
		return rERR(AS_FCIFMTILL);
	}

	if (!(pos_fci = emvtlv_get(0, TAG_006F_FCI_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PSE response don't contain 6F");
		return rERR(AS_FCINO6F);
	}
	if (!(pos_dfname = emvtlv_get(pos_fci, TAG_0084_DF_NAME, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PSE response don't contain 84");
		return rERR(AS_FCINO84);
	}
	if (!(pos_fci_prop = emvtlv_get(pos_fci, TAG_00A5_FCI_PROPRIETARY_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PSE response don't contain A5");
		return rERR(AS_FCINOA5);
	}
	if (!(pos_sfi = emvtlv_get(pos_fci_prop, TAG_0088_SFI, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PSE response don't contain SFI");
		return rERR(AS_FCINO88);
	}

    sfi = *(tlvs[pos_sfi].pvalue) & 0x1F;
    if (sfi < 1 || sfi > 10)
    {
        return rERR(AS_FCI88VALILL);
	}

	/* 从记录 1 开始逐条读取 AEF 记录：recno 必须初始化并自增，且以 6A83 或记录号上限退出，避免死循环 */
	for (recno = 1; ; recno++)
	{
		if (0 == recno)
		{
			break;	/* uint8_t 记录号回绕（已读满 0xFF 条），退出 */
		}

		resp_len = 0;
		sw = emvicc_read_record(sfi, recno, resp, &resp_len);
		if(0x9000 == sw)
		{
			ret = emvtlv_decode(resp, resp_len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ);
			if(ret != EMV_OK)
			{
				return rERR(AS_RECFMTILL);
			}

			if (!(pos_aef_data_temp = emvtlv_get(0, TAG_0070_AEF_DATA_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
			{
				return rERR(AS_RECNO70);
			}

			/* 61 模板在 AEF Data Template(70) 之下，必须遍历 70 的子节点（不是 primitive 的 88） */
			for (i = pos_aef_data_temp + 1; i <= tlvs[pos_aef_data_temp].childnum + pos_aef_data_temp; i++)
			{
				//S52.12.1
				if (tlvs[i].tagname == TAG_0061_APPLICATION_TEMPLATE && tlvs[i].valuelen > 0)
				{
					if((pos_ddf_name = emvtlv_get(i, TAG_009D_DDF_NAME, tlvs, SEARCH_ONLY_SON)))
					{
						sw = emvicc_select(tlvs[pos_ddf_name].pvalue, tlvs[pos_ddf_name].valuelen, false, NULL, NULL);
						if(0x9000 == sw)
						{
							ret = deal_pse_resp(resp, resp_len);
							eTRACE("entrypoint-info: deal_ppse_resp ret = %d", ret);

							if(ret != EMV_OK)
							{
								return ret;
							}
						}
						else if(0x6A81 == sw)
						{
							return rERR(AS_SELSW6A81);
						}
						else
						{
							return rERR(AS_SELERR);
						}

						emvicc_select((uint8_t*)DF_PSE, emv_strlen(DF_PSE), false, NULL, NULL);
					}
					else if ((pos_aid = emvtlv_get(i, TAG_004F_AID, tlvs, SEARCH_ONLY_SON)) && (tlvs[pos_aid].valuelen > 0))
					{
						for (j = 0; j < g_emv_cnxt->termapps_num; j++)
						{
							if (g_emv_cnxt->termapps[j].enable == 0)
							{
								continue;
							}

							if (app_cmp(tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen, &g_emv_cnxt->termapps[j]) == 0)
							{
								eTRACE("paypass-info: APP match");

								emv_memset(&candidate, 0x00, sizeof(candidate));
								emv_memcpy(candidate.aid, tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);
								candidate.aid_len = tlvs[pos_aid].valuelen;

								eTRACE_HEX("paypass-info: caid", tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);

								if ((pos_label = emvtlv_get(i, TAG_0050_APPLICATION_LABEL, tlvs, SEARCH_ONLY_SON)))
								{
									if (tlvs[pos_label].valuelen > 16)
									{
										tlvs[pos_label].valuelen = 16;
									}
									emv_memcpy(candidate.lable, tlvs[pos_label].pvalue, tlvs[pos_label].valuelen);
									candidate.lable_len = tlvs[pos_label].valuelen;
								}
								else
								{
									return rERR(AS_APPTMPLNO50);
								}

								if ((pos_perf_name = emvtlv_get(i, TAG_9F12_APPLICATION_PREFERRED_NAME, tlvs, SEARCH_ONLY_SON)) && judge_icti(NO_9F11))
								{
									if (tlvs[pos_perf_name].valuelen > 16)
									{
										tlvs[pos_perf_name].valuelen = 16;
									}
									emv_memcpy(candidate.preferred_name, tlvs[pos_perf_name].pvalue, tlvs[pos_perf_name].valuelen);
									candidate.preferred_name_len = tlvs[pos_perf_name].valuelen;
								}
								else
								{
									emv_memcpy(candidate.preferred_name, candidate.lable, candidate.lable_len);
									candidate.preferred_name_len = candidate.lable_len;
								}

								if ((pos_api = emvtlv_get(i, TAG_0087_API, tlvs, SEARCH_ONLY_SON)))
								{
									if (tlvs[pos_api].valuelen != 1)
	                                {
	                                    break;
	                                }
									candidate.priority = *(tlvs[pos_api].pvalue);
								}

								if (ics_option(AS_Support_CardHolder_Confirm) || !(candidate.priority & 0x80))
								{
									if (g_emv_cnxt->candidates_num >= EMV_CFG_MAX_CANDIDATES)
									{
										eTRACE("paypass-error: candidate list is full");
										return rERR(AS_ERR);
									}

									candidate.rsv[0] = j;
									candidate_add(&candidate);
								}

								break;
							}
						}
					}
					else
					{
						return rERR(AS_APPTMPLILL);
					}
					i += tlvs[i].childnum; /* continue with next '61' */
				}
			}
		}
		else if(0x6A83 == sw)
		{
			break;
		}
		else
		{
            return rERR(AS_RDRECERR);
		}
	}

	return EMV_OK;
}


int32_t deal_ppse_resp(uint8_t *resp, int32_t resp_len)
{
    int32_t i, ret,j;
    int32_t pos_fci, pos_dfname, pos_fci_prop,pos_fci_issuer;
	int32_t pos_aid,pos_label,pos_api,pos_kernelID,pos_extselect;
    EMV_CANDIDATE candidate={0};
    EMV_TLVOBJ tlvs[56]={0};
	bool mandatory_exist=false;

	if (emvtlv_decode(resp, resp_len, tlvs, sizeof(tlvs)/sizeof(tlvs[0]), SINGLE_TLVOBJ) < 0)
	{
		eTRACE("paypass-error: PPSE response format invalid");
		return rERR(AS_FCIFMTILL);
	}

	if (!(pos_fci = emvtlv_get(0, TAG_006F_FCI_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PPSE response don't contain 6F");
		return rERR(AS_FCINO6F);
	}
	if (!(pos_dfname = emvtlv_get(pos_fci, TAG_0084_DF_NAME, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PPSE response don't contain 84");
		return rERR(AS_FCINO84);
	}
	if (!(pos_fci_prop = emvtlv_get(pos_fci, TAG_00A5_FCI_PROPRIETARY_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PPSE response don't contain A5");
		return rERR(AS_FCINOA5);
	}
	if (!(pos_fci_issuer = emvtlv_get(pos_fci_prop, TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA, tlvs, SEARCH_ONLY_SON)))
	{
		eTRACE("paypass-error: PPSE response don't contain BF0C");
		return rERR(AS_FCINOBF0C);
	}

	if (tlvs[pos_fci_issuer].childnum <= 0)
	{
		eTRACE("paypass-error: BF0C don't contain child tag");
		return rERR(AS_FCIBF0CVALILL);
	}

	for (i = pos_fci_issuer + 1; i < tlvs[pos_fci_issuer].childnum + pos_fci_issuer + 1; i++)
	{
		if(tlvs[i].tagname == TAG_0061_APPLICATION_TEMPLATE)
		{
			mandatory_exist = true;
		}
//		eTRACE("paypass-info: BF0C child tag = %02X", tlvs[i].tagname);
		if(tlvs[i].tagname == TAG_9F3E_TERMINAL_CATEGORIES_SUPPORTED_LIST || tlvs[i].tagname == TAG_9F3F_SDOL || tlvs[i].tagname == TAG_008B_POI_INFOMATION)
		{
			appdata_save_tlv(tlvs+i, OVERRIDED, TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA);
		}
	}

	if(!mandatory_exist)
	{
		return rERR(AS_ERR);
	}

	//S SPI 0
	ret = spi_process();
	eTRACE("paypass-info: spi_process ret = %d", ret);

	if(ret != EMV_OK)
	{
		return ret;
	}

	eTRACE("paypass-info: goto S52.12");
	//S52.12 Build candidate list
	for (i = pos_fci_issuer + 1; i <= tlvs[pos_fci_issuer].childnum + pos_fci_issuer; i++)
	{
		//S52.12.1
		if (tlvs[i].tagname == TAG_0061_APPLICATION_TEMPLATE && tlvs[i].valuelen > 0)
		{
			if ((pos_aid = emvtlv_get(i, TAG_004F_AID, tlvs, SEARCH_ONLY_SON)) && (tlvs[pos_aid].valuelen > 0))
			{
				for (j = 0; j < g_emv_cnxt->termapps_num; j++)
				{
//					eTRACE_HEX("caid", tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);
					if (app_cmp(tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen, &g_emv_cnxt->termapps[j]) == 0)
					{
						eTRACE("paypass-info: APP match");

						emv_memset(&candidate, 0x00, sizeof(candidate));
						emv_memcpy(candidate.aid, tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);
						candidate.aid_len = tlvs[pos_aid].valuelen;

						/* Book B 3.3.3: Extended Selection —— 9F29 值追加在 ADF Name 之后作为 SELECT 数据，
						   仅当该终端应用支持扩展选择（termapps[j].rsv）且总长不超过 16 字节 */
						if ((pos_extselect = emvtlv_get(i, TAG_9F29_EXTENED_SELECTION, tlvs, SEARCH_ONLY_SON)))
						{
							if (g_emv_cnxt->termapps[j].rsv && tlvs[pos_extselect].valuelen > 0)
							{
								if (candidate.aid_len + tlvs[pos_extselect].valuelen <= 16)
								{
									emv_memcpy(candidate.aid + candidate.aid_len, tlvs[pos_extselect].pvalue, tlvs[pos_extselect].valuelen);
									candidate.aid_len += tlvs[pos_extselect].valuelen;
								}
								else
								{
									eTRACE("paypass-error: extended selection makes ADF name longer than 16");
								}
							}
						}

						eTRACE_HEX("paypass-info: caid", tlvs[pos_aid].pvalue, tlvs[pos_aid].valuelen);

						if ((pos_label = emvtlv_get(i, TAG_0050_APPLICATION_LABEL, tlvs, SEARCH_ONLY_SON)))
						{
							if (tlvs[pos_label].valuelen > 16)
							{
								tlvs[pos_label].valuelen = 16;
							}
							emv_memcpy(candidate.lable, tlvs[pos_label].pvalue, tlvs[pos_label].valuelen);
							candidate.lable_len = tlvs[pos_label].valuelen;
						}

						if ((pos_api = emvtlv_get(i, TAG_0087_API, tlvs, SEARCH_ONLY_SON)))
						{
							candidate.priority = *(tlvs[pos_api].pvalue);
						}

						if ((pos_kernelID = emvtlv_get(i, TAG_9F2A_KERNEL_ID, tlvs, SEARCH_ONLY_SON)))
						{
							if(tlvs[pos_kernelID].valuelen > sizeof(candidate.kernel_id))
							{
								candidate.kernel_id_len = sizeof(candidate.kernel_id);
							}
							else
							{
								candidate.kernel_id_len = tlvs[pos_kernelID].valuelen;
							}

							emv_memcpy(candidate.kernel_id, tlvs[pos_kernelID].pvalue, candidate.kernel_id_len);
						}

						eTRACE("paypass-info: pos_kernelID = %d", pos_kernelID);
						eTRACE("paypass-info: candidate.kernel_id_len = %d", candidate.kernel_id_len);
						eTRACE("paypass-info: g_emv_cnxt->termapps[j].kernel_id = %02X", g_emv_cnxt->termapps[j].kernel_id);
						if(pos_kernelID == 0 || candidate.kernel_id_len == 0)
						{
							if(g_emv_cnxt->termapps[j].kernel_id == 0x02)
							{
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
								candidate.kernel_id[0] = 0x02;
								candidate.kernel_id_len = 8;
							}
							else
							{
								//add rfu for S52.12 step3 list1
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
//								candidate.kernel_id[0] = g_emv_cnxt->termapps[j].kernel_id;
								candidate.kernel_id_len = 8;
							}
						}


						if(pos_kernelID && candidate.kernel_id_len > 0)
						{
							if(candidate.kernel_id[0] == 0x00 || candidate.kernel_id[0] == 0x40)
							{
								if(g_emv_cnxt->termapps[j].kernel_id == 0x02)
								{
									emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
									candidate.kernel_id[0] = 0x02;
									candidate.kernel_id_len = 8;
								}
							}
							else if(candidate.kernel_id[0] == 0x02 || candidate.kernel_id[0] == 0x42)
							{
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
								candidate.kernel_id[0] = 0x02;
								candidate.kernel_id_len = 8;
							}
							else if(candidate.kernel_id[0] == 0x08 || candidate.kernel_id[0] == 0x48)
							{
								emv_memset(candidate.kernel_id, 0, sizeof(candidate.kernel_id));
								candidate.kernel_id[0] = 0x08;
								candidate.kernel_id_len = 8;
							}
						}
						if(candidate.kernel_id_len != 0)
						{
							if(g_emv_cnxt->termapps[j].kernel_id != 0 && g_emv_cnxt->termapps[j].kernel_id != candidate.kernel_id[0])
							{
								eTRACE("paypass-info: AID don't match because kernel id invalid");
								continue;
							}
						}
						candidate.rsv[0] = j;
						//S52.12.4
						eTRACE("paypass-info: candidate add");
						eTRACE_HEX("paypass-info: AID", candidate.aid, candidate.aid_len);
						eTRACE("paypass-info: len = %d", candidate.aid_len);

						if (g_emv_cnxt->candidates_num >= EMV_CFG_MAX_CANDIDATES)
						{
							eTRACE("paypass-error: candidate list is full");
							return rERR(AS_ERR);
						}

						candidate_add(&candidate);
						continue;
					}
				}
			}
			i += tlvs[i].childnum; /* continue with next '61' */
		}
	}

	return EMV_OK;
}

int32_t parse_pse_resp(uint8_t *resp, int32_t resp_len, int32_t sw)
{
	int32_t ret;

	if(0x9000 == sw)
	{
		//S52.10
		ret = deal_pse_resp(resp, resp_len);
    	eTRACE("entrypoint-info: deal_ppse_resp ret = %d", ret);

		if(ret != EMV_OK)
		{
			return ret;
		}
	}
	else if(0x6A81 == sw)
	{
		return rERR(AS_SELSW6A81);
	}
	else
	{
        return rERR(AS_SELERR);
	}

	return EMV_OK;
}


int32_t parse_ppse_resp(uint8_t *resp, int32_t resp_len, int32_t sw)
{
	int32_t ret;

	if(0x9000 == sw)
	{
		//S52.10
		ret = deal_ppse_resp(resp, resp_len);
    	eTRACE("paypass-info: deal_ppse_resp ret = %d", ret);

		if(ret == EMV_STA_CANDIDATES_BUILT)
		{
			return ret;
		}
		else if(ret != EMV_OK)
		{
			//S52.11
			g_err_indicator.L2 = ERR_INFO_L2_PARSING_ERROR;
			g_err_indicator.MsgOnError = UIREQUEST_MESSAGEID_ERROR_OTHER_CARD;

			appdata_save(TAG_DF8115_ERROR_INDICATION, (uint8_t *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
			emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
			g_discretionary_data_len = 0;

			emv_memcpy(g_discretionary_data, "\xDF\x81\x15\x06", 4);
			g_discretionary_data_len += 4;

			emv_memcpy(g_discretionary_data+g_discretionary_data_len, (void *)&g_err_indicator, sizeof(g_err_indicator));
			g_discretionary_data_len += sizeof(g_err_indicator);

			appdata_save(TAG_FF8106_DISCRETIONARY_DATA, g_discretionary_data, g_discretionary_data_len, OVERRIDED);

			//S52.9
			return outcome_other_card(0, 0);
		}
	}
	else
	{
    	eTRACE("paypass-info: Select SW != 9000");
		//S52.7
		g_err_indicator.L2 = ERR_INFO_L2_STATUS_BYTES;
		emv_dec2hex(sw, g_err_indicator.SW12, sizeof(g_err_indicator.SW12));
    	eTRACE_HEX("paypass-info: g_err_indicator.SW12", g_err_indicator.SW12, sizeof(g_err_indicator.SW12));

		//S52.8
		g_err_indicator.MsgOnError = UIREQUEST_MESSAGEID_ERROR_OTHER_CARD;

		appdata_save(TAG_DF8115_ERROR_INDICATION, (uint8_t *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
		emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
		g_discretionary_data_len = 0;

		emv_memcpy(g_discretionary_data, "\xDF\x81\x15\x06", 4);
		g_discretionary_data_len += 4;

		emv_memcpy(g_discretionary_data+g_discretionary_data_len, (void *)&g_err_indicator, sizeof(g_err_indicator));
		g_discretionary_data_len += sizeof(g_err_indicator);

		appdata_save(TAG_FF8106_DISCRETIONARY_DATA, g_discretionary_data, g_discretionary_data_len, OVERRIDED);

		//S52.9
		return outcome_other_card(0, 0);
	}

	return EMV_OK;
}

static int32_t parse_select_resp(uint8_t *resp, int32_t resp_len, int32_t sw)
{
	EMV_TLVOBJ tlvs[64]={0};
	int32_t pos,pos1,pos2,pos3;
	int32_t i,j;
	EMV_CANDIDATE candidate={0};
	uint8_t icti;

	if(resp == NULL || resp_len < 0)
	{
		return EMV_FAIL;
	}

	if(resp[0] != 0x6F)
	{
	    eTRACE( "Missing FCI template[Tag 0x6F]");
		return AS_FCINO6F; // Cannot find FCI template, return to next entry
	}

    if (emvtlv_decode(resp, resp_len, (EMV_TLVOBJ *)&tlvs[0], sizeof(tlvs)/sizeof(EMV_TLVOBJ), SINGLE_TLVOBJ) < 0)
    {
        eTRACE("Failed to decode tlv object string");
		return AS_ERR;
	}

	if (!(pos1 = emvtlv_get(0, TAG_006F_FCI_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
    {
	    eTRACE( "Missing FCI template[Tag 0x6F]");
		return AS_FCINO6F; // Cannot find FCI template, return to next entry
	}

	if (!(pos2 = emvtlv_get(pos1, TAG_0084_DF_NAME, tlvs, SEARCH_ONLY_SON)))
    {
	    eTRACE( "Missing FCI template[Tag 0x84]");
		return AS_FCINO84; // Cannot find DF Name, return to next entry
	}
	if (!(pos3 = emvtlv_get(pos1, TAG_00A5_FCI_PROPRIETARY_TEMPLATE, tlvs, SEARCH_ONLY_SON)))
    {
	    eTRACE( "Missing FCI template[Tag 0xA5]");
		return AS_FCINOA5; // Cannot find FCI Proprietary Template , return to next entry
	}

	if ((pos1 = emvtlv_get(0, TAG_9F11_ISSUER_CODE_TABLE_INDEX, tlvs, SEARCH_ALL_DESC)))
    {
		if (tlvs[pos1].parent != pos3 || emvtlv_find_dup(pos1, tlvs))
        {
            eTRACE( "9F11 should be in template A5, and not duplicated if it exists");
			return AS_ERR; // 9F11 should be in template A5, and not duplicated if it exists
        }
    }

	if ((pos1 = emvtlv_get(0, TAG_9F12_APPLICATION_PREFERRED_NAME, tlvs, SEARCH_ALL_DESC)))
    {
		if (tlvs[pos1].parent != pos3 || emvtlv_find_dup(pos1, tlvs))
        {
			return AS_POSERR9F12; // 9F12 should be in template A5, and not duplicated if it exists
        }
	}


	// check BF0C to ensure no duplication in its children
	if ((pos1 = emvtlv_get(pos3, TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA, tlvs, SEARCH_ONLY_SON)))
    {
		// not implemented here now until cases meet
		for (i = pos1 + 1; i < tlvs[pos1].childnum + pos1; i ++)
        {
			for (j = i + 1; j <= tlvs[pos1].childnum + pos1; j ++)
            {
				if (tlvs[i].tagname == tlvs[j].tagname)
                {
                    eTRACE( "There is duplication in 0xBF0C`s children");
					return AS_ERR;
				}
			}
		}
	}

	EMV_TERMAPP partial_aid ={0};

	emv_memcpy(partial_aid.aid, "\xA0\x00\x00\x00\x25\x01", 6);
	partial_aid.aid_len = 6;
	partial_aid.asi = 0;
	partial_aid.enable = 1;
	partial_aid.rsv = 1;	//support extend select


    if (app_cmp(tlvs[pos2].pvalue, tlvs[pos2].valuelen, &partial_aid) == 0)
    {
        emv_memset(&candidate, 0x00, sizeof(candidate));

        emv_memcpy(candidate.aid, tlvs[pos2].pvalue, tlvs[pos2].valuelen);
        candidate.aid_len = tlvs[pos2].valuelen;

        if ((pos = emvtlv_get(pos3, TAG_0050_APPLICATION_LABEL, tlvs, SEARCH_ONLY_SON)))
        {
            if (tlvs[pos].valuelen > 16)
            {
                tlvs[pos].valuelen = 16;
            }
            emv_memcpy(candidate.lable, tlvs[pos].pvalue, tlvs[pos].valuelen);
            candidate.lable_len = tlvs[pos].valuelen;
        }

        if ((pos = emvtlv_get(pos3, TAG_9F11_ISSUER_CODE_TABLE_INDEX, tlvs, SEARCH_ONLY_SON)))
        {
            icti = *(tlvs[pos].pvalue);
        }
        else
        {
            icti = NO_9F11;
        }

        if ((pos = emvtlv_get(pos3, TAG_9F12_APPLICATION_PREFERRED_NAME, tlvs, SEARCH_ONLY_SON))
                && judge_icti(icti))
        {
            if (tlvs[pos].valuelen > 16)
            {
                tlvs[pos].valuelen = 16;
            }
            emv_memcpy(candidate.preferred_name, tlvs[pos].pvalue, tlvs[pos].valuelen);
            candidate.preferred_name_len = tlvs[pos].valuelen;
        }
        else
        {
            emv_memcpy(candidate.preferred_name, candidate.lable, candidate.lable_len);
            candidate.preferred_name_len = candidate.lable_len;
        }

        if ((pos = emvtlv_get(pos3, TAG_0087_API, tlvs, SEARCH_ONLY_SON)))
        {
            candidate.priority = *(tlvs[pos].pvalue);
        }

        if(!(pos && tlvs[pos].valuelen!=0x01))
        {
            if (ics_option(AS_Support_CardHolder_Confirm) || !(candidate.priority & 0x80))
            {
                if (g_emv_cnxt->candidates_num >= EMV_CFG_MAX_CANDIDATES)
                {
                    eTRACE("paypass-error: candidate list is full");
                    return rERR(AS_ERR);
                }

                candidate.rsv[0] = 0;
                candidate_add(&candidate);
            }
        }

		return EMV_OK;
    }

    return EMV_FAIL;
}

int32_t select_ddf(uint8_t *fn, int32_t fnlen, uint8_t icti)
{
	int32_t ret,rlen = 0;
    uint8_t resp[EMV_CFG_ICC_RECV_MAXLEN] = {0};

	tmTRACE("entrypoint-info: flag3");

	ret = emvicc_select(fn, fnlen, false, resp, &rlen);
	eTRACE("paypass-info: emvicc_select ret = %d", ret);
	tmTRACE("entrypoint-info: flag4");
	if(!emv_memcmp(fn, DF_PPSE, emv_strlen(DF_PSE)))
	{
		if(ret < 0)
		{
			if(ret == -106)
			{
				g_err_indicator.L1 = ERR_INFO_L1_TIME_OUT_ERROR;
	//			return EMV_APDU_TIMEOUT;
			}
			else
			{
				g_err_indicator.L1 = ERR_INFO_L1_TRANSMISSION_ERROR;
			}

			g_err_indicator.L1 = ERR_INFO_L1_TIME_OUT_ERROR;
			appdata_save(TAG_DF8115_ERROR_INDICATION, (unsigned char *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
			CreateEMVDiscretionaryData();
			return outcome_try_again();
		}

		/* 卡只返回 9000 而无 FCI 时 emvicc_select 不回写 rlen：解析前必须校验，
		   越界/为空一律按空 FCI 交给 parse_ppse_resp 走 S52.11（'1C'），绝不用未初始化长度解码 */
		if(rlen < 0 || rlen > (int32_t)sizeof(resp))
		{
			eTRACE("paypass-error: invalid select response length");
			rlen = 0;
		}

		ret = parse_ppse_resp(resp, rlen, ret);
		eTRACE("paypass-info: parse_ppse_resp ret = %d", ret);
	}
	else if(!emv_memcmp(fn, DF_PSE, emv_strlen(DF_PSE)))
	{
			if(ret < 0)
			{
				if(ret == -106)
				{
					return EMV_APDU_TIMEOUT;
				}
				else
				{
					return EMV_ENDAPPLICATION;
				}
			}

			/* 同 PPSE 分支：无响应数据时 rlen 未回写，解析前校验 */
			if(rlen < 0 || rlen > (int32_t)sizeof(resp))
			{
				eTRACE("paypass-error: invalid select response length");
				rlen = 0;
			}

			ret = parse_pse_resp(resp, rlen, ret);
			eTRACE("paypass-info: parse_ppse_resp ret = %d", ret);
	}
	tmTRACE("entrypoint-info: flag5");
	return ret;
}

static int32_t select_use_aid_list(uint8_t type)
{
    int32_t i;
    int32_t ret;
    int32_t flag6D00 = 0;
    int32_t sw = 0;
	int32_t resp_len = 0;
	uint8_t resp[EMV_CFG_ICC_RECV_MAXLEN] = {0};

    candidate_clr();

	if(type == EMV_CARD_CONTACTLESS)
	{
		sw = emvicc_select((uint8_t*)"\xA0\x00\x00\x00\x25\x01", 6, false, resp, &resp_len);
		if(0x9000 == sw)
		{
			ret = parse_select_resp(resp, resp_len, sw);
			if(ret != EMV_OK)
			{
				return outcome_request_another_payment();
			}
		}
		else
		{
			return outcome_request_another_payment();
		}

		while (1)
		{
			//partial select next
			sw = emvicc_select((uint8_t*)"\xA0\x00\x00\x00\x25\x01", 6, true, resp, &resp_len);
			if(0x9000 == sw)
			{
				ret = parse_select_resp(resp, resp_len, sw);
				if(ret != EMV_OK)	//if mandatory data is missing or FCI is not well formed,ignore returned data and goto final select
				{
					if(g_emv_cnxt->candidates_num == 0)
					{
						return outcome_request_another_payment();
					}
					else if(g_emv_cnxt->candidates_num > 0)
					{
						return EMV_OK;
					}
				}
			}
			else
			{
				if(g_emv_cnxt->candidates_num > 0)
				{
					return EMV_OK;
				}
				else
				{
					return outcome_request_another_payment();
				}
			}

		}
	}
	else if(type == EMV_CARD_CONTACT)
	{
		for (i = 0; i < g_emv_cnxt->termapps_num; i++)
		{
			if (g_emv_cnxt->termapps[i].enable == 0)
			{
				continue;
			}

			sw = emvicc_select(g_emv_cnxt->termapps[i].aid, g_emv_cnxt->termapps[i].aid_len, false, resp, &resp_len);
			if(0x9000 == sw)
			{
				ret = parse_select_resp(resp, resp_len, sw);
			}
			else if(0x6283 == sw)
			{
				return rERR(AS_SELSW6283);
			}
			else if(0x6A81 == sw)
			{
				return rERR(AS_SELSW6A81);
			}
			else if(0x6D00 == sw)
			{
				flag6D00++;
				if (flag6D00 >= 4)
				{
					return rEND(EMV_FALLBACK); /* just for CASE CM091, fallback */
				}
				continue;
			}
			else
			{
				return rERR(AS_SELERR);
			}
		}
	}

    return EMV_OK;
}


static int32_t select_use_ppse(void)
{
    candidate_clr();

    return select_ddf((uint8_t*)DF_PPSE, emv_strlen(DF_PPSE), NO_9F11);
}

static int32_t select_use_pse(void)
{
    candidate_clr();

    return select_ddf((uint8_t*)DF_PSE, emv_strlen(DF_PSE), NO_9F11);
}

int32_t emv_build_candidate_list(void)
{
    int ret;
    int i, j,k;
    EMV_CANDIDATE tmp;

	ret = select_use_ppse();
	eTRACE("paypass-info: select_use_ppse ret = %d", ret);

	if (ret != EMV_OK && ret != EMV_STA_CANDIDATES_BUILT)
	{
		return ret;
	}

	eTRACE("paypass-info: candidates_num = %d", g_emv_cnxt->candidates_num);

	if(g_emv_cnxt->candidates_num == 0)
	{
		eTRACE("paypass-info: outcome_empty_candidate_list");

		// S52.14
		g_err_indicator.L2 = ERR_INFO_L2_EMPTY_CANDIDATE_LIST;
		// S52.18
		g_err_indicator.MsgOnError = UIREQUEST_MESSAGEID_ERROR_OTHER_CARD;

		appdata_save(TAG_DF8115_ERROR_INDICATION, (uint8_t *)&g_err_indicator, sizeof(g_err_indicator), OVERRIDED);
		emv_memset(g_discretionary_data, 0, sizeof(g_discretionary_data));
		g_discretionary_data_len = 0;

		emv_memcpy(g_discretionary_data, "\xDF\x81\x15\x06", 4);
		g_discretionary_data_len += 4;

		emv_memcpy(g_discretionary_data+g_discretionary_data_len, (void *)&g_err_indicator, sizeof(g_err_indicator));
		g_discretionary_data_len += sizeof(g_err_indicator);

		appdata_save(TAG_FF8106_DISCRETIONARY_DATA, g_discretionary_data, g_discretionary_data_len, OVERRIDED);
		// S52.19
		return outcome_other_card(0, 0);
	}

	eTRACE("paypass-info: before sort and dedumplicate");
	eTRACE("candidates_num = %d",g_emv_cnxt->candidates_num);

    for (i = 0; i < g_emv_cnxt->candidates_num; i++)
    {
		g_emv_cnxt->candidates[i].priority &= 0x0F;

		if(g_emv_cnxt->candidates[i].priority == 0x00)
		{
			g_emv_cnxt->candidates[i].priority = 0x0F;
		}
		eTRACE("paypass-info: i = %d", i);
		eTRACE("paypass-info: priority = %d", g_emv_cnxt->candidates[i].priority);
		eTRACE("paypass-info: AID len = %d", g_emv_cnxt->candidates[i].aid_len);
		eTRACE("paypass-info: kernel id = %02X", g_emv_cnxt->candidates[i].kernel_id[0]);
		eTRACE("paypass-info: kernel id len= %d", g_emv_cnxt->candidates[i].kernel_id_len);
		eTRACE_HEX("paypass-info: AID in candidate", g_emv_cnxt->candidates[i].aid, g_emv_cnxt->candidates[i].aid_len);
	}

	/* 去重：同一 ADF Name + 同一 Kernel ID 只保留首次出现的候选（保持 PPSE 中出现顺序），
	   修复同一卡应用匹配多个终端 AID 时被重复加入候选的问题 */
	for (i = 0; i < g_emv_cnxt->candidates_num; i++)
	{
		j = i + 1;
		while (j < g_emv_cnxt->candidates_num)
		{
			if ((g_emv_cnxt->candidates[i].aid_len == g_emv_cnxt->candidates[j].aid_len) &&
				(g_emv_cnxt->candidates[i].kernel_id_len == g_emv_cnxt->candidates[j].kernel_id_len) &&
				!emv_memcmp(g_emv_cnxt->candidates[i].aid, g_emv_cnxt->candidates[j].aid, g_emv_cnxt->candidates[i].aid_len) &&
				!emv_memcmp(g_emv_cnxt->candidates[i].kernel_id, g_emv_cnxt->candidates[j].kernel_id, g_emv_cnxt->candidates[i].kernel_id_len))
			{
				/* 找到重复项：删除下标 j（后续项前移，首次出现者保留） */
				for (k = j; k < g_emv_cnxt->candidates_num - 1; k++)
				{
					emv_memcpy(&g_emv_cnxt->candidates[k], &g_emv_cnxt->candidates[k + 1], sizeof(EMV_CANDIDATE));
				}
				g_emv_cnxt->candidates_num--;
				continue;
			}

			j++;
		}
	}

	//insert sort
	for (i = 1; i < g_emv_cnxt->candidates_num; i++)
	{
		emv_memset((void *)&tmp, 0, sizeof(EMV_CANDIDATE));
		emv_memcpy(&tmp, &g_emv_cnxt->candidates[i], sizeof(EMV_CANDIDATE));
		j = i - 1;

		while (j >= 0 && g_emv_cnxt->candidates[j].priority > tmp.priority)
		{
			emv_memcpy(&g_emv_cnxt->candidates[j+1], &g_emv_cnxt->candidates[j], sizeof(EMV_CANDIDATE));
			j--;
		}

		emv_memcpy(&g_emv_cnxt->candidates[j+1], &tmp, sizeof(EMV_CANDIDATE));
	}

	eTRACE("paypass-info: after sort");
	eTRACE("paypass-info: g_emv_cnxt->candidates_num = %d", g_emv_cnxt->candidates_num);

    for (i = 0; i < g_emv_cnxt->candidates_num; i++)
    {
		eTRACE("paypass-info: i = %d", i);
		eTRACE("paypass-info: priority = %d", g_emv_cnxt->candidates[i].priority);
		eTRACE("paypass-info: AID len = %d", g_emv_cnxt->candidates[i].aid_len);
		eTRACE_HEX("paypass-info: AID in candidate", g_emv_cnxt->candidates[i].aid, g_emv_cnxt->candidates[i].aid_len);
	}

	g_emv_cnxt->req_sel_candidates_result = 0;

    return rNXT(EMV_STA_CANDIDATES_BUILT);
}


/*
************************************************************************************************************************
*                                               IMPLEMENTATION FUNCTIONS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
