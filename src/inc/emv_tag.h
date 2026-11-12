/**
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.
*
* @file     emv_ics.h
* @brief   	EMV��ر�ǩ
* @author   Kingsee Development Team - xinyong
* @version  V43
*
************************************************************************************************************************
*/
#ifndef EMV_TAG_H_
#define EMV_TAG_H_

#include "emv_type.h"
#include "emv_utils.h"

typedef struct{
	uint32_t Tag;
	uint8_t Update;
	uint16_t minLen;
	uint16_t maxLen;
	uint8_t Format;
	uint32_t belong;
}EMV_TAG_ITEM;

typedef struct{
	uint32_t Tag;
	uint8_t Template;
}EMV_TEMPLATE_LIST;


#define LIST_FORMAT_T		0x01
#define LIST_FORMAT_TLV		0x02

#define EMVTAGFORMAT_N      0x01	//number
#define EMVTAGFORMAT_B      0x02	//bcd code
#define EMVTAGFORMAT_ANS    0x04	//ascii code
#define EMVTAGFORMAT_CN     0x08
#define EMVTAGFORMAT_AN     0x10

#define EMVTAGUPDATECONDITION_ACT       0x10
#define EMVTAGUPDATECONDITION_RA        0x20		//RA means R-APDU,this condition means the tag returned by card
#define EMVTAGUPDATECONDITION_K         0x40
#define EMVTAGUPDATECONDITION_DET		0x80

#define EMVTAGTYPE_ALL          0xFFFFFFFF
#define EMVTAGTYPE_PAYPASS      0x00000001
#define EMVTAGTYPE_PAYWAVE      0x00000002
#define EMVTAGTYPE_QPBOC        0x00000004
#define EMVTAGTYPE_EMVCONTACT	0x00000008
#define EMVTAGTYPE_AMEX			0x00000010
#define EMVTAGTYPE_JCB   		0x00000020
#define EMVTAGTYPE_DPAS		   	0x00000040
#define EMVTAGTYPE_PURE		   	0x00000080
#define EMVTAGTYPE_RUPAY		0x00000100
#define EMVTAGTYPE_NULL         0x80000000

#define EMVTAG_MATCH_FUZZY		0x00
#define EMVTAG_MATCH_EXACTLY	0x01

/** [42] - Issuer Identification Number (IIN) | ������ʶ���루IIN��, a6, 3 */
#define TAG_0042_IIN   0X0042

/** Ӧ�ñ�ʶ��-��Ƭ|Application Dedicated File (ADF) Name[4F][b][5-16] */
#define TAG_004F_AID   0X004F

/** Ӧ�ñ�ǩ|Application Label[50][ans1-16][1-16] */
#define TAG_0050_APPLICATION_LABEL   0X0050

/** Ӧ�ñ�ǩ|Application Label[50][ans1-16][1-16] */
#define TAG_0056_TRACK1DATA   0X0056

/** [57] - Track2 Equivalent Data | ���ŵ���Ч����, b, var. up to 19 */
#define TAG_0057_TRACK2_EQUIVALENT_DATA   0X0057

/** [5A] - Application PAN | Ӧ�����˺ţ�PAN��, cn var. up to 19, var. up to 10 */
#define TAG_005A_APPLICATION_PAN   0X005A

/** [61] - Application Template */
#define TAG_0061_APPLICATION_TEMPLATE   0X0061

/** [6F] - FCI (FILE CONTROL INFO) Template */
#define TAG_006F_FCI_TEMPLATE   0X006F

/** [70] - AEF (Application Elementary File) Data Template */
#define TAG_0070_AEF_DATA_TEMPLATE   0X0070

/** [71] - Issuer Script Template 1 */
#define TAG_0071_ISSUER_SCRIPT_TEMPLATE_1   0X0071

/** [72] - Issuer Script Template 2 */
#define TAG_0072_ISSUER_SCRIPT_TEMPLATE_2   0X0072

/** [73] - Directory Discretionary Template */
#define TAG_0073_DIRECTORY_DISCRETIONARY_TEMPLATE   0X0073

/** [77] - Response Message Template Format 2 */
#define TAG_0077_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2   0X0077

/** [80] - Response Message Template Format 1 */
#define TAG_0080_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1   0X0080

/** [81] - Amount Authorised Binary */
#define TAG_0081_AMOUNT_AUTHORISED_BINARY   0X0081

/** [82] - AIP (Application Interchange Profile) */
#define TAG_0082_AIP   0X0082

/** [83] - Command Template */
#define TAG_0083_COMMAND_TEMPLATE   0X0083

/** [84] - DF (Dedicated File) Name */
#define TAG_0084_DF_NAME   0X0084

/** [86] - Issuer Script Command */
#define TAG_0086_ISSUER_SCRIPT_COMMAND   0X0086

/** Ӧ������ָʾ��|Application Priority Indicator (API)[87][b8][1] */
#define TAG_0087_API   0X0087

/** [88] - SFI (Short File Indicator) */
#define TAG_0088_SFI   0X0088

/** [89] - Authorisation Code */
#define TAG_0089_AUTH_CODE   0X0089

/** [8A] - Authorisation Response Code (ARC) | ��Ȩ��Ӧ��, an2, 2 */
#define TAG_008A_ARC   0X008A

#define TAG_008B_POI_INFOMATION	0x008B

/** [8C] - CDOL 1 (Card Risk Management Data Object List) */
#define TAG_008C_CDOL_1   0X008C

/** [8D] - CDOL 2 (Card Risk Management Data Object List) */
#define TAG_008D_CDOL_2   0X008D

/** [8E] - CVM (Cardholder Verification Method) List */
#define TAG_008E_CVM_LIST   0X008E

/** [8F] - Certification Authority Public Key Index */
#define TAG_008F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX   0X008F

/** [90] - Issuer Public Key Certificate */
#define TAG_0090_ISSUER_PUBLIC_KEY_CERTIFICATE   0X0090

/** [91] - Issuer Authentication Data */
#define TAG_0091_ISSUER_AUTHENTICATION_DATA   0X0091

