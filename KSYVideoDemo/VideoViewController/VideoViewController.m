//
//  VideoViewController.m
//  KS3PlayerDemo
//
//  Created by Blues on 15/3/18.
//  Copyright (c) 2015年 KSY. All rights reserved.
//

#import "VideoViewController.h"
#import "MediaControlViewController.h"
#import "KSYDefine.h"
#import "MediaControlView.h"

@interface VideoViewController ()

@property (nonatomic, strong) KSYPlayer *player;
@property (nonatomic) CGRect previousBounds;

@end

@implementation VideoViewController{
    MediaControlViewController *_mediaControlViewController;
    UITextField *nativeField;
    UITextField *hlsField;
    UITextField *rtmpField;
    BOOL    _isRtmp;     //是否是rtmp直播
    
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    signal(SIGPIPE, SIG_IGN); //播放直播视频时候，锁屏会导致socket连接中断，系统发SIGPIPE信号终止进程，在release模式下，忽略SIGPIPE信号。
    
    _isCycleplay = NO;
    _beforeOrientation = UIDeviceOrientationPortrait;
    _pauseInBackground = YES;
    _motionInterfaceOrientation = UIInterfaceOrientationMaskLandscape;
    self.view.backgroundColor = [UIColor whiteColor];
    
    
    // **** 测试改变URL
    UIView *demoView = [[UIView alloc] initWithFrame:self.view.bounds];
    demoView.backgroundColor = [UIColor whiteColor];
    [self.view addSubview:demoView];
    
    CGSize size = self.view.frame.size;
    nativeField = [[UITextField alloc] initWithFrame:CGRectMake(10, size.height / 2 - 70, size.width - 80, 30)];
    nativeField.placeholder = @"本地视频文件";
    nativeField.text = @"本地视频文件";
    [demoView addSubview:nativeField];
    UIButton *nativeBtn = [UIButton buttonWithType:UIButtonTypeCustom];
    [nativeBtn setTitle:@"播放" forState:UIControlStateNormal];
    [nativeBtn setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    nativeBtn.frame = CGRectMake(size.width - 60, size.height / 2 - 70, 50, 30);
    [nativeBtn addTarget:self action:@selector(clickNativeBtn:) forControlEvents:UIControlEventTouchUpInside];
    [demoView addSubview:nativeBtn];
    
    hlsField = [[UITextField alloc] initWithFrame:CGRectMake(10, size.height / 2 - 30, size.width - 80, 30)];
    hlsField.placeholder = @"HLS直播";
    hlsField.text = @"http://maichang.kssws.ks-cdn.com/test4b.mov";
    [demoView addSubview:hlsField];
    UIButton *hlsBtn = [UIButton buttonWithType:UIButtonTypeCustom];
    [hlsBtn setTitle:@"播放" forState:UIControlStateNormal];
    [hlsBtn setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    hlsBtn.frame = CGRectMake(size.width - 60, size.height / 2 - 30, 50, 30);
    [hlsBtn addTarget:self action:@selector(clickHLSBtn:) forControlEvents:UIControlEventTouchUpInside];
    [demoView addSubview:hlsBtn];
    
    rtmpField = [[UITextField alloc] initWithFrame:CGRectMake(10, size.height / 2 + 10, size.width - 80, 30)];
    rtmpField.placeholder = @"RTMP直播";
    rtmpField.text = @"rtmp://test.live.ksyun.com/live/test12345678";
    [demoView addSubview:rtmpField];
    UIButton *rtmpBtn = [UIButton buttonWithType:UIButtonTypeCustom];
    [rtmpBtn setTitle:@"播放" forState:UIControlStateNormal];
    [rtmpBtn setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    rtmpBtn.frame = CGRectMake(size.width - 60, size.height / 2 + 10, 50, 30);
    [rtmpBtn addTarget:self action:@selector(clickRTMPBtn:) forControlEvents:UIControlEventTouchUpInside];
    [demoView addSubview:rtmpBtn];
    
    UITextField *errField = [[UITextField alloc] initWithFrame:CGRectMake(10, size.height / 2 + 50, size.width - 80, 30)];
    errField.placeholder = @"RTMP直播";
    errField.text = @"http://live.ksyun.com:8080/record/all_in_end.mp4";
    [demoView addSubview:errField];
    UIButton *errBtn = [UIButton buttonWithType:UIButtonTypeCustom];
    [errBtn setTitle:@"播放" forState:UIControlStateNormal];
    [errBtn setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    errBtn.frame = CGRectMake(size.width - 60, size.height / 2 + 50, 50, 30);
    [errBtn addTarget:self action:@selector(clickErrBtn:) forControlEvents:UIControlEventTouchUpInside];
    [demoView addSubview:errBtn];
    
    nativeField.layer.masksToBounds = YES;
    nativeField.layer.cornerRadius = 2;
    nativeField.layer.borderWidth = 1.0;
    nativeField.layer.borderColor = [UIColor blackColor].CGColor;
    nativeBtn.layer.masksToBounds = YES;
    nativeBtn.layer.cornerRadius = 3;
    nativeBtn.layer.borderWidth = 1.0;
    nativeBtn.layer.borderColor = [UIColor blackColor].CGColor;
    hlsField.layer.masksToBounds = YES;
    hlsField.layer.cornerRadius = 2;
    hlsField.layer.borderWidth = 1.0;
    hlsField.layer.borderColor = [UIColor blackColor].CGColor;
    hlsBtn.layer.masksToBounds = YES;
    hlsBtn.layer.cornerRadius = 3;
    hlsBtn.layer.borderWidth = 1.0;
    hlsBtn.layer.borderColor = [UIColor blackColor].CGColor;
    rtmpField.layer.masksToBounds = YES;
    rtmpField.layer.cornerRadius = 2;
    rtmpField.layer.borderWidth = 1.0;
    rtmpField.layer.borderColor = [UIColor blackColor].CGColor;
    rtmpBtn.layer.masksToBounds = YES;
    rtmpBtn.layer.cornerRadius = 3;
    rtmpBtn.layer.borderWidth = 1.0;
    rtmpBtn.layer.borderColor = [UIColor blackColor].CGColor;
    errField.layer.masksToBounds = YES;
    errField.layer.cornerRadius = 2;
    errField.layer.borderWidth = 1.0;
    errField.layer.borderColor = [UIColor blackColor].CGColor;
    errBtn.layer.masksToBounds = YES;
    errBtn.layer.cornerRadius = 3;
    errBtn.layer.borderWidth = 1.0;
    errBtn.layer.borderColor = [UIColor blackColor].CGColor;
}

//直播需要要开启低延时模式
- (void)initPlayerWithLowTimelagType:(BOOL)isLowTimeType {
    _player = [KSYPlayer sharedKSYPlayer];
    [_player startWithMURL:_videoUrl withOptions:nil allowLog:NO appIdentifier:@"ksy"];
    _player.shouldAutoplay = YES;
    [_player prepareToPlay];
    [self.view addSubview:_player.videoView];
    _player.videoView.frame = CGRectMake(CGRectGetMinX(self.view.frame), CGRectGetMinY(self.view.frame), CGRectGetWidth(self.view.frame), CGRectGetHeight(self.view.frame)/2);
    _player.videoView.backgroundColor = [UIColor blackColor];
    
    _mediaControlViewController = [[MediaControlViewController alloc] init];
    _mediaControlViewController.delegate = self;
    [self.view addSubview:_mediaControlViewController.view];
    [_player setScalingMode:MPMovieScalingModeAspectFit];
    
    if (isLowTimeType) {
        [_player playerSetUseLowLatencyWithBenable:1 maxMs:3000 minMs:500];
        
    }
    
    
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(orientationChanged:)
                                                 name:UIDeviceOrientationDidChangeNotification
                                               object:nil];
    [self registerApplicationObservers];
}

- (void)clickNativeBtn:(id)sender
{
    
    if ([nativeField.text hasPrefix:@"http"]) {
        _videoUrl = [NSURL URLWithString:nativeField.text];
        
    }else {
        //        _videoUrl = [NSURL URLWithString:[[NSBundle mainBundle] pathForResource:@"a" ofType:@"mp4"]];
        _videoUrl = [NSURL URLWithString:@"http://ceshi.kssws.ks-cdn.com/bb.mp4"];
    }
    [self initPlayerWithLowTimelagType:NO];
    
    UIView *demoView = [(UIButton *)sender superview];
    [demoView removeFromSuperview];
}

- (void)clickHLSBtn:(id)sender
{
    _videoUrl = [NSURL URLWithString:hlsField.text];
    [self initPlayerWithLowTimelagType:YES];
    
    UIView *demoView = [(UIButton *)sender superview];
    [demoView removeFromSuperview];
}

- (void)clickRTMPBtn:(id)sender
{
    _isRtmp = YES;
    _videoUrl = [NSURL URLWithString:rtmpField.text];
    [self initPlayerWithLowTimelagType:YES];
    
    UIView *demoView = [(UIButton *)sender superview];
    [demoView removeFromSuperview];
}

- (void)clickErrBtn:(id)sender
{
    _videoUrl = [NSURL URLWithString:@"http://live.ksyun.com:8080/record/all_in_end.mp4"];
    [self initPlayerWithLowTimelagType:YES];
    
    UIView *demoView = [(UIButton *)sender superview];
    [demoView removeFromSuperview];
}

- (void)adjustVideoViewScale {
    CGFloat width = _player.videoView.frame.size.width;
    CGFloat height = _player.videoView.frame.size.height;
    CGFloat x = _player.videoView.frame.origin.x;
    CGFloat y = _player.videoView.frame.origin.y;
    if (width > height * W16H9Scale) {
        x = (width - (height * W16H9Scale)) / 2;
        width = height * W16H9Scale;
    }
    else {
        y = (height - (width / W16H9Scale)) / 2;
        height = width / W16H9Scale;
    }
    _player.videoView.frame = CGRectMake(x, y, width, height);
}

- (void)registerApplicationObservers
{
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillEnterForeground)
                                                 name:UIApplicationWillEnterForegroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationDidBecomeActive)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillResignActive)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationDidEnterBackground)
                                                 name:UIApplicationDidEnterBackgroundNotification
                                               object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(applicationWillTerminate)
                                                 name:UIApplicationWillTerminateNotification
                                               object:nil];
}

