/*
 * KSYFFMoviePlayerDef.h
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

#import <Foundation/Foundation.h>
#include "ksyplayer/ios/ksyplayer_ios.h"
#include "ksyplayer/ksymeta.h"


struct KSYSize {
    NSInteger width;
    NSInteger height;
};
typedef struct KSYSize KSYSize;

CG_INLINE KSYSize
KSYSizeMake(NSInteger width, NSInteger height)
{
    KSYSize size;
    size.width = width;
    size.height = height;
    return size;
}



struct KSYSampleAspectRatio {
    NSInteger numerator;
    NSInteger denominator;
};
typedef struct KSYSampleAspectRatio KSYSampleAspectRatio;

CG_INLINE KSYSampleAspectRatio
KSYSampleAspectRatioMake(NSInteger numerator, NSInteger denominator)
{
    KSYSampleAspectRatio sampleAspectRatio;
    sampleAspectRatio.numerator = numerator;
    sampleAspectRatio.denominator = denominator;
    return sampleAspectRatio;
}



@interface KSYFFMoviePlayerMessage : NSObject {
@public
    AVMessage _msg;
}
@end


@interface KSYFFMoviePlayerMessagePool : NSObject

- (KSYFFMoviePlayerMessagePool *)init;
- (KSYFFMoviePlayerMessage *) obtain;
- (void) recycle:(KSYFFMoviePlayerMessage *)msg;

@end
