/*
 * KSYFFMoviePlayerDef.m
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

#import "KSYFFMoviePlayerDef.h"

@implementation KSYFFMoviePlayerMessage
@end

@implementation KSYFFMoviePlayerMessagePool{
    NSMutableArray *_array;
}

- (KSYFFMoviePlayerMessagePool *)init
{
    self = [super init];
    if (self) {
        _array = [[NSMutableArray alloc] init];
    }
    return self;
}

- (KSYFFMoviePlayerMessage *) obtain
{
    KSYFFMoviePlayerMessage *msg = nil;

    @synchronized(self) {
        NSUInteger count = [_array count];
        if (count > 0) {
            msg = [_array objectAtIndex:count - 1];
            [_array removeLastObject];
        }
    }

    if (!msg)
        msg = [[KSYFFMoviePlayerMessage alloc] init];

    return msg;
}

- (void) recycle:(KSYFFMoviePlayerMessage *)msg
{
    if (!msg)
        return;

    @synchronized(self) {
        if ([_array count] <= 10)
            [_array addObject:msg];
    }
}

@end