/** [92] - Issuer Public Key Remainder */
#define TAG_0092_ISSUER_PUBLIC_KEY_REMAINDER   0X0092

/** [93] - Signed Static Application Data */
#define TAG_0093_SIGNED_STATIC_APPLICATION_DATA   0X0093

/** [94] - Application File Locator (AFL) | Ӧ���ļ���λ��, var., var. up to 252 */
#define TAG_0094_AFL   0X0094

/** [95] - TVR (Terminal Verification Results) */
#define TAG_0095_TVR   0X0095

/** [97] - TDOL (Transaction Certificate Data Object List) */
#define TAG_0097_TDOL   0X0097

/** [98] - TC (Transaction Certificate) Hash Value */
#define TAG_0098_TC_HASH_VALUE   0X0098

/** [99] - Transaction PIN (Personal Identification Number) Data */
#define TAG_0099_TRANSACTION_PIN_DATA   0X0099

/** [9A] - Transaction Date */
#define TAG_009A_TRANSACTION_DATE   0X009A

/** [9B] - Transaction Status Info */
#define TAG_009B_TSI   0X009B

/** [9C] - Transaction Type */
#define TAG_009C_TRANSACTION_TYPE   0X009C

/** [9D] - DDF (Directory Definition File) Name */
#define TAG_009D_DDF_NAME   0X009D

/** [A5] - FCI (File Control Info) Proprietary Template */
#define TAG_00A5_FCI_PROPRIETARY_TEMPLATE   0X00A5

/** [5F20] - Cardholder Name */
#define TAG_5F20_CARDHOLDER_NAME   0X5F20

/** [5F24] - Application Expiration Date */
#define TAG_5F24_APPLICATION_EXPIRATION_DATE   0X5F24

/** [5F25] - Application Effective Date */
#define TAG_5F25_APPLICATION_EFFECTIVE_DATE   0X5F25

/** [5F28] - Issuer Country Code */
#define TAG_5F28_ISSUER_COUNTRY_CODE   0X5F28

/** [5F2A] - Transaction Currency Code | ���׻��Ҵ���, n2, 1 */
#define TAG_5F2A_TRANSACTION_CURRENCY_CODE   0X5F2A

/** [5F2D] - Language Preference */
#define TAG_5F2D_LANGUAGE_PREFERENCE   0X5F2D

/** [5F30] - Service Code */
#define TAG_5F30_SERVICE_CODE   0X5F30

/** [5F34] - Application PAN Sequence | Ӧ�����˺����к�, n2, 1 */
#define TAG_5F34_APPLICATION_PAN_SEQUENCE   0X5F34

/** [5F36] - Transaction Currency Exponent */
#define TAG_5F36_TRANSACTION_CURRENCY_EXPONENT   0X5F36

#define TAG_5F50_ISSUER_URL   0X5F50

/** [5F53] - DEK Indentifirer*/
#define TAG_5F53_IBAN   0X5F53

#define TAG_5F54_BIC   0X5F54

#define TAG_5F55_ISSUER_COUNTRY_CODE_ALPHA2   0X5F55

#define TAG_5F56_ISSUER_COUNTRY_CODE_ALPHA3   0X5F56

/** [5F57] - Account Type | �ʻ�����\n n2, 1 */
#define TAG_5F57_ACCOUNT_TYPE   0X5F57

/** [9F01] - Acquirer Identifier */
#define TAG_9F01_ACQUIRER_IDENTIFIER   0X9F01

/** [9F02] - Amount Authorised Numeric */
#define TAG_9F02_AMOUNT_AUTHORISED_NUMERIC   0X9F02

/** [9F03] - Amount Other Numeric */
#define TAG_9F03_AMOUNT_OTHER_NUMERIC   0X9F03

/** [9F04] - Amount Other Binary */
#define TAG_9F04_AMOUNT_OTHER_BINARY   0X9F04

/** [9F05] - Application Discretionary Data */
#define TAG_9F05_APPLICATION_DISCRETIONARY_DATA   0X9F05

/** [9F06] - AID (Terminal) */
#define TAG_9F06_AID_TERMINAL   0X9F06

/** [9F07] - AUC (Application Usage Control) */
#define TAG_9F07_AUC   0X9F07

/** [9F08] - Application Version Number Card */
#define TAG_9F08_APPLICATION_VERSION_NUMBER_CARD   0X9F08

/** [9F09] - Application Version Number Terminal */
#define TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL   0X9F09

/** [9F0A] - Application Version Number Terminal */
#define TAG_9F0A_ASRPD   0X9F0A

/** [9F0B] - Cardholder Name Extended */
#define TAG_9F0B_CARDHOLDER_NAME_EXTENDED   0X9F0B

/** [9F0D] - Issuer Action Code Default */
#define TAG_9F0D_ISSUER_ACTION_CODE_DEFAULT   0X9F0D

/** [9F0E] - Issuer Action Code Denial */
#define TAG_9F0E_ISSUER_ACTION_CODE_DENIAL   0X9F0E

/** [9F0F] - Issuer Action Code Online */
#define TAG_9F0F_ISSUER_ACTION_CODE_ONLINE   0X9F0F

/** [9F10] - Issuer Application Data (IAD) | ������Ӧ������, b, var. up to 32 */
#define TAG_9F10_IAD   0X9F10

/** [9F11] - Issuer Code Table Index */
#define TAG_9F11_ISSUER_CODE_TABLE_INDEX   0X9F11

/** Ӧ����ѡ����|Application Preferred Name[9F12][ans1-16][1-16] */
#define TAG_9F12_APPLICATION_PREFERRED_NAME   0X9F12

/** [9F13] - Last Online ATC (Application Transaction Counter) Register */
#define TAG_9F13_LAST_ONLINE_ATC_REGISTER   0X9F13

/** [9F14] - Lower Consecutive Offline Limit */
#define TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT   0X9F14

/** [9F15] - Merchant Category Code */
#define TAG_9F15_MERCHANT_CATEGORY_CODE   0X9F15

/** [9F16] - Merchant Identifier */
#define TAG_9F16_MERCHANT_IDENTIFIER   0X9F16

