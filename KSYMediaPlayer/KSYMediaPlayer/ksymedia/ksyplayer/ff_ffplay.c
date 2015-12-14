/* * ffplay_def.c *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of KSYPlayer.
 *
 * KSYPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * KSYPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with KSYPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ff_ffplay.h"

#if CONFIG_AVFILTER
# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif

#include <math.h>
#include "ff_cmdutils.h"
#include "ff_fferror.h"
#include "ff_ffpipeline.h"
#include "ff_ffpipenode.h"
#include "ksymeta.h"
#include "KSY_Drm.h"

// FIXME: 9 work around NDKr8e or gcc4.7 bug
// isnan() may not recognize some double NAN, so we test both double and float
#if defined(__ANDROID__)
#ifdef isnan
#undef isnan
#endif
#define isnan(x) (isnan((double)(x)) || isnanf((float)(x)))
#endif

#if defined(__ANDROID__)
#define printf(...) ALOGD(__VA_ARGS__)
#endif

#define FFP_LATENCY_CACHE_MAXSIZE  38
#define FFP_LATENCY_CACHE_MINSIZE  8


#define FFP_COMPUTE_VIDEO_DURATION_SPAN  6

#define FFP_XPS_PERIOD (3)

// #define FFP_SHOW_DEMUX_CACHE
// #define FFP_SHOW_BUF_POS
// #define FFP_SHOW_PKT_RECYCLE

// #define FFP_NOTIFY_BUF_TIME
#define FFP_NOTIFY_BUF_BYTES

#define FFP_IO_STAT_STEP (50 * 1024)

#ifdef FFP_SHOW_VDPS
static int g_vdps_counter = 0;
static int64_t g_vdps_total_time = 0;
#endif

static int ffp_format_control_message(struct AVFormatContext *s, int type,
                                      void *data, size_t data_size);


#define FFP_BUF_MSG_PERIOD (3)

static AVPacket flush_pkt;
static FFPlayer *player_handle = NULL;

// FFP_MERGE: cmp_audio_fmts
// FFP_MERGE: get_valid_channel_layout

static void free_picture(Frame *vp);

static void log_callback_func(void *ptr, int level, const char *fmt, va_list vl)
{
    char buffer[2048] = {0};
    vsprintf(buffer, fmt, vl);
    switch(level){
	case AV_LOG_PANIC:
	case AV_LOG_FATAL:
	case AV_LOG_ERROR:
		ALOGE("%s", buffer);
		break;
	case AV_LOG_WARNING:
		ALOGW("%s", buffer);
		break;
	case AV_LOG_INFO:
		ALOGI("%s", buffer);
		break;
	//case AV_LOG_DEBUG:
		//ALOGD("%s", buffer);
		//break;
	default:
	;
		//ALOGV("%s", buffer);	
    }
}

static void ff_event_cb(void *ffp, int what, int arg1, int arg2) 
{
   ALOGE("[%s:%d]what=%d,arg1=%d",__FUNCTION__,__LINE__,what,arg1);
   FFPlayer *lffp = (FFPlayer *)ffp;
   ffp_notify_msg3(lffp,what,arg1,arg2);
}

static void flux_speed(void *ffp, double speed)
 {
   FFPlayer *lffp = (FFPlayer *)ffp;
   if(lffp != NULL)
   	ffp_notify_msg3(lffp,FFP_MSG_SPEED,803,speed);
   //ALOGE("realtime calc net speed:%f",speed);
}

// add by yangle2@kingsoft.com, 2015-9-14
// description: return the duration cache in demux packet queue and decode sample queue
// ID: KSYP-2 
static double get_stream_cache_time_ms(FFPlayer *ffp, int stream_type)
{
    	VideoState *is            = ffp->is;
    	int audio_time_base_valid = is->audio_st && is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    	int video_time_base_valid = is->video_st && is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;

	if(stream_type == AVMEDIA_TYPE_VIDEO){
		int64_t video_cached_duration = -1;
       	if (is->video_st && video_time_base_valid) {
            		video_cached_duration = (is->videoq.duration + is->pictq.duration) * av_q2d(is->video_st->time_base) * 1000;
        	}
		return video_cached_duration;
	}
	else{
		int64_t audio_cached_duration = -1;
       	if (is->audio_st && audio_time_base_valid) {
            		audio_cached_duration = (is->audioq.duration + is->sampq.duration) * av_q2d(is->audio_st->time_base) * 1000;
        	}
		return audio_cached_duration;
	}

	return -1;
}

// add by yangle2@kingsoft.com, 2015-9-23
// description: add the buffering machine, when the receive buffer underflow
// ID: KSYP-9
static double get_media_cache_time_ms(FFPlayer *ffp, int play_mode){
	double video_cache_time = get_stream_cache_time_ms(ffp,AVMEDIA_TYPE_VIDEO);
	double audio_cache_time = get_stream_cache_time_ms(ffp,AVMEDIA_TYPE_AUDIO);

	if(play_mode == AV_SYNC_AUDIO_MASTER && audio_cache_time>=0)
		return audio_cache_time;	
       if(play_mode == AV_SYNC_VIDEO_MASTER && video_cache_time>=0)
		return video_cache_time;
	else if(video_cache_time>=0&&audio_cache_time>=0)
		return KSYMIN(video_cache_time,audio_cache_time);
	else if(audio_cache_time>=0)
		return audio_cache_time;
	else if(video_cache_time>=0)
		return video_cache_time;
	else 
		return -1;
}

#if CONFIG_AVFILTER
// FFP_MERGE: opt_add_vfilter
#endif

#if CONFIG_AVFILTER
static inline
int cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
                   enum AVSampleFormat fmt2, int64_t channel_count2)
{
    /* If channel count == 1, planar and non-planar formats are the same */
    if (channel_count1 == 1 && channel_count2 == 1)
        return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
    else
        return channel_count1 != channel_count2 || fmt1 != fmt2;
}

static inline
int64_t get_valid_channel_layout(int64_t channel_layout, int channels)
{
    if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels)
        return channel_layout;
    else
        return 0;
}
#endif

static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
    MyAVPacketList *pkt1;

    if (q->abort_request)
       return -1;

#ifdef FFP_MERGE
    pkt1 = av_malloc(sizeof(MyAVPacketList));
#else
    pkt1 = q->recycle_pkt;
    if (pkt1) {
        q->recycle_pkt = pkt1->next;
        q->recycle_count++;
    } else {
        q->alloc_count++;
        pkt1 = av_malloc(sizeof(MyAVPacketList));
    }
#ifdef FFP_SHOW_PKT_RECYCLE
    int total_count = q->recycle_count + q->alloc_count;
    if (!(total_count % 50)) {
        ALOGE("pkt-recycle \t%d + \t%d = \t%d\n", q->recycle_count, q->alloc_count, total_count);
    }
#endif
#endif
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    if (pkt == &flush_pkt){
        q->serial++;
	 ALOGI("[%s:%d]stream_index=%d, serial=%d\n", __FUNCTION__, __LINE__, pkt->stream_index,q->serial);
    }
    pkt1->serial = q->serial;

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    if (pkt1->pkt.duration > 0)
        q->duration += pkt1->pkt.duration;
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);
    return 0;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    int ret;

    /* duplicate the packet */
    if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    SDL_LockMutex(q->mutex);
    ret = packet_queue_put_private(q, pkt);
    SDL_UnlockMutex(q->mutex);

    if (pkt != &flush_pkt && ret < 0)
        av_free_packet(pkt);

    return ret;
}

static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1;
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    q->abort_request = 1;
}

static void packet_queue_flush(PacketQueue *q)
{
    MyAVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for (pkt = q->first_pkt; pkt; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
#ifdef FFP_MERGE
        av_freep(&pkt);
#else
        pkt->next = q->recycle_pkt;
        q->recycle_pkt = pkt;
#endif
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    SDL_UnlockMutex(q->mutex);
}

static void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);

    SDL_LockMutex(q->mutex);
    while(q->recycle_pkt) {
        MyAVPacketList *pkt = q->recycle_pkt;
        if (pkt)
            q->recycle_pkt = pkt->next;
        av_freep(&pkt);
    }
    SDL_UnlockMutex(q->mutex);

    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

static void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}

static void packet_queue_start(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);
    q->abort_request = 0;
    packet_queue_put_private(q, &flush_pkt);
    SDL_UnlockMutex(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    MyAVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            if (pkt1->pkt.duration > 0)
                q->duration -= pkt1->pkt.duration;
            *pkt = pkt1->pkt;
            if (serial)
                *serial = pkt1->serial;
#ifdef FFP_MERGE
            av_free(pkt1);
#else
            pkt1->next = q->recycle_pkt;
            q->recycle_pkt = pkt1;
#endif
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

static int packet_queue_get_or_buffering(FFPlayer *ffp, PacketQueue *q, AVPacket *pkt, int *serial, int *finished)
{
    assert(finished);
    while (1) {
        int new_packet = packet_queue_get(q, pkt, 0, serial);
        if (new_packet < 0)
            return -1;
        else if (new_packet == 0) {
            if (q->is_buffer_indicator && !*finished)
                ffp_toggle_buffering(ffp, 1);
            new_packet = packet_queue_get(q, pkt, 1, serial);
            if (new_packet < 0)
                return -1;
        }

        if (*finished == *serial) {
            av_free_packet(pkt);
            continue;
        }
        else
            break;
    }

    return 1;
}

static void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
    memset(d, 0, sizeof(Decoder));
    d->avctx = avctx;
    d->queue = queue;
    d->empty_queue_cond = empty_queue_cond;
    d->start_pts = AV_NOPTS_VALUE;
}

static int decoder_decode_frame(FFPlayer *ffp, Decoder *d, AVFrame *frame, AVSubtitle *sub) {
    int got_frame = 0;

    do {
        int ret = -1;

        if (d->queue->abort_request)
            return -1;

        if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
            AVPacket pkt;
            do {
                if (d->queue->nb_packets == 0)
                    SDL_CondSignal(d->empty_queue_cond);
//                if (packet_queue_get_or_buffering(ffp, d->queue, &pkt, &d->pkt_serial, &d->finished) < 0)
                if (packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial) < 0)
                    return -1;
                if (pkt.data == flush_pkt.data) {
                    avcodec_flush_buffers(d->avctx);
                    d->finished = 0;
                    d->next_pts = d->start_pts;
                    d->next_pts_tb = d->start_pts_tb;
		      ALOGI("[%s:%d]codec_type=%d, finished = 0\n", __FUNCTION__, __LINE__, d->avctx->codec_type);
                }
            } while (pkt.data == flush_pkt.data || d->queue->serial != d->pkt_serial);
            av_free_packet(&d->pkt);
            d->pkt_temp = d->pkt = pkt;
            d->packet_pending = 1;
        }

        switch (d->avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO: {
/**
 * add by gs
**/
                if(decode_drm_pkt(ffp, &d->pkt_temp)!=0) {
                    ret = -1;
                    break;
                }
/**
 * end by gs
**/
                ret = avcodec_decode_video2(d->avctx, frame, &got_frame, &d->pkt_temp);
                if (got_frame) {
                    if (ffp->decoder_reorder_pts == -1) {
                        frame->pts = av_frame_get_best_effort_timestamp(frame);
                    } else if (ffp->decoder_reorder_pts) {
                        frame->pts = frame->pkt_pts;
                    } else {
                        frame->pts = frame->pkt_dts;
                    }
					
			frame->decode_output_cc = ffp->is->decode_output_video_cc;
			ffp->is->decode_output_video_cc++;
                }
                }
                break;
            case AVMEDIA_TYPE_AUDIO:
                ret = avcodec_decode_audio4(d->avctx, frame, &got_frame, &d->pkt_temp);
                if (got_frame) {
                    AVRational tb = (AVRational){1, frame->sample_rate};
                    if (frame->pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(frame->pts, d->avctx->time_base, tb);
                    else if (frame->pkt_pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(frame->pkt_pts, av_codec_get_pkt_timebase(d->avctx), tb);
                    else if (d->next_pts != AV_NOPTS_VALUE)
                        frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
                    if (frame->pts != AV_NOPTS_VALUE) {
                        d->next_pts = frame->pts + frame->nb_samples;
                        d->next_pts_tb = tb;
                    }
					
			frame->decode_output_cc = ffp->is->decode_output_audio_cc;
			ffp->is->decode_output_audio_cc++;
                }
                break;
            // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
            default:
                break;
        }

        if (ret < 0) {
            d->packet_pending = 0;
        } else {
            d->pkt_temp.dts =
            d->pkt_temp.pts = AV_NOPTS_VALUE;
            if (d->pkt_temp.data) {
                if (d->avctx->codec_type != AVMEDIA_TYPE_AUDIO)
                    ret = d->pkt_temp.size;
                d->pkt_temp.data += ret;
                d->pkt_temp.size -= ret;
                if (d->pkt_temp.size <= 0)
                    d->packet_pending = 0;
            } else {
                if (!got_frame) {
                    d->packet_pending = 0;
                    d->finished = d->pkt_serial;
		      ALOGI("[%s:%d]codec_type=%d, queue finished=%d\n", __FUNCTION__, __LINE__, d->avctx->codec_type,d->finished);
                }
            }
        }
    } while (!got_frame && !d->finished);

    return got_frame;
}

static void decoder_destroy(Decoder *d) {
    av_free_packet(&d->pkt);
}

static void frame_queue_unref_item(Frame *vp)
{
    av_frame_unref(vp->frame);
    SDL_VoutUnrefYUVOverlay(vp->bmp);
#ifdef FFP_MERGE
    avsubtitle_free(&vp->sub);
#endif
}

