/*
 * ksyplayer.c
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

#include "ksyplayer.h"

#include "ksyplayer_internal.h"

//#include <utils/CallStack.h>

#define MP_RET_IF_FAILED(ret) \
    do { \
        int retval = ret; \
        if (retval != 0) return (retval); \
    } while(0)

#define MPST_RET_IF_EQ_INT(real, expected, errcode) \
    do { \
        if ((real) == (expected)) return (errcode); \
    } while(0)

#define MPST_RET_IF_EQ(real, expected) \
    MPST_RET_IF_EQ_INT(real, expected, EKSY_INVALID_STATE)

inline static void ksymp_destroy(KSYMediaPlayer *mp)
{
    if (!mp)
        return;
    MPTRACE("[%s:%d]",__FUNCTION__,__LINE__);
    ffp_destroy_p(&mp->ffplayer);

    pthread_mutex_destroy(&mp->mutex);

    av_freep(&mp->data_source);
    memset(mp, 0, sizeof(KSYMediaPlayer));
    av_freep(&mp);
}

inline static void ksymp_destroy_p(KSYMediaPlayer **pmp)
{
    if (!pmp)
        return;

    MPTRACE("[%s:%d]",__FUNCTION__,__LINE__);
    ksymp_destroy(*pmp);
    *pmp = NULL;
}

void ksymp_global_init()
{
    ffp_global_init();
}

void ksymp_global_uninit()
{
    ffp_global_uninit();
}

void ksymp_global_set_log_report(int use_report)
{
    ffp_global_set_log_report(use_report);
}

void ksymp_io_stat_register(void (*cb)(const char *url, int type, int bytes))
{
    ffp_io_stat_register(cb);
}

void ksymp_io_stat_complete_register(void (*cb)(const char *url,
                                                int64_t read_bytes, int64_t total_size,
                                                int64_t elpased_time, int64_t total_duration))
{
    ffp_io_stat_complete_register(cb);
}

void ksymp_change_state_l(KSYMediaPlayer *mp, int new_state)
{
    mp->mp_state = new_state;
    MPTRACE("[%s:%d]mp->mp_state=%d\n", __FUNCTION__, __LINE__,mp->mp_state);
    ffp_notify_msg2(mp->ffplayer, FFP_MSG_PLAYBACK_STATE_CHANGED,new_state);
}

KSYMediaPlayer *ksymp_create(int (*msg_loop)(void*))
{
//#ifdef _ARM_
//    LOGW("name=%s", name);
//    android::CallStack stack;
//    stack.update(1, 100);
//    stack.dump("");
//#endif

    KSYMediaPlayer *mp = (KSYMediaPlayer *) av_mallocz(sizeof(KSYMediaPlayer));
    if (!mp)
        goto fail;

    mp->ffplayer = ffp_create();
    if (!mp)
        goto fail;

    mp->msg_loop = msg_loop;

    ksymp_inc_ref(mp);
    pthread_mutex_init(&mp->mutex, NULL);

    return mp;

    fail:
    ksymp_destroy_p(&mp);
    return NULL;
}

void ksymp_set_overlay_format(KSYMediaPlayer *mp, int chroma_fourcc)
{
    if (!mp)
        return;

    MPTRACE("ksymp_set_overlay_format(%.4s(0x%x))\n", (char*)&chroma_fourcc, chroma_fourcc);
    if (mp->ffplayer) {
        ffp_set_overlay_format(mp->ffplayer, chroma_fourcc);
    }
    MPTRACE("ksymp_set_overlay_format()=void\n");
}

void ksymp_set_format_callback(KSYMediaPlayer *mp, ksy_format_control_message cb, void *opaque)
{
    assert(mp);

    MPTRACE("ksymp_set_format_callback(%p, %p)\n", cb, opaque);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_format_callback(mp->ffplayer, cb, opaque);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_format_callback()=void\n");
}

void ksymp_set_format_option(KSYMediaPlayer *mp, const char *name, const char *value)
{
    assert(mp);

    MPTRACE("ksymp_set_format_option(%s, %s)\n", name, value);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_format_option(mp->ffplayer, name, value);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_format_option()=void\n");
}

void ksymp_set_codec_option(KSYMediaPlayer *mp, const char *name, const char *value)
{
    assert(mp);

    MPTRACE("ksymp_set_codec_option()\n");
    pthread_mutex_lock(&mp->mutex);
    ffp_set_codec_option(mp->ffplayer, name, value);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_codec_option()=void\n");
}

void ksymp_set_sws_option(KSYMediaPlayer *mp, const char *name, const char *value)
{
    assert(mp);

    MPTRACE("ksymp_set_sws_option()\n");
    pthread_mutex_lock(&mp->mutex);
    ffp_set_sws_option(mp->ffplayer, name, value);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_sws_option()=void\n");
}

void ksymp_set_picture_queue_capicity(KSYMediaPlayer *mp, int frame_count)
{
    assert(mp);

    MPTRACE("ksymp_set_picture_queue_capicity(%d)\n", frame_count);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_picture_queue_capicity(mp->ffplayer, frame_count);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_picture_queue_capicity()=void\n");
}

void ksymp_set_max_fps(KSYMediaPlayer *mp, int max_fps)
{
    assert(mp);

    MPTRACE("ksymp_set_max_fp(%d)\n", max_fps);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_max_fps(mp->ffplayer, max_fps);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_max_fp()=void\n");
}

void ksymp_set_framedrop(KSYMediaPlayer *mp, int framedrop)
{
    assert(mp);

    MPTRACE("ksymp_set_framedrop(%d)\n", framedrop);
    pthread_mutex_lock(&mp->mutex);
    ffp_set_framedrop(mp->ffplayer, framedrop);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_framedrop()=void\n");
}

/**
 * add by gs
**/