/** [9F17] - PIN (Personal Identification Number) Try Counter */
#define TAG_9F17_PIN_TRY_COUNTER   0X9F17

/** [9F18] - Issuer Script Identifier */
#define TAG_9F18_ISSUER_SCRIPT_IDENTIFIER   0X9F18

#define TAG_9F19_TOKEN_REQUESTOR_ID   0x9F19

/** [9F1A] - Terminal Country Code */
#define TAG_9F1A_TERMINAL_COUNTRY_CODE   0X9F1A

/** [9F1B] - �ն�����޶�, b, 4 */
#define TAG_9F1B_TERMINAL_FLOOR_LIMIT   0X9F1B

/** [9F1C] - Terminal Identification */
#define TAG_9F1C_TERMINAL_IDENTIFICATION   0X9F1C

/** [9F1D] - Terminal Risk Mangement Data */
#define TAG_9F1D_TERMINAL_RISK_MANGEMENT_DATA   0X9F1D

/** [9F1E] - IFD (Interface Device) Serial Number */
#define TAG_9F1E_IFD_SERIAL_NUMBER   0X9F1E

/** [9F1F] - Track1 Discretionary Data */
#define TAG_9F1F_TRACK1_DISCRETIONARY_DATA   0X9F1F

/** [9F20] - Track2 Discretionary Data */
#define TAG_9F20_TRACK2_DISCRETIONARY_DATA   0X9F20

/** [9F21] - Transaction Time */
#define TAG_9F21_TRANSACTION_TIME   0X9F21

/** [9F22] - ��֤���Ĺ�Կ���� */
#define TAG_9F22_CAPK_INDEX   0X9F22

/** [9F23] - Upper Consecutive Offline Limit */
#define TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT   0X9F23

#define TAG_9F24_PAYMENT_ACCOUNT_REFERENCE	0x9F24

/** [9F26] - Application Cryptogram */
#define TAG_9F26_APPLICATION_CRYPTOGRAM   0X9F26

/** [9F27] - Cryptogram Info Data */
#define TAG_9F27_CRYPTOGRAM_INFO_DATA   0X9F27

/** [9F29] - Extened selection */
#define TAG_9F29_EXTENED_SELECTION   0X9F29

/** [9F2A] - Kernel Identifier */
#define TAG_9F2A_KERNEL_ID   0X9F2A

/** [9F2D] - Icc PIN Encipherment Public Key Certificate */
#define TAG_9F2D_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_CERTIFICATE   0X9F2D

/** [9F2E] - Icc PIN Encipherment Public Key Exponent */
#define TAG_9F2E_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_EXPONENT   0X9F2E

/** [9F2F] - Icc PIN Encipherment Public Key Remainder */
#define TAG_9F2F_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_REMAINDER   0X9F2F

/** [9F32] - Issuer Public Key Exponent */
#define TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT   0X9F32

/** [9F33] - Terminal Capabilities */
#define TAG_9F33_TERMINAL_CAPABILITIES   0X9F33

/** [9F34] - CVM (Cardholder Verification Method) Results */
#define TAG_9F34_CVM_RESULTS   0X9F34

/** [9F35] - Terminal Type */
#define TAG_9F35_TERMINAL_TYPE   0X9F35

/** [9F36] - ATC (Application Transaction Counter) */
#define TAG_9F36_ATC   0X9F36

/** [9F37] - Unpredicatable Number */
#define TAG_9F37_UNPREDICATABLE_NUMBER   0X9F37

/** [9F38] - PDOL (Processing Options Data Object List) */
#define TAG_9F38_PDOL   0X9F38

/** [9F39] - PoS (Point of Service) Entry Mode */
#define TAG_9F39_POS_ENTRY_MODE   0X9F39

/** [9F3A] - Amount Reference Currency */
#define TAG_9F3A_AMOUNT_REFERENCE_CURRENCY   0X9F3A

/** [9F3B] - Application Reference Currency */
#define TAG_9F3B_APPLICATION_REFERENCE_CURRENCY   0X9F3B

/** [9F3C] - Transaction Reference Currency */
#define TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY   0X9F3C

/** [9F3D] - Transaction Reference Currency Exponent */
#define TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT   0X9F3D

/** [9F3E] - Terminal Categories Supported List*/
#define TAG_9F3E_TERMINAL_CATEGORIES_SUPPORTED_LIST   0X9F3E

/** [9F3F] - Selection Data Object List*/
#define TAG_9F3F_SDOL   0X9F3F

/** [9F40] - Additional Terminal Capabilities */
#define TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES   0X9F40

/** [9F41] - Transaction Sequence Counter */
#define TAG_9F41_TRANSACTION_SEQUENCE_COUNTER   0X9F41

/** [9F42] - Application Currency Code */
#define TAG_9F42_APPLICATION_CURRENCY_CODE   0X9F42

/** [9F43] - Application Reference Currency Exponent */
#define TAG_9F43_APPLICATION_REFERENCE_CURRENCY_EXPONENT   0X9F43

/** [9F44] - Application Currency Exponent */
#define TAG_9F44_APPLICATION_CURRENCY_EXPONENT   0X9F44

/** [9F45] - Data Authentication Code */
#define TAG_9F45_DATA_AUTHENTICATION_CODE   0X9F45

/** [9F46] - ICC Public Key Certificate */
#define TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE   0X9F46

/** [9F47] - ICC Public Key Exponent */
#define TAG_9F47_ICC_PUBLIC_KEY_EXPONENT   0X9F47

/** [9F48] - ICC Public Key Remainder */
#define TAG_9F48_ICC_PUBLIC_KEY_REMAINDER   0X9F48

/** [9F49] - DDOL (Dynamic Data Object List) */
#define TAG_9F49_DDOL   0X9F49

/** [9F4A] - SDA (Static Data Authentication) Tag List */
#define TAG_9F4A_SDA_TAG_LIST   0X9F4A

/** [9F4B] - Signed Dynamic Application Data */
#define TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA   0X9F4B

/** [9F4C] - ICC Dynamic Number */
#define TAG_9F4C_ICC_DYNAMIC_NUMBER   0X9F4C

/** [9F4D] - ������־��� */
#define TAG_9F4D_LOG_SFI   0X9F4D

