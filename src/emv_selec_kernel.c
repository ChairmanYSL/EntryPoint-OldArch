#include "sdk.h"
#include "emv.h"
#include "contact_api.h"
#include "emvi.h"
#include "paypass_api.h"

emv_kernel_interface emv_k_interface;

int32_t emv_select_kernel(uint8_t card_type, uint8_t emv_core)
{
	int32_t ret = EMV_OK;


	API_Trace(DBG_APP_INFO, "entrypoint-info: emv_core = %d", emv_core);
#if EMV_ANDROID_SO
	switch(emv_core)
	{

	}

#else
	if(card_type == EMV_CARD_CONTACT)
	{
//		ret = contact_init();
//		API_Trace(DBG_APP_INFO, "entrypoint-info: contact_init ret = %d", ret);
//		if(ret != EMV_OK)
//		{
//			return ret;
//		}
//
//		API_Trace(DBG_APP_INFO, "entrypoint-info: contact_process pointer = %p", contact_process);
//		emv_k_interface.emv_process = contact_process;
		return EMV_OK;
	}
	else
	{
		switch(emv_core)
		{
			case EMV_CONTACTLESSTYPE_QPBOC:
				break;
			case EMV_CONTACTLESSTYPE_QVSDC:
				break;
			case EMV_CONTACTLESSTYPE_QMSDC:		//MasterCard
				//appdata_find(TAG_0095_TVR, NULL);
				ret = paypass_init();
				API_Trace(DBG_APP_INFO, "entrypoint-info: paypass_init ret = %d", ret);
				if(ret != EMV_OK)
				{
					return ret;
				}

				ret = paypass_preprocess(g_emv_cnxt->force_online);
				API_Trace(DBG_APP_INFO, "entrypoint-info: paypass_preprocess ret = %d", ret);
				if(ret != EMV_OK)
				{
					return ret;
				}

				API_Trace(DBG_APP_INFO, "entrypoint-info: paypass_process pointer = %p", paypass_process);
				emv_k_interface.emv_process = paypass_process;

				API_Trace(DBG_APP_INFO, "entrypoint-info: emv_k_interface.emv_process pointer = %p", emv_k_interface.emv_process);
				break;
			case EMV_CONTACTLESSTYPE_JCB:
				break;
			case EMV_CONTACTLESSTYPE_AMEX:
				break;
			case EMV_CONTACTLESSTYPE_DISV:
				break;
			case EMV_CONTACTLESSTYPE_QPURE:
				break;
			default:
				ret = EMV_ERR;
				break;
		}
	}

#endif
	API_Trace(DBG_APP_INFO, "entrypoint-info: ret = %d", ret);
	return ret;
}

