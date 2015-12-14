/*
 * ffpipenode_ffplay_vdec.c
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

#include "ffpipenode_ffplay_vout.h"
#include "../ff_ffpipenode.h"
#include "../ff_ffplay.h"

typedef struct KSYFF_Pipenode_Opaque {
    FFPlayer *ffp;
} KSYFF_Pipenode_Opaque;

static void func_destroy(KSYFF_Pipenode *node)
{
    // do nothing
}

static int func_run_sync(KSYFF_Pipenode *node)
{
    KSYFF_Pipenode_Opaque *opaque = node->opaque;

    return ffp_video_decode_thread(opaque->ffp);
}

KSYFF_Pipenode *ffpipenode_create_video_decoder_from_ffplay(FFPlayer *ffp)
{
    KSYFF_Pipenode *node = ffpipenode_alloc(sizeof(KSYFF_Pipenode_Opaque));
    if (!node)
        return node;

    KSYFF_Pipenode_Opaque *opaque = node->opaque;
    opaque->ffp         = ffp;

    node->func_destroy  = func_destroy;
    node->func_run_sync = func_run_sync;

    ffp_set_video_codec_info(ffp, AVCODEC_MODULE_NAME, avcodec_get_name(ffp->is->viddec.avctx->codec_id));
    return node;
}
