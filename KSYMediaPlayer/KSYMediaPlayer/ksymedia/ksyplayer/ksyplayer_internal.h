/*
 * ksyplayer_internal.h
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

#ifndef KSYPLAYER_ANDROID__KSYPLAYER_INTERNAL_H
#define KSYPLAYER_ANDROID__KSYPLAYER_INTERNAL_H

#include <assert.h>
#include "ksysdl/ksysdl.h"
#include "ff_fferror.h"
#include "ff_ffplay.h"
#include "ksyplayer.h"

typedef struct KSYMediaPlayer {
    volatile int ref_count;
    pthread_mutex_t mutex;
    FFPlayer *ffplayer;

    int (*msg_loop)(void*);
    SDL_Thread *msg_thread;
    SDL_Thread _msg_thread;

    int mp_state;
    char *data_source;
    char *header;
    void *weak_thiz;

    int restart_from_beginning;
    int seek_req;
    long seek_msec;
} KSYMediaPlayer;

#endif