/** [9F4E] - Merchant Name and Location */
#define TAG_9F4E_MERCHANT_NAME   0X9F4E

/** [9F4F] - ������־��ʽ */
#define TAG_9F4F_LOG_FORMAT 0X9F4F

#define TAG_9F50_OFFLINE_ACCUMULATOR_BALANCE   0X9F50

#define TAG_9F51_DRDOL   0X9F51

/** [9F53] - Transaction Category Code */
#define TAG_9F53_TRANS_CATEGORY_CODE   0X9F53

/** [9F54] - DS ODS Card */
#define TAG_9F54_DS_ODS_CARD   0X9F54

/** [9F5B] - DS ODS Card */
#define TAG_9F5B_DSDOL   0X9F5B

/** [9F5C] - DS Requested Operator ID | 8, b, K/ACT/DET */
#define TAG_9F5C_DS_REQ_OPERATOR_ID   0X9F5C

/** [9F5D] - Available Offline Spending Amount (AOSA) | �����ѻ����ѽ��, n12, 6 */
#define TAG_9F5D_AVAILABLE_OFFLINE_SPENDING_AMOUNT   0X9F5D

//[9F5D] Application Capabilities Information | K,b,3
#define TAG_9F5D_APP_CAPABILITY_INFO	0x9F5D

/** [9F5E] - DS ID | var 8-11,  n 16-22,  K/RA */
#define TAG_9F5E_DS_ID   0X9F5E

/** [9F5F] - DS Slot Availability | var 8-11,  n 16-22,  K/RA */
#define TAG_9F5F_DS_SLOT_AVAILABILITY   0X9F5F

/** [9F60] -(PAYPASS)CVC3 (Track1)*/
#define TAG_9F60_CVC3_TRACK1   0X9F60

/** [9F61] - �ֿ���֤����, an40, 1-40 */
#define TAG_9F61_CARDHOLDER_DOCUMENTS_NUMBER   0X9F61

/** [9F61] -(PAYPASS)CVC3 (Track2)*/
#define TAG_9F61_CVC3_TRACK2   0X9F61

/** [9F62] - �ֿ���֤������, ����ɲο�{@link PBOC_DocumentTypes}, cn1, 1 */
#define TAG_9F62_PCVC3_TRACK1   0x9F62

#define TAG_9F62_CARDHOLDER_DOCUMENTS_TYPE   0X9F62

/** [9F63] - ��Ʒ��ʶ��Ϣ, b, 16 */
#define TAG_9F63_PUNATC_TRACK1   0x9F63

/** [9F64] - ��Ʒ��ʶ��Ϣ, b, 16 */
#define TAG_9F64_NATC_TRACK1   0X9F64

/** [9F65] - ��Ʒ��ʶ��Ϣ, b, 16 */
#define TAG_9F65_PCVC3_TRACK2   0X9F65

/** [9F66] - Terminal Transaction Qualifiers (TTQ) | �ն˽������� */
#define TAG_9F66_TERMINAL_TRANSACTION_QUALIFIERS   0X9F66

#define TAG_9F66_PUNATC_TRACK2	0x9F66

#define TAG_9F67_NATC_TRACK2	0x9F67

/** [9F69] - Card Authentication Related Data | ��Ƭ��֤�������  */
#define TAG_9F69_CARD_AUTHENTICATION_RELATED_DATA   0X9F69

/** [9F69] - UDOL  */
#define TAG_9F69_UDOL   0X9F69

/** [9F6A] - Unpredictable Number (Numeric)  */
#define TAG_9F6A_UNNUMBER_NUMERIC   0X9F6A

/** [9F6B] - Track 2 Data  */
#define TAG_9F6B_PAYPASSMSD_TRACK2DATA   0X9F6B

/** [9F6C] - Card Transaction Qualifiers (CTQ) | ��Ƭ�������� */
#define TAG_9F6C_CARD_TRANSACTION_QUALIFIERS   0X9F6C

/** [9F6D] - EC Reset Threshold | �����ֽ�������ֵ, n12, 6 */
#define TAG_9F6D_EC_RESET_THRESHOLD   0X9F6D

/** [9F6D] - *Mag-stripe Application Version Number (Reader)*/
#define TAG_9F6D_MSD_APPVER_NUM   0X9F6D

/** [9F6E] - Form Factor Indicator (FFI) | qVSDC��ʽָʾ��, b32, 4 */
#define TAG_9F6E_THIRD_PARTY_DATA 0x9F6E

//[9F6F] - DS Slot Management Control | 1,b,K/RA
#define TAG_9F6F_DS_SLOT_MANAGE_CTRL	0x9F6F

/** [9F70] - Protected Data Envelope 1 */
#define TAG_9F70_PROTECTED_DATA_ENVELOPE1   0X9F70

/** [9F71] - Protected Data Envelope 2 */
#define TAG_9F71_PROTECTED_DATA_ENVELOPE2   0X9F71

/** [9F72] - Protected Data Envelope 3 */
#define TAG_9F72_PROTECTED_DATA_ENVELOPE3   0X9F72

/** [9F73] - Protected Data Envelope 4 */
#define TAG_9F73_PROTECTED_DATA_ENVELOPE4   0X9F73

/** [9F74] - Protected Data Envelope 5 */
#define TAG_9F74_PROTECTED_DATA_ENVELOPE5   0X9F74

/** [9F74] - EC Issuer Authorization Code | �����ֽ𷢿�����Ȩ��, a, 6 */
#define TAG_9F74_EC_ISSUER_AUTHORIZATION_CODE   0X9F74

/** [9F75] - Unprotected Data Envelope 1 */
#define TAG_9F75_UNPROTECTED_DATA_ENVELOPE1   0X9F75

/** [9F76] - Unprotected Data Envelope 2 */
#define TAG_9F76_UNPROTECTED_DATA_ENVELOPE2   0X9F76

/** [9F77] - Unprotected Data Envelope 3 */
#define TAG_9F77_UNPROTECTED_DATA_ENVELOPE3   0X9F77