static int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last)
{
    int i;
    memset(f, 0, sizeof(FrameQueue));
    if (!(f->mutex = SDL_CreateMutex()))
        return AVERROR(ENOMEM);
    if (!(f->cond = SDL_CreateCond()))
        return AVERROR(ENOMEM);
    f->pktq = pktq;
    f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
    f->keep_last = !!keep_last;
    for (i = 0; i < f->max_size; i++)
        if (!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    return 0;
}

static void frame_queue_destory(FrameQueue *f)
{
    int i;
    for (i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        frame_queue_unref_item(vp);
        av_frame_free(&vp->frame);
        free_picture(vp);
    }
    SDL_DestroyMutex(f->mutex);
    SDL_DestroyCond(f->cond);
}

static void frame_queue_signal(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static Frame *frame_queue_peek(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static Frame *frame_queue_peek_next(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

static Frame *frame_queue_peek_last(FrameQueue *f)
{
    return &f->queue[f->rindex];
}

static Frame *frame_queue_peek_writable(FrameQueue *f)
{
    /* wait until we have space to put a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size >= f->max_size &&
           !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[f->windex];
}

static Frame *frame_queue_peek_readable(FrameQueue *f)
{
    /* wait until we have a readable a new frame */
    SDL_LockMutex(f->mutex);
    while (f->size - f->rindex_shown <= 0 &&
           !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if (f->pktq->abort_request)
        return NULL;

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static void frame_queue_push(FrameQueue *f)
{
    int write_index = f->windex;
    double w_duration  = f->queue[write_index].duration;

    if (++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size++;
    f->duration += w_duration;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

static void frame_queue_next(FrameQueue *f)
{
    if (f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return;
    }
	
    int read_index = f->rindex;
    double r_duration  = f->queue[read_index].duration;
	
    frame_queue_unref_item(&f->queue[f->rindex]);
    if (++f->rindex == f->max_size)
        f->rindex = 0;
    SDL_LockMutex(f->mutex);
    f->size--;
    f->duration -= r_duration;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

/* jump back to the previous frame if available by resetting rindex_shown */
static int frame_queue_prev(FrameQueue *f)
{
    int ret = f->rindex_shown;
    f->rindex_shown = 0;
    return ret;
}

/* return the number of undisplayed frames in the queue */
static int frame_queue_nb_remaining(FrameQueue *f)
{
    return f->size - f->rindex_shown;
}

/* return last shown position */
#ifdef FFP_MERGE
static int64_t frame_queue_last_pos(FrameQueue *f)
{
    Frame *fp = &f->queue[f->rindex];
    if (f->rindex_shown && fp->serial == f->pktq->serial)
        return fp->pos;
    else
        return -1;
}
#endif

static void free_picture(Frame *vp)
{
    if (vp->bmp) {
        SDL_VoutFreeYUVOverlay(vp->bmp);
        vp->bmp = NULL;
    }
}

/**
 * add by gs
**/
void set_nobuffer(FFPlayer *ffp, int nobuffer)
{
       // ffp->nobuffer = nobuffer;
}

void set_analyzeduration(FFPlayer *ffp, int duration)
{
    if(duration<0)
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_WRONGPARAM,1);
    else
        ffp->analyzeduration = duration;
}

void set_audioamplify(FFPlayer *ffp, float volume)
{
    if(ffp->audioamplify)
            ffp->audioamplify->volume = volume;
    else
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_WRONGPARAM,2);
}

void set_videorate(FFPlayer *ffp, float rate)
{
    if(rate>0.1 && rate <=5)
        ffp->videorate = rate;
    else
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_WRONGPARAM,3);
}

void set_buffersize(FFPlayer *ffp, int size)
{
    if(size>10 && size < 100)
        ffp->max_buffer_size = size * 1024 * 1024;
    else
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_WRONGPARAM,4);
}

void set_timeout(FFPlayer *ffp, int timeout)
{
    if(timeout>0)
        ffp->timeout = timeout;
    else
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_WRONGPARAM,5);
}

void set_localdir(FFPlayer *ffp, const char* localdir)
{
    if(localdir!=NULL)
        ffp->localdir = strdup(localdir);
    else
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_WRONGPARAM,6);
}

void ffp_set_use_low_latency(FFPlayer *ffp, int benable, int max_ms, int min_ms){
	ffp->use_low_latency = benable;
	ffp->buffer_cache_maxtime_ms = max_ms;
	ffp->buffer_cache_mintime_ms = min_ms;
	ffp->buffer_cache_midtime_ms = (ffp->buffer_cache_maxtime_ms + ffp->buffer_cache_mintime_ms) / 2 ; 
	//ffp_set_framedrop(ffp, 1);
	av_log(NULL, AV_LOG_INFO, "[%s:%d]set low latency = %d, max=%d, min=%d, mid=%d, framedrop=%d .This mode only use in live stream\n", 
		__FUNCTION__,__LINE__,ffp->use_low_latency,ffp->buffer_cache_maxtime_ms,ffp->buffer_cache_mintime_ms, ffp->buffer_cache_midtime_ms, ffp->framedrop);

}

void fill_bitmap(char *pixels, AVFrame *pFrame,int width, int height, int stride)
{
    //ARGB
    uint8_t *frameLine;
    int  yy,xx;
    uint8_t*  line;
    if(pFrame->format==AV_PIX_FMT_RGBA)
    {
        for (yy = 0; yy < height; yy++)
        {
            line = (uint8_t*)pixels;
            frameLine = (uint8_t *)pFrame->data[0] + (yy * pFrame->linesize[0]);
            memcpy(line,frameLine,4*width);
            pixels = pixels + stride;
        }
    }
    else if(pFrame->format==AV_PIX_FMT_RGB24)
    {
        for (yy = 0; yy < height; yy++) {
            line = (uint8_t*)pixels;
            frameLine = (uint8_t *)pFrame->data[0] + (yy * pFrame->linesize[0]);
            for (xx = 0; xx < width; xx++) {
                line[0] = frameLine[0];
                line[1] = frameLine[1];
                line[2] = frameLine[2];
                line[3] = 255;
                line+=4;
                frameLine+=3;
            }
        pixels = pixels + stride;
        }
    }
}

void video_screenshot(FFPlayer *ffp, char* buf, int width, int height, int stride)
{
    VideoState *is = ffp->is;
    Frame *vp;
    AVFrame *frame = av_frame_alloc();

    vp = frame_queue_peek(&is->pictq);
     if (vp->bmp) {
            frame->format = AV_PIX_FMT_RGB24;//AV_PIX_FMT_RGBA;//AV_PIX_FMT_RGB24;//PIX_FMT_RGB24
            frame->width = vp->width;
            frame->height = vp->height;
            if(av_image_alloc(frame->data,frame->linesize, vp->width, vp->height,frame->format,16)<0) {
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_MEM,1);
                goto failed;
            }

            SDL_VoutLockYUVOverlay(vp->bmp);
            if (SDL_VoutFFmpeg_ConvertFrame_RGB(frame, vp->bmp, &is->img_convert_ctx_rgba, ffp->sws_flags) < 0) {
                av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_MEM,2);
                goto failed;
            }
            SDL_VoutUnlockYUVOverlay(vp->bmp);
            //av_log(NULL, AV_LOG_FATAL, "gs before write file width:%d height:%d stride:%d frame width:%d height:%d  format:%d\n",
             //      width,height,stride,frame->width,frame->height,frame->format);
            fill_bitmap(buf,frame,width,height,stride);
            //av_log(NULL, AV_LOG_FATAL, "after fill bitmap\n");
            av_freep(&frame->data[0]);
    }
failed:
    av_frame_free(&frame);
    return ;
}

int get_drm_key(void* thiz, char* version, int curflag)
{
    //av_log(NULL, AV_LOG_FATAL, "get_drm_key curflag %d, version:%s\n",curflag,version);
    FFPlayer *ffp = thiz;
    VideoState* is = ffp->is;
    AVFormatContext* ic = is->ic;
    SDL_LockMutex(is->drm_mutex);
    if(curflag)
    {
        if(ic->drm_version_next!=NULL && strcmp(version,ic->drm_version_next)==0)
        {
            if(ic->drm_version!=NULL)
                av_free(ic->drm_version);
            ic->drm_version = ic->drm_version_next;
            ic->drm_version_next = NULL;
            memcpy(ic->drm_key,ic->drm_key_next,16);
            ksy_set_key(is->drm_ctx, ic->drm_key,16);
            ic->drm_flags = 2;
            goto end;
        }
        else
        {
            if(ic->drm_version!=NULL)
            {
                if(strcmp(version,ic->drm_version)==0)
                {
                    ic->drm_flags = 2;
                    goto end;
                }
                ALOGE("get unmatch key, new_version:%s, cur_version:%s\n",version,ic->drm_version);
                av_free(ic->drm_version);
            }
            ic->drm_version = strdup(version);
            ic->drm_flags = 1;
        }
    }
    else
    {
        if(ic->drm_version_next!=NULL)
            av_free(ic->drm_version_next);
        ic->drm_version_next = strdup(version);
    }
//    ALOGE("ffp_notify_msg FFP_MSG_GETDRMKEY %d, version:%s\n",FFP_MSG_GETDRMKEY,version);
    ffp_notify_msg(ffp,FFP_MSG_GETDRMKEY,version);
end:
    SDL_UnlockMutex(is->drm_mutex);
    ALOGE("ffp_notify_msg FFP_MSG_GETDRMKEY end flag:%d version:%s drm_flag:%d\n",curflag, version,ic->drm_flags);
    return 0;
    if(SDL_CondWaitTimeout(is->continue_drm_thread,is->drm_wait_mutex,1000) ) {
            ffp_notify_msg3(ffp,FFP_MSG_ERROR,FFP_ERROR_DRM,FFP_ERROR_TIMEOUT);
            ALOGE("ffp_notify_msg FFP_MSG_GETDRMKEY timeout, version:%s\n",version);
    }
    return 0;
}

void parser_key(const char* key, char* realkey)
{
    int i;
    //const char*ptr  = key;
    //char* pre_dest = realkey;
    //ALOGE("key:%s\n",key);
    for(i=0;i<16;i++) {
            *realkey = 0;
            if(key[0]<='9')
                *realkey += key[0] - '0';
            else if(key[0]<='Z')
                *realkey += key[0] -'A' + 10;
            else if(key[0]<='z')
                *realkey += key[0] - 'a' + 10;
            key++;

            *realkey = (*realkey << 4);

            if(key[0]<='9')
                *realkey += key[0] - '0';
            else if(key[0]<='Z')
                *realkey += key[0] -'A' + 10;
            else if(key[0]<='z')
                *realkey += key[0] - 'a' + 10;
            key++;

            //ALOGE("%x\n",realkey[0]);
            realkey++;
    }
}

void set_drm_key(FFPlayer *ffp, const char* version, const char* key)
{
    VideoState* is = ffp->is;
    AVFormatContext* ic = is->ic;
    ALOGE("set_drm_key key:%s version:%s\n",key,version);
    if(strlen(key)<32) {
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_DRM, FFP_ERROR_WRONGPARAM);
        return ;
    }
    char tmpkey[17] = {0};
    parser_key(key,tmpkey);
    SDL_LockMutex(is->drm_mutex);
    if(ic->drm_version_next!=NULL && strcmp(ic->drm_version_next,version)==0)
    {
        memcpy(ic->drm_key_next ,tmpkey,16);
    }
    else  if(ic->drm_version==NULL || strcmp(ic->drm_version,version)==0)
    {
        memcpy(ic->drm_key,tmpkey,16);
        ksy_set_key(is->drm_ctx, ic->drm_key, 16);
        ic->drm_flags = 2;
    }
    else
    {
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_DRM,2);
         ALOGE("set_drm_key Cannot find match version key, version:%s\n",version);
    }
    SDL_UnlockMutex(is->drm_mutex);
    SDL_CondSignal(is->continue_drm_thread);
}

int decode_drm(void *thiz, uint8* data, int size)
{
     AVFormatContext* ic = thiz;
     FFPlayer* ffp = ic->drm_thiz;
    VideoState* is = ffp->is;
    if(ic->drm_flags!=2 || is->drm_ctx==NULL)
        return -1;
    SDL_LockMutex(is->drm_mutex);
    if(ksy_flv_drm_decode(is->drm_ctx, ic->drm_length_size, data, size)!=0) {
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_DRM,3);
    }
    SDL_UnlockMutex(ffp->is->drm_mutex);
    return 0;
}

int decode_drm_pkt(FFPlayer *ffp, AVPacket* pkt)
{
    VideoState* is = ffp->is;
    AVFormatContext* ic = is->ic;
  //  av_log(NULL, AV_LOG_FATAL, "decode_drm %d\n", ic->drm_flags);
    if(pkt->flags > 99)
    {
        if(ic->drm_flags==2) {
            ALOGW("decode_drm flags:%d\n",pkt->flags);
            SDL_LockMutex(ffp->is->drm_mutex);
            ksy_flv_drm_decode( is->drm_ctx, ic->drm_length_size,  pkt->data,  pkt->size );
            SDL_UnlockMutex(ffp->is->drm_mutex);
        } else {
            ALOGE("decode_drm drop pkt flags:%d\n",pkt->flags);
            return -1;
        }
    }
    return 0;
}

/**
 * end by gs
**/
#ifdef FFP_SHOW_FPS
static int g_fps_counter = 0;
static int64_t g_fps_total_time = 0;
#endif
static void video_image_display2(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    Frame *vp;
    vp = frame_queue_peek(&is->pictq);
    if (vp->bmp) {
        SDL_VoutDisplayYUVOverlay(ffp->vout, vp->bmp);
    }
}

// FFP_MERGE: compute_mod
// FFP_MERGE: video_audio_display

static void stream_close(VideoState *is)
{
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;
    packet_queue_abort(&is->videoq);
    packet_queue_abort(&is->audioq);
    long pos = get_current_position_l(is);
    ALOGW("wait for read_tid\n");
    //if(pos<1000)
    //    SDL_WaitThread_kill(is->read_tid, NULL);
    //else
        SDL_WaitThread(is->read_tid, NULL);
    ALOGW("wait for video_refresh_tid\n");
    SDL_WaitThread(is->video_refresh_tid, NULL);

    packet_queue_destroy(&is->videoq);
    packet_queue_destroy(&is->audioq);

    /* free all pictures */
    frame_queue_destory(&is->pictq);

    frame_queue_destory(&is->sampq);
    SDL_DestroyCond(is->continue_read_thread);
    SDL_DestroyMutex(is->play_mutex);
/**
 * add by gs for drm
**/
    SDL_DestroyMutex(is->drm_mutex);
    SDL_DestroyMutex(is->drm_wait_mutex);
    SDL_DestroyCond(is->continue_drm_thread);
/**
 * end by gs
**/
    av_freep(&(is->drm_ctx));
    sws_freeContext(is->img_convert_ctx);
    sws_freeContext(is->img_convert_ctx_rgba);
    av_free(is);
}

/* display the current picture, if any */
static void video_display2(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    if (is->video_st)
        video_image_display2(ffp);
}

static double get_clock(Clock *c)
{
    if (*c->queue_serial != c->serial)
        return NAN;
    if (c->paused) {
        return c->pts;
    } else {
        double time = av_gettime_relative() / 1000000.0;
        return c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
    }
}

static void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

static void set_clock(Clock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

static void set_clock_speed(Clock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

static void init_clock(Clock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
}

static void sync_clock_to_slave(Clock *c, Clock *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

static int get_master_sync_type(VideoState *is) {
    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else  if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            return AV_SYNC_AUDIO_MASTER;
        else if (is->video_st)
            return AV_SYNC_VIDEO_MASTER;
        else
            return AV_SYNC_EXTERNAL_CLOCK;
    } else {
        return AV_SYNC_EXTERNAL_CLOCK;
    }
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    double val;

    switch (get_master_sync_type(is)) {
        case AV_SYNC_VIDEO_MASTER:
            val = get_clock(&is->vidclk);
            break;
        case AV_SYNC_AUDIO_MASTER:
            val = get_clock(&is->audclk);
            break;
        default:
            val = get_clock(&is->extclk);
            break;
    }
    return val;
}

static void check_external_clock_speed(VideoState *is) {
   if ((is->video_stream >= 0 && is->videoq.nb_packets <= MIN_FRAMES / 2) ||
       (is->audio_stream >= 0 && is->audioq.nb_packets <= MIN_FRAMES / 2)) {
       set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
   } else if ((is->video_stream < 0 || is->videoq.nb_packets > MIN_FRAMES * 2) &&
              (is->audio_stream < 0 || is->audioq.nb_packets > MIN_FRAMES * 2)) {
       set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
   } else {
       double speed = is->extclk.speed;
       if (speed != 1.0)
           set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
   }
}

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
        SDL_CondSignal(is->continue_read_thread);
    }
}

/* pause or resume the video */
static void stream_toggle_pause_l(FFPlayer *ffp, int pause_on)
{
    VideoState *is = ffp->is;
    if (is->paused && !pause_on) {
        is->frame_timer += av_gettime_relative() / 1000000.0 + is->vidclk.pts_drift - is->vidclk.pts;

        set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
    }
    set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
    is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = pause_on;

    SDL_AoutPauseAudio(ffp->aout, pause_on);
}

static void stream_update_pause_l(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    ALOGE("stream_update_pause_l: step:%d paused:%d req:%d buf:%d\n", is->step, is->paused,is->pause_req, is->buffering_on);
    if (!is->step && (is->pause_req || is->buffering_on)) {
	 ALOGE("stream_update_pause_l: 1 step:%d paused:%d req:%d buf:%d\n", is->step, is->paused,is->pause_req, is->buffering_on);
        stream_toggle_pause_l(ffp, 1);
    } else {
	 ALOGE("stream_update_pause_l: 0 step:%d paused:%d req:%d buf:%d\n", is->step, is->paused,is->pause_req, is->buffering_on);
        stream_toggle_pause_l(ffp, 0);
    }
}

static void toggle_pause_l(FFPlayer *ffp, int pause_on)
{
//    ALOGE("toggle_pause_l %d\n",pause_on);
    VideoState *is = ffp->is;
    is->pause_req = pause_on;
    ffp->auto_start = !pause_on;
    stream_update_pause_l(ffp);
    is->step = 0;
}

static void toggle_pause(FFPlayer *ffp, int pause_on)
{
    SDL_LockMutex(ffp->is->play_mutex);
    toggle_pause_l(ffp, pause_on);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

static void step_to_next_frame_l(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    /* if the stream is paused unpause it, then step */
    // ALOGE("step_to_next_frame\n");
    //if (is->paused)
    //    stream_toggle_pause_l(ffp, 0);
    is->step = 2;
}

static double compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff = 0.0f;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_clock(&is->vidclk) - get_master_clock(is);
        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }
    return delay;
}

static double vp_duration(FFPlayer *ffp ,VideoState *is, Frame *vp, Frame *nextvp) {
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
            duration = vp->duration;
/**
 * add by gs
**/
        if(get_master_sync_type(is)==AV_SYNC_VIDEO_MASTER
           && (ffp->videorate>1.01 || ffp->videorate<0.99) )
        {
                duration /= ffp->videorate;
        }
/**
 * end by gs
**/
            return duration;
    } else {
        return 0.0;
    }
}

