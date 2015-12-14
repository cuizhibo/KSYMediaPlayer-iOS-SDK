/*
 * KSYAudioKit.m
 *
 * Copyright (c) 2013-2014
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

#import "KSYAudioKit.h"

@implementation KSYAudioKit {
    __weak id<KSYAudioSessionDelegate> _delegate;

    BOOL _audioSessionInitialized;
}

+ (KSYAudioKit *)sharedInstance
{
    static KSYAudioKit *sAudioKit = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sAudioKit = [[KSYAudioKit alloc] init];
    });
    return sAudioKit;
}

- (void)setupAudioSession:(id<KSYAudioSessionDelegate>) delegate
{
    _delegate = nil;
    if (delegate == nil) {
        return;
    }

    OSStatus status = noErr;
    if (!_audioSessionInitialized) {
        status = AudioSessionInitialize(NULL,
                                        kCFRunLoopCommonModes,
                                        KSYAudioSessionInterruptionListener,
                                        NULL);
        if (status != noErr) {
            NSLog(@"KSYAudioKit: AudioSessionInitialize failed (%d)", (int)status);
            return;
        }
        _audioSessionInitialized = YES;
    }

    /* Set audio session to mediaplayback */
    UInt32 sessionCategory = kAudioSessionCategory_MediaPlayback;
    status = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(sessionCategory), &sessionCategory);
    if (status != noErr) {
        NSLog(@"KSYAudioKit: AudioSessionSetProperty(kAudioSessionProperty_AudioCategory) failed (%d)", (int)status);
        return;
    }

    status = AudioSessionSetActive(true);
    if (status != noErr) {
        NSLog(@"KSYAudioKit: AudioSessionSetActive(true) failed (%d)", (int)status);
        return;
    }

    _delegate = delegate;
    return ;
}

static void KSYAudioSessionInterruptionListener(void *inClientData, UInt32 inInterruptionState)
{
    id<KSYAudioSessionDelegate> delegate = [KSYAudioKit sharedInstance]->_delegate;
    if (delegate == nil)
        return;

    switch (inInterruptionState) {
        case kAudioSessionBeginInterruption: {
            NSLog(@"kAudioSessionBeginInterruption\n");
            dispatch_async(dispatch_get_main_queue(), ^{
                AudioSessionSetActive(false);
                if ([delegate respondsToSelector:@selector(ksyAudioBeginInterruption)]) {
                     [delegate ksyAudioBeginInterruption];
                }
            });
            break;
        }
        case kAudioSessionEndInterruption: {
            NSLog(@"kAudioSessionEndInterruption\n");
            dispatch_async(dispatch_get_main_queue(), ^{
                AudioSessionSetActive(true);
                if ([delegate respondsToSelector:@selector(ksyAudioEndInterruption)]) {
                    [delegate ksyAudioEndInterruption];
                }
            });
            break;
        }
    }
}

@end