- (void)unregisterApplicationObservers
{
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:UIApplicationWillEnterForegroundNotification
                                                  object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:UIApplicationDidBecomeActiveNotification
                                                  object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:UIApplicationWillResignActiveNotification
                                                  object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:UIApplicationDidEnterBackgroundNotification
                                                  object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:UIApplicationWillTerminateNotification
                                                  object:nil];
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:UIDeviceOrientationDidChangeNotification
                                                  object:nil];
}

- (void)applicationWillEnterForeground
{
}

- (void)applicationDidBecomeActive
{
    
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_isRtmp) {
            _player = [KSYPlayer sharedKSYPlayer];
            [_player startWithMURL:_videoUrl withOptions:nil allowLog:NO appIdentifier:@"ksy"];
            _player.shouldAutoplay = YES;
            [_player prepareToPlay];
            
            _player.videoView.frame = CGRectMake(CGRectGetMinX(self.view.frame), CGRectGetMinY(self.view.frame), CGRectGetWidth(self.view.frame), CGRectGetHeight(self.view.frame)/2);
            _player.videoView.backgroundColor = [UIColor blackColor];
            [self.view addSubview:_player.videoView];
            
            //            _mediaControlViewController = [[MediaControlViewController alloc] init];
            //            _mediaControlViewController.delegate = self;
            [self.view addSubview:_mediaControlViewController.view];
            [_player setScalingMode:MPMovieScalingModeAspectFit];
            
            [_player playerSetUseLowLatencyWithBenable:1 maxMs:3000 minMs:500];
            
        }else {
            if (![_player isPlaying]) {
                [self play];
            }
            
        }
        
    });
}

