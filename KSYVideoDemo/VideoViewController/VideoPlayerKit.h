//
//  VideoPlayerKit.h
//  KSYVideoDemo
//
//  Created by JackWong on 15/4/10.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "KSYPlayer.h"
@interface VideoPlayerKit : UIViewController

@property (nonatomic, readonly) KSYPlayer *player;
@property (readonly)            BOOL fullScreenModeToggled;         //判断全屏幕
@property (nonatomic, weak) UIView *topView;

@end
