/*
 * ksymeta.c
 *
 * This file is part of ksyPlayer.
 *
 * ksyPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ksyPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ksyPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ksymeta.h"
#include "ff_ffinc.h"
#include "ksyutil/ksyutil.h"

#define KSY_META_INIT_CAPACITY 13

typedef struct KSYMediaMeta {
    SDL_mutex *mutex;

    AVDictionary *dict;

    size_t children_count;
    size_t children_capacity;
    KSYMediaMeta **children;
} KSYMediaMeta;

KSYMediaMeta *ksymeta_create()
{
    KSYMediaMeta *meta = (KSYMediaMeta *)calloc(1, sizeof(KSYMediaMeta));
    if (!meta)
        goto fail;

    meta->mutex = SDL_CreateMutex();
    if (!meta->mutex)
        goto fail;

    return meta;
fail:
    ksymeta_destroy(meta);
    return NULL;
}

void ksymeta_destroy(KSYMediaMeta *meta)
{
    if (!meta)
        return;

    if (meta->dict) {
        av_dict_free(&meta->dict);
    }

    if (meta->children) {
        for(int i = 0; i < meta->children_count; ++i) {
            KSYMediaMeta *child = meta->children[i];
            if (child) {
                ksymeta_destroy(child);
            }
        }
        free(meta->children);
        meta->children = NULL;
    }

    SDL_DestroyMutexP(&meta->mutex);
}

void ksymeta_destroy_p(KSYMediaMeta **meta)
{
    if (!meta)
        return;

    ksymeta_destroy(*meta);
    *meta = NULL;
}

void ksymeta_lock(KSYMediaMeta *meta)
{
    if (!meta || !meta->mutex)
        return;

    SDL_LockMutex(meta->mutex);
}

void ksymeta_unlock(KSYMediaMeta *meta)
{
    if (!meta || !meta->mutex)
        return;

    SDL_UnlockMutex(meta->mutex);
}

void ksymeta_append_child_l(KSYMediaMeta *meta, KSYMediaMeta *child)
{
    if (!meta || !child)
        return;

    if (!meta->children) {
        meta->children = (KSYMediaMeta **)calloc(KSY_META_INIT_CAPACITY, sizeof(KSYMediaMeta *));
        if (!meta->children)
            return;
        meta->children_count    = 0;
        meta->children_capacity = KSY_META_INIT_CAPACITY;
    } else if (meta->children_count >= meta->children_capacity) {
        size_t new_capacity = meta->children_capacity * 2;
        KSYMediaMeta **new_children = (KSYMediaMeta **)calloc(new_capacity, sizeof(KSYMediaMeta *));
        if (!new_children)
            return;

        free(meta->children);
        meta->children          = new_children;
        meta->children_capacity = new_capacity;
    }

    meta->children[meta->children_count] = child;
    meta->children_count++;
}

void ksymeta_set_int64_l(KSYMediaMeta *meta, const char *name, int64_t value)
{
    if (!meta)
        return;

    av_dict_set_int(&meta->dict, name, value, 0);
}

void ksymeta_set_string_l(KSYMediaMeta *meta, const char *name, const char *value)
{
    if (!meta)
        return;

    av_dict_set(&meta->dict, name, value, 0);
}

static int get_bit_rate(AVCodecContext *ctx)
{
    int bit_rate;
    int bits_per_sample;

    switch (ctx->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
        case AVMEDIA_TYPE_DATA:
        case AVMEDIA_TYPE_SUBTITLE:
        case AVMEDIA_TYPE_ATTACHMENT:
            bit_rate = ctx->bit_rate;
            break;
        case AVMEDIA_TYPE_AUDIO:
            bits_per_sample = av_get_bits_per_sample(ctx->codec_id);
            bit_rate = bits_per_sample ? ctx->sample_rate * ctx->channels * bits_per_sample : ctx->bit_rate;
            break;
        default:
            bit_rate = 0;
            break;
    }
    return bit_rate;
}

void ksymeta_set_avformat_context_l(KSYMediaMeta *meta, AVFormatContext *ic)
{
    if (!meta || !ic)
        return;

    if (ic->iformat && ic->iformat->name)
        ksymeta_set_string_l(meta, KSYM_KEY_FORMAT, ic->iformat->name);

    if (ic->duration != AV_NOPTS_VALUE)
        ksymeta_set_int64_l(meta, KSYM_KEY_DURATION_US, ic->duration);

    if (ic->start_time != AV_NOPTS_VALUE)
        ksymeta_set_int64_l(meta, KSYM_KEY_START_US, ic->start_time);

    if (ic->bit_rate)
        ksymeta_set_int64_l(meta, KSYM_KEY_BITRATE, ic->bit_rate);

    if(ic->pb != NULL && strlen(ic->pb->source_ip) > 0)
	ksymeta_set_string_l(meta, KSYM_KEY_SOURCE_IP, ic->pb->source_ip);
    
    if(ic->pb != NULL && strlen(ic->pb->protocol_name) > 0)
    {
    	if (ic->iformat && ic->iformat->name)
	{
	   if ( strncmp(ic->iformat->name,"hls",3) == 0)
		ksymeta_set_string_l(meta, KSYM_KEY_PROTOCOL_NAME,"hls");
           else
		ksymeta_set_string_l(meta, KSYM_KEY_PROTOCOL_NAME, ic->pb->protocol_name);
	}
    }    

    if(ic->pb != NULL)
	ksymeta_set_int64_l(meta, KSYM_KEY_PLAY_TYPE, ic->pb->play_type);

    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        KSYMediaMeta *stream_meta = ksymeta_create();
        if (!st || !stream_meta)
            continue;

        if (st->codec) {
            AVCodecContext *avctx = st->codec;
            const char *codec_name = avcodec_get_name(avctx->codec_id);
            if (codec_name)
                ksymeta_set_string_l(stream_meta, KSYM_KEY_CODEC_NAME, codec_name);
            if (avctx->profile != FF_PROFILE_UNKNOWN) {
                const AVCodec *codec = avctx->codec ? avctx->codec : avcodec_find_decoder(avctx->codec_id);
                if (codec) {
                    const char *profile = av_get_profile_name(codec, avctx->profile);
                    if (profile)
                        ksymeta_set_string_l(stream_meta, KSYM_KEY_CODEC_PROFILE, profile);
                    if (codec->long_name)
                        ksymeta_set_string_l(stream_meta, KSYM_KEY_CODEC_LONG_NAME, codec->long_name);
                }
            }

            int64_t bitrate = get_bit_rate(avctx);
            if (bitrate > 0) {
                ksymeta_set_int64_l(stream_meta, KSYM_KEY_BITRATE, bitrate);
            }

            switch (avctx->codec_type) {
                case AVMEDIA_TYPE_VIDEO: {
                    ksymeta_set_string_l(stream_meta, KSYM_KEY_TYPE, KSYM_VAL_TYPE__VIDEO);

                    if (avctx->width > 0)
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_WIDTH, avctx->width);
                    if (avctx->height > 0)
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_HEIGHT, avctx->height);
                    if (st->sample_aspect_ratio.num > 0 && st->sample_aspect_ratio.den > 0) {
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_SAR_NUM, avctx->sample_aspect_ratio.num);
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_SAR_DEN, avctx->sample_aspect_ratio.den);
                    }

                    if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0) {
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_FPS_NUM, st->avg_frame_rate.num);
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_FPS_DEN, st->avg_frame_rate.den);
                    }
                    if (st->r_frame_rate.num > 0 && st->r_frame_rate.den > 0) {
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_TBR_NUM, st->avg_frame_rate.num);
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_TBR_DEN, st->avg_frame_rate.den);
                    }
                    break;
                }
                case AVMEDIA_TYPE_AUDIO: {
                    ksymeta_set_string_l(stream_meta, KSYM_KEY_TYPE, KSYM_VAL_TYPE__AUDIO);

                    if (avctx->sample_rate)
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_SAMPLE_RATE, avctx->sample_rate);
                    if (avctx->channel_layout)
                        ksymeta_set_int64_l(stream_meta, KSYM_KEY_CHANNEL_LAYOUT, avctx->channel_layout);
                    break;
                }
                default: {
                    ksymeta_set_string_l(stream_meta, KSYM_KEY_TYPE, KSYM_VAL_TYPE__UNKNOWN);
                    break;
                }
            }

            ksymeta_append_child_l(meta, stream_meta);
        }
    }
}

const char *ksymeta_get_string_l(KSYMediaMeta *meta, const char *name)
{
    if (!meta || !meta->dict)
        return NULL;

    AVDictionaryEntry *entry = av_dict_get(meta->dict, name, NULL, 0);
    if (!entry)
        return NULL;

    return entry->value;
}

int64_t ksymeta_get_int64_l(KSYMediaMeta *meta, const char *name, int64_t defaultValue)
{
    if (!meta || !meta->dict)
        return defaultValue;

    AVDictionaryEntry *entry = av_dict_get(meta->dict, name, NULL, 0);
    if (!entry || !entry->value)
        return defaultValue;

    return atoll(entry->value);
}

size_t ksymeta_get_children_count_l(KSYMediaMeta *meta)
{
    if (!meta || !meta->children)
        return 0;

    return meta->children_count;
}

KSYMediaMeta *ksymeta_get_child_l(KSYMediaMeta *meta, size_t index)
{
    if (!meta)
        return NULL;

    if (index >= meta->children_count)
        return NULL;

    return meta->children[index];
}