static void update_video_pts(VideoState *is, double pts, int64_t pos, int serial) {
    /* update current video pts */
    set_clock(&is->vidclk, pts, serial);
    sync_clock_to_slave(&is->extclk, &is->vidclk);
}

/**
 * comment by gs display video frame
 **/
static void video_refresh(FFPlayer *opaque, double *remaining_time)
{
    FFPlayer *ffp = opaque;
    VideoState *is = ffp->is;
    double time;

    if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
        check_external_clock_speed(is);

    if (!ffp->display_disable && is->show_mode != SHOW_MODE_VIDEO && is->audio_st) {
        time = av_gettime_relative() / 1000000.0;
        if (is->force_refresh || is->last_vis_time + ffp->rdftspeed < time) {
            video_display2(ffp);
            is->last_vis_time = time;
        }
        *remaining_time = FFMIN(*remaining_time, is->last_vis_time + ffp->rdftspeed - time);
    }

    if (is->video_st) {
        int redisplay = 0;
        if (is->force_refresh)
            redisplay = frame_queue_prev(&is->pictq);
retry:
        if (frame_queue_nb_remaining(&is->pictq) == 0) {
            // nothing to do, no picture to display in the queue
            //av_log(NULL, AV_LOG_WARNING,"nothing to do, no picture to display in the queue\n");
        } else {
            double last_duration, duration, delay;
            Frame *vp, *lastvp;

            /* dequeue the picture */
            lastvp = frame_queue_peek_last(&is->pictq);
            vp = frame_queue_peek(&is->pictq);

            if (vp->serial != is->videoq.serial) {
                frame_queue_next(&is->pictq);
                redisplay = 0;
                goto retry;
            }

            if (lastvp->serial != vp->serial && !redisplay)
                is->frame_timer = av_gettime_relative() / 1000000.0;

            if (is->paused)
                goto display;

            /* compute nominal last_duration */
            last_duration = vp_duration(ffp, is, lastvp, vp);

    	     // add by yangle2@kingsoft.com, 2015-9-11
            // description:if drop decode frame, use duration to display in low latency mode
            // ID: KSYP-2 	     
	     if(get_master_sync_type(is) == AV_SYNC_VIDEO_MASTER && ffp->use_low_latency){
			if(is->refer_video_duration == -1 || 
		   		(is->refer_video_duration>0 &&  (is->refer_video_duration/3) < last_duration && last_duration < (is->refer_video_duration*3) && lastvp->decode_output_cc+1 == vp->decode_output_cc)){
				is->refer_video_sum+=last_duration;
				is->refer_video_cnt++;
			}

			if(is->refer_video_cnt == FFP_COMPUTE_VIDEO_DURATION_SPAN){
				is->refer_video_duration = is->refer_video_sum / FFP_COMPUTE_VIDEO_DURATION_SPAN;
				is->refer_video_cnt = 0;
				is->refer_video_sum  = 0;
				ALOGI("[%s:%d]compute duration. refer_video_duration=%f,last_duration=%f\n", __FUNCTION__,__LINE__,is->refer_video_duration,last_duration);
			}

            		if(is->refer_video_duration > 0 &&  (lastvp->decode_output_cc+1) != vp->decode_output_cc){
				last_duration = is->refer_video_duration;			
	     		}
	     }	
		 
            if (redisplay)
                delay = 0.0;
            else
                delay = compute_target_delay(last_duration, is);

            time= av_gettime_relative()/1000000.0;

           // av_log(NULL, AV_LOG_ERROR,"time %f, delay %f, remaining_time:%f\n", time, delay, *remaining_time);

            if (isnan(is->frame_timer) || time < is->frame_timer)
                is->frame_timer = time;
            if (time < is->frame_timer + delay && !redisplay) {
                *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
                return;
            }

            is->frame_timer += delay;
            if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
                is->frame_timer = time;

            // ALOGI("[%s:%d]video show. refer_video_duration=%f,last_duration=%f,decode_output_cc=%d\n", __FUNCTION__,__LINE__,is->refer_video_duration,last_duration,vp->decode_output_cc);
            SDL_LockMutex(is->pictq.mutex);
            if (!redisplay && !isnan(vp->pts))
                update_video_pts(is, vp->pts, vp->pos, vp->serial);
    	     // ALOGI("[%s:%d]vclock=%f, vpts=%f\n", __FUNCTION__,__LINE__,get_clock(&is->vidclk),vp->pts);
            SDL_UnlockMutex(is->pictq.mutex);

            if (frame_queue_nb_remaining(&is->pictq) > 1) {
                Frame *nextvp = frame_queue_peek_next(&is->pictq);
                duration = vp_duration(ffp, is, vp, nextvp);
                if(!is->step && (redisplay || ffp->framedrop > 0 || (ffp->framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration) {
                    if (!redisplay)
                        is->frame_drops_late++;
                    frame_queue_next(&is->pictq);
                    redisplay = 0;
                    goto retry;
                }
            }

            // FFP_MERGE: if (is->subtitle_st) { {...}

display:
    // add by gs for pause bug 150410
            /* display picture */
            if (!ffp->display_disable && is->show_mode == SHOW_MODE_VIDEO)
                video_display2(ffp);

            frame_queue_next(&is->pictq);

            SDL_LockMutex(ffp->is->play_mutex);
            if (is->step>0) {
                is->step --;
                if (!is->paused) {
//                    ALOGW("display pause:%d req:%d auto:%d\n",is->paused,is->pause_req,ffp->auto_start);
                    stream_update_pause_l(ffp);
                }
            } else
                is->step = 0;
            SDL_UnlockMutex(ffp->is->play_mutex);
        }
    }
    is->force_refresh = 0;
}

// TODO: 9 alloc_picture in video_refresh_thread if overlay referenced by vout
/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    Frame *vp;

    vp = &is->pictq.queue[is->pictq.windex];

    free_picture(vp);

    vp->bmp = SDL_Vout_CreateOverlay(vp->width, vp->height,
                                   ffp->overlay_format,
                                   ffp->vout);

    /* RV16, RV32 contains only one plane */
    if (!vp->bmp || vp->bmp->pitches[0] < vp->width) {
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        av_log(NULL, AV_LOG_FATAL,
               "Error: the video system does not support an image\n"
                        "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                        "to reduce the image size.\n", vp->width, vp->height );
        free_picture(vp);
    }

    SDL_LockMutex(is->pictq.mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq.cond);
    SDL_UnlockMutex(is->pictq.mutex);
}

static int queue_picture(FFPlayer *ffp, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
    VideoState *is = ffp->is;
    Frame *vp;

    if (!(vp = frame_queue_peek_writable(&is->pictq)))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp || vp->reallocate || !vp->allocated ||
        vp->width  != src_frame->width ||
        vp->height != src_frame->height) {

        if (vp->width != src_frame->width || vp->height != src_frame->height)
            ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, src_frame->width, src_frame->height);

        vp->allocated  = 0;
        vp->reallocate = 0;
        vp->width = src_frame->width;
        vp->height = src_frame->height;
	 vp->decode_output_cc = src_frame->decode_output_cc;

        /* the allocation must be done in the main thread to avoid
           locking problems. */
        alloc_picture(ffp);

        if (is->videoq.abort_request)
            return -2;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        /* get a pointer on the bitmap */
        SDL_VoutLockYUVOverlay(vp->bmp);

        if (SDL_VoutFFmpeg_ConvertFrame(vp->bmp, src_frame, &is->img_convert_ctx, ffp->sws_flags) < 0) {
            av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
            ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_MEM,3);
            return -3;
        }
        /* update the bitmap content */
        SDL_VoutUnlockYUVOverlay(vp->bmp);

        vp->pts = pts;
        vp->duration = duration;
        vp->pos = pos;
        vp->serial = serial;
	 vp->decode_output_cc = src_frame->decode_output_cc;

        /* now we can update the picture count */
        frame_queue_push(&is->pictq);
    }
    return 0;
}

static int get_video_frame(FFPlayer *ffp, AVFrame *frame)
{
    VideoState *is = ffp->is;
    int got_picture;

    if ((got_picture = decoder_decode_frame(ffp, &is->viddec, frame, NULL)) < 0)
        return -1;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        if (ffp->framedrop>0 || (ffp->framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - get_master_clock(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                    diff - is->frame_last_filter_delay < 0 &&
                    is->viddec.pkt_serial == is->vidclk.serial &&
                    is->videoq.nb_packets) {
                    is->frame_drops_early++;
                    is->continuous_frame_drops_early++;
                    if (is->continuous_frame_drops_early > ffp->framedrop) {
                        is->continuous_frame_drops_early = 0;
                    } else {
                        av_frame_unref(frame);
                        got_picture = 0;
                    }
      		      ALOGI("[%s:%d]Continue drop picture. frame_drops_early=%d, continuous_frame_drops_early=%d, diff=%f, frame_last_filter_delay=%f,nb_packets=%d\n", 
			  		__FUNCTION__,__LINE__, is->frame_drops_early, is->continuous_frame_drops_early,diff,is->frame_last_filter_delay,is->videoq.nb_packets);
		  }
            }
        }
    }
    return got_picture;
}

#if CONFIG_AVFILTER
static int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph,
                                 AVFilterContext *source_ctx, AVFilterContext *sink_ctx)
{
    int ret, i;
    int nb_filters = graph->nb_filters;
    AVFilterInOut *outputs = NULL, *inputs = NULL;

    if (filtergraph) {
        outputs = avfilter_inout_alloc();
        inputs  = avfilter_inout_alloc();
        if (!outputs || !inputs) {
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        outputs->name       = av_strdup("in");
        outputs->filter_ctx = source_ctx;
        outputs->pad_idx    = 0;
        outputs->next       = NULL;

        inputs->name        = av_strdup("out");
        inputs->filter_ctx  = sink_ctx;
        inputs->pad_idx     = 0;
        inputs->next        = NULL;

        if ((ret = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, NULL)) < 0)
            goto fail;
    } else {
        if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0)
            goto fail;
    }

    /* Reorder the filters to ensure that inputs of the custom filters are merged first */
    for (i = 0; i < graph->nb_filters - nb_filters; i++)
        FFSWAP(AVFilterContext*, graph->filters[i], graph->filters[i + nb_filters]);

    ret = avfilter_graph_config(graph, NULL);
fail:
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    return ret;
}

static int configure_video_filters(FFPlayer *ffp, AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame)
{
    static const enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    char sws_flags_str[512] = "";
    char buffersrc_args[256];
    int ret;
    AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
    AVCodecContext *codec = is->video_st->codec;
    AVRational fr = av_guess_frame_rate(is->ic, is->video_st, NULL);
    AVDictionaryEntry *e = NULL;

    while ((e = av_dict_get(ffp->sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
        if (!strcmp(e->key, "sws_flags")) {
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
        } else
            av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
    }
    if (strlen(sws_flags_str))
        sws_flags_str[strlen(sws_flags_str)-1] = '\0';

    graph->scale_sws_opts = av_strdup(sws_flags_str);

    snprintf(buffersrc_args, sizeof(buffersrc_args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             frame->width, frame->height, frame->format,
             is->video_st->time_base.num, is->video_st->time_base.den,
             codec->sample_aspect_ratio.num, FFMAX(codec->sample_aspect_ratio.den, 1));
    if (fr.num && fr.den)
        av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

    if ((ret = avfilter_graph_create_filter(&filt_src,
                                            avfilter_get_by_name("buffer"),
                                            "ffplay_buffer", buffersrc_args, NULL,
                                            graph)) < 0){
        ALOGI("[%s:%d]avfilter_graph_create_filter ffplay_buffer ret = %d\n", __FUNCTION__,__LINE__, ret);
        goto fail;
    }
    ret = avfilter_graph_create_filter(&filt_out,
                                       avfilter_get_by_name("buffersink"),
                                       "ffplay_buffersink", NULL, NULL, graph);
    if (ret < 0){
	 av_log(NULL, AV_LOG_INFO,"[%s:%d]avfilter_graph_create_filter ffplay_buffersink ret = %d\n", __FUNCTION__,__LINE__, ret);
        goto fail;
    }

    if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts,  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0){
	av_log(NULL, AV_LOG_INFO,"[%s:%d]av_opt_set_int_list ret = %d\n", __FUNCTION__,__LINE__, ret);
	goto fail;
    }
    last_filter = filt_out;

/* Note: this macro adds a filter before the lastly added filter, so the
 * processing order of the filters is in reverse */
#define INSERT_FILT(name, arg) do {                                          \
    AVFilterContext *filt_ctx;                                               \
                                                                             \
    ret = avfilter_graph_create_filter(&filt_ctx,                            \
                                       avfilter_get_by_name(name),           \
                                       "ffplay_" name, arg, NULL, graph);    \
    if (ret < 0) {                                                            \
	 av_log(NULL, AV_LOG_INFO,"[%s:%d]avfilter_graph_create_filter ffplay_ ret = %d\n", __FUNCTION__,__LINE__, ret);	\
        goto fail;                                                           \
    }                                                                         \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
    if (ret < 0){                                                             \
	 av_log(NULL, AV_LOG_INFO,"[%s:%d]avfilter_link ret = %d\n", __FUNCTION__,__LINE__, ret);	\
        goto fail;                                                           \
    }                                                                         \
    last_filter = filt_ctx;                                                  \
} while (0)

    /* SDL YUV code is not handling odd width/height for some driver
     * combinations, therefore we crop the picture to an even width/height. */
    //INSERT_FILT("crop", "floor(in_w/2)*2:floor(in_h/2)*2");

    if (ffp->autorotate) {
        //double theta  = get_rotation(is->video_st);
        double theta  = ffp->cur_angle;
        if (fabs(theta - 90) < 1.0) {
            INSERT_FILT("transpose", "clock");
        } else if (fabs(theta - 180) < 1.0) {
            INSERT_FILT("hflip", NULL);
            INSERT_FILT("vflip", NULL);
        } else if (fabs(theta - 270) < 1.0) {
            INSERT_FILT("transpose", "cclock");
        } else if (fabs(theta) > 1.0) {
            char rotate_buf[64];
            snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
            INSERT_FILT("rotate", rotate_buf);
        }
	 av_log(NULL, AV_LOG_INFO,"[%s:%d]theta = %f\n", __FUNCTION__,__LINE__, theta);
    }

    if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0){
	av_log(NULL, AV_LOG_INFO,"[%s:%d]configure_filtergraph ret = %d\n", __FUNCTION__,__LINE__, ret);
	goto fail;
    }

    is->in_video_filter  = filt_src;
    is->out_video_filter = filt_out;

fail:
    return ret;
}

static int configure_audio_filters(FFPlayer *ffp, const char *afilters, int force_output_format)
{
    VideoState *is = ffp->is;
    static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
    int sample_rates[2] = { 0, -1 };
    int64_t channel_layouts[2] = { 0, -1 };
    int channels[2] = { 0, -1 };
    AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
    char aresample_swr_opts[512] = "";
    AVDictionaryEntry *e = NULL;
    char asrc_args[256];
    int ret;

    avfilter_graph_free(&is->agraph);
    if (!(is->agraph = avfilter_graph_alloc()))
        return AVERROR(ENOMEM);

    while ((e = av_dict_get(ffp->sws_opts, "", e, AV_DICT_IGNORE_SUFFIX)))
        av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);
    if (strlen(aresample_swr_opts))
        aresample_swr_opts[strlen(aresample_swr_opts)-1] = '\0';
    av_opt_set(is->agraph, "aresample_swr_opts", aresample_swr_opts, 0);

    ret = snprintf(asrc_args, sizeof(asrc_args),
                   "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d",
                   is->audio_filter_src.freq, av_get_sample_fmt_name(is->audio_filter_src.fmt),
                   is->audio_filter_src.channels,
                   1, is->audio_filter_src.freq);
    if (is->audio_filter_src.channel_layout)
        snprintf(asrc_args + ret, sizeof(asrc_args) - ret,
                 ":channel_layout=0x%"PRIx64,  is->audio_filter_src.channel_layout);

    ret = avfilter_graph_create_filter(&filt_asrc,
                                       avfilter_get_by_name("abuffer"), "ffplay_abuffer",
                                       asrc_args, NULL, is->agraph);
    if (ret < 0)
        goto end;


    ret = avfilter_graph_create_filter(&filt_asink,
                                       avfilter_get_by_name("abuffersink"), "ffplay_abuffersink",
                                       NULL, NULL, is->agraph);
    if (ret < 0)
        goto end;

    if ((ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts,  AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;
    if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN)) < 0)
        goto end;

    if (force_output_format) {
        channel_layouts[0] = is->audio_tgt.channel_layout;
        channels       [0] = is->audio_tgt.channels;
        sample_rates   [0] = is->audio_tgt.freq;
        if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "channel_layouts", channel_layouts,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "channel_counts" , channels       ,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
        if ((ret = av_opt_set_int_list(filt_asink, "sample_rates"   , sample_rates   ,  -1, AV_OPT_SEARCH_CHILDREN)) < 0)
            goto end;
    }


    if ((ret = configure_filtergraph(is->agraph, afilters, filt_asrc, filt_asink)) < 0)
        goto end;

    is->in_audio_filter  = filt_asrc;
    is->out_audio_filter = filt_asink;

