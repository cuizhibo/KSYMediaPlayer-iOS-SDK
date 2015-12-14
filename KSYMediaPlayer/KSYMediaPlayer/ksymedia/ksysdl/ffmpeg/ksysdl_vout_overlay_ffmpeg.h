/*****************************************************************************
 * ksysdl_vout_overlay_ffmpeg.h
 *****************************************************************************
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

#ifndef KSYSDL__FFMPEG__KSYSDL_VOUT_OVERLAY_FFMPEG_H
#define KSYSDL__FFMPEG__KSYSDL_VOUT_OVERLAY_FFMPEG_H

#include "../ksysdl_stdinc.h"
#include "../ksysdl_vout.h"
#include "ksysdl_inc_ffmpeg.h"

// TODO: 9 alignment to speed up memcpy when display
SDL_VoutOverlay *SDL_VoutFFmpeg_CreateOverlay(int width, int height, Uint32 format, SDL_Vout *vout);

int SDL_VoutFFmpeg_ConvertFrame(
    SDL_VoutOverlay *overlay, AVFrame *frame,
    struct SwsContext **p_sws_ctx, int sws_flags);
int SDL_VoutFFmpeg_ConvertFrame_RGB(
    AVFrame *frame,SDL_VoutOverlay *overlay,
    struct SwsContext **p_sws_ctx, int sws_flags);

#endif
