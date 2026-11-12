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
    /*
     * Value bytes reserved for this node (rounded up to 4). It never shrinks:
     * an in-place update with a shorter value only lowers valuelen, while the
     * allocated record size must stay stable so that the data area keeps being
     * a sequence of contiguous records (required by compaction).
     */
    int32_t alloclen;
} bufheader;


static bufheader**	g_hashtable = NULL;
static uint8_t*			g_currpos = NULL;

/*
 * The data area starts right after the hash table header. Every allocation is
 * a node record: bufheader followed by `alloclen` value bytes, so the whole
 * [data_start, g_currpos) range is a sequence of consecutive node records
 * (bump allocator; dead nodes are simply flagged but not yet removed).
 */
static uint8_t* appdata_data_start(void)
{
    size_t header_size = HASHTABLE_LEN * sizeof(bufheader*);
    return (uint8_t*)g_emv_cnxt->tlvbuffer + header_size;
}

static size_t appdata_node_size(const bufheader *node)
{
    return sizeof(bufheader) + (size_t)node->alloclen;
}

/* a node record is usable only if its stored metadata is self consistent */
static int32_t appdata_node_valid(const bufheader *node, const uint8_t *avail_end, const uint8_t *node_addr)
{
    if (node->valuelen < 0 || node->alloclen < 0 || node->alloclen < node->valuelen)
    {
        return 0;
    }
    if ((size_t)(avail_end - node_addr) < appdata_node_size(node))
    {
        return 0;
    }
    return 1;
}

/*
 * Rebuild the hash table from the packed data area after compaction.
 * Bucket order is not semantically significant, so nodes are re-linked with
 * head insertion.
 */
static int32_t appdata_rebuild_hashtable(void)
{
    bufheader *node;
    uint8_t *p;
    uint8_t *data_start = appdata_data_start();
    int32_t i;

    for (i = 0; i < HASHTABLE_LEN; i++)
    {
        g_hashtable[i] = NULL;
    }

    p = data_start;
    while (p < g_currpos)
    {
        node = (bufheader*)p;

        if (!appdata_node_valid(node, g_currpos, p))
        {
            return -1;
        }

        node->is_active = 1;
        node->next = g_hashtable[hash_func(node->tagname)];
        g_hashtable[hash_func(node->tagname)] = node;

        p += appdata_node_size(node);
    }

    return 0;
}

/*
 * Make room for `required` more bytes in the data area.
 *
 * P1 fix: the old implementation returned from the "enough room" branch and
 * then fell through to `return 0`, so the compaction body below it was dead
 * code and the bump allocator never reclaimed anything: once the ~10KB data
 * area filled up, every appdata_save() failed for the rest of the session.
 *
 * The compactor packs all still-active nodes towards the start of the data
 * area, preserving address order (so a memmove never overwrites a node that has
 * not been processed yet and g_currpos only moves backwards), then rebuilds the
 * hash table from the packed records.
 *
 * WARNING for callers: compaction MOVES nodes, therefore every pointer
 * previously returned by appdata_find()/GetTLV() into the data area becomes
 * invalid. Do not hold such a pointer across an appdata_save() call.
 */
static int32_t try_compact_memory(size_t required)
{
    uint8_t *buffer_end;
    uint8_t *data_start;
    uint8_t *read_pos;
    uint8_t *write_pos;
    bufheader *node;
    size_t node_size;

    if (!g_emv_cnxt || !g_emv_cnxt->tlvbuffer || !g_hashtable || !g_currpos)
    {
        return 0;
    }

    buffer_end = g_emv_cnxt->tlvbuffer + g_emv_cnxt->tlvbuffer_size;

    // 首先检查是否有足够空间（不需要整理）
    if ((size_t)(buffer_end - g_currpos) >= required)
    {
        return 1;
    }

    /* in-place compaction: drop inactive nodes, keep active ones in address order */
    data_start = appdata_data_start();
    read_pos = data_start;
    write_pos = data_start;

    while (read_pos < g_currpos)
    {
        node = (bufheader*)read_pos;

        if (!appdata_node_valid(node, g_currpos, read_pos))
        {
            return 0;       /* corrupted data area: refuse to compact it */
        }
        node_size = appdata_node_size(node);

        if (node->is_active)
        {
            if (write_pos != read_pos)
            {
                memmove(write_pos, read_pos, node_size);
            }
            write_pos += node_size;
        }

        read_pos += node_size;
    }

    g_currpos = write_pos;

    if (appdata_rebuild_hashtable() != 0)
    {
        return 0;
    }

    return ((size_t)(buffer_end - g_currpos) >= required) ? 1 : 0;
}

/*
 * Reset the application data area for the next transaction.
 *
 * P1 fix: the old code had no reset path at all. EMV_ProcessExit() only
 * memset()ed the TLV buffer without resetting this module's static allocator
 * cursor, so each transaction permanently leaked its data area and eventually
 * every appdata_save() returned BUF_BUFOVER.
 *
 * The hash table is cleared, the used part of the data area is zeroed (so no
 * card data survives between transactions) and g_currpos is moved back to the
 * first free byte. No memory is allocated, freed or reallocated.
 */