void ksymp_set_nobuffer(KSYMediaPlayer *mp, int nobuffer)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_nobuffer(mp->ffplayer, nobuffer);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_analyzeduration(KSYMediaPlayer *mp, int duration)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_analyzeduration(mp->ffplayer, duration);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_audioamplify(KSYMediaPlayer *mp, float volume)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_audioamplify(mp->ffplayer, volume);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_videorate(KSYMediaPlayer *mp, float rate)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_videorate(mp->ffplayer, rate);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_buffersize(KSYMediaPlayer *mp, int size)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_buffersize(mp->ffplayer, size);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_get_picture(KSYMediaPlayer *mp, char* buf,int width, int height, int stride)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    video_screenshot(mp->ffplayer, buf,width,height,stride);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_drmkey(KSYMediaPlayer *mp, const char* version, const char* key)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_drm_key(mp->ffplayer, version,key);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_timeout(KSYMediaPlayer *mp, int size)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_timeout(mp->ffplayer, size);
    pthread_mutex_unlock(&mp->mutex);
}

void ksymp_set_localdir(KSYMediaPlayer *mp, const char* localdir)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    set_localdir(mp->ffplayer, localdir);
    pthread_mutex_unlock(&mp->mutex);
}
/**
 * end by gs
**/

int ksymp_get_video_codec_info(KSYMediaPlayer *mp, char **codec_info)
{
    assert(mp);

    MPTRACE("%s\n", __func__);
    pthread_mutex_lock(&mp->mutex);
    int ret = ffp_get_video_codec_info(mp->ffplayer, codec_info);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("%s()=void\n", __func__);
    return ret;
}

int ksymp_get_audio_codec_info(KSYMediaPlayer *mp, char **codec_info)
{
    assert(mp);

    MPTRACE("%s\n", __func__);
    pthread_mutex_lock(&mp->mutex);
    int ret = ffp_get_audio_codec_info(mp->ffplayer, codec_info);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("%s()=void\n", __func__);
    return ret;
}

KSYMediaMeta *ksymp_get_meta_l(KSYMediaPlayer *mp)
{
    assert(mp);

    MPTRACE("%s\n", __func__);
    KSYMediaMeta *ret = ffp_get_meta_l(mp->ffplayer);
    MPTRACE("%s()=void\n", __func__);
    return ret;
}

void ksymp_shutdown_l(KSYMediaPlayer *mp)
{
    assert(mp);
    ksymp_change_state_l(mp, MP_STATE_RELEASING);
    MPTRACE("ksymp_shutdown_l()\n");
    if (mp->ffplayer) {
        ffp_stop_l(mp->ffplayer);
        ffp_wait_stop_l(mp->ffplayer);
    }
    ksymp_change_state_l(mp, MP_STATE_END);
    MPTRACE("ksymp_shutdown_l()=void\n");
}

