#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emvbuf.h
* @author   Kingsee Development Team xinyong
* @version  V43
* @date     2015��10��21��
* @brief    Ӧ�����ݽӿ�
*
************************************************************************************************************************
*/

#include "emvi.h"

#define HASHTABLE_LEN   (271)

#define hash_func(x)    ((x) % HASHTABLE_LEN)


typedef struct _bufhdr
{
    uint32_t tagname;
    int32_t valuelen;
    struct _bufhdr *next;
	uint8_t is_active;
} bufheader;


static bufheader**	g_hashtable = NULL;
static uint8_t*			g_currpos = NULL;


int32_t appdata_init(void)
{
	int32_t i;
	htTRACE("paypass-info: start appdata_init");

	if (!g_emv_cnxt || !g_emv_cnxt->tlvbuffer)
	{
        htTRACE("FATAL: Context or buffer not initialized!");
        return -1;
    }

    size_t header_size = HASHTABLE_LEN * sizeof(bufheader*);
    if (g_emv_cnxt->tlvbuffer_size < header_size + 1024)
	{
        htTRACE("FATAL: Buffer too small! Min:%d Have:%d", header_size + 1024, g_emv_cnxt->tlvbuffer_size);
        return -1;
    }

//    emv_memset(g_emv_cnxt->tlvbuffer, 0x00, g_emv_cnxt->tlvbuffer_size);
//	eTRACE("DEBUG: g_emv_cnxt:%p g_emv_cnxt->tlvbuffer:%p", g_emv_cnxt, g_emv_cnxt->tlvbuffer);

    g_hashtable = (bufheader**)g_emv_cnxt->tlvbuffer;
    g_currpos = (uint8_t *)g_emv_cnxt->tlvbuffer + header_size;

    for (i = 0; i < HASHTABLE_LEN; i++)
	{
        g_hashtable[i] = NULL;
    }

	// 数据区域从哈希表头之后开始
	g_currpos = (uint8_t*)g_emv_cnxt->tlvbuffer + header_size;

	// 清空数据区域
	size_t data_size = g_emv_cnxt->tlvbuffer_size - header_size;
	emv_memset(g_currpos, 0, data_size);

    return 0;
}

static int32_t try_compact_memory(size_t required)
{
    int32_t i;
    uint8_t* new_pos;
    uint8_t* buffer_end;

    if (!g_emv_cnxt || !g_emv_cnxt->tlvbuffer || !g_hashtable)
    {
        return 0;
    }

    buffer_end = g_emv_cnxt->tlvbuffer + g_emv_cnxt->tlvbuffer_size;

    // 首先检查是否有足够空间（不需要整理）
    if ((uint8_t*)g_currpos + required <= buffer_end)
    {
        return 1;
    }

	return 0;

    // 计算哈希表头大小
    size_t header_size = HASHTABLE_LEN * sizeof(bufheader*);
    new_pos = g_emv_cnxt->tlvbuffer + header_size;

    // 第一遍：重建哈希表并移动活跃节点
    for (i = 0; i < HASHTABLE_LEN; i++)
    {
        bufheader* current = g_hashtable[i];
        bufheader** prev_next = &g_hashtable[i];  // 指向当前链表最后一个节点的next指针

        while (current != NULL)
        {
            bufheader* next = current->next;

            if (current->is_active)
            {
                size_t node_size = sizeof(bufheader) + ((current->valuelen + 3) & ~0x03);

                // 检查空间
                if (new_pos + node_size > buffer_end)
                {
                    return 0; // 内存不足
                }

                // 移动节点到新位置
                if ((uint8_t*)current != new_pos)
                {
                    memmove(new_pos, current, node_size);
                }

                // 更新链表
                *prev_next = (bufheader*)new_pos;
                prev_next = &((bufheader*)new_pos)->next;

                new_pos += node_size;
            }
            else
            {
                // 移除非活跃节点，不更新prev_next
            }

            current = next;
        }

        // 终止当前链表的最后一个节点
        *prev_next = NULL;
    }

    g_currpos = new_pos;

    // 最终检查
    return ((uint8_t*)g_currpos + required <= buffer_end);
}

int32_t is_valid_pointer(void *ptr)
{
    // 检查是否在TLV缓冲区范围内
    uintptr_t start = (uintptr_t)g_emv_cnxt->tlvbuffer;
    uintptr_t end = start + g_emv_cnxt->tlvbuffer_size;
    uintptr_t p = (uintptr_t)ptr;

    // 检查是否在全局变量范围内（可选）

    return (p >= start && p < end);
}

