#include <string.h>
/**
 *******************************************************************************
 * @file    emvtlv.c
 * @author  KINGSEE Development Team xinyong
 * @version V43
 * @date    2015��10��21��
 * @brief   <�ļ�����>
 *******************************************************************************
 * @copy
 * <h2><center>&copy; COPYRIGHT 2012 Kingsee Tech. Co., Ltd.</center></h2>
 */

#include "emvi.h"
#include <stdlib.h>

static int32_t recur_decode(int32_t parent, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags);

#define is_constructed(c)       ((c) & 0x20)
#define is_primitive(c)         (!is_constructed((c)))
#define has_subsequent(c)       (((c) & 0x1F) == 0x1F)
#define another_byte_follow(c)  ((c) & 0x80)
#define lenlen_exceed_one(c)    ((c) & 0x80)


int32_t tlv_get_tag(uint8_t* buf, uint32_t* tag)
{
    int32_t taglen = 0;
    uint32_t tmptag = 0;

    if (has_subsequent(buf[taglen]))
    {
        do {
            tmptag = (tmptag | buf[taglen++]) << 8;
            if (taglen == 4)
            {
                return -1; //��ǩ�򳤶����4�ֽ�
            }
        } while (another_byte_follow(buf[taglen]));
    }

    *tag = (tmptag | buf[taglen++]);

    return taglen;
}

int32_t tlv_get_len(uint8_t* buf, int32_t* len)
{
    int32_t i, lenlen = 0;
    int32_t tmplen = 0;

    tmplen = buf[lenlen] & 0x7F;
    if (lenlen_exceed_one(buf[lenlen++]))
    {
        if (tmplen < 1 || tmplen > 3)
        {
            return -1; //�����򳤶����4�ֽ�
        }

        i = tmplen;
        tmplen = 0;
        while (i > 0)
        {
            tmplen |= buf[lenlen++] << (--i * 8);
        }
    }

    *len = tmplen;

    return lenlen;
}

int32_t tlv_set_tag(uint32_t tag, uint8_t* buf)
{
    int32_t i, taglen;
    uint32_t tmptag = tag;
    int32_t tmplen;
    uint8_t  tmpbuf[5];

    if (tmptag == 0)
    {
        tmpbuf[0] = 0x00;
        taglen = 1;
    }
    else
    {
        for (i = 3; i >= 0; i--)
        {
            if ((uint8_t)((tmptag >> (i * 8)) & 0xff) != 0x00)
            {
                break;
            }
        }

        taglen = i + 1; // tagΪ0����1�ֽ�

        for (; i >= 0; i--)
        {
            tmpbuf[i] = (uint8_t)(tmptag & 0xFF);
            tmptag >>= 8;
        }
    }

    //����ǩ�Ƿ�Ϸ�
#if 1
    tmptag = 0;
    tmplen = tlv_get_tag(tmpbuf, &tmptag);
    if (tmplen != taglen || tmptag != tag)
    {
        return -1;
    }
#endif

    emv_memcpy(buf, tmpbuf, taglen);

    return taglen;
}

int32_t tlv_set_len(int32_t len, uint8_t* buf)
{
    int32_t lenlen;

    if (len < 0 || len >= 16777216 /* 256*256*256 */)
    {
        return -1;
    }
    else if (len < 128)
    {
        lenlen = 1;
        buf[0] = (uint8_t)len;
    }
    else if (len < 256)
    {
        lenlen = 2;
        buf[0] = (uint8_t)0x81;
        buf[1] = (uint8_t)len;
    }
    else if (len < 65536 /* 256*256 */)
    {
        lenlen = 3;
        buf[0] = (uint8_t)0x82;
        buf[1] = (uint8_t)((len >>  8) & 0xff);
        buf[2] = (uint8_t)((len >>  0) & 0xff);
    }
    else
    {
        lenlen = 4;
        buf[0] = (uint8_t)0x83;
        buf[1] = (uint8_t)((len >> 16) & 0xff);
        buf[2] = (uint8_t)((len >>  8) & 0xff);
        buf[3] = (uint8_t)((len >>  0) & 0xff);
    }

    return lenlen;
}

static int32_t recur_decode_paypass(int32_t parent, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags)
{
	int32_t tmptag;
	int32_t tmplen, i;
	uint8_t *data = tlvs[parent].pvalue;
	int32_t data_len = tlvs[parent].valuelen;
	int32_t data_index;
	int32_t tlvs_index = parent + 1;

	for(data_index = 0; data_index < data_len;)
	{
//        if (data[data_index] == 0x00 || data[data_index] == 0xFF)
		if (data[data_index] == 0x00)
        {
            data_index++;
            continue;
        }

		if (tlvs_index == tlvs_max)
		{
			return rERR(TLV_NOOBJSPACE);
		}

		if (is_primitive(data[data_index]))
		{
			tlvs[tlvs_index].childnum = -1;
		}
		else
		{
			tlvs[tlvs_index].childnum = 0;
		}
		////////////////////////////
		tmptag = 0;
		if (has_subsequent(data[data_index]))
		{
			i = 0;
			do {
				tmptag = (tmptag | data[data_index++]) << 8;
				if (++i == 4 || data_index == data_len)
				{
					return rERR(TLV_DETAGLENERR);
				}
			} while (another_byte_follow(data[data_index]));
		}
		tlvs[tlvs_index].tagname = tmptag | data[data_index++];

		if (data_index == data_len)
		{
			return rERR(TLV_DETAGLENERR);
		}
		////////////////////////////
		tmplen = data[data_index] & 0x7F;
		if (lenlen_exceed_one(data[data_index++]))
		{
			if (tmplen < 1 || tmplen > 3 || tmplen > (data_len - data_index))
			{
				return rERR(TLV_DELENLENERR);
			}
			i = tmplen;
			tmplen = 0;
			while (i > 0)
			{
				tmplen |= data[data_index++] << (--i * 8);
			}
		}
		if (tmplen > (data_len - data_index))
		{
			return rERR(TLV_DELENLENERR);
		}
		////////////////////////////
		tlvs[tlvs_index].valuelen = tmplen;
		tlvs[tlvs_index].parent = parent;
		tlvs[tlvs_index].pvalue = data + data_index;
		data_index += tmplen;

		if (tlvs[tlvs_index].childnum == 0 && !(flags & DECODE_LEVEL1))
		{
			if ((tmplen = recur_decode(tlvs_index, tlvs, tlvs_max, flags)))
			{
				return tmplen;
			}
			tlvs_index += tlvs[tlvs_index].childnum;
		}
		tlvs_index++;
	}
	tlvs[parent].childnum = tlvs_index - parent - 1;
	return 0;
}