void ksymp_shutdown(KSYMediaPlayer *mp)
{
    if(mp->mp_state != MP_STATE_END && mp->mp_state != MP_STATE_RELEASING)
    	return ksymp_shutdown_l(mp);
    else
	MPTRACE("[%s:%d]mp_state=%d\n", __FUNCTION__,__LINE__,mp->mp_state);
}

void ksymp_inc_ref(KSYMediaPlayer *mp)
{
    assert(mp);
    __sync_fetch_and_add(&mp->ref_count, 1);
    MPTRACE("[%s:%d]ksymp increase the refer count. ref_count=%d, mp=0x%x\n",__FUNCTION__,__LINE__,mp->ref_count,mp);
}

void ksymp_dec_ref(KSYMediaPlayer *mp)
{
    if (!mp)
        return;

    int ref_count = __sync_sub_and_fetch(&mp->ref_count, 1);
    MPTRACE("ksymp decrease the refer count. ref_count=%d, mp=0x%x\n",ref_count,mp);
    if (ref_count == 0) {
        MPTRACE("ksymp_dec_ref(): ref=0\n");
        ksymp_shutdown(mp);
        ksymp_destroy_p(&mp);
    }
}

void ksymp_dec_ref_p(KSYMediaPlayer **pmp)
{
    if (!pmp)
        return;

    ksymp_dec_ref(*pmp);
    *pmp = NULL;
}

static int ksymp_set_data_source_l(KSYMediaPlayer *mp, const char *url, const char *header)
{
    assert(mp);
    assert(url);

    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    av_freep(&mp->data_source);
    mp->data_source = av_strdup(url);
    if (!mp->data_source)
        return EKSY_OUT_OF_MEMORY;

    if(header != NULL){
    	av_freep(&mp->header);
    	mp->header = av_strdup(header);
    	if (!mp->header)
       	return EKSY_OUT_OF_MEMORY;
    }

    ksymp_change_state_l(mp, MP_STATE_INITIALIZED);
    return 0;
}

int ksymp_set_data_source(KSYMediaPlayer *mp, const char *url, const char *header)
{
//#ifdef _ARM_
//    LOGW("name=%s", name);
//    android::CallStack stack;
//    stack.update(1, 100);
//    stack.dump("");
//#endif

    assert(mp);
    assert(url);
    MPTRACE("ksymp_set_data_source(url=\"%s\")\n", url);
    pthread_mutex_lock(&mp->mutex);
    int retval = ksymp_set_data_source_l(mp, url, header);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_set_data_source(url=\"%s\")=%d\n", url, retval);
    return retval;
}

static int ksymp_prepare_async_l(KSYMediaPlayer *mp)
{
    assert(mp);

    MPTRACE("[%s:%d]mp_state=%d\n", __FUNCTION__, __LINE__,mp->mp_state);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    assert(mp->data_source);

    ksymp_change_state_l(mp, MP_STATE_ASYNC_PREPARING);

    msg_queue_start(&mp->ffplayer->msg_queue);

    // released in msg_loop
    ksymp_inc_ref(mp);
    mp->msg_thread = SDL_CreateThreadEx(&mp->_msg_thread, mp->msg_loop, mp, "ff_msg_loop");
    // TODO: 9 release weak_thiz if pthread_create() failed;

    int retval = ffp_prepare_async_l(mp->ffplayer, mp->data_source, mp->header);
    if (retval < 0) {
        ksymp_change_state_l(mp, MP_STATE_ERROR);
        return retval;
    }

    return 0;
}

int ksymp_prepare_async(KSYMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ksymp_prepare_async()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ksymp_prepare_async_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_prepare_async()=%d\n", retval);
    return retval;
}

