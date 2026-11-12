#ifndef __KS_CRYPT__H
#define __KS_CRYPT__H
//----------------------------------------------------------------------------------------------------------------------
//
//	V1.0	2014/12/4 10:09:22	初始版本管理，修正函数名大小写。
//	V1.1 	2014/12/4 10:18:30	修改API_Hash,增加HASH_FLAG,用于处理大块的HASH计算
//  V1.2 	2014/12/4 11:00:30  增加HASH_RESULT_LEN定义，API_Hash要求强制输入
//  V1.3 	2014/12/5 09:39:51  规范注释说明
//
//----------------------------------------------------------------------------------------------------------------------
//@随机数生产

    enum CRYPT_RET
    { 
    	CRYPT_OK				=	 0,
    	CRYPT_PAR_ERR			=    1,
    	CRYPT_NOT_SUPPORTED		=	 2,
    	CRYPT_ERR		    	=	-1,
	};

	    /**
	     * 获取随机数
	     * @param buf 	随机数输出缓存
	     * @param len 	随机数获取长度
	     * @return 		CRYPT_RET
	     */
    int API_Rng(u8* buf, int len);

//----------------------------------------------------------------------------------------------------------------------
//@哈希计算

    enum HASH_TYPE 
    {
        HASH_TYPE_SHA1      = 1,    //
        HASH_TYPE_SHA224    = 2,    // 不支持
        HASH_TYPE_SHA256    = 3,
        HASH_TYPE_SHA384    = 4,    // 不支持
        HASH_TYPE_SHA512    = 5,    //
        HASH_TYPE_MD2       = 11,   // 不支持
        HASH_TYPE_MD4       = 12,   // 不支持
        HASH_TYPE_MD5       = 13,   //
        HASH_TYPE_SM3       = 21,   //
    };

    enum HASH_RESULT_LEN
    {
		HASH_RESULT_LEN_SHA1 	= 20,
		HASH_RESULT_LEN_SHA224	= 32,
    	HASH_RESULT_LEN_SHA256	= 32,
		HASH_RESULT_LEN_SHA384	= 64,
		HASH_RESULT_LEN_SHA512	= 64,
		HASH_RESULT_LEN_MD2		= 16,
		HASH_RESULT_LEN_MD4		= 16,
    	HASH_RESULT_LEN_MD5		= 16,
    	HASH_RESULT_LEN_SM3		= 20,
    };

    enum HASH_FLAG
    {
    	HASH_DIRECT			= 0,	// 直接计算HASH				input,inlen,output必须, 
    	HASH_INIT			= 1,	// 分块计算HASH 初始化,   	output必须
    	HASH_UPDATE			= 2,	// 分块计算HASH 更新数据, 	input,inlen,output必须
    	HASH_RESULT			= 3,	// 分块计算HASH 计算结果  	input,inlen,output必须, 
    };

    /**
     * 哈希运算
     *
     * @param type      HASH_TYPE
     * @param input     输入数据
     * @param inlen     输入数据长度
     * @param output    输出结果缓存
     * @param flag		HASH_FLAG
     * @return 			CRYPT_RET
     */
    int API_Hash(int type, u8* input, int inlen, u8* output, int flag);