static int32_t recur_decode(int32_t parent, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags)
{
	int32_t tmptag;
	int32_t tmplen, i;
	uint8_t *data = tlvs[parent].pvalue;
	int32_t data_len = tlvs[parent].valuelen;
	int32_t data_index;
	int32_t tlvs_index = parent + 1;

	for (data_index = 0; data_index < data_len;)
	{
        if (data[data_index] == 0x00 || data[data_index] == 0xFF)
        {
            data_index++;
            continue;
        }

		if (tlvs_index == tlvs_max)
		{
			return rERR(TLV_NOOBJSPACE);
		}

		if (is_primitive(data[data_index]))
		{
			tlvs[tlvs_index].childnum = -1;
		}
		else
		{
			tlvs[tlvs_index].childnum = 0;
		}
		////////////////////////////
		tmptag = 0;
		if (has_subsequent(data[data_index]))
		{
			i = 0;
			do {
				tmptag = (tmptag | data[data_index++]) << 8;
				if (++i == 4 || data_index == data_len)
				{
					return rERR(TLV_DETAGLENERR);
				}
			} while (another_byte_follow(data[data_index]));
		}
		tlvs[tlvs_index].tagname = tmptag | data[data_index++];

		if (data_index == data_len)
		{
			return rERR(TLV_DETAGLENERR);
		}
		////////////////////////////
		tmplen = data[data_index] & 0x7F;
		if (lenlen_exceed_one(data[data_index++]))
		{
			if (tmplen < 1 || tmplen > 3 || tmplen > (data_len - data_index))
			{
				return rERR(TLV_DELENLENERR);
			}
			i = tmplen;
			tmplen = 0;
			while (i > 0)
			{
				tmplen |= data[data_index++] << (--i * 8);
			}
		}
		if (tmplen > (data_len - data_index))
		{
			return rERR(TLV_DELENLENERR);
		}
		////////////////////////////
		tlvs[tlvs_index].valuelen = tmplen;
		tlvs[tlvs_index].parent = parent;
		tlvs[tlvs_index].pvalue = data + data_index;
		data_index += tmplen;

		if (tlvs[tlvs_index].childnum == 0 && !(flags & DECODE_LEVEL1))
		{
			if ((tmplen = recur_decode(tlvs_index, tlvs, tlvs_max, flags)))
			{
				return tmplen;
			}
			tlvs_index += tlvs[tlvs_index].childnum;
		}
		tlvs_index++;
	}
	tlvs[parent].childnum = tlvs_index - parent - 1;
	return EMV_OK;
}

int32_t emvtlv_decode_paypass(uint8_t *data, int32_t data_len, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags)
{
	int32_t ret;

	if (data == NULL || tlvs == NULL || tlvs_max <= 1)
	{
		return rERR(TLV_INPUTDATAERR);
	}

//	eTRACE("data = %p", data);
//	eTRACE("tlvs = %p", tlvs);
//	eTRACE("tlvs_max = %d", tlvs_max);
//	eTRACE("recur_decode_back = %p", recur_decode_back);

	emv_memset(tlvs, 0x00, sizeof(EMV_TLVOBJ) * tlvs_max);

	tlvs[0].pvalue = data;
	tlvs[0].valuelen = data_len;

//	eTRACE_HEX("tlvs[0].pvalue", tlvs[0].pvalue, tlvs[0].valuelen);

	ret = recur_decode_paypass(0, tlvs, tlvs_max, flags);
//	eTRACE("recur_decode ret = %d", ret);
//	ret = recur_decode_back(0, tlvs, tlvs_max, flags);
//	eTRACE("recur_decode_back ret = %d", ret);
	if (ret != 0)
	{
		return ret;
	}

	if (flags & SINGLE_TLVOBJ)
	{
		if ((tlvs[1].pvalue + tlvs[1].valuelen) != (data + data_len))
		{
//			eTRACE("tlvs[1].pvalue = %p");
//			eTRACE("tlvs[1].valuelen = %d");
//			eTRACE("data = %p");
//			eTRACE("data_len = %d");
//			eTRACE("tlv end ptr = %p", (tlvs[1].pvalue + tlvs[1].valuelen));
//			eTRACE("resp end ptr = %p", (data + data_len));
			return rERR(TLV_NOTSINGLEOBJ);
		}
	}

	return EMV_OK;
}


int32_t emvtlv_decode(uint8_t *data, int32_t data_len, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags)
{
	int32_t ret;

	if (data == NULL || tlvs == NULL)
	{
		return rERR(TLV_INPUTDATAERR);
	}

	emv_memset(tlvs, 0x00, sizeof(EMV_TLVOBJ) * tlvs_max);

	tlvs[0].pvalue = data;
	tlvs[0].valuelen = data_len;

	if ((ret = recur_decode(0, tlvs, tlvs_max, flags)))
	{
		return ret;
	}

	if (flags & SINGLE_TLVOBJ)
	{
		if ((tlvs[1].pvalue + tlvs[1].valuelen) != (data + data_len))
		{
			return rERR(TLV_NOTSINGLEOBJ);
		}
	}

	return EMV_OK;
}

//if u use this interface,it will malloc memory,need to free in outside caller
int32_t emvtlv_decode_itemlist(uint8_t *data, int32_t data_len, EMV_TLVOBJ *tlvs, int32_t tlvs_max, int32_t flags)
{
	int32_t i, j, k, ret;
	int32_t t_len,len_byte,rest_byte,v_len;

	if (data == NULL || tlvs == NULL)
	{
		return rERR(TLV_INPUTDATAERR);
	}

	emv_memset(tlvs, 0x00, sizeof(EMV_TLVOBJ) * tlvs_max);
	tlvs[0].valuelen = data_len;
	tlvs[0].pvalue = (uint8_t *)malloc(tlvs[0].valuelen);
	emv_memset(tlvs[0].pvalue, 0, tlvs[0].valuelen);
	emv_memcpy(tlvs[0].pvalue, data, tlvs[0].valuelen);

	if (flags & STRING_TLVOBJ)
	{
		i = 0;
		j = 1;

		while (i < data_len)
		{
			if(j > tlvs_max || i >= data_len)
			{
				break;
			}

			//store T
			if(data[i] & 0x7F)
			{
				if(data[i+1] & 0x80)
				{
					t_len = 3;
				}
				else
				{
					t_len = 2;
				}
			}
			else
			{
				t_len = 1;
			}

			tlvs[j].tagname = emv_hex2dec(data+i, t_len);
			i += t_len;

			//store L
			if(data[i] & 0x80)
			{
				len_byte = (data[i] & 0x7F) + 1;
			}
			else
			{
				len_byte = 1;
			}

			rest_byte = len_byte - 1;
			if(rest_byte > 1)
			{
				for(k = 1; k < rest_byte; k++)
				{
					tlvs[j].valuelen |= data[i+k] << (8 * (len_byte - k));
				}
			}
			else
			{
				if(len_byte > 1)
				{
					tlvs[j].valuelen = data[i+1];
				}
				else
				{
					tlvs[j].valuelen = data[i];
				}
			}

			i += len_byte;

			//store V
			if(tlvs[j].valuelen != 0)
			{
				tlvs[j].pvalue = (uint8_t *)malloc(tlvs[j].valuelen);
				emv_memset(tlvs[j].pvalue, 0, tlvs[j].valuelen);
				emv_memcpy(tlvs[j].pvalue, data+i, tlvs[j].valuelen);
				i += tlvs[j].valuelen;
			}
			j++;

		}

		tlvs[0].childnum = j - 1;
		return j;
	}
	else
	{
		return TLV_ERR;
	}
}


