/*
 * ksyplayer.h
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

#ifndef KSYPLAYER_ANDROID__KSYPLAYER_H
#define KSYPLAYER_ANDROID__KSYPLAYER_H

#include <stdbool.h>
#include "ff_ffmsg_queue.h"

#include "ksyutil/ksyutil.h"
#include "ksymeta.h"

#ifndef MPTRACE
#define MPTRACE ALOGW
#endif

typedef struct KSYMediaPlayer KSYMediaPlayer;
typedef struct FFPlayer FFPlayer;
typedef struct SDL_Vout SDL_Vout;

/*-
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_IDLE);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_INITIALIZED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_ASYNC_PREPARING);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_PREPARED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_STARTED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_PAUSED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_COMPLETED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_STOPPED);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_ERROR);
 MPST_CHECK_NOT_RET(mp->mp_state, MP_STATE_END);
 */

/*-
 * ksymp_set_data_source()  -> MP_STATE_INITIALIZED
 *
 * ksymp_reset              -> self
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_IDLE               0

/*-
 * ksymp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_INITIALIZED        1

/*-
 *                   ...    -> MP_STATE_PREPARED
 *                   ...    -> MP_STATE_ERROR
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_ASYNC_PREPARING    2

/*-
 * ksymp_seek_to()          -> self
 * ksymp_start()            -> MP_STATE_STARTED
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_PREPARED           3

/*-
 * ksymp_seek_to()          -> self
 * ksymp_start()            -> self
 * ksymp_pause()            -> MP_STATE_PAUSED
 * ksymp_stop()             -> MP_STATE_STOPPED
 *                   ...    -> MP_STATE_COMPLETED
 *                   ...    -> MP_STATE_ERROR
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_STARTED            4

/*-
 * ksymp_seek_to()          -> self
 * ksymp_start()            -> MP_STATE_STARTED
 * ksymp_pause()            -> self
 * ksymp_stop()             -> MP_STATE_STOPPED
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_PAUSED             5

/*-
 * ksymp_seek_to()          -> self
 * ksymp_start()            -> MP_STATE_STARTED (from beginning)
 * ksymp_pause()            -> self
 * ksymp_stop()             -> MP_STATE_STOPPED
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_COMPLETED          6

/*-
 * ksymp_stop()             -> self
 * ksymp_prepare_async()    -> MP_STATE_ASYNC_PREPARING
 *
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_STOPPED            7

/*-
 * ksymp_reset              -> MP_STATE_IDLE
 * ksymp_release            -> MP_STATE_END
 */
#define MP_STATE_ERROR              8

// add by yangle2@kingsoft.com, 2015-9-17
// description:add the start release state
// ID: KSYP-5	     
/*-
 * start ksymp_release            -> self
 */
#define MP_STATE_RELEASING                9

/*-
 * ksymp_release   end         -> self
 */
#define MP_STATE_END                10



#define KSYMP_IO_STAT_READ 1



void            ksymp_global_init();
void            ksymp_global_uninit();
void            ksymp_global_set_log_report(int use_report);
void            ksymp_io_stat_register(void (*cb)(const char *url, int type, int bytes));
void            ksymp_io_stat_complete_register(void (*cb)(const char *url,
                                                           int64_t read_bytes, int64_t total_size,
                                                           int64_t elpased_time, int64_t total_duration));

// ref_count is 1 after open
KSYMediaPlayer *ksymp_create(int (*msg_loop)(void*));
void            ksymp_set_format_callback(KSYMediaPlayer *mp, ksy_format_control_message cb, void *opaque);
void            ksymp_set_format_option(KSYMediaPlayer *mp, const char *name, const char *value);
void            ksymp_set_codec_option(KSYMediaPlayer *mp, const char *name, const char *value);
void            ksymp_set_sws_option(KSYMediaPlayer *mp, const char *name, const char *value);
void            ksymp_set_overlay_format(KSYMediaPlayer *mp, int chroma_fourcc);
void            ksymp_set_picture_queue_capicity(KSYMediaPlayer *mp, int frame_count);
void            ksymp_set_max_fps(KSYMediaPlayer *mp, int max_fps);
void            ksymp_set_framedrop(KSYMediaPlayer *mp, int framedrop);
/**
 * add by gs
**/
void            ksymp_set_nobuffer(KSYMediaPlayer *mp, int nobuffer);
void            ksymp_set_analyzeduration(KSYMediaPlayer *mp, int duration);
void            ksymp_set_audioamplify(KSYMediaPlayer *mp, float vol);
void            ksymp_set_videorate(KSYMediaPlayer *mp, float rate);
void            ksymp_set_buffersize(KSYMediaPlayer *mp, int size);
void            ksymp_get_picture(KSYMediaPlayer *mp, char* buf,int width, int height, int stride);
void            ksymp_set_drmkey(KSYMediaPlayer *mp, const char* version, const char* key);
void            ksymp_set_timeout(KSYMediaPlayer *mp, int size);
void            ksymp_set_localdir(KSYMediaPlayer *mp, const char* localdir);
/**
 * end by gs
**/
int             ksymp_get_video_codec_info(KSYMediaPlayer *mp, char **codec_info);
int             ksymp_get_audio_codec_info(KSYMediaPlayer *mp, char **codec_info);

// must be freed with free();
KSYMediaMeta   *ksymp_get_meta_l(KSYMediaPlayer *mp);

// preferred to be called explicity, can be called multiple times
// NOTE: ksymp_shutdown may block thread
void            ksymp_shutdown(KSYMediaPlayer *mp);

void            ksymp_inc_ref(KSYMediaPlayer *mp);

void            ksymp_change_state_l(KSYMediaPlayer *mp, int new_state);
// call close at last release, also free memory
// NOTE: ksymp_dec_ref may block thread
void            ksymp_dec_ref(KSYMediaPlayer *mp);
void            ksymp_dec_ref_p(KSYMediaPlayer **pmp);

int             ksymp_set_data_source(KSYMediaPlayer *mp, const char *url, const char *header);
int             ksymp_prepare_async(KSYMediaPlayer *mp);
int             ksymp_start(KSYMediaPlayer *mp);
int             ksymp_pause(KSYMediaPlayer *mp);
int             ksymp_stop(KSYMediaPlayer *mp);
int             ksymp_seek_to(KSYMediaPlayer *mp, long msec);
int             ksymp_get_state(KSYMediaPlayer *mp);
bool            ksymp_is_playing(KSYMediaPlayer *mp);
long            ksymp_get_current_position(KSYMediaPlayer *mp);
long            ksymp_get_duration(KSYMediaPlayer *mp);
long            ksymp_get_playable_duration(KSYMediaPlayer *mp);

void           *ksymp_get_weak_thiz(KSYMediaPlayer *mp);
void           *ksymp_set_weak_thiz(KSYMediaPlayer *mp, void *weak_thiz);

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int             ksymp_get_msg(KSYMediaPlayer *mp, AVMessage *msg, int block);

void ksymp_set_use_low_latency(KSYMediaPlayer *mp,int benable, int max_ms, int min_ms);

#endif