//----------------------------------------------------------------------------------------------------------------------
//@加解密计算

    enum CRYPT_TYPE 
    {
        CRYPT_TYPE_DES      = 1,
        CRYPT_TYPE_SM4      = 2,
        CRYPT_TYPE_AES      = 3,
        CRYPT_TYPE_RSA      = 11,
        CRYPT_TYPE_SM2      = 12,
        CRYPT_TYPE_SM1      = 13,   // ADD BY OLIVER
    };

    enum CRYPT_MODE 
    {
        /* 加密解密 */
        CRYPT_MODE_ENCRYPT     = 0x00,	//公钥加密
        CRYPT_MODE_DECRYPT     = 0x01,	//私钥解码
        CRYPT_MODE_SIGN		= 0x02,	//LIUYANG ADD 获取签名(暂不实现)
        CRYPT_MODE_VERIFY     	= 0x04,	//LIUYANG ADD 验签(目前仅支持SM2的，签名数据从outbuf传进来)

        /* 工作模式 */
        CRYPT_MODE_ECB         = 0x0000,
        CRYPT_MODE_CBC         = 0x0100,
        CRYPT_MODE_OFB         = 0x0200,
        CRYPT_MODE_CFB         = 0x0400,
        CRYPT_MODE_MAC_X919    = 0x1000,   //ANSI X9.9 or X9.19
        CRYPT_MODE_MAC_CBC     = 0x2000,
        CRYPT_MODE_MAC_CUP     = 0x4000,
        CRYPT_MODE_MAC_EMV     = 0x8000,
    };
    
	
    #define MAX_RSA_MODULUS_BITS    2048
    #define MAX_RSA_MODULUS_LEN     ((MAX_RSA_MODULUS_BITS + 7) / 8)
    #define MAX_RSA_PRIME_BITS      ((MAX_RSA_MODULUS_BITS + 1) / 2)
    #define MAX_RSA_PRIME_LEN       ((MAX_RSA_PRIME_BITS   + 7) / 8)
    #define MAX_RSA_EXPONENT_LEN    4

    // 公钥
    typedef struct 
    {
		u32  	length;                                 /* length in bytes of modulus */
		u8 		modulus[MAX_RSA_MODULUS_LEN];           /* modulus N */
		u8 		exponent[MAX_RSA_EXPONENT_LEN];         /* public exponent E */ //不足位前补0
    } RSA_PUBKEY;

    // 私钥
    typedef struct 
    {
		u32  	length;                                 /* length in bytes of modulus */
		u8 		modulus[MAX_RSA_MODULUS_LEN];           /* modulus N */
		u8 		publicExponent[MAX_RSA_EXPONENT_LEN];   /* public exponent E */ //不足位前补0
		u8 		exponent[MAX_RSA_MODULUS_LEN];          /* private exponent D */ //不足位前补0
		u8 		prime[2][MAX_RSA_PRIME_LEN];            /* prime factors P,Q */
		u8 		primeExponent[2][MAX_RSA_PRIME_LEN];    /* exponents for CRT */
		u8 		coefficient[MAX_RSA_PRIME_LEN];         /* CRT coefficient */
    } RSA_PRIVKEY;

    // 公钥
    typedef struct 
    {
        u8 xQ[32];
        u8 yQ[32];
    } SM2_PUBKEY;

    // 私钥
    typedef struct 
    {
        u8 xQ[32];
        u8 yQ[32];
        u8  d[32];
    } SM2_PRIVKEY;

	    /**
	     * 对称运算加解密运算
	     * @param type      算法类型 CRYPT_TYPE
	     * @param key       密钥
	     * @param keylen    密钥长度（8/16/24）
	     * @param iv        初始值（mode含DES_CBC时有效, 为NULL时默认值全0）
	     * @param input     输入数据
	     * @param inlen     输入数据长度（8的倍数）
	     * @param output    输出数据
	     * @param mode      参考DES_MODE, 工作模式|加密解密 @see CRYPT_MODE
	     * @return  		CRYPT_RET
	     */
    int API_Crypt(int type, const u8* key, int keylen, const u8* iv, const u8* input, int inlen, u8* output, int * outlen, int mode);