end:
    if (ret < 0)
        avfilter_graph_free(&is->agraph);
    return ret;
}
#endif  /* CONFIG_AVFILTER */


static int audio_thread(void *arg)
{
    ALOGI("[%s:%d]Starting time = %lld\n", __FUNCTION__,__LINE__, av_gettime());
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    AVFrame *frame = av_frame_alloc();
    Frame *af;

    int got_frame = 0;
    AVRational tb;
    int ret = 0;

    int64_t start_decode_time = 0;
    int64_t last_log_time = 0;

    if (!frame)
        return AVERROR(ENOMEM);

    //if(ffp->use_low_latency)
    	//packet_queue_flush(&is->audioq);

    start_decode_time = av_gettime() ;
    ALOGI("[%s:%d]Start decode time = %lld\n", __FUNCTION__,__LINE__, start_decode_time);

    do {
	 //if(last_log_time + 200000 <  av_gettime()){
	//	last_log_time = av_gettime() ;
    	//	ALOGI("[%s:%d]decoding. time = %lld\n", __FUNCTION__,__LINE__, last_log_time);
	 //}
	 
        if ((got_frame = decoder_decode_frame(ffp, &is->auddec, frame, NULL)) < 0)
            goto the_end;

        if (got_frame) {		
                tb = (AVRational){1, frame->sample_rate};

                if (!(af = frame_queue_peek_writable(&is->sampq)))
                    goto the_end;

                af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
                af->pos = av_frame_get_pkt_pos(frame);
                af->serial = is->auddec.pkt_serial;
                af->duration = av_q2d((AVRational){frame->nb_samples, frame->sample_rate});

                av_frame_move_ref(af->frame, frame);
                frame_queue_push(&is->sampq);	
        }
    } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
 the_end:

    av_frame_free(&frame);
    return ret;
}

/**
 *  comment by gs
  * decodec video threads
**/
static int ffplay_video_decode_thread(void *arg)
{
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    AVFrame *frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    AVRational tb = is->video_st->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

    int64_t last_log_time = 0;
    int64_t start_decode_time = av_gettime() ;
    int start_drop_frame = 0;
	
    //if(ffp->use_low_latency)
    	//packet_queue_flush(&is->videoq);

#if CONFIG_AVFILTER
    AVFilterGraph *graph = avfilter_graph_alloc();
    AVFilterContext *filt_out = NULL, *filt_in = NULL;
    int last_w = 0;
    int last_h = 0;
    enum AVPixelFormat last_format = -2;
    int last_serial = -1;
    int last_vfilter_idx = 0;
    double last_angle=0;
    if (!graph) {
        av_frame_free(&frame);
        return AVERROR(ENOMEM);
    }
#else
    //ffp_notify_msg2(ffp, FFP_MSG_VIDEO_ROTATION_CHANGED, ffp_get_video_rotate_degrees(ffp));
#endif

    if (!frame) {
#if CONFIG_AVFILTER
        avfilter_graph_free(&graph);
#endif
        return AVERROR(ENOMEM);
    }
	
    ALOGI("[%s:%d]Start decode time = %lld nb_packets=%d, max_size=%d\n", __FUNCTION__,__LINE__, start_decode_time, is->videoq.nb_packets, is->pictq.max_size);

    for (;;) {
	 //if(last_log_time + 1000000 <  av_gettime()){
		//last_log_time = av_gettime() ;
    		//ALOGI("[%s:%d]Video decoding. time = %lld, size=%d\n", __FUNCTION__,__LINE__, last_log_time, frame_queue_nb_remaining(&is->pictq));
	 //}

        ret = get_video_frame(ffp, frame);
        if (ret < 0)
            goto the_end;
        if (!ret)
            continue;
		
    	 // add by yangle2@kingsoft.com, 2015-9-11
        // description:check to drop frame in video master mode.
        // ID: KSYP-2 	   
	 if(get_master_sync_type(is) == AV_SYNC_VIDEO_MASTER && ffp->use_low_latency) {
	 	double cache_delay_ms = get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_VIDEO);
		if(cache_delay_ms >= 0 && cache_delay_ms > ffp->buffer_cache_maxtime_ms && start_drop_frame == 0) { 
			ALOGI("[%s:%d]is->videoq.nb_packets=%d,  is->pictq.size=%d,cache_delay_ms=%f,enter droping\n", __FUNCTION__,__LINE__,
				is->videoq.nb_packets,  frame_queue_nb_remaining(&is->pictq),cache_delay_ms);
	 		start_drop_frame = 1;
	 	}
		else if(cache_delay_ms >= 0 &&cache_delay_ms < ffp->buffer_cache_mintime_ms && start_drop_frame == 1){
			ALOGI("[%s:%d]is->videoq.nb_packets=%d,  is->pictq.size=%d,cache_delay_ms=%f,end droping\n", __FUNCTION__,__LINE__,
				is->videoq.nb_packets,  frame_queue_nb_remaining(&is->pictq),cache_delay_ms);
			start_drop_frame = 0;
		}

		if(start_drop_frame == 1){
			is->frame_drops_early++;
			av_frame_unref(frame);
			continue;
		}
	 }

#if CONFIG_AVFILTER
	 ffp->cur_angle = frame->angle;
	 if(ffp->cur_angle != last_angle) {
	 	ALOGI("[%s:%d]ffp->cur_angle=%f, frame_angle=%f\n", __FUNCTION__,__LINE__, ffp->cur_angle, frame->angle);
	 }
        if (   last_w != frame->width
            || last_h != frame->height
            || last_format != frame->format
            || last_serial != is->viddec.pkt_serial
            || last_vfilter_idx != is->vfilter_idx
            || ffp->cur_angle != last_angle) {
            av_log(NULL, AV_LOG_INFO,
                   "[%s:%d]Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",__FUNCTION__,__LINE__,
                   last_w, last_h,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
                   frame->width, frame->height,
                   (const char *)av_x_if_null(av_get_pix_fmt_name(frame->format), "none"), is->viddec.pkt_serial);
            avfilter_graph_free(&graph);
            graph = avfilter_graph_alloc();
            if ((ret = configure_video_filters(ffp, graph, is, ffp->vfilters_list ? ffp->vfilters_list[is->vfilter_idx] : NULL, frame)) < 0) {
                // FIXME: post error
                av_log(NULL, AV_LOG_INFO,"[%s:%d]configure_video_filters ret = %d\n", __FUNCTION__,__LINE__, ret);
                goto the_end;
            }
            filt_in  = is->in_video_filter;
            filt_out = is->out_video_filter;
            last_w = frame->width;
            last_h = frame->height;
            last_format = frame->format;
            last_serial = is->viddec.pkt_serial;
            last_vfilter_idx = is->vfilter_idx;
            frame_rate = filt_out->inputs[0]->frame_rate;
	     last_angle = ffp->cur_angle;
	     av_log(NULL, AV_LOG_INFO, "[%s:%d]last_w=%d, last_h = %d, frame_rate= %d\n",__FUNCTION__,__LINE__,last_w,last_h,frame_rate);
        }

        ret = av_buffersrc_add_frame(filt_in, frame);
        if (ret < 0)
            goto the_end;

        while (ret >= 0) {
            is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

            ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
            if (ret < 0) {
                if (ret == AVERROR_EOF){
                    is->viddec.finished = is->viddec.pkt_serial;
		      ALOGI("[%s:%d]video_type, queue finished=%d\n", __FUNCTION__, __LINE__, is->viddec.finished);
                }
		   ret = 0;
                break;
            }

            is->frame_last_filter_delay = av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
            if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
                is->frame_last_filter_delay = 0;
            tb = filt_out->inputs[0]->time_base;
#endif

            duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);
            pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
            ret = queue_picture(ffp, frame, pts, duration, av_frame_get_pkt_pos(frame), is->viddec.pkt_serial);
            av_frame_unref(frame);

#if CONFIG_AVFILTER
        }
#endif
        if (ret < 0)
            goto the_end;
    }
 the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&graph);
#endif
    av_frame_free(&frame);
    return 0;
}

static int video_thread(void *arg)
{
    ALOGI("[%s:%d]Starting time = %lld\n", __FUNCTION__,__LINE__, av_gettime());
    FFPlayer *ffp = (FFPlayer *)arg;
    int       ret = 0;

    if (ffp->node_vdec) {
        ret = ffpipenode_run_sync(ffp->node_vdec);
    }
    return ret;
}

/* copy samples for viewing in editor window */
static void update_sample_display(VideoState *is, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}

/* return the wanted number of samples to get better sync if sync_type is video
 * or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples)
{
    int wanted_nb_samples = nb_samples;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
        double diff, avg_diff;
        int min_nb_samples, max_nb_samples;

        diff = get_clock(&is->audclk) - get_master_clock(is);

        if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
                    min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
                    wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
                }
//                av_dlog(NULL, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
//                        diff, avg_diff, wanted_nb_samples - nb_samples,
//                        is->audio_clock, is->audio_diff_threshold);
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum       = 0;
        }
    }

    return wanted_nb_samples;
}

/**
 * add by gs
**/
static int filter_frame(FFPlayer *ffp, Frame* af)
{
    //av_log(NULL, AV_LOG_ERROR,"filter_frame");
    VolumeContext *vol    = ffp->audioamplify;
    AVFrame *buf = af->frame;
    int nb_samples        = buf->nb_samples;

    if (vol->volume <= 1.01 && vol->volume >= 0.99)
        return 0;

    int p, plane_samples;

    if (av_sample_fmt_is_planar(buf->format))
        plane_samples = FFALIGN(nb_samples, vol->samples_align);
    else
        plane_samples = FFALIGN(nb_samples * vol->channels, vol->samples_align);

    if (av_get_packed_sample_fmt(vol->sample_fmt) == AV_SAMPLE_FMT_FLT) {
        for (p = 0; p < vol->planes; p++) {
            vol->fdsp->vector_fmul_scalar((float *)buf->extended_data[p],
                                            (const float *)buf->extended_data[p],
                                            vol->volume, plane_samples);
        }
    } else {
        for (p = 0; p < vol->planes; p++) {
            vol->fdsp->vector_dmul_scalar((double *)buf->extended_data[p],
                                            (const double *)buf->extended_data[p],
                                            vol->volume, plane_samples);
        }
    }
//    emms_c();

    return 0;
}
/**
 * end by gs
**/

/**
 * Decode one audio frame and return its uncompressed size.
 *
 * The processed audio frame is decoded, converted if required, and
 * stored in is->audio_buf, with size in bytes given by the return
 * value.
 */
static int audio_decode_frame(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    int data_size, resampled_data_size;
    int64_t dec_channel_layout;
    av_unused double audio_clock0;
    int wanted_nb_samples;
    Frame *af;

    if (is->paused)
        return -1;

get_frame_retry:
    do {
        if (!(af = frame_queue_peek_readable(&is->sampq)))
            return -1;

    	if(get_master_sync_type(is) == AV_SYNC_AUDIO_MASTER && ffp->use_low_latency){
	 	double cache_delay_ms = get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_AUDIO);
		if(cache_delay_ms > ffp->buffer_cache_maxtime_ms && is->audio_play_mode != AUDIO_DROP_PLAY) { 
			ALOGI("[%s:%d]is->audioq.nb_packets=%d,  is->sampq.size=%d,cache_delay_ms=%f,Enter droping!!!\n", __FUNCTION__,__LINE__,
				is->audioq.nb_packets,  frame_queue_nb_remaining(&is->sampq),cache_delay_ms);
	 		is->audio_play_mode = AUDIO_DROP_PLAY;
	 	}
		else if(is->audio_play_mode == AUDIO_DROP_PLAY && cache_delay_ms < ffp->buffer_cache_midtime_ms){
			ALOGI("[%s:%d]is->audioq.nb_packets=%d,  is->sampq.size=%d,cache_delay_ms=%f,End droping!!!\n", __FUNCTION__,__LINE__,
				is->audioq.nb_packets,  frame_queue_nb_remaining(&is->sampq),cache_delay_ms);
			is->audio_play_mode = AUDIO_NOMAL_PLAY;			
		}
    	}	
       frame_queue_next(&is->sampq);
    } while (af->serial != is->audioq.serial || is->audio_play_mode == AUDIO_DROP_PLAY);

/**
 * add by gs
**/
    //filter_frame(ffp,af);
/**
 * end by gs
**/

    {
        data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
                                               af->frame->nb_samples,
                                               af->frame->format, 1);

        dec_channel_layout =
            (af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
            af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
        wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);

/**
 * add by gs
**/
                if(get_master_sync_type(is)==AV_SYNC_AUDIO_MASTER
                   && (ffp->videorate>1.01 || ffp->videorate<0.99) )
                {
                        wanted_nb_samples /= ffp->videorate;
                }
/**
 * end by gs
**/

        if (af->frame->format        != is->audio_src.fmt            ||
            dec_channel_layout       != is->audio_src.channel_layout ||
            af->frame->sample_rate   != is->audio_src.freq           ||
            (wanted_nb_samples       != af->frame->nb_samples && !is->swr_ctx)) {
            swr_free(&is->swr_ctx);
            is->swr_ctx = swr_alloc_set_opts(NULL,
                                             is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
                                             dec_channel_layout,           af->frame->format, af->frame->sample_rate,
                                             0, NULL);
            if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
                av_log(NULL, AV_LOG_ERROR,
                       "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                        af->frame->sample_rate, av_get_sample_fmt_name(af->frame->format), av_frame_get_channels(af->frame),
                        is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
                swr_free(&is->swr_ctx);
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,1);
                return -1;
            }
            is->audio_src.channel_layout = dec_channel_layout;
            is->audio_src.channels       = av_frame_get_channels(af->frame);
            is->audio_src.freq = af->frame->sample_rate;
            is->audio_src.fmt = af->frame->format;
        }

        if (is->swr_ctx) {
            const uint8_t **in = (const uint8_t **)af->frame->extended_data;
            uint8_t **out = &is->audio_buf1;
            int out_count = (int)((int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256);
            int out_size  = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
            int len2;
            if (out_size < 0) {
                av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,2);
                return -1;
            }
            if (wanted_nb_samples != af->frame->nb_samples) {
                if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
                                            wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
                    ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,3);
                    return -1;
                }
            }
            av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
            if (!is->audio_buf1) {
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_MEM,4);
                return AVERROR(ENOMEM);
            }
            len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
            if (len2 < 0) {
                av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,4);
                return -1;
            }
            if (len2 == out_count) {
                av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
                if (swr_init(is->swr_ctx) < 0)
                    swr_free(&is->swr_ctx);
            }
            is->audio_buf = is->audio_buf1;
            resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
        } else {
            is->audio_buf = af->frame->data[0];
            resampled_data_size = data_size;
        }

        audio_clock0 = is->audio_clock;
        /* update the audio clock with the pts */
        if (!isnan(af->pts))
            is->audio_clock = af->pts + (double) af->frame->nb_samples / af->frame->sample_rate;
        else
            is->audio_clock = NAN;
        is->audio_clock_serial = af->serial;
    }
    return resampled_data_size;
}