/** [9F77] - EC Balance Limit | �����ֽ��������, n12, 6 */
#define TAG_9F77_EC_BALANCE_LIMIT   0X9F77

/** [9F78] - Unprotected Data Envelope 4 */
#define TAG_9F78_UNPROTECTED_DATA_ENVELOPE4   0x9F78

/** [9F78] - EC Single Transaction Limit | �����ֽ𵥱ʽ����޶�, n12, 6 */
#define TAG_9F78_EC_SINGLE_TRANSACTION_LIMIT   0x9F78

/** [9F79] - Unprotected Data Envelope 5 */
#define TAG_9F79_UNPROTECTED_DATA_ENVELOPE5   0x9F79

/** [9F79] - EC Balance | �����ֽ����, n12, 6 */
#define TAG_9F79_EC_BALANCE   0x9F79

/** [9F7A] - EC Terminal Support Indicator | �����ֽ��ն�֧��ָʾ��, b, 1 */
#define TAG_9F7A_EC_TERMINAL_SUPPORT_INDICATOR   0x9F7A

/** [9F7B] - EC Terminal Transaction Limit | �����ֽ��ն˽����޶�, n12, 6 */
#define TAG_9F7B_EC_TERMINAL_TRANSACTION_LIMIT   0x9F7B

/** [9F7C] - Customer Exclusive Data (CED) | qVSDC���˻�����, b8--258, 1--32 */
#define TAG_9F7C_QVSDC_CED  0x9F7C

/** [9F7C] - Merchant Custom Data | 20,b,K/ACT/DET */
#define TAG_9F7C_MERCHANT_CUSTOM_DATA  0x9F7C

/** [9F7C] - Merchant Custom Data | 20,b,K/ACT/DET */
#define TAG_9F7D_DS_SUMMARY1  0x9F7D

/** [9F7E] - Mobile Support Indicator | 1,b,K */
#define TAG_9F7E_MOBILE_SUPPORT_INDICATOR  0x9F7E

/** [9F7F] - DS Unpredictable Number | 4,b,K/RA */
#define TAG_9F7F_DS_UNPREDICT_NUMBER  0x9F7F

/** [BF0C] - FCI (File Control Info) Issuer Discretionary Data */
#define TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA   0xBF0C

/** [DF01] - Ӧ��ѡ��ָʾ����ASI��, b, 1 */
#define TAG_DF01_ASI   0xDF01

/** [DF02] - ��֤���Ĺ�Կģ, b, 248 */
#define TAG_DF02_CAPK_MODULUS   0xDF02

/** [DF03] - ��֤���Ĺ�ԿУ��ֵ, b, ??? */
#define TAG_DF03_CAPK_CHECKSUM   0xDF03

/** [DF04] - ��֤���Ĺ�Կָ��, b, 1 or 3 */
#define TAG_DF04_CAPK_EXPONENT   0xDF04

/** [DF05] - ��֤���Ĺ�Կ��Ч��, n8, 8 */
#define TAG_DF05_CAPK_EXPIRATION_DATE   0xDF05

/** [DF06] - ��֤���Ĺ�Կ��ϣ�㷨��ʶ, b, 1 */
#define TAG_DF06_CAPK_HASH_ALGORITHM_INDICATOR   0xDF06

/** [DF07] - ��֤���Ĺ�Կ�㷨��ʶ, b, 1 */
#define TAG_DF07_CAPK_ALGORITHM_INDICATOR   0xDF07

/** [DF11] - �ն���Ϊ����-ȱʡ, b, 5 */
#define TAG_DF11_TAC_DEFAULT   0xDF11

/** [DF12] - �ն���Ϊ����-����, b, 5 */
#define TAG_DF12_TAC_ONLINE   0xDF12

/** [DF13] - �ն���Ϊ����-�ܾ�, b, 5 */
#define TAG_DF13_TAC_DENIAL   0xDF13

/** [DF14] - ȱʡDDOL */
#define TAG_DF14_DEFAULT_DDOL   0xDF55 //xinyong 160223 �޸�TAG DF14 ֵΪ0XDF55��0XDF14��V2CA0130004V4.1a������ͻ

/** [DF15] - ƫ�����ѡ��ֵ, b, 4 */
#define TAG_DF15_THRESHOLD_VALUE   0xDF15

/** [DF16] - ƫ�����ѡ�����Ŀ��ٷ���, b, 1 FXIME */
#define TAG_DF16_MAX_TARGET_PERCENTAGE   0xDF16

/** [DF17] - ���ѡ��Ŀ��ٷ���, b, 1 FXIME */
#define TAG_DF17_TARGET_PERCENTAGE   0xDF17

/** [DF18] - �ն�����PIN֧������, b, 1 */
#define TAG_DF18_ONLINE_PIN_SUPPORT_INDICATOR   0xDF18

/** [DF19] - �ǽӴ��ѻ�����޶�, cn, 6 */
#define TAG_DF19_CONTACTLESS_FLOOR_LIMIT   0xDF19

/** [DF20] - �ǽӴ������޶�, cn, 6 */
#define TAG_DF20_CONTACTLESS_TRANSACTION_LIMIT   0xDF20

/** [DF21] - �ֿ�����֤������CVM�������޶�, cn, 6 */
#define TAG_DF21_CVM_REQUIRED_LIMIT   0xDF21

/** [DF22] - ������ѡ�� */
#define TAG_DF22_ZERO_AMT_CHECK_OPTION 0xDF22

/** [DF31] - �����нű�������� */
#define TAG_DF31_ISSUER_SCRIPT_RESULTS   0xDF31

/** [DF4B] - POS Cardholder Interaction Information, b,3, K/RA */
#define TAG_DF4B_POS_CARDHOLDER_INTERACT_INFO 0xDF4B

/** [DF4D] - Ȧ����־���, b16, 2 */
#define TAG_DF4D_LOAD_LOG_SFI   0XDF4D

/** [DF4D] - Ȧ����־��ʽ, b, var. */
#define TAG_DF4F_LOAD_LOG_FORMAT   0XDF4F

/** [DF60] - DS Input (Card), b, 8, k */
#define TAG_DF60_DS_INPUT_CARD   0XDF60