static int ksymp_chkst_start_l(int mp_state)
{
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ksymp_start_l(KSYMediaPlayer *mp)
{
    assert(mp);

    MP_RET_IF_FAILED(ksymp_chkst_start_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_START);

    return 0;
}

int ksymp_start(KSYMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ksymp_start()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ksymp_start_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_start()=%d\n", retval);
    return retval;
}

static int ksymp_chkst_pause_l(int mp_state)
{
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

static int ksymp_pause_l(KSYMediaPlayer *mp)
{
    assert(mp);

    MP_RET_IF_FAILED(ksymp_chkst_pause_l(mp->mp_state));

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    ffp_notify_msg1(mp->ffplayer, FFP_REQ_PAUSE);

    return 0;
}

int ksymp_pause(KSYMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ksymp_pause()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ksymp_pause_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_pause()=%d\n", retval);
    return retval;
}

static int ksymp_stop_l(KSYMediaPlayer *mp)
{
    assert(mp);

    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_INITIALIZED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_COMPLETED);
    // MPST_RET_IF_EQ(mp->mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp->mp_state, MP_STATE_END);

    ffp_remove_msg(mp->ffplayer, FFP_REQ_START);
    ffp_remove_msg(mp->ffplayer, FFP_REQ_PAUSE);
    int retval = ffp_stop_l(mp->ffplayer);
    if (retval < 0) {
        return retval;
    }

    ksymp_change_state_l(mp, MP_STATE_STOPPED);
    return 0;
}

int ksymp_stop(KSYMediaPlayer *mp)
{
    assert(mp);
    MPTRACE("ksymp_stop()\n");
    pthread_mutex_lock(&mp->mutex);
    int retval = ksymp_stop_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_stop()=%d\n", retval);
    return retval;
}

bool ksymp_is_playing(KSYMediaPlayer *mp)
{
    assert(mp);
    if (mp->mp_state == MP_STATE_PREPARED ||
        mp->mp_state == MP_STATE_STARTED) {
        return true;
    }

    return false;
}

static int ksymp_chkst_seek_l(int mp_state)
{
    MPST_RET_IF_EQ(mp_state, MP_STATE_IDLE);
    MPST_RET_IF_EQ(mp_state, MP_STATE_INITIALIZED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_ASYNC_PREPARING);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PREPARED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_STARTED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_PAUSED);
    // MPST_RET_IF_EQ(mp_state, MP_STATE_COMPLETED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_STOPPED);
    MPST_RET_IF_EQ(mp_state, MP_STATE_ERROR);
    MPST_RET_IF_EQ(mp_state, MP_STATE_END);

    return 0;
}

int ksymp_seek_to_l(KSYMediaPlayer *mp, long msec)
{
    assert(mp);

    MP_RET_IF_FAILED(ksymp_chkst_seek_l(mp->mp_state));

    mp->seek_req = 1;
    mp->seek_msec = msec;
    ffp_remove_msg(mp->ffplayer, FFP_REQ_SEEK);
    ffp_notify_msg2(mp->ffplayer, FFP_REQ_SEEK, (int)msec);
    // TODO: 9 64-bit long?

    return 0;
}

int ksymp_seek_to(KSYMediaPlayer *mp, long msec)
{
    assert(mp);
    MPTRACE("ksymp_seek_to(%ld)\n", msec);
    pthread_mutex_lock(&mp->mutex);
    int retval = ksymp_seek_to_l(mp, msec);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_seek_to(%ld)=%d\n", msec, retval);

    return retval;
}

int ksymp_get_state(KSYMediaPlayer *mp)
{
    return mp->mp_state;
}

static long ksymp_get_current_position_l(KSYMediaPlayer *mp)
{
    return ffp_get_current_position_l(mp->ffplayer);
}

long ksymp_get_current_position(KSYMediaPlayer *mp)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    long retval;
    if (mp->seek_req)
        retval = mp->seek_msec;
    else
        retval = ksymp_get_current_position_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static long ksymp_get_duration_l(KSYMediaPlayer *mp)
{
    return ffp_get_duration_l(mp->ffplayer);
}

long ksymp_get_duration(KSYMediaPlayer *mp)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    long retval = ksymp_get_duration_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

static long ksymp_get_playable_duration_l(KSYMediaPlayer *mp)
{
    return ffp_get_playable_duration_l(mp->ffplayer);
}

long ksymp_get_playable_duration(KSYMediaPlayer *mp)
{
    assert(mp);
    pthread_mutex_lock(&mp->mutex);
    long retval = ksymp_get_playable_duration_l(mp);
    pthread_mutex_unlock(&mp->mutex);
    return retval;
}

void *ksymp_get_weak_thiz(KSYMediaPlayer *mp)
{
    return mp->weak_thiz;
}

void *ksymp_set_weak_thiz(KSYMediaPlayer *mp, void *weak_thiz)
{
    void *prev_weak_thiz = mp->weak_thiz;

    mp->weak_thiz = weak_thiz;

    return prev_weak_thiz;
}

