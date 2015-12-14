//
//  KSYPlayer.h
//  KSYMediaPlayer
//
//  Created by JackWong on 15/3/23.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "KSYMediaPlayback.h"
@class KSYFFOptions;
@class DrmRelativeAllModel;
@class DrmRelativeModel;
#define kk_KSYM_VAL_TYPE__UNKNOWN  @"unknown"

#define kk_KSYM_KEY_STREAMS       @"streams"

typedef NS_ENUM(NSInteger, KSYPlayerState)
{
    KSYPlayerStateError,          //< Player has generated an error
    KSYPlayerStateIdle,
    KSYPlayerStateInitialized,
    KSYPlayerStatePreparing,
    KSYPlayerStatePrepared,
    KSYPlayerStatePlaying,        //< Stream is playing
    KSYPlayerStatePaused,         //< Stream is paused
    KSYPlayerStateCompleted,
    KSYPlayerStateStopped,        //< Player has stopped
    KSYPlayerStateSeekingForward
};

typedef NS_ENUM(NSInteger, KSYBufferingState)
{
    KSYPlayerBufferingStart,    //缓存区缓存开始
    KSYPlayerBufferingEnd       //缓存区缓存完成
};

@protocol KSYMediaPlayerDelegate;

extern NSString *const KSYMediaPlayerStateChanged;

extern NSString *const KSYMediaPlayerWithError;

@interface KSYPlayer : NSObject <KSYMediaPlayback>

//播放器显示的view
@property (NS_NONATOMIC_IOSONLY, readonly)  UIView *videoView;
//播放器delegate
@property (NS_NONATOMIC_IOSONLY, weak) id<KSYMediaPlayerDelegate> delegate;
//当前播放的时间
@property (NS_NONATOMIC_IOSONLY) NSTimeInterval currentPlaybackTime;
//视屏时间总长
@property (NS_NONATOMIC_IOSONLY, readonly) NSTimeInterval duration;
//缓存区进度
@property (NS_NONATOMIC_IOSONLY, readonly) NSInteger bufferingProgress;
//是否准备播放
@property (NS_NONATOMIC_IOSONLY, readonly) BOOL isPreparedToPlay;
//播放状态
@property (NS_NONATOMIC_IOSONLY, readonly) KSYPlayerState state;
//视屏分辨率宽
@property (NS_NONATOMIC_IOSONLY, readonly) NSInteger videoWidth;
//视屏分辨率高
@property (NS_NONATOMIC_IOSONLY, readonly) NSInteger videoHeight;
//是否自动播放
@property (NS_NONATOMIC_IOSONLY) BOOL shouldAutoplay;
//帧率
@property (NS_NONATOMIC_IOSONLY, readonly) CGFloat fpsInMeta;
//输出帧率
@property (NS_NONATOMIC_IOSONLY, readonly) NSTimeInterval playableDuration;

@property (NS_NONATOMIC_IOSONLY, readonly) CGFloat fpsAtOutput;

@property (NS_NONATOMIC_IOSONLY,strong) KSYFFOptions *option;

@property (NS_NONATOMIC_IOSONLY, readonly) NSDictionary *mediaMeta;

@property (NS_NONATOMIC_IOSONLY, readonly) MPMovieLoadState loadState;

@property (NS_NONATOMIC_IOSONLY) MPMovieControlStyle controlStyle;

@property (NS_NONATOMIC_IOSONLY) MPMovieScalingMode scalingMode;


/**
 *  统计性能频率，默认是1s
 */
@property (NS_NONATOMIC_IOSONLY) NSTimeInterval timeInterval;


/**
 *  初始化播放器
 *
 *  @param mUrl          视屏url
 *  @param options       
 *  @param isAllow       是否允许日志上传
 *  @param appIdentifier 应用唯一标识，由用户自定义(isAllow = NO，可为空，否则不可空)
 *
 *  @return 播放器对象
 */
- (void)startWithMURL:(NSURL *)mUrl
       withOptions:(KSYFFOptions *)options
          allowLog:(BOOL)isAllow
     appIdentifier:(NSString *)appIdentifier;

//单例，全局唯一播放器实例
+ (KSYPlayer *)sharedKSYPlayer;
//启用低延时模式,如果要启用低延时模式，要在play方法之前调用。一般直播时启用，点播不启用。
- (void)playerSetUseLowLatencyWithBenable:(int)benable maxMs:(int)maxMs minMs:(int)minMs;
//启动播放器
- (void)play;
//暂停播放器
- (void)pause;
//停止播放器
- (void)stop;
//播放器是否在播放
- (BOOL)isPlaying;
//重置播放器
- (void)reset;
//关闭播放器
- (void)shutdown;
//准备播放，在初始化播放器之后*!必须!*调用
- (void)prepareToPlay;
//播放器seek时间
- (void)seekTo:(long)msec;
//播放器调整声音
- (void)setKSYPlayerVolume:(float)volume;
//调整亮度
- (void)setKSYPlayerBrightness:(float)brightness;
/**
 *  视屏截屏
 *
 *  @当前帧的图片
 */
- (UIImage *)thumbnailImageAtCurrentTime;
/**
 *  设置音频放大
 *
 *  @param amplify 音频放大比率
 */
- (void)setAudioAmplify:(float)amplify;
/**
 *  设置速率快放倍数
 *
 *  @param value 
 */
- (void)setRate:(float)value;

- (void)setAnalyzeduration:(int)duration;

//设置缓存区大小
- (void)setPlayerBuffersize:(int)size;
//设置视屏保存的路径
- (void)saveVideoLocalPath:(NSString *)localpath;
//设置DrmKey
- (void)setDrmKey:(NSString *)drmVersion cek:(NSString *)cek;

- (void)setRelativeFullURL:(DrmRelativeAllModel *)drmRelativeAllModel;

- (void)setRelativeFullURLWithSecretKey:(NSString *)secretKey
                       drmRelativeModel:(DrmRelativeModel *)drmRelativeModel;


#pragma mark- 用户自定义打点数据
/*
    不采集日志可以忽略
 */
- (void)insertOneRecordWithBegainParam:(NSDictionary *)paramDic field:(NSString *)field;
- (void)insertOneRecordWithEndParam:(NSDictionary *)paramDic field:(NSString *)field;
- (void)insertOneRecordWithStatusParam:(NSDictionary *)paramDic field:(NSString *)field;

@end


#pragma mark- KSYMediaPlayerDelegate

@protocol KSYMediaPlayerDelegate <NSObject>

@optional

//播放器播放状态改变
- (void)mediaPlayerStateChanged:(KSYPlayerState)PlayerState;
//播放器错误
- (void)mediaPlayerWithError:(NSError *)error;
//播放器播放完成
- (void)mediaPlayerCompleted:(KSYPlayer *)player;
//播放器缓存区状态
- (void)mediaPlayerBuffing:(KSYBufferingState)bufferingState;
//播放器seek完成
- (void)mediaPlayerSeekCompleted:(KSYPlayer *)player;
//注册DRM,UI层实现
- (void)retiveDrmKey:(NSString *)drmVersion player:(KSYPlayer *)player;
//buffer的时长
- (void)mediaPlayerBufferingPosition:(NSInteger)position;
@end