int32_t emvtlv_get(int32_t parent, uint32_t tag, EMV_TLVOBJ *tlvs, int32_t level)
{
    int32_t i, end;

    if (parent < 0)
    {
        parent = 0;
    }

    if (tlvs == NULL || tlvs[parent].childnum < 0)
    {
        return 0;
    }

    i = parent + 1;
	end = tlvs[parent].childnum + i;

	while (i < end)
	{
		if (tlvs[i].tagname == tag)
		{
			return i;
		}

		if (level == SEARCH_ONLY_SON)
		{
			if (tlvs[i].childnum > 0)
			{
				i += tlvs[i].childnum;
			}
		}

		i++;
	}

    return 0;
}

int32_t emvtlv_find_dup(int32_t pos, EMV_TLVOBJ *tlvs)
{
    int32_t i;
    int32_t end;

	if (pos <= 0 || tlvs == NULL)
	{
		return 0;
	}

	end = tlvs[0].childnum;
	i = pos + 1;
	while (i <= end)
	{
		if (tlvs[i].tagname == tlvs[pos].tagname)
		{
			return i;
		}
		i++;
	}

    return 0;
}




////////////////////////////////////////////////////////////////////////////////
//TODO ����ȷ��
static const uint32_t tag_N[] = {
    0x9A,   0x9C,   0x5F24, 0x5F25, 0x5F28, 0x5F2A, 0x5F34, 0x5F36, 0x9F02,
    0x9F03, 0x9F11, 0x9F15, 0x9F1A, 0x9F21, 0x9F35, 0x9F39, 0x9F3C, 0x9F3D,
    0x9F41, 0x9F42, 0x9F44, 0x9F51, 0x9F54, 0x9F57, 0x9F5C, 0x9F73, 0x9F75,
    0x9F76
};

static const uint32_t tag_emv_reserved[] = { 0x85,0x8b,0x96,0x9e,0x9f0a,0x9F28, 0x9F29, 0x9F2a,0x9F2b,0x9f30, 0x9f31,0x9F3e,0x9F3f,
										0x9f50,0x9f51,0x9f52,0x9f53,0x9f54,0x9f55,0x9f56,0x9f57,0x9f58,0x9f59,0x9f5a,0x9f5b,0x9f5c,0x9f5d,0x9f5e,0x9f5f,
										0x9f60,0x9f61,0x9f62,0x9f63,0x9f64,0x9f65,0x9f66,0x9f67,0x9f68,0x9f69,0x9f6a,0x9f6b,0x9f6c,0x9f6d,0x9f6e,0x9f6f,
										0x9f70,0x9f71,0x9f72,0x9f73,0x9f74,0x9f75,0x9f76,0x9f77,0x9f78,0x9f79,0x9f7a,0x9f7b,0x9f7c,0x9f7d,0x9f7e,0x9f7f
										};


static const uint32_t tag_CN[] = { 0x5A, 0x9F20, 0x9F62 };

static const int32_t tag_NOTApply[] = {0x8B, 0x9F3E, 0x9F3F};

////////////////////////////////////////////////////////////////////////////////

#define N    	0x01
#define B    	0x02
#define ANS  	0x04
#define CN   	0x08
#define AN   	0x10
#define NOT_APP	0x20
#define OTH		0x40


static int32_t find_object_format(int32_t tagname)
{
    int32_t left, right, mid;

    left = 0;
    right = sizeof(tag_N) / sizeof(int32_t) - 1;
    while (left <= right) {
        mid = (left + right) / 2;
        if (tagname == tag_N[mid])
            return N;
        if (tagname > tag_N[mid])
            left = mid + 1;
        else
            right = mid - 1;
    }

    left = 0;
    right = sizeof(tag_CN) / sizeof(int32_t) - 1;
    while (left <= right) {
        mid = (left + right) / 2;
        if (tagname == tag_CN[mid])
            return CN;
        if (tagname > tag_CN[mid])
            left = mid + 1;
        else
            right = mid - 1;
    }

    left = 0;
    right = sizeof(tag_NOTApply) / sizeof(int32_t) - 1;
    while (left <= right) {
        mid = (left + right) / 2;
        if (tagname == tag_NOTApply[mid])
            return NOT_APP;
        if (tagname > tag_NOTApply[mid])
            left = mid + 1;
        else
            right = mid - 1;
    }

    return OTH;
}

static int32_t find_emv_reserved_tag(uint32_t tagname)
{
    int32_t left, right, mid;

    left = 0;
    right = sizeof(tag_emv_reserved) / sizeof(uint32_t) - 1;
    while (left <= right) {
        mid = (left + right) / 2;
        if (tagname == tag_emv_reserved[mid])
            return 0;
        if (tagname > tag_emv_reserved[mid])
            left = mid + 1;
        else
            right = mid - 1;
    }
	return -1;
}