int32_t appdata_reset(void)
{
    uint8_t *data_start;
    uint8_t *buffer_end;

    if (!g_emv_cnxt || !g_emv_cnxt->tlvbuffer || !g_hashtable)
    {
        return -1;
    }

    data_start = appdata_data_start();
    buffer_end = g_emv_cnxt->tlvbuffer + g_emv_cnxt->tlvbuffer_size;

    emv_memset(g_hashtable, 0, HASHTABLE_LEN * sizeof(bufheader*));

    if (g_currpos != NULL && g_currpos > data_start && g_currpos <= buffer_end)
    {
        emv_memset(data_start, 0, (uint32_t)(g_currpos - data_start));
    }

    g_currpos = data_start;

    return 0;
}

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

int32_t is_valid_pointer(void *ptr)
{
    // 检查是否在TLV缓冲区范围内
    uintptr_t start;
    uintptr_t end;
    uintptr_t p;

    if (!g_emv_cnxt || !g_emv_cnxt->tlvbuffer)
    {
        return 0;
    }

    start = (uintptr_t)g_emv_cnxt->tlvbuffer;
    end = start + g_emv_cnxt->tlvbuffer_size;
    p = (uintptr_t)ptr;

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

    /* P2 fix: a negative (or larger than the whole data area) length used to
     * flow into the size arithmetic / emv_memcpy as a huge size_t. */
    if (valuelen < 0 || valuelen > (g_emv_cnxt->tlvbuffer_size - (int32_t)sizeof(bufheader)))
    {
        htTRACE("hash-table-error: invalid valuelen [%d]", valuelen);
        return BUF_BUFOVER;
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
            int32_t oldlen = existing->valuelen;

            /* alloclen is intentionally left untouched: the record keeps its
             * reserved size so the data area stays a contiguous record array. */
            existing->valuelen = valuelen;

            if (value && valuelen > 0)
			{
                emv_memcpy(dest, value, valuelen);
            }
			else if (oldlen > 0)
			{
                emv_memset(dest, 0x00, oldlen);
            }

            /* clear the now unreachable tail of the old, longer value */
            if (oldlen > valuelen && valuelen >= 0)
            {
                emv_memset(dest + valuelen, 0x00, oldlen - valuelen);
            }
            return 0;
        }

        /* P1 note: the dead node is only flagged AFTER the replacement node has
         * been allocated successfully (see below); otherwise a failed save
         * would silently remove the still valid old value from the lookup. */
    }

	required = sizeof(bufheader) + (((size_t)valuelen + 3U) & ~(size_t)0x03U);
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

    if (!is_valid_pointer(new_pos))
	{
        htTRACE("hash-table-error: New allocation range invalid!");
        return BUF_BUFOVER;
    }

	// 分配新空间
	bufheader *new_node = (bufheader*)g_currpos;

//	eTRACE("new_node = [%p]", new_node);
	new_node->tagname = tag;
	new_node->valuelen = valuelen;
	new_node->alloclen = (int32_t)(((size_t)valuelen + 3U) & ~(size_t)0x03U);
	new_node->is_active = 1;

    /*
     * P1 fix: the node that replaces a shorter existing value must inherit the
     * old node's successor. The old code set next = NULL and then relinked
     * *pprev, which silently dropped every other tag stored later in the same
     * hash bucket (those tags became unreachable for appdata_find()).
     */
	new_node->next = (existing != NULL) ? existing->next : NULL;

	// 添加到链表
	*pprev = new_node;
//	eTRACE("*pprev = [%p]", *pprev);

    if (existing != NULL)
    {
        existing->is_active = 0;    /* dead node: reclaimed by try_compact_memory() */
    }

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

/*
 * Save a value for a fixed kernel-internal tag without the EMV tag library
 * checks.
 *
 * P1 fix for the 71/72 issuer script path: EMV_ResultOnline() renames the
 * online response templates to 0xDF8100/0xDF8200, tags that are intentionally
 * absent from g_emv_tag_lib. appdata_save_tlv() therefore rejected them with
 * TAG_NOT_IN_DATABASE (and the caller ignores the return value), so issuer
 * scripts were silently lost before they could be retrieved.
 *
 * This entry point performs exactly the same length/capacity validation as
 * appdata_save() but skips the update-condition (RA/K/...) and template
 * checks, so the kernel can store its own fixed tags.
 *
 * Buffer ownership: the value is copied into the kernel TLV data area and the
 * caller keeps ownership of `value`; `value` may be NULL only when valuelen==0.
 */
int32_t appdata_save_raw(uint32_t tag, const uint8_t *value, int32_t valuelen, int32_t override)
{
    return appdata_save(tag, (uint8_t*)value, valuelen, override);
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

	if(ptmp == NULL || len < 0 || index < 0 || index >= len)
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

	if(ptmp == NULL || len < 0 || index < 0 || index >= len)
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
