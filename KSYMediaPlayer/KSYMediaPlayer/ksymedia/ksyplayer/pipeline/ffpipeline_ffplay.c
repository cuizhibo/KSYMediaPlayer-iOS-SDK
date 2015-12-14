/*
 * ffpipeline_ffplay.c
 *
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

#include "ffpipeline_ffplay.h"
#include "ffpipenode_ffplay_vdec.h"
#include "ffpipenode_ffplay_vout.h"
#include "../ff_ffplay.h"

static SDL_Class g_pipeline_class = {
    .name = "ffpipeline_ffplay",
};

 typedef struct KSYFF_Pipeline_Opaque {
    FFPlayer *ffp;
} KSYFF_Pipeline_Opaque;

static void func_destroy(KSYFF_Pipeline *pipeline)
{
    // do nothing
}

static KSYFF_Pipenode *func_open_video_decoder(KSYFF_Pipeline *pipeline, FFPlayer *ffp)
{
    return ffpipenode_create_video_decoder_from_ffplay(ffp);
}

static KSYFF_Pipenode *func_open_video_output(KSYFF_Pipeline *pipeline, FFPlayer *ffp)
{
    return ffpipenode_create_video_output_from_ffplay(ffp);
}

KSYFF_Pipeline *ffpipeline_create_from_ffplay(FFPlayer *ffp)
{
    KSYFF_Pipeline *pipeline = ffpipeline_alloc(&g_pipeline_class, sizeof(KSYFF_Pipeline_Opaque));
    if (!pipeline)
        return pipeline;

    KSYFF_Pipeline_Opaque *opaque = pipeline->opaque;
    opaque->ffp                   = ffp;

    pipeline->func_destroy            = func_destroy;
    pipeline->func_open_video_decoder = func_open_video_decoder;
    pipeline->func_open_video_output  = func_open_video_output;

    return pipeline;
}