/** [DF61] - DS Digest H, b, 8, k */
#define TAG_DF61_DS_DIGEST_H   0XDF61

/** [DF62] - DS ODS Info, b, 8, k */
#define TAG_DF62_DS_ODS_INFO   0XDF62

/** [DF63] - DS ODS Term b, 8, k */
#define TAG_DF63_DS_ODS_TERM   0XDF63

/** [DF69] - SM�㷨֧��ָʾ��, b8, 1 */
#define TAG_DF69_SM_SUPPORT_INDICATOR   0xDF69

/** [DF70] - ������֧��ָʾ�� */
#define TAG_DF70_EXCEPTION_FILE_SUPPORT 0xDF70

//**[DF8101] DS Summary 2
#define TAG_DF8101_DS_SUMMARY2	0xDF8101

//**[DF8102] DS Summary 3
#define TAG_DF8102_DS_SUMMARY3	0xDF8102

//**[DF8104] Balance before GAC
#define TAG_DF8104_BALANCE_BEFOREGAC	0xDF8104

//**[DF8105] Balance after GAC
#define TAG_DF8105_BALANCE_AFTERGAC	0xDF8105

//**[DF8106] Data Needed
#define TAG_DF8106_DATA_NEEDED	0xDF8106

//**[DF8107]CDOL1 Related Data
#define TAG_DF8107_CDOL1_RELATED_DATA	0xDF8107

//**[DF8108]DS AC Type
#define TAG_DF8108_DS_AC_TYPE	0xDF8108

//**[DF8109]DS Input (Term)
#define TAG_DF8109_DS_INPUT_TERM	0xDF8109

//**[DF810A]DS ODS Info For Reader
#define TAG_DF810A_DS_ODS_INFO_FOR_READER	0xDF810A

//**[DF810B]DS Summary Status
#define TAG_DF810B_DS_SUMMARY_STATUS 0xDF810B

//**[DF810C]Kernel ID
#define TAG_DF810C_KERNEL_ID 0xDF810C

//**[DF810D]DSVN Term || Var b k
#define TAG_DF810D_DSVN_TERM 0xDF810D

//**[DF810E]Post-Gen AC Put Data Status
#define TAG_DF810E_POSTGAC_PUTDATA_STATUS 0xDF810E

//**[DF810F]Post-Gen AC Put Data Status
#define TAG_DF810F_PREGAC_PUTDATA_STATUS 0xDF810F

//**[DF8110] Proceed To First Write Flag
#define TAG_DF8110_PROCEED_TO_FIRST_WRITE_FLAG 0xDF8110

//**[DF8111] PDOL Related Data
#define TAG_DF8111_PDOL_RELATED_DATA 0xDF8111

//**[DF8112] Tags To Read
#define TAG_DF8112_TAGS_TO_READ	0xDF8112

//**[DF8113] Tags To Read Yet
#define TAG_DF8113_TAGS_TO_READ_YET	0xDF8113

//**[DF8114] Reference Control Parameter
#define TAG_DF8114_REF_CONTROL_PARAM	0xDF8114

//**[DF8115] Error Indication
#define TAG_DF8115_ERROR_INDICATION	0xDF8115

//**[DF8116] User Interface Request Data
#define TAG_DF8116_UI_REQUEST_DATA	0xDF8116

//**[DF8117] Card Data Input Capability
#define TAG_DF8117_CARDDATA_INPUT_CAP	0xDF8117

//**[DF8118] CVM Capability �?CVM Required
#define TAG_DF8118_CVMCAP_CVMREQ	0xDF8118

//**[DF8119] CVM Capability �?No CVM Required
#define TAG_DF8119_CVMCAP_NOCVM	0xDF8119

//**[DF811A] Default UDOL
#define TAG_DF811A_DEFAULT_UDOL	0xDF811A

//**[DF811B] Kernel Configuration
#define TAG_DF811B_KERNEL_CONFIG	0xDF811B

//**[DF811C] max life time of torn recover log
#define TAG_DF811C_TORNLOG_MAX_LIFETIME	0xDF811C

//**[DF811E] Mag-stripe CVM Capability �?CVM Required
#define TAG_DF811E_MSD_CVMCAP_CVMREQ	0xDF811E

//**[DF811F] Security Capability
#define TAG_DF811F_SECURITY_CAP	0xDF811F

//**[DF8120] TAC-Default
#define TAG_DF8120_TAC_DEFAULT	0xDF8120

//**[DF8121] TAC-Denial
#define TAG_DF8121_TAC_DENIAL	0xDF8121

//**[DF8122] TAC-Online
#define TAG_DF8122_TAC_ONLINE	0xDF8122

//**[DF8123] Reader Contactless Floor Limit
#define TAG_DF8123_READER_CTL_FLOOR_LIMIT	0xDF8123

//**[DF8124] Reader Contactless Transaction Limit (No On-device CVM)
#define TAG_DF8124_READER_CTL_TRX_LIMIT_NOCDCVM	0xDF8124

//**[DF8125] Reader Contactless Transaction Limit (On-device CVM)
#define TAG_DF8125_READER_CTL_TRX_LIMIT_CDCVM	0xDF8125

//**[DF8126] Reader CVM Required Limit
#define TAG_DF8126_READER_CVM_LIMIT	0xDF8126

//**[DF8127] Time Out Value
#define TAG_DF8127_TIMEOUT_VALUE 0xDF8127

//**[DF8128] IDS Status
#define TAG_DF8128_IDS_STATUS	0xDF8128

//**[DF8129] Outcome Parameter Set
#define TAG_DF8129_OUTCOME_PARAM_SET 0xDF8129

//**[DF812A] DD Card (Track1)
#define TAG_DF812A_DDCARD_TRACK1	0xDF812A

//**[DF812B] DD Card (Track2)
#define TAG_DF812B_DDCARD_TRACK2	0xDF812B

//**[DF812C] Mag-stripe CVM Capability �?No CVM Required
#define TAG_DF812C_MSD_CVMCAP_NOCVM	0xDF812C

//**[DF812D] Message Hold Time
#define TAG_DF812D_MESSAGE_HOLD_TIME	0xDF812D

