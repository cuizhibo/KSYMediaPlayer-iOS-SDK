//
//  KSYMediaModule.m
//  KSYMediaPlayer
//
//  Created by KingSoft on 14-3-14.
//  Copyright (c) 2014年 bilibili. All rights reserved.
//

#import "KSYMediaModule.h"

@implementation KSYMediaModule

@synthesize appIdleTimerDisabled         = _appIdleTimerDisabled;
@synthesize mediaModuleIdleTimerDisabled = _mediaModuleIdleTimerDisabled;

+ (KSYMediaModule *)sharedModule
{
    static KSYMediaModule *obj = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        obj = [[KSYMediaModule alloc] init];
    });
    return obj;
}

- (void)setAppIdleTimerDisabled:(BOOL) idleTimerDisabled
{
    _appIdleTimerDisabled = idleTimerDisabled;
    [self updateIdleTimer];
}

- (BOOL)isAppIdleTimerDisabled
{
    return _appIdleTimerDisabled;
}

- (void)setMediaModuleIdleTimerDisabled:(BOOL) idleTimerDisabled
{
    _mediaModuleIdleTimerDisabled = idleTimerDisabled;
    [self updateIdleTimer];
}

- (BOOL)isMediaModuleIdleTimerDisabled
{
    return _mediaModuleIdleTimerDisabled;
}

- (void)updateIdleTimer
{
    if (self.appIdleTimerDisabled || self.mediaModuleIdleTimerDisabled) {
        [UIApplication sharedApplication].idleTimerDisabled = YES;
    } else {
        [UIApplication sharedApplication].idleTimerDisabled = NO;
    }
}

@end
