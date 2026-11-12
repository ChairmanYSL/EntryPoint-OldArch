/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.
*
* @file     emvport.h
* @author   Kingsee Development Team xinyong
* @version  V43
* @date     2015��11��17��
* @brief    TODO
*
************************************************************************************************************************
*/
#ifndef EMVPORT_H_
#define EMVPORT_H_


/*
************************************************************************************************************************
*                                                     INCLUDE FILES
************************************************************************************************************************
*/
#include "emv_type.h"

/*
************************************************************************************************************************
*                                                        EXTERNS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                        DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                      ERROR CODES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                      DATA TYPES
************************************************************************************************************************
*/





/*
************************************************************************************************************************
*                                                   GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                         MACRO'S
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*                                                  FUNCTION PROTOTYPES
************************************************************************************************************************
*/
#if 0
int32_t emvporting_para_init(void);
int32_t emvporting_para_save(void);

uint8_t emvporting_para_get_kernel_index(void);
void emvporting_para_set_kernel_index(uint8_t val);

uint8_t emvporting_para_get_force_accept_sw(void);
void emvporting_para_set_force_accept_sw(uint8_t val);

uint8_t emvporting_para_get_force_online_sw(void);
void emvporting_para_set_force_online_sw(uint8_t val);

uint8_t emvporting_para_get_account_type_sel_sw(void);
void emvporting_para_set_account_type_sel_sw(uint8_t val);

uint8_t emvporting_para_get_pin_pass_sw(void);
void emvporting_para_set_pin_pass_sw(uint8_t val);

uint8_t emvporting_para_get_online_packet_type(void);
void emvporting_para_set_online_packet_type(uint8_t val);

uint8_t emvporting_para_get_trans_print_sw(void);
void emvporting_para_set_trans_print_sw(uint8_t val);

uint32_t emvporting_para_get_tsc(void);
void emvporting_para_set_tsc(uint32_t val);

uint8_t emvporting_para_get_status_check_sw(void);
void emvporting_para_set_status_check_sw(uint8_t val);

void emvporting_para_get_floor_limit(uint8_t* buf);
void emvporting_para_set_floor_limit(uint8_t* buf);

uint8_t emvporting_para_get_cl_trans_limit_exist(void);
void emvporting_para_set_cl_trans_limit_exist(uint8_t val);
void emvporting_para_get_cl_trans_limit(uint8_t* buf);
void emvporting_para_set_cl_trans_limit(uint8_t* buf);

uint8_t emvporting_para_get_cl_floor_limit_exist(void);
void emvporting_para_set_cl_floor_limit_exist(uint8_t val);
void emvporting_para_get_cl_floor_limit(uint8_t* buf);
void emvporting_para_set_cl_floor_limit(uint8_t* buf);

uint8_t emvporting_para_get_cvm_limit_exist(void);
void emvporting_para_set_cvm_limit_exist(uint8_t val);
void emvporting_para_get_cvm_limit(uint8_t* buf);
void emvporting_para_set_cvm_limit(uint8_t* buf);

void emvporting_para_get_host_ip(uint8_t* ip);
void emvporting_para_set_host_ip(uint8_t* ip);
int32_t emvporting_para_get_host_port(void);
void emvporting_para_set_host_port(int32_t val);
uint8_t emvporting_para_get_host_comtype(void);
void emvporting_para_set_host_comtype(uint8_t val);

int32_t emvporting_autotest_get_flag(void);
void emvporting_autotest_set_flag(int32_t val);

int32_t emvporting_autotest_get_appindex(void);
void emvporting_autotest_set_appindex(int32_t val);

void emvporting_autotest_get_passwd(uint8_t* passwd);
void emvporting_autotest_set_passwd(uint8_t* passwd);

int32_t emvporting_autotest_get_transtype(void);
void emvporting_autotest_set_transtype(int32_t val);

void emvporting_autotest_get_cash(s8 * cash);
void emvporting_autotest_set_cash(s8 * cash);

void emvporting_autotest_get_cashback(s8 * cash);
void emvporting_autotest_set_cashback(s8 * cash);

int32_t emvporting_autotest_get_next_waittime(void);
void emvporting_autotest_set_next_waittime(int32_t val);

int32_t emvporting_autotest_get_maxcount(void);
void emvporting_autotest_set_maxcount(int32_t val);

int32_t emvporting_autotest_get_autoincard(void);
void emvporting_autotest_set_autoincard(int32_t val);
#endif

void emvporting_get_kernel_index(uint8_t *val);
void emvporting_set_kernel_index(uint8_t val);

uint32_t emvporting_get_tsc(void);
void emvporting_set_tsc(uint32_t val);

int32_t emvporting_set_termparam(uint8_t* par, int32_t len);
int32_t emvporting_get_termparam(EMV_TERMPARAM* tpar);

int32_t emvporting_clr_aidparam(void);
int32_t emvporting_add_aidparam(uint8_t* par, int32_t len);
int32_t emvporting_get_aidparam(EMV_AIDPARAM* apar, int32_t inx);
int32_t emvporting_get_termapps(EMV_TERMAPP* apps);
int32_t emvporting_clr_oneaidparam(uint8_t* par, int32_t len);

int32_t emvporting_clr_capkparam(void);
int32_t emvporting_add_capkparam(uint8_t* par, int32_t len);
int32_t emvporting_get_capkparam(EMV_CAPKPARAM* capk, uint8_t pki, const uint8_t* rid);
int32_t emvporting_clr_onecapkparam(uint8_t* par, int32_t len);

int32_t emvporting_clr_cardblack(void);
int32_t emvporting_add_cardblack(EMV_CARDBLACK* blk);
int32_t emvporting_fnd_cardblack(const uint8_t* pan, int32_t panlen, uint8_t seq);

int32_t emvporting_clr_certblack(void);
int32_t emvporting_add_certblack(EMV_CERTBLACK* blk);
int32_t emvporting_fnd_certblack(const uint8_t* rid, uint8_t pki, const uint8_t* csn);

int32_t emvporting_clr_translog(void);
int32_t emvporting_sav_translog(const uint8_t* pan, uint8_t panlen, uint32_t amount);
uint32_t emvporting_get_translog_amount(const uint8_t* pan, uint8_t panlen, uint8_t policy);

int32_t emvporting_clr_transwater(void);
int32_t emvporting_save_transwater(uint8_t* buf, int32_t len);
void emvporting_get_transwatername(char *name);
int32_t emvporting_get_transwater(int32_t inx, uint8_t* buf, int32_t *len);

int32_t emvparanew_get_termparam(EMV_TERMPARAM* par);
int32_t emvparanew_get_aidparam(EMV_AIDPARAM* apar, int32_t inx);
int32_t emvparanew_get_termapps(EMV_TERMAPP* apps);

int32_t emvporting_write_file(char *filename, uint32_t filesize, uint8_t *buf, uint32_t buflen, uint32_t flag);


/*
************************************************************************************************************************
*                                                 CONFIGURATION ERRORS
************************************************************************************************************************
*/


int32_t emvporting_clr_termparam(void);
int32_t emvporting_get_termparam_tlv(uint8_t* par, int32_t *rlen);
int32_t emvporting_get_aidparam_tlv(int32_t inx, uint8_t* par, int32_t *rlen);
int32_t emvporting_get_capkparam_tlv(int32_t inx, uint8_t* par, int32_t *rlen);
int32_t emvporting_clr_drl(void);
int32_t emvporting_add_drl(void * par, int32_t len);
#endif /* EMVPORT_H_ */
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2015 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
