#ifndef _EMVSDK_H
#define _EMVSDK_H

/**
	 * 字符串转double
	 * @param s 	以'0'结尾的字符串
	 * @return 		数值
	 */
	extern float	API_atof(const char* s);

	/**
	 * 字符串转int
	 * @param s 	以'0'结尾的字符串
	 * @return 		数值
	 */
	extern int	    API_atoi(const char* s);

	/**
	 * 字符串转long
	 * @param s 	以'0'结尾的字符串
	 * @return 		数值
	 */
	extern long   	API_atol(const char* s);

	/**
	 * 产生随机数
	 *
	 * 可以生成0~RAND_MAX之间的一个随机数, RAND_MAX它与系统有关
	 * @return 		数值
	 */
	extern int		API_rand (void);

	/**
	 * 设置随机数生成器播散种子
	 * @param n 	种子
	 */	
	extern void		API_srand(unsigned int n);

	/**
	 * int取传入参数的绝对值
	 * @param n 	数值
	 * @return 		n的绝对值
	 */	
	extern int		API_abs	(int  n);

	/**
	 * long取传入参数的绝对值
	 * @param n 	数值
	 * @return 		n的绝对值
	 */	
	extern long   	API_labs(long n);


	/**
	 * 字符串格式化命令，主要功能是把格式化的数据写入某个字符串中
	 * 
	 * @param str 		char型指针，指向将要写入的字符串的缓冲区。
	 * @param format 	格式化字符串。
	 * @param ... 		可选参数，可以是任何类型的数据。
	 * @return 			字符串长度（strlen）
	 */
	extern int 	API_sprintf(char* str, const char* format, ...);

	/**
	 * 字符串格式化命令，主要功能是把格式化的数据写入某个字符串中
	 * 
	 * @param str 		char型指针，指向将要写入的字符串的缓冲区。
	 * @param size 		缓冲区的大小
	 * @param format 	格式化字符串。
	 * @param ... 		可选参数，可以是任何类型的数据。
	 * @return 			字符串长度（strlen）
	 */
	extern int 	API_snprintf(char *str, int size, const char *format, ...);

	/**
	 * 从str里读进数据，依照format的格式将数据写入到argument里
	 *
	 * @param str 		存储的数据
	 * @param format 	格式控制字符串。
	 * @param ... 		可选参数, 选择性设定字符串
	 */
	extern void 	API_sscanf(const char *str, const char *format, ...);
	
	extern void* 	API_memchr(const void*, int, int);
	extern int  	API_memcmp(const void*, const void*, int);
	extern void*  	API_memcpy(void*, const void*, int);
	extern void* 	API_memmove(void*, const void*, int);
	extern void* 	API_memset(void*, int, int);
	extern char* 	API_strcat(char*, const char*);
	extern char* 	API_strchr(const char*, int);
	extern int 		API_strcmp(const char*, const char*);
	extern int 		API_strcoll(const char*, const char*);
	extern char* 	API_strcpy(char*, const char*);
	extern int 		API_strcspn(const char*, const char*);
	extern char* 	API_strerror(int); 
	extern int 		API_strlen(const char*);
	extern char* 	API_strpbrk(const char*, const char*);
	extern char* 	API_strrchr(const char*, int);
	extern int 		API_strspn(const char*, const char*);
	extern char* 	API_strstr(const char*, const char*);
	extern char* 	API_strtok(char*, const char*);
	extern int 		API_strxfrm(char*, const char*, int);

	/**
	 * 初始化{@link API_malloc}和{@link API_free}使用的内存
	 * 如果资源充足则不使用, linux系统下不实现
	 *
	 * @param mem_addr 		内存地址
	 * @param mem_size 		内存大小
	 * @param particle_size 分配大小
	 * @return 				[1]成功　[0]失败
	 */
	extern int	 	API_heapinit(void* mem_addr,int mem_size,int particle_size);

	/**
	 * 分配长度为size字节的内存块
	 *
	 * @param size 	内存大小
	 * @return 		如果分配成功则返回指向被分配内存的指针(此存储区中的初始值不确定)，否则返回空指针NULL。
	 *				当内存不再使用时，应使用free()函数将内存块释放。函数返回的指针一定要适当对齐，使其可以用于任何数据对象。
	 */
	extern void*   API_malloc(int size);

	/**
	 * 释放ptr指向的存储空间。被释放的空间通常被送入可用存储区池，以后可在调用malloc、realloc以及calloc函数来再分配。
	 *
	 * @param ptr 通过{@link API_malloc}分配的指针
	 */
	extern void    API_free(void* ptr);