//**[DF8130]Hold Time Value
#define TAG_DF8130_HOLD_TIME_VALUE	0xDF8130

//**[DF8131]Phone Message Table
#define TAG_DF8131_PHONE_MESS_TABLE	0xDF8131

//**[DF8132]Minimum Relay Resistance Grace Period
#define TAG_DF8132_MIN_RR_GRACE_PERIOD	0xDF8132

//**[DF8133]Maximum Relay Resistance Grace Period
#define TAG_DF8133_MAX_RR_GRACE_PERIOD	0xDF8133

//**[DF8134]Terminal Expected Transmission Time For Relay Resistance C-APDU
#define TAG_DF8134_TERMEXPECTTRANSTIME_FOR_RRP_CAPDU	0xDF8134

//**[DF8135]Terminal Expected Transmission Time For Relay Resistance R-APDU
#define TAG_DF8135_TERMEXPECTTRANSTIME_FOR_RRP_RAPDU	0xDF8135

//**[DF8136]Relay Resistance Accuracy Threshold
#define TAG_DF8136_RR_ACCURACY_THRESHOLD	0xDF8136

//**[DF8137]Relay Resistance Transmission Time Mismatch Threshold
#define TAG_DF8137_RR_TRANSTIME_MISMATCH_THRESHOLD	0xDF8137

//**[DF8301]Terminal Relay Resistance Entropy
#define TAG_DF8301_TERM_RR_ENTROPY	0xDF8301

//**[DF8302]Device Relay Resistance Entropy
#define TAG_DF8302_DEVICE_RR_ENTROPY	0xDF8302

//**[DF8303]Min Time For Processing Relay Resistance APDU
#define TAG_DF8303_MIN_TIME_FOR_PROCESS_RR_APDU	0xDF8303

//**[DF8304]Max Time For Processing Relay Resistance APDU
#define TAG_DF8304_MAX_TIME_FOR_PROCESS_RR_APDU	0xDF8304

//**[DF8305]Device Estimated Transmission Time For Relay Resistance R-APDU
#define TAG_DF8305_DEVICE_EST_TRANSTIME_RR_RAPDU	0xDF8305

//**[DF8306]Measured Relay Resistance Processing Time
#define TAG_DF8306_MEASURED_RR_PROCESS_TIME	0xDF8306

//**[DF8307]RRP Counter
#define TAG_DF8307_RRP_COUNTER	0xDF8307

#define TAG_DF830A_PROPRIETARY_TAG	0xDF830A

//**[FF8102]Tags To Write Before Gen AC
#define TAG_FF8102_TAGS_TO_WRITE_BEFOREGAC	0xFF8102

//**[FF8103]Tags To Write After Gen AC
#define TAG_FF8103_TAGS_TO_WRITE_AFTERGAC	0xFF8103

//**[FF8104]Data To Send
#define TAG_FF8104_DATA_TO_SEND	0xFF8104

//**[FF8105]Data Record
#define TAG_FF8105_DATA_RECORD	0xFF8105

#define TAG_FF8106_DISCRETIONARY_DATA	0xFF8106

#define TAG_FF8113_TAGS_TO_WRITE_YET_AFTERGAC	0xFF8113

#define TAG_FF8114_TAGS_TO_WRITE_YET_BEFOREGAC	0xFF8114

#define TAG_FF8115_TAGS_MISSING_PDOLDATA_FLAG	0xFF8115

#define TAG_FF8116_FAILED_MS_CNTR				0xFF8116

#define TAG_FF8117_ACTIVE_AFL					0xFF8117



/**
 * Self-defined tags for internal usage.
 * These tag names are just defined according to
 * current hash value spread. So if the hash
 * function changes in emvbuf.c leading to
 * a different spread, these tag names may
 * be changed too.
 * Always use these macros instead of
 * any real integer.
 */
#define TAG_PDOL_VALUE          0XB6
#define TAG_CDOL1_VALUE         0xB7
#define TAG_CDOL2_VALUE         0xB8
#define TAG_AUTH_DATA           0xB9
#define TAG_ISSUER_PK           0xBA
#define TAG_ICCARD_PK           0xBB
#define TAG_CDAAUTH_VALUE       0xBC
#define TAG_ONLINE_PIN          0xBD
#define TAG_UDOL_VALUE          0xBF

/** Cumulative Total Transaction Amount | �ۼ��ѻ����׽�� */
#define TAG_CUMULATIVE_TOTAL_TRANSACTION_AMOUNT   0XC5
#define TAG_CTTA   TAG_CUMULATIVE_TOTAL_TRANSACTION_AMOUNT

/** Cumulative Total Transaction Amount Limit | �ۼ��ѻ����׽�������� */
#define TAG_CUMULATIVE_TOTAL_TRANSACTION_AMOUNT_LIMI   0XC6
#define TAG_CTTAL   TAG_CUMULATIVE_TOTAL_TRANSACTION_AMOUNT_LIMI

#define TAG_ISSUER_SCRIPT_71_BASE   0XDF8100
#define TAG_ISSUER_SCRIPT_72_BASE   0XDF8200


#define TAG_TRANSACTION_REFERENCE_CURRENCY_CONVERSION   0XDF8307


/** ָ֪ͨʾ�� */
#define TAG_ADVICE_INDICATOR   0XDF8405

/** ǩ��ָʾ�� */
#define TAG_SIGNATURE_INDICATOR   0XDF8406


/** һ�������� */
#define TAG_ICS   0X1F01
/** ״̬���֧��ָʾ�� */
#define TAG_STATUS_CHECK_SUPPORT_INDICATOR   0X1F02

/** ״̬���֧��ָʾ�� */
#undef  TAG_STATUS_CHECK_SUPPORT_INDICATOR
#define TAG_STATUS_CHECK_SUPPORT_INDICATOR   0xDFC108//0X1F02//
/** Ĭ��TDOL */
#define TAG_DEFAULT_TDOL   0xDF8102//0X1F03

//Transaction Reference Currency Conversion(n8) <reserve
#undef  TAG_TRANSACTION_REFERENCE_CURRENCY_CONVERSION
#define TAG_TRANSACTION_REFERENCE_CURRENCY_CONVERSION   0XDF8101//0XDF8307

