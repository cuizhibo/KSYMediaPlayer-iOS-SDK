/*
 * KSYSDLGLView.h
 *
 * Copyright (c) 2013 
 *
 * based on https://github.com/kolyvan/kxmovie
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

#import <UIKit/UIKit.h>

#include "ksysdl/ksysdl_vout.h"

@interface KSYSDLGLView : UIView

- (id) initWithFrame:(CGRect)frame;
- (void) display: (SDL_VoutOverlay *) overlay;

- (UIImage*) snapshot;

@property(nonatomic,strong) NSLock  *appActivityLock;
@property(nonatomic)        CGFloat  fps;

@end