//-----------------------------------------------------------------------------------------------------------------------
	/*
	*文件系统使用规则：
	* 1.在应用程序第一次进行系统初始化时，建立所有需要使用的文件，且文件是定长的
	* 2.在应用运行过程中，不允许修改文件的信息（名称，长度，存放地址）
	* 3.对于频繁写入的内容，尽量保存在独立文件中，不要和其他内容存放在一起
	* 4.现在只能同时操作1个文件
	* 5.尽量不要删除文件
	* 6.只支持文件名操作，不支持带路径操作。文件名长度是8.3格式(名字最长为8字节，后缀名为3字节)
	*/
	/**
	 * 文件打开类型
	 */
	enum FILE_FLAG
	{
		F_RDONLY	=00000000,		//!< 只读
		F_WRONLY	=00000001,		//!< 只写
		F_RDWR		=00000002,		//!< 读写
		F_CREAT		=00000100,		//!< 自动创建
		F_APPEND	=00002000		//!< 追加到尾部
	};

	/**
	 * 文件seek模式
	 */
	enum FILE_SEEK
	{
		F_SEEK_SET	=00000001,		//!< 文件开头
		F_SEEK_CUR	=00000002,		//!< 当前位置
		F_SEEK_END	=00000004		//!< 文件结尾
	};

	/**
	 * 打开文件，返回文件句柄，大于0成功
	 *
	 * @param filename 	文件名，路径为当前应用路径
	 * @param flags 	打开方式,参考{@link FILE_FLAG}
	 * @return 			[>0]文件句柄　[<=0]错误
	 */
	extern int 	API_fopen(const char * filename, int flags);

	/**
	 * 关闭文件
	 *
	 * @param fd 	{@link API_fopen}打开有效文件句柄
	 * @return 		[>=0]文件操作当前位置 [<0]错误
	 */
	extern int 	API_fclose(int fd);

	/**
	 * 移动读写位置
	 *
	 * @param fd 		{@link API_fopen}打开有效文件句柄
	 * @param offset 	偏移位置
	 * @param whence 	起始位置，参考{@link FILE_SEEK}定义
	 * @return 			成功，返回0，失败返回-1
	 */
	extern int 	API_fseek(int fd,int offset ,int whence);

	/**
	 * 读文件
	 *
	 * @param fd 		文件句柄
	 * @param buf [out]	读取缓冲区
	 * @param count 	读取大小
	 * @return 			[>=0]返回实际读出的数据大小 [<0]错误
	 */
	extern int 	API_fread(int fd,void * buf, u32 count);

	/**
	 * 写文文件
	 *
	 * @param fd 	文件句柄
	 * @param buf 	写入缓冲区
	 * @param count 写入大小
	 * @return 		[>=0]返回实际写入的数据大小 [<0]错误
	 */
	extern int 	API_fwrite(int fd,const void * buf,u32 count);

	/**
	 * 释放映射内存
	 *
	 * @param buf 内存指针
	 * @param len 长度
	 */
	extern void 	API_funmap(void* buf,u32 len);
	
	/**
	 * 删除文件
	 *
	 * @param filename 文件名，路径为当前应用路径
	 * @return 			[0]成功 [非0]失败	
	 */	
	extern int 	API_fremove(const char *filename);
	/**
	 * 发回文件长度
	 *
	 * @param filename 	文件名，路径为当前应用路径
	 * @return 			[-1]找不到文件 [>=0]文件长度
	 */
	extern int 	API_fsize(const char *filename);
	
	/**
	 * 修改文件名
	 *
	 * @param oldfile 	原文件名, 路径为当前应用路径
	 * @param newfile   新文件名, 路径为当前应用路径
	 * @return 			[-1]找不到文件 [>=0]文件长度
	 */
	extern int 	API_fchange(const char *oldfile, const char* newfile);
	/**
	 * 将文件同步到磁盘
	 */
	extern void	API_fsync(void);
	
	//SDKV2.0 新增接口
	typedef struct  
	{
		u32 f_bsize; 		//!< 块大小, 块大小都为1
		u32 f_blocks; 		//!< 文件系统数据块总数, 此成员保存文件系统总存储大小
		u32 f_bfree; 		//!< 可用块数, 此成员保存文件系统剩余存储大小
		u32 f_files; 		//!< 文件结点总数
		u32 f_ffree; 		//!< 可用文件结点数
		u32 f_namelen; 		//!< 文件名的最大长度
	}FILE_SYSTEM;
	
	/**
	* 获取文件系统相关信息
	* @param pfs 输出{@link FILE_SYSTEM}结构指针
	* @return 		[0]成功 [非0]失败	
	*/
	extern int API_statfs(FILE_SYSTEM * pfs);
//-----------------------------------------------------------------------------------------------------------------------


int GetLibVerSion(char *ver);

#endif
