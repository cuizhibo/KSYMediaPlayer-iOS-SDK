#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "KSY_Drm.h"
#include "KSY_Aes.h"

//static const int8_t ref_zero[] = {
//     2,  0,  0,  0,  0, -1,  1, -1,
//    -1,  1,  1,  1,  1, -1,  2,  2,
//     2,  2,  2,  0,  2,  2,  2,  2,
//     2,  2,  2,  2,  2,  2,  2,  2
//};

int ksy_set_key( aes_context* ctx, char *ksyKey, int len )
{
    if(len!= ctx->len)
        return -1;
    ksy_aes_set_key(ctx, (uint8*)ksyKey);
	return 0;
}

aes_context* ksy_drm_init(int len, int count)
{
	aes_context* ctx = malloc(sizeof(aes_context));
	memset(ctx,0,sizeof(aes_context));
	ctx->len = len;
	ctx->count = count;
	return ctx;
}

/*
 * return 0 : len too small , so no encode
 * ksy_codec_counts > 0 : encode the blocks data
 * ksy_codec_counts <=0 : encode all the data
 */
static int ksy_decode_data( aes_context* ctx, uint8 *srcData, uint32_t dataLen)
{
	int decodeBlocks = 0, index = 0;

	if( ctx->count > 0 ){
		decodeBlocks = ctx->count < ( dataLen / ctx->len )? ctx->count:( dataLen / ctx->len );
		for( index = 0; index < decodeBlocks; index++ ){
			ksy_aes_decrypt( ctx, srcData, srcData );
			srcData += ctx->len;
		}
	}else{
		decodeBlocks = dataLen  / ctx->len;
		for( index = 0; index < decodeBlocks; index++ ){
			ksy_aes_decrypt( ctx, srcData, srcData );
			srcData += ctx->len;
		}
	}
	return 0;
}

static uint32_t ksy_get_data_len( int32_t lengthSize, uint8 *srcData , int64_t srcDataLen )
{
	if( !srcData || srcDataLen < lengthSize )
		return -1;
	uint32_t dataLen = 0;
	dataLen =  ((((uint32_t)srcData[0]) << 24 ) | (((uint32_t)srcData[1]) << 16 ) | (((uint32_t)srcData[2]) << 8 )  | srcData[3]);
	//memcpy( &dataLen , srcData, lengthSize );
	//return ntohl( dataLen );
	return dataLen;
}

int ksy_flv_drm_decode( aes_context* ctx, int32_t lengthSize, uint8 *srcData, int64_t srcDataLen )
{
	int64_t dataLen = -1;
	while( 1 ){
		dataLen = ksy_get_data_len( lengthSize, srcData, srcDataLen );
		if( dataLen <= 0 ){
			return -1;
		}
		if( dataLen > (srcDataLen - lengthSize) )
			return -1;
		srcData += lengthSize;
		switch (srcData[0] & 0x1F) {
        case 1:
        case 5:
            ksy_decode_data( ctx, srcData + 1, dataLen -1 );
            break;
		}
		srcDataLen = srcDataLen - lengthSize - dataLen;
		if( srcDataLen <= 0 )
			return 0;
		srcData += dataLen;
	}
	return 0;
}