/**
 * comment by gs for audio output
**/
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    FFPlayer *ffp = opaque;
    VideoState *is = ffp->is;
    int audio_size, len1;

    ffp->audio_callback_time = av_gettime_relative();

    //ALOGI("[%s:%d] time = %lld, len=%d\n", __FUNCTION__,__LINE__, av_gettime(), len);
    while (len > 0) {
        if (is->audio_buf_index >= is->audio_buf_size) {
           audio_size = audio_decode_frame(ffp);
           if (audio_size < 0) {
                /* if error, just output silence */
               is->audio_buf      = is->silence_buf;
               is->audio_buf_size = sizeof(is->silence_buf) / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
           } else {
               if (is->show_mode != SHOW_MODE_VIDEO)
                   update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        if (is->auddec.pkt_serial != is->audioq.serial) {
            // ALOGE("aout_cb: flush\n");
            is->audio_buf_index = is->audio_buf_size;
            memset(stream, 0, len);
            // stream += len;
            // len = 0;
            SDL_AoutFlushAudio(ffp->aout);
            break;
        }
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
    is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
    /* Let's assume the audio driver that is used by SDL has two periods. */
    if (!isnan(is->audio_clock)) {
        set_clock_at(&is->audclk, is->audio_clock - (double)(is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec - SDL_AoutGetLatencySeconds(ffp->aout), is->audio_clock_serial, ffp->audio_callback_time / 1000000.0);
        sync_clock_to_slave(&is->extclk, &is->audclk);
    }
}

static int audio_open(FFPlayer *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
    FFPlayer *ffp = opaque;
    SDL_AudioSpec wanted_spec, spec;
    const char *env;
    static const int next_nb_channels[] = {0, 0, 1, 6, 2, 6, 4, 6};
    static const int next_sample_rates[] = {6000, 11025, 12000, 22050, 24000, 44100, 48000};
    int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

    env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        wanted_nb_channels = atoi(env);
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
    }
    if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
        wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
    }
    wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = wanted_sample_rate;
    if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
        av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,5);
        return -1;
    }
    while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
        next_sample_rate_idx--;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.silence = 0;
    wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    wanted_spec.callback = sdl_audio_callback;
    wanted_spec.userdata = opaque;
    while (SDL_AoutOpenAudio(ffp->aout, &wanted_spec, &spec) < 0) {
        av_log(NULL, AV_LOG_WARNING, "SDL_OpenAudio (%d channels, %d Hz): %s\n",
               wanted_spec.channels, wanted_spec.freq, SDL_GetError());
        wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
        if (!wanted_spec.channels) {
            wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
            wanted_spec.channels = wanted_nb_channels;
            if (!wanted_spec.freq) {
                av_log(NULL, AV_LOG_ERROR,
                       "No more combinations to try, audio open failed\n");
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,6);
                return -1;
            }
        }
        wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
    }
    if (spec.format != AUDIO_S16SYS) {
        av_log(NULL, AV_LOG_ERROR,
               "SDL advised audio format %d is not supported!\n", spec.format);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,7);
        return -1;
    }
    if (spec.channels != wanted_spec.channels) {
        wanted_channel_layout = av_get_default_channel_layout(spec.channels);
        if (!wanted_channel_layout) {
            av_log(NULL, AV_LOG_ERROR,
                   "SDL advised channel count %d is not supported!\n", spec.channels);
            ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,8);
            return -1;
        }
    }

    audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
    audio_hw_params->freq = spec.freq;
    audio_hw_params->channel_layout = wanted_channel_layout;
    audio_hw_params->channels =  spec.channels;
    audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
    audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
    if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_MEM,5);
        return -1;
    }

    SDL_AoutSetDefaultLatencySeconds(ffp->aout, ((double)(2 * spec.size)) / audio_hw_params->bytes_per_sec);
    return spec.size;
}

/**
 * add by gs
**/
static inline void scale_samples_u8(uint8_t *dst, const uint8_t *src,
                                    int nb_samples, int volume)
{
    int i;
    for (i = 0; i < nb_samples; i++)
        dst[i] = av_clip_uint8(((((int64_t)src[i] - 128) * volume + 128) >> 8) + 128);
}

static inline void scale_samples_u8_small(uint8_t *dst, const uint8_t *src,
                                          int nb_samples, int volume)
{
    int i;
    for (i = 0; i < nb_samples; i++)
        dst[i] = av_clip_uint8((((src[i] - 128) * volume + 128) >> 8) + 128);
}

static inline void scale_samples_s16(uint8_t *dst, const uint8_t *src,
                                     int nb_samples, int volume)
{
    int i;
    int16_t *smp_dst       = (int16_t *)dst;
    const int16_t *smp_src = (const int16_t *)src;
    for (i = 0; i < nb_samples; i++)
        smp_dst[i] = av_clip_int16(((int64_t)smp_src[i] * volume + 128) >> 8);
}

static inline void scale_samples_s16_small(uint8_t *dst, const uint8_t *src,
                                           int nb_samples, int volume)
{
    int i;
    int16_t *smp_dst       = (int16_t *)dst;
    const int16_t *smp_src = (const int16_t *)src;
    for (i = 0; i < nb_samples; i++)
        smp_dst[i] = av_clip_int16((smp_src[i] * volume + 128) >> 8);
}

static inline void scale_samples_s32(uint8_t *dst, const uint8_t *src,
                                     int nb_samples, int volume)
{
    int i;
    int32_t *smp_dst       = (int32_t *)dst;
    const int32_t *smp_src = (const int32_t *)src;
    for (i = 0; i < nb_samples; i++)
        smp_dst[i] = av_clipl_int32((((int64_t)smp_src[i] * volume + 128) >> 8));
}

static void volume_init(VolumeContext *vol)
{
    int volume_i = 256;
    switch (av_get_packed_sample_fmt(vol->sample_fmt)) {
    case AV_SAMPLE_FMT_U8:
        if (volume_i < 0x1000000)
            vol->scale_samples = scale_samples_u8_small;
        else
            vol->scale_samples = scale_samples_u8;
        break;
    case AV_SAMPLE_FMT_S16:
        if (volume_i < 0x10000)
            vol->scale_samples = scale_samples_s16_small;
        else
            vol->scale_samples = scale_samples_s16;
        break;
    case AV_SAMPLE_FMT_S32:
        vol->scale_samples = scale_samples_s32;
        break;
    case AV_SAMPLE_FMT_FLT:
        vol->samples_align = 4;
        break;
    case AV_SAMPLE_FMT_DBL:
        vol->samples_align = 8;
        break;
    default:
        vol->samples_align = 1;
    }
}
/**
 * end by gs
 **/
/* open a given stream. Return 0 if OK */
static int stream_component_open(FFPlayer *ffp, int stream_index)
{
    VideoState *is = ffp->is;
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    const char *forced_codec_name = NULL;
    AVDictionary *opts;
    AVDictionaryEntry *t = NULL;
    int sample_rate, nb_channels;
    int64_t channel_layout;
    int ret = 0;
    int stream_lowres = ffp->lowres;

    if (stream_index < 0 || stream_index >= ic->nb_streams) {
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_NOFILE,1);
        return -1;
    }
    avctx = ic->streams[stream_index]->codec;

    codec = avcodec_find_decoder(avctx->codec_id);

    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO   : is->last_audio_stream    = stream_index; forced_codec_name = ffp->audio_codec_name; break;
        // FFP_MERGE: case AVMEDIA_TYPE_SUBTITLE:
        case AVMEDIA_TYPE_VIDEO   : is->last_video_stream    = stream_index; forced_codec_name = ffp->video_codec_name; break;
        default: break;
    }
    if (forced_codec_name)
        codec = avcodec_find_decoder_by_name(forced_codec_name);
    if (!codec) {
        // FIXME: 9 report unknown codec id/name
        if (forced_codec_name) av_log(NULL, AV_LOG_WARNING,
                                      "No codec could be found with name '%s'\n", forced_codec_name);
        else                   av_log(NULL, AV_LOG_WARNING,
                                      "No codec could be found with id %d\n", avctx->codec_id);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,9);
        return -1;
    }

    avctx->codec_id = codec->id;
    if(stream_lowres > av_codec_get_max_lowres(codec)){
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                av_codec_get_max_lowres(codec));
        stream_lowres = av_codec_get_max_lowres(codec);
    }
    av_codec_set_lowres(avctx, stream_lowres);

    if(stream_lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
    if (ffp->fast)    avctx->flags2 |= CODEC_FLAG2_FAST;
    if(codec->capabilities & CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;

    opts = filter_codec_opts(ffp->codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);
    if (!av_dict_get(opts, "threads", NULL, 0))
        av_dict_set(&opts, "threads", "auto", 0);
    if (stream_lowres)
        av_dict_set_int(&opts, "lowres", stream_lowres, 0);
    if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
        av_dict_set(&opts, "refcounted_frames", "1", 0);
    //if (avctx->codec_type != AVMEDIA_TYPE_VIDEO) {
    if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,10);
        goto fail;
    }
    //}
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
    }

    is->eof = 0;
    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        sample_rate    = avctx->sample_rate;
        nb_channels    = avctx->channels;
        channel_layout = avctx->channel_layout;
/**
 * add by gs
**/
    if(ffp->audioamplify)
        {
            if( ffp->audioamplify->fdsp)
                av_freep(& ffp->audioamplify->fdsp);
            memset(ffp->audioamplify,0,sizeof(VolumeContext));
        }
        else
            ffp->audioamplify = av_mallocz(sizeof(VolumeContext));
        ffp->audioamplify->channels = avctx->channels;
        ffp->audioamplify->sample_fmt = avctx->sample_fmt;
        ffp->audioamplify->volume = 3.0;
        ffp->audioamplify->fdsp = avpriv_float_dsp_alloc(0);
        ffp->audioamplify->planes     = av_sample_fmt_is_planar(avctx->sample_fmt) ? ffp->audioamplify->channels : 1;
        volume_init(ffp->audioamplify);
/**
 * end by gs
 **/

        /* prepare audio output */
        is->paused = 1;
        if ((ret = audio_open(ffp, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0)
            goto fail;
        ffp_set_audio_codec_info(ffp, AVCODEC_MODULE_NAME, avcodec_get_name(avctx->codec_id));
        is->audio_hw_buf_size = ret;
        is->audio_src = is->audio_tgt;
        is->audio_buf_size  = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef  = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio fifo fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = 2.0 * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec;

        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];

        packet_queue_start(&is->audioq);
        decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
        if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
            is->auddec.start_pts = is->audio_st->start_time;
            is->auddec.start_pts_tb = is->audio_st->time_base;
        }
	 ALOGI("[%s:%d]sample_rate=%d,nb_channels=%d,channel_layout=%d,audio_diff_avg_coef=%f,audio_diff_threshold=%f\n", __FUNCTION__, __LINE__,
	 		sample_rate,nb_channels,channel_layout,is->audio_diff_avg_coef,is->audio_diff_threshold );
        is->audio_tid = SDL_CreateThreadEx(&is->_audio_tid, audio_thread, ffp, "ff_audio_dec");
        SDL_AoutPauseAudio(ffp->aout, 1);
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        packet_queue_start(&is->videoq);
        decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);
        ffp->node_vdec = ffpipeline_open_video_decoder(ffp->pipeline, ffp);
        if (!ffp->node_vdec)
            goto fail;
        is->video_tid = SDL_CreateThreadEx(&is->_video_tid, video_thread, ffp, "ff_video_dec");
        is->queue_attachments_req = 1;

        if(is->video_st->avg_frame_rate.den && is->video_st->avg_frame_rate.num) {
            double fps = av_q2d(is->video_st->avg_frame_rate);
            if (fps > ffp->max_fps && fps < 100.0) {
                is->is_video_high_fps = 1;
                ALOGI("fps: %lf (too high)\n", fps);
            } else {
                ALOGI("fps: %lf (normal)\n", fps);
            }
        }
        if(is->video_st->r_frame_rate.den && is->video_st->r_frame_rate.num) {
            double tbr = av_q2d(is->video_st->r_frame_rate);
            if (tbr > ffp->max_fps && tbr < 100.0) {
                is->is_video_high_fps = 1;
                ALOGI("fps: %lf (too high)\n", tbr);
            } else {
                ALOGI("fps: %lf (normal)\n", tbr);
            }
        }

        if (is->is_video_high_fps) {
            avctx->skip_frame       = FFMAX(avctx->skip_frame, AVDISCARD_NONREF);
            avctx->skip_loop_filter = FFMAX(avctx->skip_loop_filter, AVDISCARD_NONREF);
            avctx->skip_idct        = FFMAX(avctx->skip_loop_filter, AVDISCARD_NONREF);
        }

        break;
    default:
        break;
    }
fail:
    av_dict_free(&opts);

    return ret;
}

