/*
 * ksymeta.h
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

#ifndef KSYPLAYER__KSYMETA_H
#define KSYPLAYER__KSYMETA_H

#include <stdint.h>
#include <stdlib.h>

// media meta
#define KSYM_KEY_FORMAT         "format"
#define KSYM_KEY_DURATION_US    "duration_us"
#define KSYM_KEY_START_US       "start_us"
#define KSYM_KEY_BITRATE        "bitrate"
#define KSYM_KEY_VIDEO_STREAM   "video"
#define KSYM_KEY_AUDIO_STREAM   "audio"
#define KSYM_KEY_SOURCE_IP   "source_ip"
#define KSYM_KEY_PLAY_TYPE   "play_type"
#define KSYM_KEY_PROTOCOL_NAME "protocol_name"

// stream meta
#define KSYM_KEY_TYPE           "type"
#define KSYM_VAL_TYPE__VIDEO    "video"
#define KSYM_VAL_TYPE__AUDIO    "audio"
#define KSYM_VAL_TYPE__UNKNOWN  "unknown"

#define KSYM_KEY_CODEC_NAME      "codec_name"
#define KSYM_KEY_CODEC_PROFILE   "codec_profile"
#define KSYM_KEY_CODEC_LONG_NAME "codec_long_name"

// stream: video
#define KSYM_KEY_WIDTH          "width"
#define KSYM_KEY_HEIGHT         "height"
#define KSYM_KEY_FPS_NUM        "fps_num"
#define KSYM_KEY_FPS_DEN        "fps_den"
#define KSYM_KEY_TBR_NUM        "tbr_num"
#define KSYM_KEY_TBR_DEN        "tbr_den"
#define KSYM_KEY_SAR_NUM        "sar_num"
#define KSYM_KEY_SAR_DEN        "sar_den"
// stream: audio
#define KSYM_KEY_SAMPLE_RATE    "sample_rate"
#define KSYM_KEY_CHANNEL_LAYOUT "channel_layout"

// reserved for user
#define KSYM_KEY_STREAMS        "streams"


typedef struct KSYMediaMeta KSYMediaMeta;
typedef struct AVFormatContext AVFormatContext;

KSYMediaMeta *ksymeta_create();
void ksymeta_destroy(KSYMediaMeta *meta);
void ksymeta_destroy_p(KSYMediaMeta **meta);

void ksymeta_lock(KSYMediaMeta *meta);
void ksymeta_unlock(KSYMediaMeta *meta);

void ksymeta_append_child_l(KSYMediaMeta *meta, KSYMediaMeta *child);
void ksymeta_set_int64_l(KSYMediaMeta *meta, const char *name, int64_t value);
void ksymeta_set_string_l(KSYMediaMeta *meta, const char *name, const char *value);
void ksymeta_set_avformat_context_l(KSYMediaMeta *meta, AVFormatContext *ic);

// must be freed with free();
const char   *ksymeta_get_string_l(KSYMediaMeta *meta, const char *name);
int64_t       ksymeta_get_int64_l(KSYMediaMeta *meta, const char *name, int64_t defaultValue);
size_t        ksymeta_get_children_count_l(KSYMediaMeta *meta);
// do not free
KSYMediaMeta *ksymeta_get_child_l(KSYMediaMeta *meta, size_t index);

#endif//KSYPLAYER__KSYMETA_H
