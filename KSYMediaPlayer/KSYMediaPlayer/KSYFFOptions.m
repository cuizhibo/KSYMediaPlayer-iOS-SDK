//
//  KSYFFOptions.m
//  KSYMediaPlayer
//
//  Created by KingSoft on 13-10-17.
//  Copyright (c) 2013年 bilibili. All rights reserved.
//

#import "KSYFFOptions.h"
#include "ksyplayer/ios/ksyplayer_ios.h"

@implementation KSYFFOptions

+ (KSYFFOptions *)optionsByDefault
{
    KSYFFOptions *options = [[KSYFFOptions alloc] init];

    options.skipLoopFilter  = KSY_AVDISCARD_ALL;
    options.skipFrame       = KSY_AVDISCARD_NONREF;

    options.frameBufferCount  = 3;
    options.maxFps            = 30;
    options.frameDrop         = 0;
    options.pauseInBackground = YES;

    options.timeout         = -1;
    options.userAgent = @"";

    return options;
}

- (void)applyTo:(KSYMediaPlayer *)mediaPlayer
{
    [self logOptions];

    [self setCodecOption:@"skip_loop_filter"
               withInt64:self.skipLoopFilter
                      to:mediaPlayer];
    [self setCodecOption:@"skip_frame"
               withInt64:self.skipFrame
                      to:mediaPlayer];

    ksymp_set_picture_queue_capicity(mediaPlayer, _frameBufferCount);
    ksymp_set_max_fps(mediaPlayer, _maxFps);
    ksymp_set_framedrop(mediaPlayer, _frameDrop);

    if (self.timeout > 0) {
        [self setFormatOption:@"timeout"
                    withInt64:self.timeout
                           to:mediaPlayer];
    }
    if ([self.userAgent isEqualToString:@""] == NO) {
        [self setFormatOption:@"user-agent" withString:self.userAgent to:mediaPlayer];
    }
}

- (void)logOptions
{
    NSMutableString *echo = [[NSMutableString alloc] init];
    [echo appendString:@"========================================\n"];
    [echo appendString:@"= FFmpeg options:\n"];
    [echo appendFormat:@"= skip_loop_filter: %@\n",   [KSYFFOptions getDiscardString:self.skipLoopFilter]];
    [echo appendFormat:@"= skipFrame:        %@\n",   [KSYFFOptions getDiscardString:self.skipFrame]];
    [echo appendFormat:@"= frameBufferCount: %d\n",   self.frameBufferCount];
    [echo appendFormat:@"= maxFps:           %d\n",   self.maxFps];
    [echo appendFormat:@"= timeout:          %lld\n", self.timeout];
    [echo appendString:@"========================================\n"];
    NSLog(@"%@", echo);
}

+ (NSString *)getDiscardString:(KSYAVDiscard)discard
{
    switch (discard) {
        case KSY_AVDISCARD_NONE:
            return @"avdiscard none";
        case KSY_AVDISCARD_DEFAULT:
            return @"avdiscard default";
        case KSY_AVDISCARD_NONREF:
            return @"avdiscard nonref";
        case KSY_AVDISCARD_BIDIR:
            return @"avdicard bidir;";
        case KSY_AVDISCARD_NONKEY:
            return @"avdicard nonkey";
        case KSY_AVDISCARD_ALL:
            return @"avdicard all;";
        default:
            return @"avdiscard unknown";
    }
}

- (void)setFormatOption:(NSString *)optionName
              withInt64:(int64_t)value
                     to:(KSYMediaPlayer *)mediaPlayer
{
    ksymp_set_format_option(mediaPlayer,
                           [optionName UTF8String],
                           [[NSString stringWithFormat:@"%lld", value] UTF8String]);
}

- (void)setFormatOption:(NSString *)optionName
              withString:(NSString*)value
                     to:(KSYMediaPlayer *)mediaPlayer
{
    ksymp_set_format_option(mediaPlayer,
                            [optionName UTF8String],
                            [value UTF8String]);
}


- (void)setCodecOption:(NSString *)optionName
             withInt64:(int64_t)value
                    to:(KSYMediaPlayer *)mediaPlayer
{
    ksymp_set_codec_option(mediaPlayer,
                           [optionName UTF8String],
                           [[NSString stringWithFormat:@"%lld", value] UTF8String]);
}

@end