int32_t remove_tag_from_tags2readyet(int32_t tagname)
{
	uint8_t buf[256]={0},tag_hex[3] = {0};
	uint8_t *tags_to_read_yet,*tag_add;
	int32_t cur_len,tag_len,i=0;
	int32_t start=-1,end=-1,pos;
	int32_t move_count;

	eTRACE("paypass-info: start remove_tag_from_tags2readyet");

	eTRACE("paypass-info: input tag: %03X", tagname);

	tags_to_read_yet = appdata_find(TAG_DF8113_TAGS_TO_READ_YET, &cur_len);
	if(!tags_to_read_yet || cur_len <= 0 || cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	emv_memcpy(buf, tags_to_read_yet, cur_len);

	tag_len = GetTagLength(tagname);
	eTRACE("paypass-info: tag_len: %d", tag_len);

	emv_dec2hex(tagname, tag_hex, tag_len);

	for (pos = 0; pos <= cur_len - tag_len; pos++)
	{
		if (!emv_memcmp(tag_hex, buf + pos, tag_len))
		{
		    move_count = cur_len - pos - tag_len;
		    if (move_count > 0)
			{
		        memmove(buf + pos, buf + pos + tag_len, move_count);
		    }

		    appdata_save(TAG_DF8113_TAGS_TO_READ_YET, buf, cur_len - tag_len, OVERRIDED);
		    return EMV_OK;
		}
	}

	return EMV_ERR;
}

int32_t remove_tag_from_tags2write_before_gac(int32_t tagname)
{
	uint8_t buf[256]={0},tag_hex[3] = {0};
	uint8_t *tags_to_read_yet,*tag_add;
	int32_t cur_len,tag_len,i=0;
	int32_t start=-1,end=-1,pos;
	int32_t move_count;

	eTRACE("paypass-info: start remove_tag_from_tags2write_before_gac");

	eTRACE("paypass-info: input tag: %03X", tagname);

	tags_to_read_yet = appdata_find(TAG_FF8102_TAGS_TO_WRITE_BEFOREGAC, &cur_len);
	if(!tags_to_read_yet || cur_len <= 0 || cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	emv_memcpy(buf, tags_to_read_yet, cur_len);

	tag_len = GetTagLength(tagname);
	eTRACE("paypass-info: tag_len: %d", tag_len);

	emv_dec2hex(tagname, tag_hex, tag_len);

	for (pos = 0; pos <= cur_len - tag_len; pos++)
	{
		if (!emv_memcmp(tag_hex, buf + pos, tag_len))
		{
		    move_count = cur_len - pos - tag_len;
		    if (move_count > 0)
			{
		        memmove(buf + pos, buf + pos + tag_len, move_count);
		    }

		    appdata_save(TAG_FF8102_TAGS_TO_WRITE_BEFOREGAC, buf, cur_len - tag_len, OVERRIDED);
		    return EMV_OK;
		}
	}

	return EMV_ERR;
}

int32_t add_tag_to_tags2readyet(int32_t tagname)
{
	uint8_t buf[256]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,add_len;

	eTRACE("paypass-info: start add_tag_to_tags2readyet");
	eTRACE("paypass-info: tagname = %03X", tagname);

	data_need = appdata_find(TAG_DF8113_TAGS_TO_READ_YET, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	add_len = GetTagLength(tagname);
	emv_dec2hex(tagname, tag_hex, add_len);

	if(data_need)
	{
		if(cur_len > 0)
		{
			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + add_len > sizeof(buf))
			{
				return EMV_ERR;
			}

			emv_memcpy(buf+cur_len, tag_hex, add_len);
			cur_len += add_len;
			appdata_save(TAG_DF8113_TAGS_TO_READ_YET, buf, cur_len, OVERRIDED);
		}
		else
		{
			appdata_save(TAG_DF8113_TAGS_TO_READ_YET, tag_hex, add_len, OVERRIDED);
		}
	}

	return EMV_OK;
}

int32_t add_tag_to_dataneed(int32_t tagname)
{
	uint8_t buf[256]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,add_len;

	eTRACE("paypass-info: start add_tag_to_dataneed");

	data_need = appdata_find(TAG_DF8106_DATA_NEEDED, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	add_len = GetTagLength(tagname);
	eTRACE("paypass-info: add_len = %d", add_len);

	emv_dec2hex(tagname, tag_hex, add_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, add_len);

	if(data_need)
	{
		if(cur_len > 0)
		{
			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + add_len > sizeof(buf))
			{
				return EMV_ERR;
			}

			emv_memcpy(buf+cur_len, tag_hex, add_len);
			cur_len += add_len;
			appdata_save(TAG_DF8106_DATA_NEEDED, buf, cur_len, OVERRIDED);
		}
		else
		{
			appdata_save(TAG_DF8106_DATA_NEEDED, tag_hex, add_len, OVERRIDED);
		}
	}

	return EMV_OK;
}

int32_t add_tag_to_tags2write_yet_before_gac(int32_t tagname)
{
	uint8_t buf[256]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,add_len,i=0;

	eTRACE("paypass-info: start add_tag_to_tags2write_yet_before_gac");

	data_need = appdata_find(TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	add_len = GetTagLength(tagname);
	eTRACE("paypass-info: add_len = %d", add_len);

	emv_dec2hex(tagname, tag_hex, add_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, add_len);

	if(data_need)
	{
		if(cur_len > 0)
		{
			while (i < cur_len)
			{
				if(!emv_memcmp(data_need+i, tag_hex, add_len))
				{
					return EMV_OK;
				}
				i++;
			}

			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + add_len > sizeof(buf))
			{
				return EMV_ERR;
			}

			emv_memcpy(buf+cur_len, tag_hex, add_len);
			cur_len += add_len;
			appdata_save(TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC, buf, cur_len, OVERRIDED);
		}
		else
		{
			appdata_save(TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC, tag_hex, add_len, OVERRIDED);
		}
	}

	return EMV_OK;
}

int32_t add_tag_to_tags2write_yet_after_gac(int32_t tagname)
{
	uint8_t buf[256]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,add_len,i=0;

	eTRACE("paypass-info: start add_tag_to_tags2write_yet_before_gac");

	data_need = appdata_find(TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	add_len = GetTagLength(tagname);
	eTRACE("paypass-info: add_len = %d", add_len);

	emv_dec2hex(tagname, tag_hex, add_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, add_len);

	if(data_need)
	{
		if(cur_len > 0)
		{
			while (i < cur_len)
			{
				if(!emv_memcmp(data_need+i, tag_hex, add_len))
				{
					return EMV_OK;
				}
				i++;
			}

			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + add_len > sizeof(buf))
			{
				return EMV_ERR;
			}

			emv_memcpy(buf+cur_len, tag_hex, add_len);
			cur_len += add_len;
			appdata_save(TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC, buf, cur_len, OVERRIDED);
		}
		else
		{
			appdata_save(TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC, tag_hex, add_len, OVERRIDED);
		}
	}

	return EMV_OK;
}


bool data2send_contains_key(int32_t tagname)
{
	uint8_t buf[512]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,tag_len,len_byte,i;

	data_need = appdata_find(TAG_FF8104_DATA_TO_SEND, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	tag_len = GetTagLength(tagname);
	eTRACE("paypass-info: tag_len = %d", tag_len);

	emv_dec2hex(tagname, tag_hex, tag_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, tag_len);

	i = 0;
	while (i < cur_len)
	{
		if(!emv_memcmp(data_need+i, tag_hex, tag_len))
		{
			return true;
		}
		//skip T
		if((data_need[i] & 0x1F) == 0x1F)
		{
			if((data_need[i+1] & 0x80) == 0x80)
			{
				i += 3;
			}
			else
			{
				i += 2;
			}
		}
		else
		{
			i += 1;
		}

		if(data_need[i] & 0x80)
		{

		}

	}

	return false;
}

int32_t add_tlv_to_tags2write_yet_after_gac(int32_t tagname, int32_t len, uint8_t *value)
{
	uint8_t buf[512]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,tag_len,len_byte;
	int32_t offset = 0;

	eTRACE("paypass-info: start add_tlv_to_tags2write_yet_after_gac");

	data_need = appdata_find(TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	tag_len = GetTagLength(tagname);
	eTRACE("paypass-info: tag_len = %d", tag_len);

	emv_dec2hex(tagname, tag_hex, tag_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, tag_len);

	if(len > 127)
	{
		len_byte = 2;
	}
	else
	{
		len_byte = 1;
	}

	if (data_need && cur_len > 0)
	{
		while (offset < cur_len)
		{
			int32_t parsed_tag = 0;
            int32_t parsed_tag_len = tlv_get_tag(data_need + offset, &parsed_tag);
            if (parsed_tag_len <= 0)
			{
				break;
			}

            offset += parsed_tag_len;

			int32_t value_len = 0;
            int32_t value_len_bytes = tlv_get_len(data_need + offset, &value_len);
            if (value_len_bytes <= 0)
			{
				break;
			}
            offset += value_len_bytes;

			if (parsed_tag == tagname)
			{
                eTRACE("paypass-info: tag already exists, skip");
                return EMV_OK;
            }
			offset += value_len;

		}
	}


	if(data_need)
	{
		if(cur_len > 0)
		{
			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + tag_len + len + len_byte> sizeof(buf))
			{
				return EMV_ERR;
			}
			//T
			emv_memcpy(buf+cur_len, tag_hex, tag_len);
			cur_len += tag_len;
			//L
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;
			}
			else
			{
				buf[cur_len++] = (uint8_t)len;
			}
			//V
			if(len > 0 && value != NULL)
			{
				emv_memcpy(buf+cur_len, value, len);
			}
			cur_len += len;
			appdata_save(TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC, buf, cur_len, OVERRIDED);
		}
		else
		{
			cur_len = 0;

			if(cur_len + tag_len + len + len_byte> sizeof(buf))
			{
				return EMV_ERR;
			}

			//T
			emv_memcpy(buf+cur_len, tag_hex, tag_len);
			cur_len += tag_len;
			//L
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;
			}
			else
			{
				buf[cur_len++] = (uint8_t)len;
			}
			//V
			if(len > 0 && value != NULL)
			{
				emv_memcpy(buf+cur_len, value, len);
			}
			cur_len += len;

			appdata_save(TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC, buf, cur_len, OVERRIDED);
		}
	}

	return EMV_OK;
}


int32_t add_tlv_to_tags2write_yet_before_gac(int32_t tagname, int32_t len, uint8_t *value)
{
	uint8_t buf[512]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,tag_len,len_byte;
	int32_t offset = 0;

	eTRACE("paypass-info: start add_tlv_to_tags2write_yet_before_gac");

	data_need = appdata_find(TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	tag_len = GetTagLength(tagname);
	eTRACE("paypass-info: tag_len = %d", tag_len);

	emv_dec2hex(tagname, tag_hex, tag_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, tag_len);

	if(len > 127)
	{
		len_byte = 2;
	}
	else
	{
		len_byte = 1;
	}

	if (data_need && cur_len > 0)
	{
		while (offset < cur_len)
		{
			int32_t parsed_tag = 0;
            int32_t parsed_tag_len = tlv_get_tag(data_need + offset, &parsed_tag);
            if (parsed_tag_len <= 0)
			{
				break;
			}

            offset += parsed_tag_len;

			int32_t value_len = 0;
            int32_t value_len_bytes = tlv_get_len(data_need + offset, &value_len);
            if (value_len_bytes <= 0)
			{
				break;
			}
            offset += value_len_bytes;

			if (parsed_tag == tagname)
			{
                eTRACE("paypass-info: tag already exists, skip");
                return EMV_OK;
            }
			offset += value_len;

		}
	}


	if(data_need)
	{
		if(cur_len > 0)
		{
			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + tag_len + len + len_byte> sizeof(buf))
			{
				return EMV_ERR;
			}
			//T
			emv_memcpy(buf+cur_len, tag_hex, tag_len);
			cur_len += tag_len;
			//L
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;
			}
			else
			{
				buf[cur_len++] = (uint8_t)len;
			}
			//V
			if(len > 0 && value != NULL)
			{
				emv_memcpy(buf+cur_len, value, len);
			}
			cur_len += len;
			appdata_save(TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC, buf, cur_len, OVERRIDED);
		}
		else
		{
			cur_len = 0;

			if(cur_len + tag_len + len + len_byte> sizeof(buf))
			{
				return EMV_ERR;
			}

			//T
			emv_memcpy(buf+cur_len, tag_hex, tag_len);
			cur_len += tag_len;
			//L
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;
			}
			else
			{
				buf[cur_len++] = (uint8_t)len;
			}
			//V
			if(len > 0 && value != NULL)
			{
				emv_memcpy(buf+cur_len, value, len);
			}
			cur_len += len;

			appdata_save(TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC, buf, cur_len, OVERRIDED);
		}
	}

	return EMV_OK;
}


