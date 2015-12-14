/*
 * ff_ffpipeline.c
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

#include "ff_ffpipeline.h"
#include <stdlib.h>
#include <string.h>

KSYFF_Pipeline *ffpipeline_alloc(SDL_Class *opaque_class, size_t opaque_size)
{
    KSYFF_Pipeline *pipeline = (KSYFF_Pipeline*) calloc(1, sizeof(KSYFF_Pipeline));
    if (!pipeline)
        return NULL;

    pipeline->opaque_class = opaque_class;
    pipeline->opaque       = calloc(1, opaque_size);
    if (!pipeline->opaque) {
        free(pipeline);
        return NULL;
    }

    return pipeline;
}

void ffpipeline_free(KSYFF_Pipeline *pipeline)
{
    if (!pipeline)
        return;

    if (pipeline->func_destroy) {
        pipeline->func_destroy(pipeline);
    }

    free(pipeline->opaque);
    memset(pipeline, 0, sizeof(KSYFF_Pipeline));
    free(pipeline);
}

void ffpipeline_free_p(KSYFF_Pipeline **pipeline)
{
    if (!pipeline)
        return;

    ffpipeline_free(*pipeline);
}

KSYFF_Pipenode* ffpipeline_open_video_decoder(KSYFF_Pipeline *pipeline, FFPlayer *ffp)
{
    return pipeline->func_open_video_decoder(pipeline, ffp);
}

KSYFF_Pipenode* ffpipeline_open_video_output(KSYFF_Pipeline *pipeline, FFPlayer *ffp)
{
    return pipeline->func_open_video_output(pipeline, ffp);
}