//���ز���ָʾλ
#define TAG_LOADPARA_INDICATOR   0XDF8103

//���ݲɼ���ʽ
#define TAG_DATA_CAPTURE   0XDF8104

//Certificate Serial Number
#define TAG_CERTIFICATION_SN   0XDF8105

//����ָʾλ
#define TAG_TRANSACTION_INDICATOR   0XDF8106

//���ŵ�������
#define TAG_TRACK2_REAL_DATA   0XDF8107

/** ������֧��ָʾ�� */
#define TAG_ZERO_CHECK_SUPPORT_INDICATOR   0X1F04
/** �����鴦���� */
#define TAG_ZERO_CHECK_OPTION  0X1F05
/** DRL����֧��ָʾ�� */
#define TAG_DRL_SUPPORT_INDICATOR   0X1F06
/** �ֽ���AUC��� */
#define TAG_AUC_CASH   0X1F07
/** ���ֽ���AUC��� */
#define TAG_AUC_CASHBACK  0X1F08
/** ���ö�Ӧ���ݲ����� */
#define TAG_CONFIG_DATA_NOT_TRANSMIT   0X1F09
/** �ǽ��޶���֧��ָʾ�� */
#define TAG_CL_TRANS_LIMIT_SUPPORT_INDICATOR   0X1F0A
/** �ǽ��ѻ��޶���֧��ָʾ�� */
#define TAG_CL_FLOOR_LIMIT_SUPPORT_INDICATOR   0X1F0B
/** �ǽ�CVM�޶���֧��ָʾ�� */
#define TAG_CVM_LIMIT_SUPPORT_INDICATOR   0X1F0C
#define TAG_EXTENDED_SELECT_SUPPORT	0x1F30


//////////////////////////////////////////////////////
//0XDF8F00 ~ 0XDF8F7F : ��Ӧ�����ݣ��ں�ר��
//////////////////////////////////////////////////////

/** �ں�ר�����ݻ�ֵ����Ӧ�����ݣ��ں�ר�ã� */
#define TAG_KERNEL_BASE   0XDF8800

/** �ں˰汾�ţ���Ӧ�����ݣ��ں�ר�ã� */
#define TAG_KERNEL_VERSION   0XDF8F01

/** �ں�ChkSum����Ӧ�����ݣ��ں�ר�ã� */
#define TAG_KERNEL_CHECKSUM   0XDF8F02

/** ����ChkSum��0��ʾĬ�����ã���Ӧ�����ݣ��ں�ר�ã� */
#define TAG_CONFIG_CHECKSUM   0xDF8F03

/** Ӧ�ú�ѡ�б�����Ӧ�����ݣ��ں�ר�ã� */
#define TAG_CANDIDATE_LIST   0XDF8F04

//////////////////////////////////////////////////////
//////////////////////////////////////////////////////
EMV_TLVOBJ GetTLV(uint32_t tag);
uint8_t *GetTagName(uint32_t ui_tag);
int32_t GetTagLength(uint32_t tag);
bool IsEmpty(uint32_t tag);
bool IsNotEmpty(uint32_t tag);
bool IsNotPresent(uint32_t tag);
bool IsKnown(uint32_t tag);
bool IsEmptyList(uint32_t List);
int32_t OutputTLV(EMV_TLVOBJ *tlvs, int32_t tlv_size, uint8_t *output, int32_t output_size);
uint8_t GetTagUpdateCondition(uint32_t tag, uint32_t belong);
uint8_t *GetAndRemoveFromList(uint32_t *itemLen, uint32_t List);
int32_t emv_taglib_add_proprietary_tag(uint32_t tag);
bool emv_taglib_find_proprietary_tag(uint32_t tag);
uint8_t emv_taglib_read_UpdateCondition(uint32_t tag, uint32_t belong, uint8_t matchMode, uint8_t Update, EMV_TAG_ITEM *item);
int32_t emv_taglib_ReadItemWithExactMatch(uint32_t tag, uint32_t belong, EMV_TAG_ITEM *item);
uint8_t emv_ParseExtLenWithMaxLen(uint8_t *ParseData,uint16_t *index,uint16_t *DataLen,uint16_t MaxLen);
int32_t GetItemFromList(uint32_t List, EMV_TLVOBJ *item_arry, int32_t size);
int32_t GetTagFromList(uint32_t List, uint32_t *item_arry, int32_t size);
uint8_t *GetAndRemoveFromList(uint32_t *itemLen, uint32_t List);
uint32_t GetNextGetDataTagFromList(uint32_t List, bool remove);
bool IsEmptyList(uint32_t List);
bool IsNotEmptyList(uint32_t List);
bool IsItemIncludeList(uint8_t *item, uint8_t itemLen, uint32_t List);
bool IsKnown(uint32_t tag);
bool IsPresent(uint32_t tag);
bool IsNotPresent(uint32_t tag);
bool IsNotEmpty(uint32_t tag);
bool IsEmpty(uint32_t tag);
void Initialize(uint32_t tag);
int32_t GetTagLength(uint32_t tag);
uint8_t *GetTagName(uint32_t ui_tag);
EMV_TLVOBJ GetTLV(uint32_t tag);
int32_t GetLength(uint32_t tag);
bool IsTagPrivateClass(uint32_t tag_dec);
uint8_t GetTagUpdateCondition(uint32_t tag, uint32_t belong);
int32_t OutputTLV(EMV_TLVOBJ *tlvs, int32_t tlv_size, uint8_t *output, int32_t output_size);
bool IsTagIncludeTemplateValid(uint32_t tag, uint32_t tmplate, uint8_t RFU);
bool ParseAndStoreCardResponse(uint8_t *resp, int32_t resp_len, uint32_t tmplate, EMV_TLVOBJ *tlvs, int32_t tlvs_size);


#endif /* EMV_TAG_H_ */
/*
************************************************************************************************************************
*                                        COPYRIGHT (C) 2014 Kingsee Tech. Co., Ltd.               ~~~ END OF FILE ~~~
************************************************************************************************************************
*/