- (void)applicationWillResignActive
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground && [_player isPlaying]) {
            if (_isRtmp) {
                [_player shutdown];
                [_mediaControlViewController.view removeFromSuperview];
                
            }else {
                [self pause];
            }
        }
    });
    
}

- (void)applicationDidEnterBackground
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground && [_player isPlaying]) {
            if (_isRtmp) {
                [_player shutdown];
                
            }else {
                [self pause];
            }
            
        }
    });
}

- (void)applicationWillTerminate
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (_pauseInBackground && [_player isPlaying]) {
            if (_isRtmp) {
                [_player shutdown];
                
            }else {
                [self pause];
            }
            
        }
    });
}

- (void)orientationChanged:(NSNotification *)notification
{
    UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
    if (self.deviceOrientation!=orientation) {
        if (orientation == UIDeviceOrientationPortrait)
        {
            self.deviceOrientation = orientation;
            [self minimizeVideo];
        }
        else if (orientation == UIDeviceOrientationLandscapeRight||orientation == UIDeviceOrientationLandscapeLeft)
        {
            self.deviceOrientation = orientation;
            [self launchFullScreen];
        }
        [_mediaControlViewController reSetLoadingViewFrame];
    }
}

- (void)launchFullScreen
{
    [[UIApplication sharedApplication] setStatusBarHidden:YES];
    self.navigationController.navigationBar.hidden = YES;
    if (!_fullScreenModeToggled) {
        _fullScreenModeToggled = YES;
        [self launchFullScreenWhileUnAlwaysFullscreen];
    }
    else {
        [self launchFullScreenWhileFullScreenModeToggled];
    }
    [_mediaControlViewController reSetLoadingViewFrame];
}