int32_t add_tlv_to_data2send(int32_t tagname, int32_t len, uint8_t *value)
{
	uint8_t buf[1024]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,tag_len,len_byte;
	int32_t offset = 0;

	eTRACE("paypass-info: start add_tlv_to_data2send");

	data_need = appdata_find(TAG_FF8104_DATA_TO_SEND, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	tag_len = GetTagLength(tagname);
	eTRACE("paypass-info: tag_len = %d", tag_len);

	emv_dec2hex(tagname, tag_hex, tag_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, tag_len);

	if(len > 127)
	{
		len_byte = 2;
	}
	else
	{
		len_byte = 1;
	}

	if (data_need && cur_len > 0)
	{
		while (offset < cur_len)
		{
			int32_t parsed_tag = 0;
            int32_t parsed_tag_len = tlv_get_tag(data_need + offset, &parsed_tag);
            if (parsed_tag_len <= 0)
			{
				break;
			}

            offset += parsed_tag_len;

			int32_t value_len = 0;
            int32_t value_len_bytes = tlv_get_len(data_need + offset, &value_len);
            if (value_len_bytes <= 0)
			{
				break;
			}
            offset += value_len_bytes;

			if (parsed_tag == tagname)
			{
                eTRACE("paypass-info: tag already exists, skip");
                return EMV_OK;
            }
			offset += value_len;

		}
	}


	if(data_need)
	{
		if(cur_len > 0)
		{
			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + tag_len + len + len_byte> sizeof(buf))
			{
				return EMV_ERR;
			}
			//T
			emv_memcpy(buf+cur_len, tag_hex, tag_len);
			cur_len += tag_len;
			//L
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;
			}
			else
			{
				buf[cur_len++] = (uint8_t)len;
			}
			//V
			if(len > 0 && value != NULL)
			{
				emv_memcpy(buf+cur_len, value, len);
			}
			cur_len += len;
			appdata_save(TAG_FF8104_DATA_TO_SEND, buf, cur_len, OVERRIDED);
		}
		else
		{
			cur_len = 0;

			if(cur_len + tag_len + len + len_byte> sizeof(buf))
			{
				return EMV_ERR;
			}

			//T
			emv_memcpy(buf+cur_len, tag_hex, tag_len);
			cur_len += tag_len;
			//L
			if(len_byte > 1)
			{
				buf[cur_len++] = 0x81;
				buf[cur_len++] = (uint8_t)len;
			}
			else
			{
				buf[cur_len++] = (uint8_t)len;
			}
			//V
			if(len > 0 && value != NULL)
			{
				emv_memcpy(buf+cur_len, value, len);
			}
			cur_len += len;

			appdata_save(TAG_FF8104_DATA_TO_SEND, buf, cur_len, OVERRIDED);
		}
	}

	return EMV_OK;
}