static void stream_component_close(FFPlayer *ffp, int stream_index)
{
    if(NULL == ffp)
	return;
    VideoState *is = ffp->is;
    if(NULL == is)
	return;
    AVFormatContext *ic = is->ic;
    if(NULL == ic)
	return;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        packet_queue_abort(&is->audioq);

        frame_queue_signal(&is->sampq);
        SDL_AoutCloseAudio(ffp->aout);
        SDL_WaitThread(is->audio_tid, NULL);

        decoder_destroy(&is->auddec);
        packet_queue_flush(&is->audioq);
        swr_free(&is->swr_ctx);
        av_freep(&is->audio_buf1);
/**
 * add by gs
**/
        if(ffp->audioamplify!=NULL)
        {
            av_freep(&ffp->audioamplify->fdsp);
            av_freep(&ffp->audioamplify);
        }
/**
 * end by gs
**/
        is->audio_buf1_size = 0;
        is->audio_buf = NULL;

        break;
    case AVMEDIA_TYPE_VIDEO:
        packet_queue_abort(&is->videoq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        frame_queue_signal(&is->pictq);

        SDL_WaitThread(is->video_tid, NULL);

        decoder_destroy(&is->viddec);
        packet_queue_flush(&is->videoq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
    switch (avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    default:
        break;
    }
}

static int decode_interrupt_cb(void *ctx)
{
    VideoState *is = ctx;
    return is->abort_request;
}

static int is_realtime(AVFormatContext *s)
{
    if(   !strcmp(s->iformat->name, "rtp")
       || !strcmp(s->iformat->name, "rtsp")
       || !strcmp(s->iformat->name, "sdp")
    )
        return 1;

    if(s->pb && (   !strncmp(s->filename, "rtp:", 4)
                 || !strncmp(s->filename, "udp:", 4)
                 || !strncmp(s->filename, "p2p:", 4)
                 || !strncmp(s->filename, "rtmp:", 5))
    )
        return 1;
    return 0;
}

static int is_play_end(FFPlayer *ffp)
{
	VideoState *is = ffp->is;

	if(is->realtime)
		return 0;

	double pos= ffp_get_current_position_l(ffp);
	double duration= ffp_get_duration_l(ffp);

	if(pos <= 0 || duration <= 0)
		return 0;

	double percent = pos * 100 / duration;

	return percent > 98;
}

/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg)
{
    ALOGI("[%s:%d:%d]Starting time = %lld\n", __FUNCTION__,__LINE__, pthread_self(), av_gettime());
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    AVFormatContext *ic = NULL;
    int err, i, ret __unused;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int eof = 0;
    int64_t stream_start_time;
    int completed = 0;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t;
    AVDictionary **opts;
    int orig_nb_streams;
    SDL_mutex *wait_mutex = SDL_CreateMutex();
    int scan_all_pmts_set = 0;
    int last_error = 0;
    int64_t prev_io_tick_counter = 0;
    int64_t io_tick_counter = 0;
    int64_t start_stream_time = 0;
    int64_t last_buffer_check_time = 0;

    av_log(NULL, AV_LOG_INFO, "[%s:%d]set low latency = %d, max=%d, mid=%d, min = %d framedrop=%d.This mode only use in live stream\n", 
		__FUNCTION__,__LINE__,ffp->use_low_latency,ffp->buffer_cache_maxtime_ms,ffp->buffer_cache_midtime_ms, ffp->buffer_cache_mintime_ms,ffp->framedrop);
    // ffp->use_debug_mode = 1;

    is->eof = 0;
    is->cache_buffering_on = 0;
    is->audio_play_mode = AUDIO_NOMAL_PLAY;
	
    memset(st_index, -1, sizeof(st_index));
    is->last_video_stream = is->video_stream = -1;
    is->last_audio_stream = is->audio_stream = -1;

    ic = avformat_alloc_context();
    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = is;

    ic->event_cb = ff_event_cb;
    ic->ffp = ffp;
	
    if (!av_dict_get(ffp->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
        av_dict_set(&ffp->format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = 1;
    }
    if (ffp->format_control_message) {
        av_format_set_control_message_cb(ic, ffp_format_control_message);
        av_format_set_opaque(ic, ffp);
    }

    ALOGI("[%s:%d]Set para. localdir=%s, timeout=%d, header=%s\n", __FUNCTION__,__LINE__, ffp->localdir, ffp->timeout, is->header);  
/**
 * add by gs
**/
    ic->drm_thiz = ffp;
    if(ffp->analyzeduration>0)
        ic->max_analyze_duration2 = ffp->analyzeduration * 1000;
//    else
//        ic->max_analyze_duration2 = 30*AV_TIME_BASE;
    if(ffp->nobuffer)
        ic->flags |= AVFMT_FLAG_NOBUFFER;

    // if the local cache directory is setting and http protocol and not hls protocol
    if(ffp->localdir!=NULL
       && av_strstart(is->filename, "http:", NULL) !=0
       && strstr(is->filename, ".m3u8") == NULL
       && av_strnlen(is->filename,sizeof(is->filename))<sizeof(is->filename)-10 ) {
        av_dict_set(&ffp->format_opts, "localdir",ffp->localdir, 0);
        char tmp[4096] = {0};
        av_strlcpy(tmp,is->filename, sizeof(tmp));
        snprintf(is->filename,sizeof(is->filename),"httplocal:%s",tmp);
    }
    
    char tmp[4096] = {0};
    av_strlcpy(tmp,is->filename, sizeof(tmp));
    if( strncmp(tmp,"rtsp",4) != 0) {
    	if(ffp->timeout>0) {
        char tmp[100];
        snprintf(tmp, 90,"%d000",ffp->timeout);
        av_dict_set(&ffp->format_opts, "timeout",tmp, 0);
       }
    }

    is->ic = ic;
/**
 * end by gs
**/

    if(strlen(is->header) > 0){
	av_dict_set(&ffp->format_opts, "headers",is->header, 0);
    }

    // set progress callback
    err = avformat_open_input(&ic, is->filename, is->iformat, &ffp->format_opts);
    if (err < 0) {
	ALOGE("[%s:%d]avformat_open_input error. err =%d\n", __FUNCTION__,__LINE__,err);
        print_error(is->filename, err);
        ret = -1;
        //ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_TIMEOUT,1);
        ffp->last_error = FFP_ERROR_TIMEOUT;
        last_error = err;
        goto fail;
    }
    if ( ic->pb){
    	ic->pb->flux_speed = flux_speed;
	ic->pb->ffp_opaque = ffp;
    }
    ALOGI("[%s:%d]avformat_open_input done. time = %lld\n", __FUNCTION__,__LINE__, av_gettime());
    AVStream *lst;
    for (i = 0; i < ic->nb_streams; i++) {
	lst = ic->streams[i];
	ALOGI("[%s:%d]index=%d, time_base=%f\n", __FUNCTION__,__LINE__,  lst->index, av_q2d(lst->time_base));
    }

    if ( ic->pb)
    {
    	if(strlen(ic->pb->source_ip) > 0)
		ALOGI("[%s:%d]source_ip=%s.\n", __FUNCTION__,__LINE__, ic->pb->source_ip);
    }

    if (scan_all_pmts_set)
        av_dict_set(&ffp->format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

    if ((t = av_dict_get(ffp->format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
    }
    //is->ic = ic;

    if (ffp->genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;

    av_format_inject_global_side_data(ic);

    opts = setup_find_stream_info_opts(ic, ffp->codec_opts);
    orig_nb_streams = ic->nb_streams;

    err = avformat_find_stream_info(ic, opts);

    for (i = 0; i < orig_nb_streams; i++)
        av_dict_free(&opts[i]);
    av_freep(&opts);

    if (err < 0) {
        av_log(NULL, AV_LOG_WARNING,
               "%s: could not find codec parameters %d\n", is->filename,err);
        ret = -1;
        //ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,11);
        ffp->last_error = FFP_ERROR_UNSUPPORT;
        last_error = 11;
        goto fail;
    }

    if (ic->pb)
        ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

    if (ffp->seek_by_bytes < 0)
        ffp->seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);

    is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    ALOGI("[%s:%d]avformat_find_stream_info done max_frame_duration: %.3f. time = %lld\n", __FUNCTION__,__LINE__, is->max_frame_duration, av_gettime());

    /* if seeking requested, we execute it */
    if (ffp->start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = ffp->start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
                    is->filename, (double)timestamp / AV_TIME_BASE);
            ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_SEEKUNREACHABLE,ret);
        }
	 ALOGI("[%s:%d]avformat_seek_file done start_time=%0.3f. time = %lld\n", __FUNCTION__,__LINE__, ic->start_time, av_gettime());
    }

    is->realtime = is_realtime(ic);
    // if can't get hls,http protocol status
    // use is_realtime() 
    if ( ic->pb)
    {
    	if ( ic->pb->play_type == 0 )
    	{
    	    if(is->realtime)
    	    {
      	       ic->pb->play_type = 1;
    	    }else
    	    {
      	       ic->pb->play_type = 2;
    	    }
    	}
    }

    if (true || ffp->show_status)
        av_dump_format(ic, 0, is->filename, 0);

    int video_stream_count = 0;
    int h264_stream_count = 0;
    int first_h264_stream = -1;
    for (i = 0; i < ic->nb_streams; i++) {
        AVStream *st = ic->streams[i];
        enum AVMediaType type = st->codec->codec_type;
        st->discard = AVDISCARD_ALL;
        if (ffp->wanted_stream_spec[type] && st_index[type] == -1)
            if (avformat_match_stream_specifier(ic, st, ffp->wanted_stream_spec[type]) > 0)
                st_index[type] = i;

        // choose first h264
        AVCodecContext *codec = ic->streams[i]->codec;
        if (codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_count++;
            if (codec->codec_id == AV_CODEC_ID_H264) {
                h264_stream_count++;
                if (first_h264_stream < 0)
                    first_h264_stream = i;
            }
        }
    }
    if (video_stream_count > 1 && st_index[AVMEDIA_TYPE_VIDEO] < 0) {
        st_index[AVMEDIA_TYPE_VIDEO] = first_h264_stream;
        av_log(NULL, AV_LOG_WARNING, "multiple video stream found, prefer first h264 stream: %d\n", first_h264_stream);
    }
    if (!ffp->video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
    if (!ffp->audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                st_index[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);

    ksymeta_set_avformat_context_l(ffp->meta, ic);

    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0)
        ksymeta_set_int64_l(ffp->meta, KSYM_KEY_VIDEO_STREAM, st_index[AVMEDIA_TYPE_VIDEO]);
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0)
        ksymeta_set_int64_l(ffp->meta, KSYM_KEY_AUDIO_STREAM, st_index[AVMEDIA_TYPE_AUDIO]);

    is->show_mode = ffp->show_mode;

    // add by yangle2@kingsoft.com, 2015-9-11
    // ID: KSYP-2 
    is->refer_video_duration = -1;
    is->refer_video_sum= 0;
    is->refer_video_cnt = 0;
    is->decode_output_video_cc = 1;
    is->decode_output_audio_cc = 1;	

    /* open the streams */
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(ffp, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret = -1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret = stream_component_open(ffp, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (is->video_stream < 0 && is->audio_stream < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n", is->filename);
        ret = -1;
        //ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_NOFILE,2);
        ffp->last_error = FFP_ERROR_NOFILE;
        last_error = 2;
        goto fail;
    }
    if (is->audio_stream >= 0) {
        is->audioq.is_buffer_indicator = 1;
        is->buffer_indicator_queue = &is->audioq;
    } else if (is->video_stream >= 0) {
        is->videoq.is_buffer_indicator = 1;
        is->buffer_indicator_queue = &is->videoq;
    } else {
        //assert("invalid streams");
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNKNOWN,1);
    }

    if (ffp->infinite_buffer < 0 && is->realtime)
        ffp->infinite_buffer = 1;

    ffp->prepared = true;
    is->play_state = FFPLAY_STATE_PREPARED;	
    ALOGI("[%s:%d]Player to prepared\n", __FUNCTION__,__LINE__);
    ffp_notify_msg1(ffp, FFP_MSG_PREPARED);
    if (is->video_st && is->video_st->codec) {
        AVCodecContext *avctx = is->video_st->codec;
        ffp_notify_msg3(ffp, FFP_MSG_VIDEO_SIZE_CHANGED, avctx->width, avctx->height);
        ffp_notify_msg3(ffp, FFP_MSG_SAR_CHANGED, avctx->sample_aspect_ratio.num, avctx->sample_aspect_ratio.den);
    }
    if (ffp->auto_start) {
        av_log(NULL, AV_LOG_INFO,  "auto_start    FFP_REQ_START\n");
        ffp_notify_msg1(ffp, FFP_REQ_START);
        ffp->auto_start = 0;
    }

    start_stream_time = av_gettime() ;
    int audio_time_base_valid = is->audio_st && is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    int video_time_base_valid = is->video_st && is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;
    ALOGI("[%s:%d]Start downloading!***************************************\n", __FUNCTION__,__LINE__);
    ALOGI("[%s:%d]stream_t= %lld, starttime=%lld, duration=%lld, atb_valid=%d,vtb_valid=%d\n", __FUNCTION__,__LINE__,start_stream_time, is->ic->start_time,is->ic->duration,audio_time_base_valid,video_time_base_valid);
    if(audio_time_base_valid && is->audio_st)
	ALOGI("[%s:%d]index=%d, audio_time_base=%f\n", __FUNCTION__,__LINE__,is->audio_st->index,av_q2d(is->audio_st->time_base));
    if(video_time_base_valid && is->video_st)
	ALOGI("[%s:%d]index=%d, video_time_base=%f\n", __FUNCTION__,__LINE__,is->video_st->index,av_q2d(is->video_st->time_base));
    ALOGI("[%s:%d]av_sync_type=%d, use_low_latency=%d, realtime=%d, infinite_buffer=%d\n", __FUNCTION__,__LINE__,
		ffp->av_sync_type,ffp->use_low_latency,is->realtime, ffp->infinite_buffer);

	int last_calc_cache = 0;
    for (;;) {
        if (is->abort_request){
		ALOGE("[%s:%d]exit.\n", __FUNCTION__,__LINE__);
		break;
        }
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->paused &&
                (!strcmp(ic->iformat->name, "rtsp") ||
                 (ic->pb && !strncmp(ffp->input_filename, "mmsh:", 5)))) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            ALOGE("is->paused: eof:%d (error=%d) paused:%d req:%d step:%d\n", is->eof, ffp->error, is->paused, is->pause_req, is->step);
            SDL_Delay(20);
            continue;
        }
#endif
        if (is->seek_req) {
			last_calc_cache = 0;
			// add by luohengfeng@kingsoft.com, 2015-11-25
			// description: resolve seek_pos overflow
			int64_t seek_target = is->seek_pos;
			//if(is->seek_pos >= ffp_get_duration_l(ffp))
           	//{
               //seek_target = ffp_get_duration_l(ffp);  
           	//}
            int64_t seek_min    = is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            int64_t seek_max    = is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
// FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_pos/seek_rel variables
            ALOGE("[%s:%d]seek buffering 1 pause:%d req:%d seek_target:%lld seek_min:%lld seek_max:%lld seek_flags=%d step:%d",__FUNCTION__,__LINE__,
            			is->paused,is->pause_req,seek_target,seek_min,seek_max,is->seek_flags,is->step);
            if(!is->paused)
                ffp->auto_start = 1;
            ffp_toggle_buffering(ffp, 1);
            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR,  "%s: error while seeking\n", is->ic->filename);
                ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_SEEKUNREACHABLE,ret);
            } else {
                if (is->audio_stream >= 0) {
                    packet_queue_flush(&is->audioq);
                    packet_queue_put(&is->audioq, &flush_pkt);
                }

                if (is->video_stream >= 0) {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
                if (is->seek_flags & AVSEEK_FLAG_BYTE) {
                   set_clock(&is->extclk, NAN, 0);
                } else {
                   set_clock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
                }
            }
            ffp->current_high_water_mark_in_ms = ffp->start_high_water_mark_in_ms;
            is->seek_req = 0;
            is->queue_attachments_req = 1;
            is->eof = 0;

            completed = 0;
            SDL_LockMutex(ffp->is->play_mutex);
            if (ffp->auto_start) {
                is->pause_req = 0;
                is->buffering_on = 1;
                stream_update_pause_l(ffp);
                ffp->auto_start = is->paused;
               ALOGE("seek: auto_start:%d pause:%d req:%d\n",ffp->auto_start,is->paused,is->pause_req);
            } else
                is->step = 2;
            if (is->pause_req)
                step_to_next_frame_l(ffp);
            SDL_UnlockMutex(ffp->is->play_mutex);
            ffp_notify_msg1(ffp, FFP_MSG_SEEK_COMPLETE);
            ALOGE("seek buffering 2 pause:%d req:%d step:%d auto:%d",is->paused,is->pause_req,is->step,ffp->auto_start);
            ffp_toggle_buffering(ffp, 1);
        }
        if (is->queue_attachments_req) {
            if (is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                AVPacket copy;
                if ((ret = av_copy_packet(&copy, &is->video_st->attached_pic)) < 0) {
                    ffp->last_error = FFP_ERROR_MEM;
                    last_error = 6;
                    goto fail;
                }
                packet_queue_put(&is->videoq, &copy);
                packet_queue_put_nullpacket(&is->videoq, is->video_stream);
            }
            is->queue_attachments_req = 0;
        }

        ffp->infinite_buffer = 1;
        /* if the queue are full, no need to read more */
        if (ffp->infinite_buffer<1 && !is->seek_req &&
              (is->audioq.size + is->videoq.size > ffp->max_buffer_size
            || (   (is->audioq   .nb_packets > MIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
                && (is->videoq   .nb_packets > MIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request
                    || (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))))) {
            if (!is->eof) {
//                ALOGE("ffp_toggle_buffering: full paused:%d req:%d step:%d\n", is->paused, is->pause_req, is->step);
                if(is->paused && !ffp->auto_start) {
                    is->pause_req = 1;
                }
                ffp_toggle_buffering(ffp, 0);
                is->pause_req = 0;
            }
            /* wait 10 ms */
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }
        if (is->eof && (!is->paused || completed) &&
            (!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0) || is->audio_play_mode == AUDIO_MUTE_PLAY) &&
            (!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {
            // check play complete to loop play again.
            if (ffp->loop != 1 && (!ffp->loop || --ffp->loop)) {
                stream_seek(is, ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0, 0, 0);
            } else if (ffp->autoexit) {
                ret = AVERROR_EOF;
                goto fail;
            } else {
                if (completed) {
                    //av_log(NULL, AV_LOG_INFO, "ffp_toggle_buffering: eof\n");
                    SDL_LockMutex(wait_mutex);
                    // infinite wait may block shutdown
                    while(!is->abort_request && !is->seek_req)
                        SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 100);
                    SDL_UnlockMutex(wait_mutex);
                    if (!is->abort_request)
                        continue;
                } else {
                    completed = 1;
                    ffp->auto_start = 0;

                    // TODO: 0 it's a bit early to notify complete here
                    ffp_toggle_buffering(ffp, 0);
                    toggle_pause(ffp, 1);
                    //if ((ffp->error == 0 || ffp->error == AVERROR_EOF) && is_play_end(ffp)) {
                    if ((ffp->error == 0 || ffp->error == AVERROR_EOF) && is->realtime == 0) {
    			   ALOGI("[%s:%d]Stream complete.aserial=%d vserial=%d vp=%d ap=%d vf=%d af=%d vcache=%f, acache=%f\n", __FUNCTION__,__LINE__, 
				is->audioq.serial,is->videoq.serial,is->videoq.nb_packets, is->audioq.nb_packets, is->pictq.size, is->sampq.size, 
				get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_VIDEO), get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_AUDIO));					
                        ffp_notify_msg1(ffp, FFP_MSG_COMPLETED);
                    } else {
    			   ALOGI("[%s:%d]Stream error.aserial=%d vserial=%d vp=%d ap=%d vf=%d af=%d vcache=%f, acache=%f\n", __FUNCTION__,__LINE__, 
				is->audioq.serial,is->videoq.serial,is->videoq.nb_packets, is->audioq.nb_packets, is->pictq.size, is->sampq.size, 
				get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_VIDEO), get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_AUDIO));	                   
			   ffp_notify_msg3(ffp, FFP_MSG_ERROR,FFP_ERROR_TIMEOUT,ffp->error);
                    }
                }
            }
        }

        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            int pb_eof = 0;
            int pb_error = 0;

	     // add by yangle2@kingsoft.com, 2015-11-5
	     // check to try again. wait and try read again.
	     if (ret == AVERROR(EAGAIN) ||  ic->pb->error == AVERROR(EAGAIN)){
		 	ALOGE("[%s:%d]ret:%d error:%d, eof:%d ffp->error:%d EAGAIN!\n", __FUNCTION__, __LINE__, ret,  ic->pb->error, is->eof,ffp->error);
			ffp->error=0;
            		SDL_LockMutex(wait_mutex);
            		SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            		SDL_UnlockMutex(wait_mutex);
            		continue;	 	
	     }

            if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
                pb_eof = 1;
		  pb_error = AVERROR_EOF;			// add eof error info.add by yangle2@kingsoft.com
                // check error later
            }
	     if(ret == AVERROR_EIO) {
		  pb_eof = 1;
		  pb_error = AVERROR_EIO;
	     }
            if (ic->pb && ic->pb->error) {
                pb_eof = 1;
                pb_error = ic->pb->error;
            }
            if (ret == AVERROR_EXIT) {
                pb_eof = 1;
                pb_error = AVERROR_EXIT;
            }

            if (pb_eof != 0) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