- (void)minimizeVideo
{
    if (_fullScreenModeToggled) {
        _fullScreenModeToggled = NO;
        self.navigationController.navigationBar.hidden = NO;
        [[UIApplication sharedApplication] setStatusBarHidden:NO
                                                withAnimation:UIStatusBarAnimationFade];
        [self minimizeVideoWhileUnAlwaysFullScreen];
    }
    [_mediaControlViewController reSetLoadingViewFrame];
}

- (void)launchFullScreenWhileFullScreenModeToggled{
    if ([UIApplication sharedApplication].statusBarOrientation == (UIInterfaceOrientation)[[UIDevice currentDevice] orientation]) {
        return;
    }
    UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
    if (!KSYSYS_OS_IOS8) {
        [[UIApplication sharedApplication] setStatusBarOrientation:(UIInterfaceOrientation)orientation];
    }
    [UIView animateWithDuration:[UIApplication sharedApplication].statusBarOrientationAnimationDuration
                          delay:0.0f
                        options:(UIViewAnimationOptions)UIViewAnimationCurveLinear
                     animations:^{
                         float deviceHeight = [[UIScreen mainScreen] bounds].size.height;
                         float deviceWidth = [[UIScreen mainScreen] bounds].size.width;
                         UIDeviceOrientation orientation = [UIDevice currentDevice].orientation;
                         CGFloat angle =((orientation==UIDeviceOrientationLandscapeLeft)?(-M_PI):M_PI);
                         
                         _player.videoView.transform = CGAffineTransformRotate(_player.videoView.transform, angle);
                         _mediaControlViewController.view.transform = CGAffineTransformRotate(_mediaControlViewController.view.transform, angle);
                         
                         [_player.videoView setCenter:CGPointMake(deviceWidth/2, deviceHeight/2)];
                         _player.videoView.bounds = CGRectMake(0, 0, deviceHeight, deviceWidth);
                         [(MediaControlView *)_mediaControlViewController.view updateSubviewsLocation];
                     }
                     completion:^(BOOL finished) {
                         _beforeOrientation = [UIDevice currentDevice].orientation;
                     }];
}

- (void)launchFullScreenWhileUnAlwaysFullscreen
{
    UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
    if (orientation == UIDeviceOrientationLandscapeRight) {
        if (!KSYSYS_OS_IOS8) {
            [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeLeft animated:YES];
        }
        else {
        }
    }
    else {
        if (!KSYSYS_OS_IOS8) {
            [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationLandscapeRight animated:YES];
            
        }
        else {
        }
    }
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
                             _mediaControlViewController.view.transform = CGAffineTransformRotate( _mediaControlViewController.view.transform, -M_PI_2);
                             MediaControlView *mediaControlView = (MediaControlView *)(_mediaControlViewController.view);
                             mediaControlView.center= _player.videoView.center;
                             
                         }else{
                             _player.videoView.transform = CGAffineTransformRotate(_player.videoView.transform, M_PI_2);
                             _mediaControlViewController.view.transform = CGAffineTransformRotate( _mediaControlViewController.view.transform, M_PI_2);
                             MediaControlView *mediaControlView = (MediaControlView *)(_mediaControlViewController.view);
                             mediaControlView.center= _player.videoView.center;
                         }
                         
                         if ([UIDevice currentDevice].systemVersion.floatValue < 8 ) {
                             
                             [_player.videoView setCenter:CGPointMake(deviceWidth/2, deviceHeight/2)];
                             _player.videoView.bounds = CGRectMake(0, 0, deviceHeight, deviceWidth);
                             
                             MediaControlView *mediaControlView = (MediaControlView *)(_mediaControlViewController.view);
                             mediaControlView.bounds = _player.videoView.bounds;
                             mediaControlView.center = CGPointMake(deviceWidth/2, deviceHeight/2);
                         }else{
                             [_player.videoView setCenter:CGPointMake(deviceWidth/2, deviceHeight/2)];
                             _player.videoView.bounds = CGRectMake(0, 0, deviceHeight, deviceWidth);
                             
                             MediaControlView *mediaControlView = (MediaControlView *)(_mediaControlViewController.view);
                             mediaControlView.center = CGPointMake(deviceWidth/2, deviceHeight/2);
                             mediaControlView.bounds = CGRectMake(0, 0, deviceHeight, deviceWidth);
                         }
                         [(MediaControlView *)_mediaControlViewController.view updateSubviewsLocation];
                     }
                     completion:^(BOOL finished) {
                         _beforeOrientation = [UIDevice currentDevice].orientation;
                     }
     ];
}

