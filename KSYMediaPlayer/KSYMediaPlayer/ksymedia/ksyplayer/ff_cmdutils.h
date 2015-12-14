/*
 * ff_cmdutils.h
 *      based on ffmpeg/cmdutils.h
 *
 * Copyright (c) 2000-2003 Fabrice Bellard
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

#ifndef FFPLAY__FF_CMDUTILS_H
#define FFPLAY__FF_CMDUTILS_H

#include "ff_ffinc.h"
#include "display.h"
#include "eval.h"

void            print_error(const char *filename, int err);
double          get_rotation(AVStream *st);
AVDictionary  **setup_find_stream_info_opts(AVFormatContext *s, AVDictionary *codec_opts);
AVDictionary   *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
                                  AVFormatContext *s, AVStream *st, AVCodec *codec);

#endif