int32_t appdata_save(uint32_t tag, uint8_t *value, int32_t valuelen, int32_t override)
{
    int32_t ihash, ret;
    bufheader *ptmp, **pprev,*existing=NULL;
	uint8_t *dest=NULL, *value_ptr;
	size_t required;
	static int max_len=0;
	static int count=0;

//	eTRACE("hash-table-info: memory check: g_hashtable = [%p], g_emv_cnxt =  [%p], g_emv_cnxt->tlvbuffer = [%p], g_currpos = [%p]", g_hashtable, g_emv_cnxt, g_emv_cnxt->tlvbuffer, g_currpos);

	if (g_hashtable == NULL || g_emv_cnxt == NULL || g_emv_cnxt->tlvbuffer == NULL || g_currpos == NULL)
	{
		htTRACE("hash-table-error: save space is null");
		return BUF_BUFOVER;
	}

    if (value == NULL && valuelen != 0)
	{
        return BUF_ERR;
    }

	if(valuelen > max_len)
	{
		max_len = valuelen;
//		htTRACE("max_len = [%d]", max_len);
	}

    htTRACE("tag = [%02X]", tag);
	if(value && valuelen > 0)
	{
		{
			int32_t offset = 0;
			int32_t index = 1;
			int32_t bytes_to_print;
			char label[64]={0};

			while (offset < valuelen)
			{
				bytes_to_print = valuelen - offset;
				if (bytes_to_print > 480)
				{
					bytes_to_print = 480;
				}
				emv_memset(label, 0, sizeof(label));
				snprintf(label, sizeof(label), "paypass-info: value%d ", index);

				htTRACE_HEX(label, value + offset, bytes_to_print);

				offset += bytes_to_print;
				index++;
			}
		}
	}

    ihash = hash_func(tag);

    // 检查哈希索引是否有效
    if (ihash < 0 || ihash >= HASHTABLE_LEN)
	{
        htTRACE("hash-table-error: Invalid hash index: %d", ihash);
        return BUF_ERR;
    }

//	htTRACE("hash-table-info: g_hashtable[4]: %p", g_hashtable[4]);

    pprev = &g_hashtable[ihash];
	ptmp = *pprev;
//    eTRACE("pprev = [%p], ptmp = %p", pprev, ptmp);

    if (ptmp != NULL && !is_valid_pointer(ptmp))
	{
        htTRACE("hash-table-error: Hash chain head pointer invalid!");
        ptmp = NULL; // 避免使用无效指针
    }

	while (ptmp != NULL)
	{
        if (!is_valid_pointer(ptmp))
		{
            htTRACE("hash-table-error: Invalid pointer in hash chain!");
            break;
        }

		if (ptmp->tagname == tag)
		{
		    existing = ptmp;
		    break;
		}

        // 检查next指针有效性
        if (!is_valid_pointer(ptmp->next) && ptmp->next != NULL)
		{
            htTRACE("hash-table-error: Invalid next pointer!");
            break;
        }

		pprev = &ptmp->next;
		ptmp = ptmp->next;
	}

//    eTRACE("existing = [%p]", existing);

	// 处理已存在项
    if (existing != NULL)
	{
        if (override == UNIQUE)
		{
            return BUF_OBJDUP;
        }

		dest = (uint8_t*)existing + sizeof(bufheader);

//		eTRACE("dest = [%p]", dest);

        if (valuelen <= existing->valuelen)
		{
            existing->valuelen = valuelen;

            if (value && valuelen > 0)
			{
                emv_memcpy(dest, value, valuelen);
            }
			else if (existing->valuelen > 0)
			{
                emv_memset(dest, 0x00, existing->valuelen);
            }
            return 0;
        }

        existing->is_active = 0;
    }

	required = sizeof(bufheader) + ((valuelen + 3) & ~0x03);
//	eTRACE("required = [%d]", required);

	ret = try_compact_memory(required);
//	htTRACE("try_compact_memory ret = [%d]", ret);

	if (!ret)
	{
        return BUF_BUFOVER;
    }

    if (!is_valid_pointer(g_currpos))
	{
        htTRACE("hash-table-error: g_currpos invalid before allocation!");
        return BUF_BUFOVER;
    }

    uint8_t* new_pos = g_currpos;
    uint8_t* new_end = new_pos + required;
    uint8_t* buffer_end = g_emv_cnxt->tlvbuffer + g_emv_cnxt->tlvbuffer_size;

    if (new_end > buffer_end)
	{
        htTRACE("hash-table-error: Allocation would exceed buffer!");
        return BUF_BUFOVER;
    }

    if (!is_valid_pointer(new_pos) || !is_valid_pointer(new_end))
	{
        htTRACE("hash-table-error: New allocation range invalid!");
        return BUF_BUFOVER;
    }

	// 分配新空间
	bufheader *new_node = (bufheader*)g_currpos;

//	eTRACE("new_node = [%p]", new_node);
	new_node->tagname = tag;
	new_node->valuelen = valuelen;
	new_node->next = NULL;
	new_node->is_active = 1;

	// 添加到链表
	*pprev = new_node;
//	eTRACE("*pprev = [%p]", *pprev);

    if (valuelen > 0)
	{
        value_ptr = (uint8_t*)new_node + sizeof(bufheader);
//		eTRACE("value_ptr = [%p]", value_ptr);
        if (value)
		{
            emv_memcpy(value_ptr, value, valuelen);
        }
		else
		{
            emv_memset(value_ptr, 0x00, valuelen);
        }
    }

	g_currpos += required;

    if (!is_valid_pointer(g_currpos))
	{
        htTRACE("hash-table-warning: g_currpos became invalid after update!");
        // 这里可能需要回滚操作
    }
	return 0;
}

