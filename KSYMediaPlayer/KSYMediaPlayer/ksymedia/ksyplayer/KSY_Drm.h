#ifndef __KSY_DRM_H_
#define __KSY_DRM_H_
#include "KSY_Aes.h"
#ifndef BYTE
#define BYTE unsigned char
#endif

#define KEY_TYPE_128	128   //num of bits
#define KEY_TYPE_192	192
#define KEY_TYPE_256	256

typedef struct ksy_nal_header{
	BYTE ksy_nal_unit_type 			:5;
	BYTE ksy_nal_ref_idc			:2;
	BYTE ksy_forbidden_zero_bit		:1;
}ksy_nal_unit_header_t;

/**
 * 设置keyValue
 */
int ksy_set_key( aes_context* ctx, char *ksyKey, int len);

/**
 * 初始化加密需要得信息 eg: key ctx;
 */
aes_context* ksy_drm_init(int len, int count);

/**
 * 解密数据块，传递进去的为加密后的ES流
 */
int ksy_flv_drm_decode(aes_context* ctx, int32_t lengthSize, uint8 *srcData, int64_t dataLen );


#endif
