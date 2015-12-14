//
//  VideoPlayerKit.m
//  KSYVideoDemo
//
//  Created by JackWong on 15/4/10.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import "VideoPlayerKit.h"
#import "KSYDefine.h"
@interface VideoPlayerKit ()
@property (nonatomic) CGRect previousBounds;
@end

@implementation VideoPlayerKit{
    UIDeviceOrientation _beforeOrientation;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _beforeOrientation = UIDeviceOrientationPortrait;

}
- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    
    if (_fullScreenModeToggled) {
        
    }else{
        
    }
}

- (void)setURL:(NSURL *)url
{
    if (!url) {
        return;
    }
    if (_player) {
        [_player startWithMURL:url withOptions:nil allowLog:YES appIdentifier:@"ksy"];
    }
    _player.videoView.frame = CGRectZero;
    self.view = _player.videoView;
}



- (void)launchFullScreenWhileUnAlwaysFullscreen{
    UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
    
    //    //NSLog(@"launchFullScreenWhileUnAlwaysFullscreen====orientation==%d",orientation);
    if (orientation == UIDeviceOrientationLandscapeRight) {
        if (!KSYSYS_OS_IOS8) {
            [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeLeft animated:YES];
        }else{
            //            [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeLeft animated:NO];
        }
        
    }else{
        if (!KSYSYS_OS_IOS8) {
            [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeRight animated:YES];
            
        }else{
            //            [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeRight animated:NO];
        }
    }
    
    /*
     UIScreen *screen = [UIScreen mainScreen];
     if ([screen respondsToSelector:@selector(fixedCoordinateSpace)]) {
     [screen.coordinateSpace convertRect:screen.bounds toCoordinateSpace:screen.fixedCoordinateSpace];
     }
     */
    self.previousBounds = _player.videoView.frame;
    [UIView animateWithDuration:[UIApplication sharedApplication].statusBarOrientationAnimationDuration
                          delay:0.0f
                        options:UIViewAnimationOptionLayoutSubviews//UIViewAnimationCurveLinear
                     animations:^{
                         float deviceHeight = KSYSYS_OS_IOS8?[[UIScreen mainScreen] bounds].size.width:[[UIScreen mainScreen] bounds].size.height;
                         float deviceWidth = KSYSYS_OS_IOS8?[[UIScreen mainScreen] bounds].size.height:[[UIScreen mainScreen] bounds].size.width;
                         
                         deviceHeight = [[UIScreen mainScreen] bounds].size.height;
                         deviceWidth = [UIScreen mainScreen].bounds.size.width;
                         if (orientation == UIDeviceOrientationLandscapeRight) {
                             _player.videoView.transform = CGAffineTransformRotate(_player.videoView.transform, -M_PI_2);
                             
                         }else{
                             _player.videoView.transform = CGAffineTransformRotate(_player.videoView.transform, M_PI_2);
                         }
                         
                         if ([UIDevice currentDevice].systemVersion.floatValue < 8 ) {
                             
                             [_player.videoView setCenter:CGPointMake(deviceWidth/2, deviceHeight/2)];
                             _player.videoView.bounds = CGRectMake(0, 0, deviceHeight, deviceWidth);
                         }else{
                             [_player.videoView setCenter:CGPointMake(deviceWidth/2, deviceHeight/2)];
                             _player.videoView.bounds = CGRectMake(0, 0, deviceHeight, deviceWidth);
                             
                         }
                         [_player.videoView.superview bringSubviewToFront:_player.videoView];
                     }
                     completion:^(BOOL finished) {
//                         if (self.topView) {
//                             [self.topView removeFromSuperview];
//                             
//                             if ([UIDevice currentDevice].systemVersion.floatValue < 8 ) {
//                                 self.topView.frame = CGRectMake(0,0, [[UIScreen mainScreen] bounds].size.height, self.topView.frame.size.height);
//                             }else{
//                                 self.topView.frame = CGRectMake(0,-20, [[UIScreen mainScreen] bounds].size.height, self.topView.frame.size.height);
//                             }
//                             [_videoPlayerView addSubview:self.topView];
                         
//                             _beforeOrientation = [UIDevice currentDevice].orientation;
//                         }
                     }];
    
}
- (void)minimizeVideoWhileUnAlwaysFullScreen{
    [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationPortrait animated:YES];
    [UIView animateWithDuration:[UIApplication sharedApplication].statusBarOrientationAnimationDuration
                          delay:0.0f
                        options:UIViewAnimationOptionCurveLinear
                     animations:^{
                         _player.videoView.transform = CGAffineTransformIdentity;
                         
                          _player.videoView.frame = self.previousBounds;
                     }
                     completion:^(BOOL success){
//                         if (self.topView) {
//                             [self.topView removeFromSuperview];
//                         }
//                         [self showControls];
                     }];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];

}

@end