int32_t appdata_save_tlv(EMV_TLVOBJ *tlv, int32_t override, uint32_t template)
{
	uint8_t res;
	EMV_TAG_ITEM item={0};
	bool template_invalid = false;

    if (NULL == tlv)
    {
        return TAG_ERR;
    }

    eTRACE("save tag: %02X", tlv->tagname);


	res = emv_taglib_read_UpdateCondition(tlv->tagname, EMVTAGTYPE_PAYPASS, 1, EMVTAGUPDATECONDITION_RA, &item);
    eTRACE("emv_taglib_read_UpdateCondition ret = %d", res);

	if(res)
	{
		eTRACE("hashTable-error: save tag error due to tag is not in DataBase");
		eTRACE("template = %02X", template);
		return TAG_NOT_IN_DATABASE;
	}
	else
	{
		if(tlv->valuelen > item.maxLen || tlv->valuelen < item.minLen)
		{
			eTRACE("hashTable-error: save tag error due to tag data len invalid");
			return TAG_LEN_INVALID;
		}
	}

	template_invalid = IsTagIncludeTemplateValid(tlv->tagname, template, 0);
	if(!template_invalid)
	{
		if(TAG_0070_AEF_DATA_TEMPLATE == template)
		{
			if(tlv->tagname == TAG_9F4C_ICC_DYNAMIC_NUMBER)
			{
				eTRACE("hashTable-error: save tag error due to tag exist in invalid template");
				return TAG_IN_INVALID_TEMPLATE;
			}
		}
		else
		{
			eTRACE("hashTable-error: save tag error due to tag exist in invalid template");
			return TAG_IN_INVALID_TEMPLATE;
		}
	}

    return appdata_save(tlv->tagname, tlv->pvalue, tlv->valuelen, override);
}

int32_t appdata_save_or(uint32_t tag, int32_t index, uint8_t value)
{
	int32_t len;
	uint8_t *ptmp = appdata_find(tag, &len);

	if(ptmp == NULL || len < 0 || index >= len)
	{
		return EMV_FAIL;
	}

	ptmp[index] |= value;

	return appdata_save(tag, ptmp, len, OVERRIDED);
}

int32_t appdata_save_and(uint32_t tag, int32_t index, uint8_t value)
{
	int32_t len;
	uint8_t *ptmp = appdata_find(tag, &len);

	if(ptmp == NULL || len < 0 || index >= len)
	{
		return EMV_FAIL;
	}

	ptmp[index] &= value;

	return appdata_save(tag, ptmp, len, OVERRIDED);
}

int32_t appdata_save_singlebyte(uint32_t tag, uint8_t value, int32_t override)
{
	uint8_t temp_value = value;
	return appdata_save(tag, &temp_value, 1, override);
}


int32_t appdata_save_iccdata(uint32_t tag)
{
    int32_t len;
    uint8_t buf[EMV_CFG_ICC_RECV_MAXLEN];
    EMV_TLVOBJ tlvs[10];

    if (emvicc_get_data(tag, buf, &len) == 0x9000)
    {
        if (emvtlv_decode(buf, len, tlvs, 10, SINGLE_TLVOBJ) == 0)
        {
            if (tlvs[1].tagname == tag)
            {
                appdata_save_tlv(tlvs + 1, OVERRIDED, 0xFFFFFFFF);
                return 0;
            }
        }
    }

    return -1;
}

uint8_t* appdata_find(uint32_t tagname, int32_t *pvaluelen)
{
    bufheader *ptmp;

    htTRACE("tag = [%02X]", tagname);

    if (g_hashtable == NULL)
        ptmp = NULL;
    else
        ptmp = g_hashtable[hash_func(tagname)];

//	htTRACE("hash-table-info: g_hashtable[4]: %p", g_hashtable[4]);
//    htTRACE("hash index = %d,  ptmp = [%p]", hash_func(tagname), ptmp);

    while (ptmp != NULL)
    {
        if (ptmp->tagname == tagname)
        {
            if (pvaluelen != NULL)
            {
                *pvaluelen = ptmp->valuelen;
            }
			if(TAG_005A_APPLICATION_PAN == tagname || TAG_0057_TRACK2_EQUIVALENT_DATA == tagname || TAG_AUTH_DATA == tagname)
			{

			}
			else
			{
				if(((uint8_t*)ptmp + sizeof(bufheader)) != NULL)
				{
					htTRACE_HEX("value", (uint8_t*)ptmp + sizeof(bufheader), ptmp->valuelen);
				}
				else
				{
					htTRACE("value : NULL");
				}
			}
            return (uint8_t*)ptmp + sizeof(bufheader);
        }
        ptmp = ptmp->next;
    }

    if (pvaluelen != NULL)
    {
        *pvaluelen = 0;
    }
    return NULL;
}

/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
