/*
 * ff_ffpipeline.h
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

#ifndef FFPLAY__FF_FFPIPELINE_H
#define FFPLAY__FF_FFPIPELINE_H

#include "ksysdl/ksysdl_class.h"
#include "ksysdl/ksysdl_mutex.h"
#include "ff_ffpipenode.h"

typedef struct KSYFF_Pipeline_Opaque KSYFF_Pipeline_Opaque;
typedef struct KSYFF_Pipeline KSYFF_Pipeline;
typedef struct KSYFF_Pipeline {
    SDL_Class             *opaque_class;
    KSYFF_Pipeline_Opaque *opaque;

    void            (*func_destroy)            (KSYFF_Pipeline *pipeline);
    KSYFF_Pipenode *(*func_open_video_decoder) (KSYFF_Pipeline *pipeline, FFPlayer *ffp);
    KSYFF_Pipenode *(*func_open_video_output)  (KSYFF_Pipeline *pipeline, FFPlayer *ffp);
} KSYFF_Pipeline;

KSYFF_Pipeline *ffpipeline_alloc(SDL_Class *opaque_class, size_t opaque_size);
void ffpipeline_free(KSYFF_Pipeline *pipeline);
void ffpipeline_free_p(KSYFF_Pipeline **pipeline);

KSYFF_Pipenode* ffpipeline_open_video_decoder(KSYFF_Pipeline *pipeline, FFPlayer *ffp);
KSYFF_Pipenode* ffpipeline_open_video_output(KSYFF_Pipeline *pipeline, FFPlayer *ffp);

#endif