int32_t add_tag_to_data2send(int32_t tagname)
{
	uint8_t buf[512]={0},tag_hex[3] = {0};
	uint8_t *data_need,*tag_add;
	int32_t cur_len,add_len;

	eTRACE("paypass-info: start add_tag_to_dataneed");

	data_need = appdata_find(TAG_FF8104_DATA_TO_SEND, &cur_len);
	if(cur_len > sizeof(buf))
	{
		return EMV_ERR;
	}

	add_len = GetTagLength(tagname);
	eTRACE("paypass-info: add_len = %d", add_len);

	emv_dec2hex(tagname, tag_hex, add_len);
	eTRACE_HEX("paypass-info: tag_hex", tag_hex, add_len);

	if(data_need)
	{
		if(cur_len > 0)
		{
			emv_memcpy(buf, data_need, cur_len);

			if(cur_len + add_len > sizeof(buf))
			{
				return EMV_ERR;
			}

			emv_memcpy(buf+cur_len, tag_hex, add_len);
			cur_len += add_len;
			appdata_save(TAG_FF8104_DATA_TO_SEND, buf, cur_len, OVERRIDED);
		}
		else
		{
			appdata_save(TAG_FF8104_DATA_TO_SEND, tag_hex, add_len, OVERRIDED);
		}
	}

	return EMV_OK;
}

int32_t sdol_packet(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t val_max_len, bool match)
{
    int32_t i, j, fmt;
    int32_t reqlen, reallen, val_len;
    int32_t tagname;
    uint8_t* pdata;

    if (dol == NULL || val == NULL)
    {
        return 0;
	}

    val_len = 0;
    emv_memset(val, 0x00, val_max_len);

    for (i = 0; i < dollen;)
    {
        fmt = is_constructed(dol[i]);
        tagname = 0;
        if (has_subsequent(dol[i]))
        {
            j = 0;
            do
            {
                tagname = (tagname | dol[i++]) << 8;
                if (++j == 4)
                {
                    return -1;
                }
            } while (another_byte_follow(dol[i]));
        }
        tagname = tagname | dol[i++];

        reqlen = dol[i++] & 0x7F;
        if (lenlen_exceed_one(dol[i - 1]))
        {
            if (reqlen > 4)
            { //TODO ȷ����3����4
                return -1;
            }
            j = reqlen;
            reqlen = 0;
            while (j > 0)
            {
                reqlen |= dol[i++] << (--j * 8);
            }
        }

        eTRACE("fmt = [%d] tagname = [%02X], reqlen = [%d]", fmt, tagname, reqlen);
        eTRACE("i = [%d] dollen = [%d]", i, dollen);
        if (i > dollen)
        {
            return -1;
        }

		pdata = appdata_find(tagname, &reallen);
        eTRACE("reallen = [%d]", reallen);

        if (fmt || pdata == NULL)
        {
            emv_memset(val + val_len, 0x00, reqlen);
        }
        else
        {
            fmt = find_object_format(tagname);
            if (reallen < reqlen)
            {
                if (fmt & N)
                {
                    emv_memcpy(val + val_len + reqlen - reallen, pdata, reallen); /* right align, left zero padded */
				}
                else if(fmt & CN)
                {
                    emv_memcpy(val + val_len, pdata, reallen);
                    emv_memset(val + val_len + reallen, 0xFF, reqlen - reallen); /* left align, buf right 'FF' padded */
				}
				else
				{
                    emv_memcpy(val + val_len, pdata, reallen);
                    emv_memset(val + val_len + reallen, 0x00, reqlen - reallen); /* left align, buf right '00' padded */
				}
            }
            else
            {
                if (fmt & N)
                {
                    emv_memcpy(val + val_len, pdata + reallen - reqlen, reqlen);
				}
                else
                {
                    emv_memcpy(val + val_len, pdata, reqlen);
				}
            }

			//Book3 Section 5.4.2.e
			if(match)	//If a data object is in the list and is meaningful to the terminal but represents data that is not applicable to the current transaction, the portion of the command field representing the data object shall be filled with hexadecimal zeroes.
			{
				emv_memset(val + val_len, 0x00, reqlen);
			}
        }
        val_len += reqlen;
    }
    return val_len;
}