int ksymp_get_msg(KSYMediaPlayer *mp, AVMessage *msg, int block)
{
    assert(mp);
    while (1) {
        int continue_wait_next_msg = 0;
        int retval = msg_queue_get(&mp->ffplayer->msg_queue, msg, block);
        if (retval <= 0)
            return retval;

        switch (msg->what) {
        case FFP_MSG_PREPARED:
            MPTRACE("ksymp_get_msg: FFP_MSG_PREPARED\n");
            pthread_mutex_lock(&mp->mutex);
            if (mp->mp_state == MP_STATE_ASYNC_PREPARING) {
                ksymp_change_state_l(mp, MP_STATE_PREPARED);
            } else {
                // FIXME: 1: onError() ?
                ALOGE("FFP_MSG_PREPARED: expecting mp_state==MP_STATE_ASYNC_PREPARING\n");
            }
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_MSG_COMPLETED:
            MPTRACE("ksymp_get_msg: FFP_MSG_COMPLETED\n");

            pthread_mutex_lock(&mp->mutex);
            mp->restart_from_beginning = 1;
            ksymp_change_state_l(mp, MP_STATE_COMPLETED);
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_MSG_SEEK_COMPLETE:
            MPTRACE("ksymp_get_msg: FFP_MSG_SEEK_COMPLETE\n");

            pthread_mutex_lock(&mp->mutex);
            mp->seek_req = 0;
            mp->seek_msec = 0;
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_REQ_START:
            MPTRACE("ksymp_get_msg: FFP_REQ_START\n");
            continue_wait_next_msg = 1;
            pthread_mutex_lock(&mp->mutex);
            if (0 == ksymp_chkst_start_l(mp->mp_state)) {
                // FIXME: 8 check seekable
                if (mp->mp_state == MP_STATE_COMPLETED) {
                    if (mp->restart_from_beginning) {
                        ALOGD("ksymp_get_msg: FFP_REQ_START: restart from beginning\n");
                        retval = ffp_start_from_l(mp->ffplayer, 0);
                        if (retval == 0)
                            ksymp_change_state_l(mp, MP_STATE_STARTED);
                    } else {
                        ALOGD("ksymp_get_msg: FFP_REQ_START: restart from seek pos\n");
                        retval = ffp_start_l(mp->ffplayer);
                        if (retval == 0)
                            ksymp_change_state_l(mp, MP_STATE_STARTED);
                    }
                    mp->restart_from_beginning = 0;
                } else {
                    ALOGD("ksymp_get_msg: FFP_REQ_START: start on fly\n");
                    retval = ffp_start_l(mp->ffplayer);
                    if (retval == 0)
                        ksymp_change_state_l(mp, MP_STATE_STARTED);
                }
            }
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_REQ_PAUSE:
            MPTRACE("ksymp_get_msg: FFP_REQ_PAUSE\n");
            continue_wait_next_msg = 1;
            pthread_mutex_lock(&mp->mutex);
            if (0 == ksymp_chkst_pause_l(mp->mp_state)) {
                int pause_ret = ffp_pause_l(mp->ffplayer);
                if (pause_ret == 0)
                    ksymp_change_state_l(mp, MP_STATE_PAUSED);
            }
            pthread_mutex_unlock(&mp->mutex);
            break;

        case FFP_REQ_SEEK:
            MPTRACE("ksymp_get_msg: FFP_REQ_SEEK\n");
            continue_wait_next_msg = 1;

            pthread_mutex_lock(&mp->mutex);
            if (0 == ksymp_chkst_seek_l(mp->mp_state)) {
                if (0 == ffp_seek_to_l(mp->ffplayer, msg->arg1)) {
                    ALOGD("ksymp_get_msg: FFP_REQ_SEEK: seek to %d\n", (int)msg->arg1);
                    mp->restart_from_beginning = 0;
                }
            }
            pthread_mutex_unlock(&mp->mutex);
            break;
        }

        if (continue_wait_next_msg)
            continue;

        return retval;
    }

    return -1;
}
void ksymp_set_use_low_latency(KSYMediaPlayer *mp,int benable,int max_ms, int min_ms)
{
    if (!mp)
        return;

    if (mp->ffplayer) {
        ffp_set_use_low_latency(mp->ffplayer,benable,max_ms,min_ms);
    }
}