/*
 * ksyplayer_ios.h
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

#include "ksyplayer/ksyplayer.h"
#import "KSYSDLGLView.h"

// ref_count is 1 after open
KSYMediaPlayer *ksymp_ios_create(int (*msg_loop)(void*));

void            ksymp_ios_set_glview(KSYMediaPlayer *mp, KSYSDLGLView *glView);