int32_t emvdol_packet(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t val_max_len)
{
    int32_t i, j, fmt;
    int32_t reqlen, reallen, val_len;
    int32_t tagname;
    uint8_t* pdata;
	EMV_TAG_ITEM item={0};

    if (dol == NULL || val == NULL)
    {
        return 0;
	}

    val_len = 0;
    emv_memset(val, 0x00, val_max_len);

    for (i = 0; i < dollen;)
    {
        fmt = is_constructed(dol[i]);
        tagname = 0;
        if (has_subsequent(dol[i]))
        {
            j = 0;
            do
            {
                tagname = (tagname | dol[i++]) << 8;
                if (++j == 4)
                {
                    return -1;
                }
            } while (another_byte_follow(dol[i]));
        }
        tagname = tagname | dol[i++];

        reqlen = dol[i++];

        eTRACE("fmt = [%d] tagname = [%02X], reqlen = [%d]", fmt, tagname, reqlen);
        eTRACE("i = [%d] dollen = [%d]", i, dollen);
        if (i > dollen)
        {
            return -1;
        }

		pdata = appdata_find(tagname, &reallen);
        eTRACE("reallen = [%d]", reallen);

        if (fmt || pdata == NULL)
        {
            emv_memset(val + val_len, 0x00, reqlen);
        }
        else
        {
            fmt = find_object_format(tagname);

			if (reallen < reqlen)
			{
				if (fmt & N)
				{
					emv_memcpy(val + val_len + reqlen - reallen, pdata, reallen); /* right align, left zero padded */
				}
				else if(fmt & CN)
				{
					emv_memcpy(val + val_len, pdata, reallen);
					emv_memset(val + val_len + reallen, 0xFF, reqlen - reallen); /* left align, buf right 'FF' padded */
				}
				else
				{
					emv_memcpy(val + val_len, pdata, reallen);
					emv_memset(val + val_len + reallen, 0x00, reqlen - reallen); /* left align, buf right '00' padded */
				}
			}
			else
			{
			   if (fmt & N)
			   {
					emv_memcpy(val + val_len, pdata + reallen - reqlen, reqlen);
			   }
			   else
			   {
					emv_memcpy(val + val_len, pdata, reqlen);
			   }
			}

			//Book3 Section 5.4.2.e
			if(!IsKnown(tagname) && tagname != 0xCF && tagname != 0x9F7E)	//If a data object is in the list and is meaningful to the terminal but represents data that is not applicable to the current transaction, the portion of the command field representing the data object shall be filled with hexadecimal zeroes.
			{
				emv_memset(val + val_len, 0x00, reqlen);
			}
        }
        val_len += reqlen;
    }
    return val_len;
}

int32_t emvdol_packet_emv(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t val_max_len)
{
    int32_t i, j, fmt;
    int32_t reqlen, reallen, val_len;
    uint32_t tagname;
    uint8_t* pdata;

    if (dol == NULL || val == NULL)
        return 0;

    val_len = 0;
    emv_memset(val, 0x00, val_max_len);

    for (i = 0; i < dollen;)
    {
        fmt = is_constructed(dol[i]);
        tagname = 0;
        if (has_subsequent(dol[i]))
        {
            j = 0;
            do
            {
                tagname = (tagname | dol[i++]) << 8;
                if (++j == 4)
                {
                    return -1;
                }
            } while (another_byte_follow(dol[i]));
        }
        tagname = tagname | dol[i++];

        reqlen = dol[i++] & 0x7F;
        if (lenlen_exceed_one(dol[i - 1]))
        {
            if (reqlen > 4)
            { //TODO ������?��?3?1��?4
                return -1;
            }
            j = reqlen;
            reqlen = 0;
            while (j > 0)
            {
                reqlen |= dol[i++] << (--j * 8);
            }
        }

        eTRACE("fmt = [%d] tagname = [%02X], reqlen = [%d]", fmt, tagname, reqlen);
        eTRACE("i = [%d] dollen = [%d]", i, dollen);
        if (i > dollen)
        {
            return -1;
        }

        if (fmt || (pdata = appdata_find(tagname, &reallen)) == NULL)
        {
            emv_memset(val + val_len, 0x00, reqlen);
        }
        else
        {
        	fmt = find_emv_reserved_tag(tagname);
			if(fmt == 0)
			{
				emv_memset(val + val_len, 0x00, reqlen);
			}
			else
			{
	            fmt = find_object_format(tagname);
	            if (reallen < reqlen)
	            {
	                if (fmt & N)
	                    emv_memcpy(val + val_len + reqlen - reallen, pdata, reallen); /* right align, left zero padded */
	                else
	                    emv_memcpy(val + val_len, pdata, reallen);
	                if (fmt & CN)
	                    emv_memset(val + val_len + reallen, 0xFF, reqlen - reallen); /* left align, buf right 'FF' padded */
	            }
	            else
	            {
	                if (fmt & N)
	                    emv_memcpy(val + val_len, pdata + reallen - reqlen, reqlen);
	                else
	                    emv_memcpy(val + val_len, pdata, reqlen);
	            }
			}
        }
        val_len += reqlen;
    }
    return val_len;
}

int32_t emvdol_packet_tlv(uint8_t *dol, int32_t dollen, uint8_t *val, int32_t vallen, uint8_t *tlv, int32_t tlvlmt)
{
    int32_t i, j, k, tlvlen;
    int32_t reqlen;

    if (dol == NULL || val == NULL || tlv == NULL || dollen <= 0 || vallen <= 0 || tlvlmt <= 0)
    {
        return -1;
    }

    for (i = 0, k = 0, tlvlen = 0; i < dollen && k < vallen; )
    {
        if (is_constructed(dol[i]))
        {
            return -1;
        }
        if (has_subsequent(dol[i]))
        {
            j = 0;
            do
            {
                tlv[tlvlen++] = dol[i++];
                if (++j == 4)
                {
                    return -1;
                }
            } while (another_byte_follow(dol[i]));
        }
        tlv[tlvlen++] = dol[i++];
        eTRACE_HEX("tlv", tlv, tlvlen);

        tlv[tlvlen++] = dol[i];
        reqlen = dol[i++] & 0x7F;
        if (lenlen_exceed_one(dol[i - 1]))
        {
            if (reqlen > 4)
            {
                return -1;
            }
            j = reqlen;
            reqlen = 0;
            while (j > 0)
            {
                tlv[tlvlen++] = dol[i];
                reqlen |= dol[i++] << (--j * 8);
            }
        }
        eTRACE_HEX("tlv", tlv, tlvlen);
        eTRACE("i = %d", i);
        if (i > dollen || tlvlen > tlvlmt)
        {
            return -1;
        }

        emv_memcpy(tlv + tlvlen, val + k, reqlen);
        k += reqlen;
        tlvlen += reqlen;
        eTRACE_HEX("tlv", tlv, tlvlen);
        eTRACE("k = %d", k);
        if (k > vallen || tlvlen > tlvlmt)
        {
            return -1;
        }
    }
    eTRACE("i = %d, k = %d", i, k);
    if (i == dollen && k == vallen)
    {
        return tlvlen;
    }
    else
    {
        return -1;
    }
}

/**
 * @brief   ����DOL�еı�ǩ
 * @param   tagname     ��ǩ��
 * @param   dol         DOL
 * @param   dollen      DOL����
 * @return  0-û�ҵ�, 1-���ҵ�
 */