#ifdef FFP_MERGE
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
#endif
                is->eof = 1;
            }
            if (pb_error != 0) {
                if (is->video_stream >= 0)
                    packet_queue_put_nullpacket(&is->videoq, is->video_stream);
                if (is->audio_stream >= 0)
                    packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
#ifdef FFP_MERGE
                if (is->subtitle_stream >= 0)
                    packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
#endif
                is->eof = 1;
                ffp->error = pb_error;

		  ALOGI("[%s:%d]av_read_frame ret:%d error:%d", __FUNCTION__, __LINE__,ret,ffp->error);
                // break;
            } else if(ret > 0){
                ffp->error = 0;
            }
            if (is->eof) {
		  // play the cache frame with no buffer.
                ffp_toggle_buffering(ffp, 0);
                SDL_Delay(1000);
            }
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);

			// add by luohengfeng@kingsoft.com, 2015-11-25
			// description: resolve cache progress error bug
			if ( 0 == last_calc_cache) {
				ffp_check_buffering_l(ffp,ret);
				last_calc_cache = 1;
			}
            continue;
        }
	 else {
            is->eof = 0;
        }

	 // --------------------------------------------------
	 // debug info, set behind packet_queue_put to  avoid crash issue. add by yangle2@kingsoft.com.
	 if(last_buffer_check_time + 50000 <  av_gettime()){
		last_buffer_check_time = av_gettime() ;
		if(ffp->use_debug_mode ) {
    			ALOGI("[%s:%d]downloading.vfnum=%d afnum=%d, vcache=%f, acache=%f, position=%ld vclock=%f, aclock=%f, drop_frame=%d, mode=%d\n", 
				__FUNCTION__,__LINE__, 
				(is->videoq.nb_packets + is->pictq.size), (is->audioq.nb_packets+is->sampq.size), 
				get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_VIDEO), get_stream_cache_time_ms(ffp, AVMEDIA_TYPE_AUDIO), 
				get_current_position_l(is),get_clock(&is->vidclk),get_clock(&is->audclk),is->frame_drops_early,is->audio_play_mode);
       		ALOGI("[%s:%d]videoq.duration=%lld pictq.duration=%f, audioq.duration=%lld, sampq.duration=%f\n", __FUNCTION__,__LINE__, 
				is->videoq.duration, is->pictq.duration, is->audioq.duration, is->sampq.duration);          
			if(pkt->stream_index == is->audio_stream){
				ALOGI("[%s:%d]audio stream. pkt_pts=%d\n", __FUNCTION__,__LINE__,pkt->pts); 
			}
			else{
				ALOGI("[%s:%d]video stream. pkt_pts=%d\n", __FUNCTION__,__LINE__,pkt->pts); 
			}
		}

		ffp_notify_msg3(ffp, FFP_MSG_DEMUX_CACHE_DATA,is->videoq.nb_packets, is->audioq.nb_packets);
		ffp_notify_msg3(ffp, FFP_MSG_DECODE_CACHE_DATA,frame_queue_nb_remaining(&is->pictq), frame_queue_nb_remaining(&is->sampq));
		ffp_notify_msg3(ffp, FFP_MSG_PLAY_TIME, abs(get_clock(&is->audclk)-get_clock(&is->vidclk))*1000, 0);
	 }	

        /* check if packet is in play range specified by user, then queue, otherwise discard */
        stream_start_time = ic->streams[pkt->stream_index]->start_time;
        pkt_in_play_range = ffp->duration == AV_NOPTS_VALUE ||
                (pkt->pts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                (double)(ffp->start_time != AV_NOPTS_VALUE ? ffp->start_time : 0) / 1000000
                <= ((double)ffp->duration / 1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   && !(is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))) {
            packet_queue_put(&is->videoq, pkt);
        } else {
            av_free_packet(pkt);
        }

        io_tick_counter = SDL_GetTickHR();
        if (abs((int)(io_tick_counter - prev_io_tick_counter)) > BUFFERING_CHECK_PER_MILLISECONDS) {
            prev_io_tick_counter = io_tick_counter;
            ffp_check_buffering_l(ffp,ret);
        }	
        //SDL_Delay(0);
    }
    /* wait until the end */
    while (!is->abort_request) {
        SDL_Delay(100);
    }
    ret = 0;
 fail:
    /* close each stream */
    if (is->audio_stream >= 0)
        stream_component_close(ffp, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(ffp, is->video_stream);

    if (ic) {
        avformat_close_input(&is->ic);
        is->ic = NULL;
    }

    if (!ffp->prepared || !is->abort_request) {
        //ffp->last_error = last_error;
        //ffp_notify_msg2(ffp, FFP_MSG_ERROR, last_error);
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, ffp->last_error,last_error);
    }
    SDL_DestroyMutex(wait_mutex);

    ALOGI("[%s:%d]Ending time = %lld\n", __FUNCTION__,__LINE__, av_gettime());
    return ret;
}

static int video_refresh_thread(void *arg);
static VideoState *stream_open(FFPlayer *ffp, const char *filename, const char *header, AVInputFormat *iformat)
{
    assert(!ffp->is);
    VideoState *is;

    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    if(filename!=NULL)
    	av_strlcpy(is->filename, filename, sizeof(is->filename));
    if(header != NULL)
    	av_strlcpy(is->header, header, sizeof(is->header));
    is->iformat = iformat;
    is->ytop    = 0;
    is->xleft   = 0;
    is->paused = 1;
    is->play_state = FFPLAY_STATE_UNINIT;

    /* start video display */
    if (frame_queue_init(&is->pictq, &is->videoq, ffp->pictq_size, 1) < 0)
        goto fail;
    if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
        goto fail;

    packet_queue_init(&is->videoq);
    packet_queue_init(&is->audioq);

    is->continue_read_thread = SDL_CreateCond();

    init_clock(&is->vidclk, &is->videoq.serial);
    init_clock(&is->audclk, &is->audioq.serial);
    init_clock(&is->extclk, &is->extclk.serial);
    is->audio_clock_serial = -1;
    is->av_sync_type = ffp->av_sync_type;

    is->play_mutex = SDL_CreateMutex();

/**
 * add by gs for drm
**/
    is->drm_mutex = SDL_CreateMutex();
    is->drm_wait_mutex = SDL_CreateMutex();
    is->continue_drm_thread = SDL_CreateCond();
    is->drm_ctx = ksy_drm_init(16, 1);
    is->ic = NULL;
/**
 * end by gs
**/
    ffp->is = is;

    is->video_refresh_tid = SDL_CreateThreadEx(&is->_video_refresh_tid, video_refresh_thread, ffp, "ff_vout");
    if (!is->video_refresh_tid) {
        av_freep(&ffp->is);
        return NULL;
    }

    is->read_tid = SDL_CreateThreadEx(&is->_read_tid, read_thread, ffp, "ff_read");
    if (!is->read_tid) {
fail:
        is->abort_request = true;
        if (is->video_refresh_tid)
            SDL_WaitThread(is->video_refresh_tid, NULL);
        stream_close(is);
        return NULL;
    }
    return is;
}

static int ffplay_video_refresh_thread(void *arg)
{
    FFPlayer *ffp = arg;
    VideoState *is = ffp->is;
    double remaining_time = 0.0;
    while (!is->abort_request) {
        if (remaining_time > 0.0)
            av_usleep((int)(int64_t)(remaining_time * 1000000.0));
        remaining_time = REFRESH_RATE;
        // add by gs for pause bug 150410
        if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh || is->step))
            video_refresh(ffp, &remaining_time);
    }

    return 0;
}

static int video_refresh_thread(void *arg)
{
    FFPlayer *ffp = (FFPlayer *)arg;

    KSYFF_Pipenode *node = ffpipeline_open_video_output(ffp->pipeline, ffp);
    int ret = ffpipenode_run_sync(node);
    ffpipenode_free_p(&node);
    return ret;
}

static int lockmgr(void **mtx, enum AVLockOp op)
{
    switch (op) {
    case AV_LOCK_CREATE:
        *mtx = SDL_CreateMutex();
        if (!*mtx)
            return 1;
        return 0;
    case AV_LOCK_OBTAIN:
        return !!SDL_LockMutex(*mtx);
    case AV_LOCK_RELEASE:
        return !!SDL_UnlockMutex(*mtx);
    case AV_LOCK_DESTROY:
        SDL_DestroyMutex(*mtx);
        return 0;
    }
    return 1;
}

// FFP_MERGE: main

/*****************************************************************************
 * end last line in ffplay.c
 ****************************************************************************/

static bool g_ffmpeg_global_inited = false;
static bool g_ffmpeg_global_use_log_report = false;

static void ffp_log_callback_brief(void *ptr, int level, const char *fmt, va_list vl)
{
    int ffplv __unused = KSY_LOG_VERBOSE;
    if (level <= AV_LOG_ERROR)
        ffplv = KSY_LOG_ERROR;
    else if (level <= AV_LOG_WARNING)
        ffplv = KSY_LOG_WARN;
    else if (level <= AV_LOG_INFO)
        ffplv = KSY_LOG_INFO;
    else if (level <= AV_LOG_VERBOSE)
        ffplv = KSY_LOG_VERBOSE;
    else
        ffplv = KSY_LOG_DEBUG;

    if (level <= AV_LOG_INFO)
        VLOG(ffplv, KSY_LOG_TAG, fmt, vl);
}

static void ffp_log_callback_report(void *ptr, int level, const char *fmt, va_list vl)
{
    int ffplv __unused = KSY_LOG_VERBOSE;
    if (level <= AV_LOG_ERROR)
        ffplv = KSY_LOG_ERROR;
    else if (level <= AV_LOG_WARNING)
        ffplv = KSY_LOG_WARN;
    else if (level <= AV_LOG_INFO)
        ffplv = KSY_LOG_INFO;
    else if (level <= AV_LOG_VERBOSE)
        ffplv = KSY_LOG_VERBOSE;
    else
        ffplv = KSY_LOG_DEBUG;

    va_list vl2;
    char line[1024];
    static int print_prefix = 1;

    va_copy(vl2, vl);
//    av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);

    ALOG(ffplv, KSY_LOG_TAG, "%s", line);
}

void ffp_global_init()
{
    if (g_ffmpeg_global_inited)
        return;

    av_log_set_callback(log_callback_func);

    /* register all codecs, demux and protocols */
    avcodec_register_all();
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
#if CONFIG_AVFILTER
    avfilter_register_all();
#endif
    av_register_all();
    avformat_network_init();

    av_lockmgr_register(lockmgr);
//    if (g_ffmpeg_global_use_log_report) {
//        av_log_set_callback(ffp_log_callback_report);
//    } else {
//        av_log_set_callback(ffp_log_callback_brief);
//    }
    av_drm_register(get_drm_key,decode_drm, NULL);

    av_init_packet(&flush_pkt);
    flush_pkt.data = (uint8_t *)&flush_pkt;

    g_ffmpeg_global_inited = true;
}

void ffp_global_uninit()
{
    if (!g_ffmpeg_global_inited)
        return;

    av_lockmgr_register(NULL);

#if CONFIG_AVFILTER
    avfilter_uninit();
    //av_freep(&vfilters);
#endif
    avformat_network_deinit();

    g_ffmpeg_global_inited = false;
}

void ffp_global_set_log_report(int use_report)
{
    g_ffmpeg_global_use_log_report = use_report;
    if (use_report) {
        av_log_set_callback(ffp_log_callback_report);
    } else {
        av_log_set_callback(ffp_log_callback_brief);
    }
}

void ffp_io_stat_register(void (*cb)(const char *url, int type, int bytes))
{
    // avksy_io_stat_register(cb);
}

void ffp_io_stat_complete_register(void (*cb)(const char *url,
                                              int64_t read_bytes, int64_t total_size,
                                              int64_t elpased_time, int64_t total_duration))
{
    // avksy_io_stat_complete_register(cb);
}

FFPlayer *ffp_create()
{
    FFPlayer* ffp = (FFPlayer*) av_mallocz(sizeof(FFPlayer));
    if (!ffp)
        return NULL;

    msg_queue_init(&ffp->msg_queue);
    ffp_reset_internal(ffp);
    ffp->meta = ksymeta_create();
    player_handle = ffp;
    return ffp;
}

void ffp_destroy(FFPlayer *ffp)
{
    if (!ffp)
        return;

    if (ffp && ffp->is) {
        av_log(NULL, AV_LOG_WARNING, "ffp_destroy_ffplayer: force stream_close()");
        stream_close(ffp->is);
        ffp->is = NULL;
    }

    SDL_VoutFreeP(&ffp->vout);
    SDL_AoutFreeP(&ffp->aout);
    ffpipenode_free_p(&ffp->node_vdec);
    ffpipeline_free_p(&ffp->pipeline);
    ffp_reset_internal(ffp);

    msg_queue_destroy(&ffp->msg_queue);

    ksymeta_destroy_p(&ffp->meta);

    av_free(ffp);
}

void ffp_destroy_p(FFPlayer **pffp)
{
    if (!pffp)
        return;

    ALOGI("[%s:%d]\n", __FUNCTION__,__LINE__);
    ffp_destroy(*pffp);
    *pffp = NULL;
}

void ffp_set_format_callback(FFPlayer *ffp, ksy_format_control_message cb, void *opaque)
{
    ffp->format_control_message = cb;
    ffp->format_control_opaque  = opaque;
}

void ffp_set_format_option(FFPlayer *ffp, const char *name, const char *value)
{
    if (!ffp)
        return;

    av_dict_set(&ffp->format_opts, name, value, 0);
}

void ffp_set_codec_option(FFPlayer *ffp, const char *name, const char *value)
{
    if (!ffp)
        return;

    av_dict_set(&ffp->codec_opts, name, value, 0);
}

void ffp_set_sws_option(FFPlayer *ffp, const char *name, const char *value)
{
    if (!ffp)
        return;

    av_dict_set(&ffp->sws_opts, name, value, 0);
}

void ffp_set_overlay_format(FFPlayer *ffp, int chroma_fourcc)
{
    switch (chroma_fourcc) {
        case SDL_FCC_I420:
        case SDL_FCC_YV12:
        case SDL_FCC_RV16:
        case SDL_FCC_RV24:
        case SDL_FCC_RV32:
            ffp->overlay_format = chroma_fourcc;
            break;
        default:
            ALOGE("ffp_set_overlay_format: unknown chroma fourcc: %d\n", chroma_fourcc);
            ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_UNSUPPORT,12);
            break;
    }
}

void ffp_set_picture_queue_capicity(FFPlayer *ffp, int frame_count)
{
    ffp->pictq_size = frame_count;
}

void ffp_set_max_fps(FFPlayer *ffp, int max_fps)
{
    ffp->max_fps = max_fps;
}

void ffp_set_framedrop(FFPlayer *ffp, int framedrop)
{
    ffp->framedrop = framedrop;
}

int ffp_get_video_codec_info(FFPlayer *ffp, char **codec_info)
{
    if (!codec_info)
        return -1;

    // FIXME: not thread-safe
    if (ffp->video_codec_info) {
        *codec_info = strdup(ffp->video_codec_info);
    } else {
        *codec_info = NULL;
    }
    return 0;
}

int ffp_get_audio_codec_info(FFPlayer *ffp, char **codec_info)
{
    if (!codec_info)
        return -1;

    // FIXME: not thread-safe
    if (ffp->audio_codec_info) {
        *codec_info = strdup(ffp->audio_codec_info);
    } else {
        *codec_info = NULL;
    }
    return 0;
}