- (void)minimizeVideoWhileUnAlwaysFullScreen{
    [[UIApplication sharedApplication] setStatusBarOrientation:UIInterfaceOrientationPortrait animated:YES];
    [UIView animateWithDuration:[UIApplication sharedApplication].statusBarOrientationAnimationDuration
                          delay:0.0f
                        options:UIViewAnimationOptionCurveLinear
                     animations:^{
                         _player.videoView.transform = CGAffineTransformIdentity;
                         _mediaControlViewController.view.transform = CGAffineTransformIdentity;
                         _player.videoView.frame = self.previousBounds;
                         MediaControlView *mediaControlView = (MediaControlView *)(_mediaControlViewController.view);
                         mediaControlView.bounds = _player.videoView.bounds;
                         mediaControlView.center = CGPointMake(mediaControlView.bounds.size.width / 2, mediaControlView.bounds.size.height/2);
                         
                         [(MediaControlView *)_mediaControlViewController.view updateSubviewsLocation];
                     }
                     completion:^(BOOL success){
                         _beforeOrientation = [UIDevice currentDevice].orientation;
                         
                     }];
}

#pragma mark - minimize Exchange

- (void)minimizeVideoWhileIsAlwaysFullScreen{
    
    [UIView animateWithDuration:[UIApplication sharedApplication].statusBarOrientationAnimationDuration
                          delay:0.0f
                        options:UIViewAnimationOptionCurveLinear
                     animations:^{
                         _player.videoView.frame = CGRectMake(self.view.frame.origin.x, self.view.frame.origin.y,CGRectGetWidth(self.view.frame), CGRectGetHeight(self.view.frame)/2);
                     }
                     completion:^(BOOL success){
                         _beforeOrientation = [UIDevice currentDevice].orientation;
                     }];
}

- (void)getVideoState
{
    //    //NSLog(@"[_player state] = = =%d",[_player state]);
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
}

- (KSYPlayer *)player {
    return _player;
}

#pragma mark - KSYMediaPlayDelegate

- (void)play {
    [_player play];
}

- (void)pause {
    [_player pause];
}

- (void)stop {
    [_player stop];
}

- (BOOL)isPlaying {
    return [_player isPlaying];
}

- (void)shutdown {
    [_player shutdown];
}

- (void)seekProgress:(CGFloat)position {
    [_player setCurrentPlaybackTime:position];
}

- (void)setVideoQuality:(KSYVideoQuality)videoQuality {
    //NSLog(@"set video quality");
}

- (void)setVideoScale:(KSYVideoScale)videoScale {
    CGRect videoRect = [[UIScreen mainScreen] bounds];
    NSInteger scaleW = 16;
    NSInteger scaleH = 9;
    switch (videoScale) {
        case kKSYVideo16W9H:
            scaleW = 16;
            scaleH = 9;
            break;
        case kKSYVideo4W3H:
            scaleW = 4;
            scaleH = 3;
            break;
        default:
            break;
    }
    if (videoRect.size.height >= videoRect.size.width * scaleW / scaleH) {
        videoRect.origin.x = 0;
        videoRect.origin.y = (videoRect.size.height - videoRect.size.width * scaleW / scaleH) / 2;
        videoRect.size.height = videoRect.size.width * scaleW / scaleH;
    }
    else {
        videoRect.origin.x = (videoRect.size.width - videoRect.size.height * scaleH / scaleW) / 2;
        videoRect.origin.y = 0;
        videoRect.size.width = videoRect.size.height * scaleH / scaleW;
    }
    _player.videoView.frame = videoRect;
}

- (void)setAudioAmplify:(CGFloat)amplify {
    [_player setAudioAmplify:amplify];
}

- (void)setCycleplay:(BOOL)isCycleplay {
    
}

#pragma mark - UIInterface layout subviews

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation
{
    return (toInterfaceOrientation == UIInterfaceOrientationPortrait);
}

- (BOOL)shouldAutorotate
{
    return NO;
}

- (NSUInteger)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskPortrait;//只支持这一个方向(正常的方向)
}
- (void)dealloc
{
    [_player shutdown];
    
    [self unregisterApplicationObservers];
}

@end