int32_t emvdol_tag_exists(uint32_t tagname, uint8_t *dol, int32_t dollen)
{
    int i, j;
    int reqlen;
    uint32_t tag;

    if (dol == NULL)
	{
        return 0;
    }

	eTRACE("input tagname: %02X", tagname);

    for (i = 0; i < dollen; )
	{
        tag = 0;
        if (has_subsequent(dol[i]))
		{
            j = 0;
            do
			{
                tag = (tag | dol[i++]) << 8;
                if (++j == 2)
				{
					eTRACE("return 0 flag1");
                    return 0;
                }
            } while (another_byte_follow(dol[i]));
        }
        tag = tag | dol[i++];

		eTRACE("tag in dol: %02X", tag);

        reqlen = dol[i++];
		eTRACE("reqlen: %d", reqlen);

        if (i > dollen)
		{
			eTRACE("return 0 flag3");
            return 0;
        }

        if (tag == tagname)
		{
            return 1;
        }
    }

	eTRACE("return 0 flag4");
    return 0;
}

int cdol_packet(uint8_t *dol, int dollen, uint8_t *val, int val_max_len)
{
    int i, j, fmt;
    int reqlen, reallen, val_len;
    uint32_t tagname;
    uint8_t* pdata;
	EMV_TAG_ITEM item={0};

    if (dol == NULL || val == NULL)
    {
        return 0;
	}

    val_len = 0;
    emv_memset(val, 0x00, val_max_len);

    for (i = 0; i < dollen;)
    {
        fmt = is_constructed(dol[i]);
        tagname = 0;
        if (has_subsequent(dol[i]))
        {
            j = 0;
            do
            {
                tagname = (tagname | dol[i++]) << 8;
                if (++j == 4)
                {
                    return -1;
                }
            } while (another_byte_follow(dol[i]));
        }
        tagname = tagname | dol[i++];

        reqlen = dol[i++];

        eTRACE("fmt = [%d] tagname = [%02X], reqlen = [%d]", fmt, tagname, reqlen);
        eTRACE("i = [%d] dollen = [%d]", i, dollen);
        if (i > dollen)
        {
            return -1;
        }

		pdata = appdata_find(tagname, &reallen);
        eTRACE("reallen = [%d]", reallen);

        if (fmt || pdata == NULL)
        {
            emv_memset(val + val_len, 0x00, reqlen);
        }
        else
        {
            fmt = find_object_format(tagname);

			if (reallen < reqlen)
			{
				if (fmt & N)
				{
					emv_memcpy(val + val_len + reqlen - reallen, pdata, reallen); /* right align, left zero padded */
				}
				else if(fmt & CN)
				{
					emv_memcpy(val + val_len, pdata, reallen);
					emv_memset(val + val_len + reallen, 0xFF, reqlen - reallen); /* left align, buf right 'FF' padded */
				}
				else
				{
					emv_memcpy(val + val_len, pdata, reallen);
					emv_memset(val + val_len + reallen, 0x00, reqlen - reallen); /* left align, buf right '00' padded */
				}
			}
			else
			{
			   if (fmt & N)
			   {
					emv_memcpy(val + val_len, pdata + reallen - reqlen, reqlen);
			   }
			   else
			   {
					emv_memcpy(val + val_len, pdata, reqlen);
			   }
			}

			//Book3 Section 5.4.2.e
			if(tagname == TAG_9F5F_DS_SLOT_AVAILABILITY || tagname == TAG_9F7F_DS_UNPREDICT_NUMBER || tagname == TAG_9F7D_DS_SUMMARY1 \
				|| tagname == TAG_9F54_DS_ODS_CARD || tagname == TAG_9F6F_DS_SLOT_MANAGE_CTRL || tagname ==TAG_9F5E_DS_ID)	//If a data object is in the list and is meaningful to the terminal but represents data that is not applicable to the current transaction, the portion of the command field representing the data object shall be filled with hexadecimal zeroes.
			{
				emv_memset(val + val_len, 0x00, reqlen);
			}
        }
        val_len += reqlen;
    }
    return val_len;
}

int udol_packet(uint8_t *dol, int dollen, uint8_t *val, int val_max_len)
{
    int i, j, fmt;
    int reqlen, reallen, val_len;
    uint32_t tagname;
    uint8_t* pdata;
	EMV_TAG_ITEM item={0};

    if (dol == NULL || val == NULL)
    {
        return 0;
	}

    val_len = 0;
    emv_memset(val, 0x00, val_max_len);

    for (i = 0; i < dollen;)
    {
        fmt = is_constructed(dol[i]);
        tagname = 0;
        if (has_subsequent(dol[i]))
        {
            j = 0;
            do
            {
                tagname = (tagname | dol[i++]) << 8;
                if (++j == 4)
                {
                    return -1;
                }
            } while (another_byte_follow(dol[i]));
        }
        tagname = tagname | dol[i++];

        reqlen = dol[i++];

        eTRACE("fmt = [%d] tagname = [%02X], reqlen = [%d]", fmt, tagname, reqlen);
        eTRACE("i = [%d] dollen = [%d]", i, dollen);
        if (i > dollen)
        {
            return -1;
        }

		pdata = appdata_find(tagname, &reallen);
        eTRACE("reallen = [%d]", reallen);

        if (fmt || pdata == NULL)
        {
            emv_memset(val + val_len, 0x00, reqlen);
        }
        else
        {
            fmt = find_object_format(tagname);

			if (reallen < reqlen)
			{
				if (fmt & N)
				{
					emv_memcpy(val + val_len + reqlen - reallen, pdata, reallen); /* right align, left zero padded */
				}
				else if(fmt & CN)
				{
					emv_memcpy(val + val_len, pdata, reallen);
					emv_memset(val + val_len + reallen, 0xFF, reqlen - reallen); /* left align, buf right 'FF' padded */
				}
				else
				{
					emv_memcpy(val + val_len, pdata, reallen);
					emv_memset(val + val_len + reallen, 0x00, reqlen - reallen); /* left align, buf right '00' padded */
				}
			}
			else
			{
			   if (fmt & N)
			   {
					emv_memcpy(val + val_len, pdata + reallen - reqlen, reqlen);
			   }
			   else
			   {
					emv_memcpy(val + val_len, pdata, reqlen);
			   }
			}

			//Book3 Section 5.4.2.e
			if(tagname == TAG_9F5F_DS_SLOT_AVAILABILITY || tagname == TAG_9F7F_DS_UNPREDICT_NUMBER || tagname == TAG_9F7D_DS_SUMMARY1 \
				|| tagname == TAG_9F54_DS_ODS_CARD || tagname == TAG_9F6F_DS_SLOT_MANAGE_CTRL || tagname ==TAG_9F5E_DS_ID)	//If a data object is in the list and is meaningful to the terminal but represents data that is not applicable to the current transaction, the portion of the command field representing the data object shall be filled with hexadecimal zeroes.
			{
				emv_memset(val + val_len, 0x00, reqlen);
			}
        }
        val_len += reqlen;
    }
    return val_len;
}



/************** (C) COPYRIGHT 2012 Kingsee Tech. Co., Ltd. *****END OF FILE****/