int ffp_prepare_async_l(FFPlayer *ffp, const char *file_name, const char *header)
{
    assert(ffp);
    assert(!ffp->is);
    assert(file_name);


    ALOGD("[%s:%d:%d] file_name = %s, header = %s\n", __FUNCTION__, __LINE__, pthread_self(), (file_name!=NULL) ? file_name : "null", (header!=NULL) ? header : "null");
    VideoState *is = stream_open(ffp, file_name, header, NULL);
    if (!is) {
        av_log(NULL, AV_LOG_WARNING, "ffp_prepare_async_l: stream_open failed OOM");
        ffp_notify_msg3(ffp, FFP_MSG_ERROR, FFP_ERROR_MEM,7);
        return EKSY_OUT_OF_MEMORY;
    }

    is->play_state = FFPLAY_STATE_INITED;
    ffp->is = is;
    ALOGI("[%s:%d]Player to inited\n", __FUNCTION__,__LINE__);
    return 0;
}

int ffp_start_from_l(FFPlayer *ffp, long msec)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EKSY_NULL_IS_PTR;
    av_log(NULL, AV_LOG_ERROR,  "ffp_start_at_l auto:%d paused:%d req:%d step:%d\n",ffp->auto_start,is->paused,is->pause_req,is->step);
    ffp->auto_start = 1;
    ffp_toggle_buffering(ffp, 1);
    ffp_seek_to_l(ffp, msec);
    if (is){		
    	is->play_state = FFPLAY_STATE_PLAYED;
	ALOGI("[%s:%d]Player to Played\n", __FUNCTION__,__LINE__);
    }
    return 0;
}

int ffp_start_l(FFPlayer *ffp)
{
    // ALOGE("ffp_start_l\n");
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EKSY_NULL_IS_PTR;
    toggle_pause(ffp, 0);
    if (is){		
    	is->play_state = FFPLAY_STATE_PLAYED;
	ALOGI("[%s:%d]Player to Played\n", __FUNCTION__,__LINE__);
    }
    return 0;
}

int ffp_pause_l(FFPlayer *ffp)
{
    // ALOGE("ffp_pause_l\n");
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EKSY_NULL_IS_PTR;

    toggle_pause(ffp, 1);
    if (is)	{
    	is->play_state = FFPLAY_STATE_PAUSED;
	ALOGI("[%s:%d]Player to Paused\n", __FUNCTION__,__LINE__);
    }
    return 0;
}

int ffp_stop_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (is)
        is->abort_request = 1;

    msg_queue_abort(&ffp->msg_queue);
    if (is) {	
    	is->play_state = FFPLAY_STATE_STOPED;
	ALOGI("[%s:%d]Player to Stoped\n", __FUNCTION__,__LINE__);
    }
    return 0;
}

int ffp_wait_stop_l(FFPlayer *ffp)
{
    assert(ffp);

    if (ffp->is) {
        ffp_stop_l(ffp);
        stream_close(ffp->is);
        ffp->is = NULL;
    }
    return 0;
}

int ffp_seek_to_l(FFPlayer *ffp, long msec)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is)
        return EKSY_NULL_IS_PTR;

    int64_t seek_pos = milliseconds_to_fftime(msec);
    int64_t start_time = is->ic->start_time;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        seek_pos += start_time;

    // FIXME: 9 seek by bytes
    // FIXME: 9 seek out of range
    // FIXME: 9 seekable
    ALOGI("stream_seek %"PRId64"(%d) + %"PRId64", \n", seek_pos, (int)msec, start_time);
    stream_seek(is, seek_pos, 0, 0);
    return 0;
}

long get_current_position_l(VideoState *is)
{
    if (!is || !is->ic) {
	 //ALOGE("[%s:%d]is or is->ic is null\n", __FUNCTION__,__LINE__);
        return 0;   
    }

    int64_t start_time = is->ic->start_time;
    int64_t start_diff = 0;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        start_diff = fftime_to_milliseconds(start_time);

    int64_t pos = 0;
    double pos_clock = get_master_clock(is);
    if (isnan(pos_clock)) {
       //ALOGE("[%s:%d]pos = seek_pos: %d\n", __FUNCTION__,__LINE__, (int)is->seek_pos);
        pos = fftime_to_milliseconds(is->seek_pos);
    } else {
        //ALOGE("[%s:%d]pos = pos_clock: %f\n", __FUNCTION__,__LINE__,pos_clock);
        pos = pos_clock * 1000;
    }

    if (pos < 0 || pos < start_diff)
        return 0;

    int64_t adjust_pos = pos - start_diff;
    //ALOGE("[%s:%d]pos=%ld\n", __FUNCTION__,__LINE__,(long)adjust_pos);
    return (long)adjust_pos;
}

long ffp_get_current_position_l(FFPlayer *ffp)
{
    assert(ffp);
    if(NULL == ffp){
	ALOGE("[%s:%d]ffp is null\n", __FUNCTION__,__LINE__);
	return 0;
    }
    VideoState *is = ffp->is;

    return get_current_position_l(is);
}

long ffp_get_duration_l(FFPlayer *ffp)
{
    assert(ffp);
    VideoState *is = ffp->is;
    if (!is || !is->ic)
        return 0;

    int64_t start_time = is->ic->start_time;
    int64_t start_diff = 0;
    if (start_time > 0 && start_time != AV_NOPTS_VALUE)
        start_diff = fftime_to_milliseconds(start_time);

    int64_t duration = fftime_to_milliseconds(is->ic->duration);
    if(duration <= 0)
	return 0;

    // modify by luohengfeng@kingsoft.com
    // return the realy duration
    //ALOGE("dur=%ld\n", (long)duration);
    return duration;

    /*if (duration < 0 || duration < start_diff)
        return 0;

    int64_t adjust_duration = duration - start_diff;
    return (long)adjust_duration;*/
}

long ffp_get_playable_duration_l(FFPlayer *ffp)
{
    assert(ffp);
    if (!ffp)
        return 0;

    return (long)ffp->playable_duration_ms;
}

void ffp_packet_queue_init(PacketQueue *q)
{
    return packet_queue_init(q);
}

void ffp_packet_queue_destroy(PacketQueue *q)
{
    return packet_queue_destroy(q);
}

void ffp_packet_queue_abort(PacketQueue *q)
{
    return packet_queue_abort(q);
}

void ffp_packet_queue_start(PacketQueue *q)
{
    return packet_queue_start(q);
}

void ffp_packet_queue_flush(PacketQueue *q)
{
    return packet_queue_flush(q);
}

int ffp_packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
    return packet_queue_get(q, pkt, block, serial);
}

int ffp_packet_queue_get_or_buffering(FFPlayer *ffp, PacketQueue *q, AVPacket *pkt, int *serial, int *finished)
{
    return packet_queue_get_or_buffering(ffp, q, pkt, serial, finished);
}

int ffp_packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    return packet_queue_put(q, pkt);
}

bool ffp_is_flush_packet(AVPacket *pkt)
{
    if (!pkt)
        return false;

    return pkt->data == flush_pkt.data;
}

Frame *ffp_frame_queue_peek_writable(FrameQueue *f)
{
    return frame_queue_peek_writable(f);
}

void ffp_frame_queue_push(FrameQueue *f)
{
    return frame_queue_push(f);
}

void ffp_toggle_buffering_l(FFPlayer *ffp, int buffering_on)
{
    VideoState *is = ffp->is;
    ALOGW("ffp_toggle_buffering_l buffering_on:%d auto:%d paused:%d req:%d step:%d\n",buffering_on, ffp->auto_start,is->paused,is->pause_req,is->step);
    if (buffering_on && !is->buffering_on) {
        ALOGW("ffp_toggle_buffering_l: start\n");
        is->buffering_on = 1;
        stream_update_pause_l(ffp);
        ffp_notify_msg1(ffp, FFP_MSG_BUFFERING_START);
    } else if (!buffering_on && is->buffering_on){
        ALOGW("ffp_toggle_buffering_l: end\n");
        is->buffering_on = 0;
        stream_update_pause_l(ffp);
        ffp_notify_msg1(ffp, FFP_MSG_BUFFERING_END);
    }
}

void ffp_toggle_buffering(FFPlayer *ffp, int start_buffering)
{
    SDL_LockMutex(ffp->is->play_mutex);
    ffp_toggle_buffering_l(ffp, start_buffering);
    SDL_UnlockMutex(ffp->is->play_mutex);
}

void ffp_check_buffering_l(FFPlayer *ffp,int error)
{
    VideoState *is            = ffp->is;
    int hwm_in_ms             = ffp->current_high_water_mark_in_ms; // use fast water mark for first loading
    int buf_size_percent      = -1;
    int buf_time_percent      = -1;
    int hwm_in_bytes          = ffp->high_water_mark_in_bytes;
    int need_stop_buffering  = 0;
    int audio_time_base_valid = is->audio_st && is->audio_st->time_base.den > 0 && is->audio_st->time_base.num > 0;
    int video_time_base_valid = is->video_st && is->video_st->time_base.den > 0 && is->video_st->time_base.num > 0;
    int64_t buf_time_position = -1;
    if (hwm_in_ms > 0) {
        int     cached_duration_in_ms = -1;
        int64_t audio_cached_duration = -1;
        int64_t video_cached_duration = -1;

        if (is->audio_st && audio_time_base_valid) {
            audio_cached_duration = is->audioq.duration * av_q2d(is->audio_st->time_base) * 1000;
        }

        if (is->video_st && video_time_base_valid) {
            video_cached_duration = is->videoq.duration * av_q2d(is->video_st->time_base) * 1000;
        }

        is->audioq_duration = audio_cached_duration;
        is->videoq_duration = video_cached_duration;

        if (video_cached_duration > 0 && audio_cached_duration > 0) {
            cached_duration_in_ms = (int)KSYMIN(video_cached_duration, audio_cached_duration);
        } else if (video_cached_duration > 0) {
            cached_duration_in_ms = (int)video_cached_duration;
        } else if (audio_cached_duration > 0) {
            cached_duration_in_ms = (int)audio_cached_duration;
        }

        if (cached_duration_in_ms >= 0) {
            buf_time_position = ffp_get_current_position_l(ffp) + cached_duration_in_ms;
            ffp->playable_duration_ms = buf_time_position;

            buf_time_percent = (int)av_rescale(cached_duration_in_ms, 1005, hwm_in_ms * 10);
#ifdef FFP_NOTIFY_BUF_TIME
            ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_TIME_UPDATE, cached_duration_in_ms, hwm_in_ms);
#endif
        }
    }

    int cached_size = is->audioq.size + is->videoq.size;
    if (hwm_in_bytes > 0) {
        buf_size_percent = (int)av_rescale(cached_size, 1005, hwm_in_bytes * 10);
#ifdef FFP_SHOW_DEMUX_CACHE
        ALOGE("size cache=%%%d (%d/%d)\n", buf_size_percent, cached_size, hwm_in_bytes);
#endif
#ifdef FFP_NOTIFY_BUF_BYTES
        ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_BYTES_UPDATE, cached_size, hwm_in_bytes);
#endif
    }

    int buf_percent = -1;
    if (buf_time_percent >= 0) {
        // alwas depend on cache duration if valid
        if (buf_time_percent >= 100) {
/*add by gs for drm */
            AVFormatContext* ic = is->ic;
            if(ic->drm_flags==1) {

            } else {
                need_stop_buffering = 1;
            }
        }
        buf_percent = buf_time_percent;
    } else {
        if (buf_size_percent >= 100)
            need_stop_buffering = 1;
        buf_percent = buf_size_percent;
    }

    if (buf_time_percent >= 0 && buf_size_percent >= 0) {
        buf_percent = FFMIN(buf_time_percent, buf_size_percent);
    }
    if (buf_percent) {
		if( error == AVERROR_EOF )
		  buf_time_position = (long)ffp_get_duration_l(ffp);
        ffp_notify_msg3(ffp, FFP_MSG_BUFFERING_UPDATE, (int)buf_time_position, buf_percent);
    }

    // --------------------------------------------------
    // add by yangle2@kingsoft.com, 2015-9-23
    // description: add the buffering machine, when the receive buffer underflow
    // ID: KSYP-9
    if(is->play_state == FFPLAY_STATE_PLAYED) { 
	double media_cache_ms= get_media_cache_time_ms(ffp, get_master_sync_type(is));
	if (!is->buffering_on && media_cache_ms>=0 && media_cache_ms < ffp->buffer_cache_mintime_ms) { 
		ffp_toggle_buffering(ffp,1);
		is->cache_buffering_on = is->buffering_on;
		ALOGI("[%s:%d]Enter buffering.media_cache_ms=%f,cache_buffering_on=%d,v_packet=%d,a_packet=%d\n", __FUNCTION__,__LINE__,
			media_cache_ms,is->cache_buffering_on,(is->videoq.nb_packets + is->pictq.size), (is->audioq.nb_packets+is->sampq.size)); 
	}
	else if(is->buffering_on && media_cache_ms > ffp->buffer_cache_midtime_ms) {
		ffp_toggle_buffering(ffp,0);
		is->cache_buffering_on = is->buffering_on;
		ALOGI("[%s:%d]Leave buffering.media_cache_ms=%f,cache_buffering_on=%d,v_packet=%d,a_packet=%d\n", __FUNCTION__,__LINE__,
			media_cache_ms,is->cache_buffering_on,(is->videoq.nb_packets + is->pictq.size), (is->audioq.nb_packets+is->sampq.size)); 
	}
   }

    // buffering only use in vod mode. that deal in read_thread.
    // add by yangle2@kingsoft.com.2015-11-12
/*
    if (need_stop_buffering && is->cache_buffering_on == 0) {
        if (hwm_in_ms < ffp->next_high_water_mark_in_ms) {
            hwm_in_ms = ffp->next_high_water_mark_in_ms;
        } else {
            hwm_in_ms *= 2;
        }

        if (hwm_in_ms > ffp->max_high_water_mark_in_ms)
            hwm_in_ms = ffp->max_high_water_mark_in_ms;

        ffp->current_high_water_mark_in_ms = hwm_in_ms;

        if (is->buffer_indicator_queue && is->buffer_indicator_queue->nb_packets > 0) {
           ALOGE("ffp_check_buffering_l, paused:%d req:%d step:%d",is->paused,is->pause_req,is->step);
            if(is->paused && !ffp->auto_start)
                is->pause_req = 1;
            ffp_toggle_buffering(ffp, 0);
            is->pause_req = 0;
        }
    }
 */
}

int ffp_video_decode_thread(FFPlayer *ffp)
{
    return ffplay_video_decode_thread(ffp);
}

int ffp_video_refresh_thread(FFPlayer *ffp)
{
    return ffplay_video_refresh_thread(ffp);
}

void ffp_set_video_codec_info(FFPlayer *ffp, const char *module, const char *codec)
{
    av_freep(&ffp->video_codec_info);
    ffp->video_codec_info = NULL;
    ffp->video_codec_info = av_asprintf("%s, %s", module ? module : "", codec ? codec : "");
    if ( ffp->video_codec_info)
    ALOGI("VideoCodec: %s", ffp->video_codec_info);
}

void ffp_set_audio_codec_info(FFPlayer *ffp, const char *module, const char *codec)
{
    av_freep(&ffp->audio_codec_info);
    ffp->audio_codec_info = av_asprintf("%s, %s", module ? module : "", codec ? codec : "");
    ALOGI("AudioCodec: %s", ffp->audio_codec_info);
}

int ffp_get_video_rotate_degrees(FFPlayer *ffp)
{
    VideoState *is = ffp->is;
    if (!is)
        return 0;

    int theta  = abs((int)((int64_t)round(fabs(get_rotation(is->video_st))) % 360));
    switch (theta) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        case 360:
            theta = 0;
            break;
        default:
            ALOGW("Unknown rotate degress: %d\n", theta);
            theta = 0;
            break;
    }

    return theta;
}

KSYMediaMeta *ffp_get_meta_l(FFPlayer *ffp)
{
    if (!ffp)
        return NULL;

    return ffp->meta;
}

static int ffp_format_control_message(struct AVFormatContext *s, int type,
                                      void *data, size_t data_size)
{
    if (s == NULL)
        return -1;

    FFPlayer *ffp = (FFPlayer *)s->opaque;
    if (ffp == NULL)
        return -1;

    if (!ffp->format_control_message)
        return -1;

    return ffp->format_control_message(ffp->format_control_opaque, type, data, data_size);
}