//----------------------------------------------------------------------------------------------------------------------
//@密钥容器管理（建议一般情况下不使用此接口）
	
	// 密钥索引定义:					0~31,0xFF临时内存存储
	
    // 密钥类型定义
    #define ALG_KEY_TYPE_RSA        	CRYPT_TYPE_RSA    		// RSA 
    #define ALG_KEY_TYPE_SM2        	CRYPT_TYPE_SM2    		// SM2 
    
	// 生成密钥长度定义
    #define ALG_KEY_BIT_SM2_FP_256  	256     				//  256/8=32
    #define ALG_KEY_BIT_RSA_1024    	1024    				// 1024/8=128
    #define ALG_KEY_BIT_RSA_2048    	2048    				// 2048/8=256
    
	    /**
	     * 在keystore中生成密钥对
	     * @param index		参考密钥索引定义（不支持0xFF）
	     * @param keytype 	ALG_KEY_TYPE_RSA,ALG_KEY_TYPE_SM2
	     * @param maxbit	ALG_KEY_BIT_SM2_FP_256,ALG_KEY_BIT_RSA_1024,ALG_KEY_BIT_RSA_2048
	     * @return CRYPT_RET
	     */
    int API_Crypt_GenKey(int index, int keytype, int maxbit);

	    /**
	     * 导入私钥到keystore中存储
	     * @param index 	参考密钥索引定义
	     * @param keytype 	ALG_KEY_TYPE_RSA,ALG_KEY_TYPE_SM2
	     * @param privkey 	SM2对应SM2_PRIVKEY,RSA对应RSA_PRIVKEY
	     * @param keylen 	密钥长度
	     * @return 			CRYPT_RET
	     */
    int API_Crypt_ImportPrivateKey(int index, int keytype, u8 * privkey, int keylen);

		/**
	     * 导入公钥到keystore中存储
	     * @param index 	参考密钥索引定义
	     * @param keytype 	ALG_KEY_TYPE_RSA,ALG_KEY_TYPE_SM2
	     * @param publickey SM2对应SM2_PUBKEY,RSA对应RSA_PUBKEY
	     * @param keylen 	密钥长度
	     * @return CRYPT_RET
	     */
    int API_Crypt_ImportPublicKey(int index, int keytype, u8 * publickey, int keylen);

	    /**
	     * 获取指定索引公钥信息
	     * @param index 	参考密钥索引定义
	     * @param pubkey 	SM2对应SM2_PUBKEY,RSA对应RSA_PUBKEY
	     * @param keylen 	密钥长度
	     * @return CRYPT_RET
	     */
    int API_Crypt_GetPubKey(int index, u8 * pubkey, int * keylen);

	    /**
	     * 公钥运算
	     * @param index 	参考密钥索引定义
	     * @param input 	数据
	     * @param inputlen 	数据长度
	     * @param output 	输出数据
	     * @param outputlen 输出数据的长度
	     * @param flag 		算法标志 [0]-已经填充过的数据(默认) [1]-保留
	     * @return 			CRYPT_RET
	     */
    int API_Crypt_Calc_Use_PublicKey(int index, u8* input, int inputlen, u8* output, int * outputlen, int flag);

	    /**
	     * 私钥运算
	     * @param index 	参考密钥索引定义
	     * @param input 	数据
	     * @param inputlen 	数据长度
	     * @param output 	输出数据
	     * @param outputlen 输出数据的长度
	     * @param flag 		算法标志 [0]-已经填充过的数据(默认) [1]-保留
	     * @return CRYPT_RET
	     */
    int API_Crypt_Calc_Use_PrivateKey(int index, u8* input, int inputlen, u8* output, int * outputlen, int flag);

		/**
	     * 导入对称密钥到keystore中存储，无对称密钥管理
	     * 考虑到对称密钥管理导入模式，使用RSA公钥加密，约定一个索引的密钥对作为堆成密钥管理密钥对
	     * @param index 	参考密钥索引定义
	     * @param keytype 	CRYPT_TYPE 中定义的对称秘钥
	     * @param key 		对称密钥
	     * @param keylen 	对称密钥长度
	     * @param flag 		保留使用用于安全密钥导入
	     * @return 			CRYPT_RET
	     */
    int API_Crypt_ImportSecretKey(int index, int keytype, u8 * key, int keylen, int flag);

	    /**
	     * 对称密钥加解密运算
	     * @param index 	参考密钥索引定义
	     * @param input 	数据
	     * @param inputlen 	数据长度
	     * @param output 	输出数据
	     * @param outputlen 输出数据的长度
	     * @param flag 		算法标志 [CRYPT_MODE_ENCRYPT]加密 [CRYPT_MODE_DECRYPT]解密 
	     * @return 			CRYPT_RET
	     */
    int API_Crypt_Calc_Use_SecretKey(int index, u8* input, int inputlen, u8* output, int * outputlen, int flag);
    
    /**
	     * AC5数据传输
	     * @param input 	数据
	     * @param inputlen 	数据长度
	     * @param output 	输出数据
	     * @param outputlen 输出数据的长度
	     * @return 			CRYPT_RET
	     */
    int API_Crypt_Transceiver(u8* input, int inputlen, u8* output, int * outputlen);

//----------------------------------------------------------------------------------------------------------------------

#endif

