/*
 * ksyplayer_ios.c
 *
 * Copyright (c) 2013
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

#import "ksyplayer_ios.h"

#import "ksysdl/ios/ksysdl_ios.h"

#include <stdio.h>
#include <assert.h>
#include "ksyplayer/ff_fferror.h"
#include "ksyplayer/ff_ffplay.h"
#include "ksyplayer/ksyplayer.h"
#include "ksyplayer/ksyplayer_internal.h"
#include "ksyplayer/pipeline/ffpipeline_ffplay.h"

KSYMediaPlayer *ksymp_ios_create(int (*msg_loop)(void*))
{
    KSYMediaPlayer *mp = ksymp_create(msg_loop);
    if (!mp)
        goto fail;
    
    mp->ffplayer->vout = SDL_VoutIos_CreateForGLES2();
    if (!mp->ffplayer->vout)
        goto fail;
    
    mp->ffplayer->aout = SDL_AoutIos_CreateForAudioUnit();
    if (!mp->ffplayer->vout)
        goto fail;
    
    mp->ffplayer->pipeline = ffpipeline_create_from_ffplay(mp->ffplayer);
    if (!mp->ffplayer->pipeline)
        goto fail;
    // add by gs
    msg_queue_start(&mp->ffplayer->msg_queue);
    // released in msg_loop
    ksymp_inc_ref(mp);
    mp->msg_thread = SDL_CreateThreadEx(&mp->_msg_thread, mp->msg_loop, mp, "ff_msg_loop");
    ksymp_change_state_l(mp, MP_STATE_IDLE);
    
    //end
    
    return mp;
    
fail:
    ksymp_dec_ref_p(&mp);
    return NULL;
}

void ksymp_ios_set_glview_l(KSYMediaPlayer *mp, KSYSDLGLView *glView)
{
    assert(mp);
    assert(mp->ffplayer);
    assert(mp->ffplayer->vout);
    
    SDL_VoutIos_SetGLView(mp->ffplayer->vout, glView);
}

void ksymp_ios_set_glview(KSYMediaPlayer *mp, KSYSDLGLView *glView)
{
    assert(mp);
    MPTRACE("ksymp_ios_set_view(glView=%p)\n", (void*)glView);
    pthread_mutex_lock(&mp->mutex);
    ksymp_ios_set_glview_l(mp, glView);
    pthread_mutex_unlock(&mp->mutex);
    MPTRACE("ksymp_ios_set_view(glView=%p)=void\n", (void*)glView);
}
