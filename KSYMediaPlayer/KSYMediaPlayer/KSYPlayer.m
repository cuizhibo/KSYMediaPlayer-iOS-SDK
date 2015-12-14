//
//  KSYPlayer.m
//  KSYMediaPlayer
//
//  Created by JackWong on 15/3/23.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import "KSYPlayer.h"
#import "KSYFFMoviePlayerDef.h"
#import "KSYFFMrl.h"
#import "KSYMediaPlayback.h"
#import "KSYAudioKit.h"
#import "KSYMediaModule.h"
#import "KSYEventManager.h"
#import <MediaPlayer/MediaPlayer.h>
#import "KSYPlayerUtil.h"
#import "KSYAppInfoUnit.h"
#import "KSYLogClient.h"
#import "KSYMediaPlayback.h"
#import "KSYFFOptions.h"
#import "DrmRelativeModel.h"
#import "DrmRelativeAllModel.h"

#ifndef __OPTIMIZE__
#define NSLog(...) NSLog(__VA_ARGS__)
#else
#define NSLog(...) {}
#endif
NSString *const KSYMediaPlayerStateChanged    = @"KSYMediaPlayerStateChanged";
NSString *const KSYMediaPlayerWithError       = @"KSYMediaPlayerWithError";
NSString *const KSYMediaPlayerDefaultRunLoopMode = @"com.ksyun.KSYMediaPlayerDefaultRunLoopMode";

NSString *const KS3PlayerScheme = @"https";

#define KS3CEKHOST @"https://115.231.96.89"
#define KS3CEKURL  @":80/test/GetCek?signature=16I/xKLT8S/aHJpApgYfye6CI6o=&accesskeyid=8oN7siZgTOSFHft0cXTg&expire=1710333224&nonce=4e1f2519c626cbfbab1520c255830c26&cekurl="

@interface KSYPlayer () <KSYAudioSessionDelegate,NSURLConnectionDataDelegate,NSURLConnectionDelegate>
@property(nonatomic, readonly) NSDictionary *videoMeta;
@property(nonatomic, readonly) NSDictionary *audioMeta;
@property (nonatomic,copy) NSString *mediaUrl;
@end

@implementation KSYPlayer{
    KSYFFMrl *_ffMrl;
    id<KSYMediaSegmentResolver> _segmentResolver;
    KSYMediaPlayer *_mediaPlayer;
    KSYSDLGLView *_glView;
    KSYFFMoviePlayerMessagePool *_msgPool;
    
    NSInteger _sampleAspectRatioNumerator;
    NSInteger _sampleAspectRatioDenominator;
    
    BOOL      _seeking;
    NSInteger _bufferingTime;
    NSInteger _bufferingPosition;
    
    BOOL _keepScreenOnWhilePlaying;
    BOOL _pauseInBackground;
    char *_drmVersion;
    NSMutableData  *_cekBody;
    KSYLogClient    *_KSYLogClient;
    BOOL            _isFirstScreen; //是否首屏
    NSTimeInterval  _timeBegin;
    NSTimeInterval  _timeEnd;
    NSInteger       _cacheBufferSize;
    NSTimer         *_timer;
    NSInteger       _testInter;
    BOOL            _isSeekBegin;
    NSMutableDictionary *_playMetaDataDic;
    
    BOOL            _isShutDownFinish;
    NSTimer *_shutDownTimer;
}

@synthesize videoView = _videoView;
@synthesize currentPlaybackTime;
@synthesize duration;
@synthesize playableDuration;
@synthesize bufferingProgress = _bufferingProgress;
@synthesize isPreparedToPlay = _isPreparedToPlay;
@synthesize state = _state;
@synthesize loadState = _loadState;
@synthesize controlStyle = _controlStyle;
@synthesize scalingMode = _scalingMode;
@synthesize shouldAutoplay = _shouldAutoplay;
@synthesize mediaMeta = _mediaMeta;
@synthesize videoMeta = _videoMeta;
@synthesize audioMeta = _audioMeta;
@synthesize videoWidth = _videoWidth;
@synthesize videoHeight = _videoHeight;



void KSYFFIOStatCompleteRegister(void (*cb)(const char *url,
                                            int64_t read_bytes, int64_t total_size,
                                            int64_t elpased_time, int64_t total_duration))
{
    ksymp_io_stat_complete_register(cb);
}

#pragma mark av_format_control_message
int onControlResolveSegment(KSYPlayer *mpc, int type, void *data, size_t data_size)
{
    if (mpc == nil)
        return -1;
    KSYFormatSegmentContext *fsc = data;
    if (fsc == NULL || sizeof(KSYFormatSegmentContext) != data_size) {
        NSLog(@"IJKAVF_CM_RESOLVE_SEGMENT: invalid call\n");
        return -1;
    }
    NSString *url = [mpc->_segmentResolver urlOfSegment:fsc->position];
    if (url == nil)
        return -1;
    const char *rawUrl = [url UTF8String];
    if (url == NULL)
        return -1;
    fsc->url = strdup(rawUrl);
    if (fsc->url == NULL)
        return -1;
    fsc->url_free = free;
    return 0;
}


int format_control_message(void *opaque, int type, void *data, size_t data_size)
{
    KSYPlayer *mpc = (__bridge KSYPlayer*)opaque;
    
    switch (type) {
        case KSYAVF_CM_RESOLVE_SEGMENT:
            return onControlResolveSegment(mpc, type, data, data_size);
        default: {
            return -1;
        }
    }
}



inline static void fillMetaInternal(NSMutableDictionary *meta, KSYMediaMeta *rawMeta, const char *name, NSString *defaultValue)
{
    if (!meta || !rawMeta || !name)
        return;
    
    NSString *key = [NSString stringWithUTF8String:name];
    const char *value = ksymeta_get_string_l(rawMeta, name);
    if (value) {
        [meta setObject:[NSString stringWithUTF8String:value] forKey:key];
    } else if (defaultValue){
        [meta setObject:defaultValue forKey:key];
    } else {
        [meta removeObjectForKey:key];
    }
}

- (KSYFFMoviePlayerMessage *) obtainMessage {
    return [_msgPool obtain];
}

inline static KSYPlayer *ffplayerRetain(void *arg) {
    return (__bridge_transfer KSYPlayer *) arg;
}

int media_player_msg_loop(void* arg)
{
    @autoreleasepool {
        KSYMediaPlayer *mp = (KSYMediaPlayer*)arg;
        __weak KSYPlayer *ffpController = ffplayerRetain(ksymp_set_weak_thiz(mp, NULL));
        
        while (ffpController) {
            @autoreleasepool {
                KSYFFMoviePlayerMessage *msg = [ffpController obtainMessage];
                if (!msg)
                    break;
                
                int retval = ksymp_get_msg(mp, &msg->_msg, 1);
                if (retval < 0)
                    break;
                
                assert(retval > 0);
                
                [ffpController performSelectorOnMainThread:@selector(postEvent:) withObject:msg waitUntilDone:NO];
            }
        }
        ksymp_dec_ref_p(&mp);
        return 0;
    }
}
#pragma mark Mark ---PlayerStatus
- (void)postEvent:(KSYFFMoviePlayerMessage *)msg
{
    if (!msg)
        return;
    
    AVMessage *avmsg = &msg->_msg;
    NSLog(@"msg->what is: %d", avmsg->what);
    
    switch (avmsg->what) {
        case FFP_MSG_FLUSH:
            break;
        case FFP_MSG_ERROR: {
            NSLog(@"FFP_MSG_ERROR: %d", avmsg->arg1);
            if (_KSYLogClient) {
                [_KSYLogClient insertPlayStatePlayState:[NSString stringWithFormat:@"%@",@(avmsg->arg1)]];
                
            }
            
            [self setScreenOn:NO];
            _state = KSYPlayerStateError;
            if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerWithError:)]) {
                [_delegate mediaPlayerWithError:[NSError errorWithDomain:@"com.kingsoft.test" code:avmsg->arg1 userInfo:nil]];
            }
            break;
        }
        case FFP_MSG_PREPARED: {
            _state = KSYPlayerStatePrepared;
            NSLog(@"FFP_MSG_PREPARED:");
            
            if (!_mediaPlayer) {
                KSYMediaMeta *rawMeta = ksymp_get_meta_l(_mediaPlayer);
                if (rawMeta) {
                    ksymeta_lock(rawMeta);
                    
                    NSMutableDictionary *newMediaMeta = [[NSMutableDictionary alloc] init];
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_FORMAT, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_DURATION_US, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_START_US, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_BITRATE, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_START_US, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_BITRATE, nil);
                    
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_SOURCE_IP, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_PLAY_TYPE, nil);
                    fillMetaInternal(newMediaMeta, rawMeta, KSYM_KEY_PROTOCOL_NAME, nil);
                    
                    int64_t video_stream = ksymeta_get_int64_l(rawMeta, KSYM_KEY_VIDEO_STREAM, -1);
                    int64_t audio_stream = ksymeta_get_int64_l(rawMeta, KSYM_KEY_AUDIO_STREAM, -1);
                    
                    NSMutableArray *streams = [[NSMutableArray alloc] init];
                    
                    size_t count = ksymeta_get_children_count_l(rawMeta);
                    for(size_t i = 0; i < count; ++i) {
                        KSYMediaMeta *streamRawMeta = ksymeta_get_child_l(rawMeta, i);
                        NSMutableDictionary *streamMeta = [[NSMutableDictionary alloc] init];
                        
                        if (streamRawMeta) {
                            fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_TYPE, kk_KSYM_VAL_TYPE__UNKNOWN);
                            const char *type = ksymeta_get_string_l(streamRawMeta, KSYM_KEY_TYPE);
                            if (type) {
                                fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_CODEC_NAME, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_CODEC_PROFILE, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_CODEC_LONG_NAME, nil);
                                fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_BITRATE, nil);
                                
                                if (0 == strcmp(type, KSYM_VAL_TYPE__VIDEO)) {
                                    fillMetaInternal(streamMeta, streamRawMeta,KSYM_KEY_WIDTH, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_HEIGHT, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_FPS_NUM, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_FPS_DEN, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_TBR_NUM, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_TBR_DEN, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_SAR_NUM, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_SAR_DEN, nil);
                                    
                                    if (video_stream == i) {
                                        _videoMeta = streamMeta;
                                        
                                        int64_t fps_num = ksymeta_get_int64_l(streamRawMeta, KSYM_KEY_FPS_NUM, 0);
                                        int64_t fps_den = ksymeta_get_int64_l(streamRawMeta, KSYM_KEY_FPS_DEN, 0);
                                        if (fps_num > 0 && fps_den > 0) {
                                            _fpsInMeta = ((CGFloat)(fps_num)) / fps_den;
                                            NSLog(@"fps in meta %f\n", _fpsInMeta);
                                        }
                                    }
                                    
                                } else if (0 == strcmp(type, KSYM_VAL_TYPE__AUDIO)) {
                                    fillMetaInternal(streamMeta, streamRawMeta,KSYM_KEY_SAMPLE_RATE, nil);
                                    fillMetaInternal(streamMeta, streamRawMeta, KSYM_KEY_CHANNEL_LAYOUT, nil);
                                    if (audio_stream == i) {
                                        _audioMeta = streamMeta;
                                    }
                                }
                            }
                        }
                        [streams addObject:streamMeta];
                    }
                    [newMediaMeta setObject:streams forKey:kk_KSYM_KEY_STREAMS];
                    
                    [_playMetaDataDic setObject:[newMediaMeta objectForKey:@"play_type"] forKey:@"playType"];
                    [_playMetaDataDic setObject:[newMediaMeta objectForKey:@"protocol_name"] forKey:@"protocol"];
                    [_playMetaDataDic setObject:[newMediaMeta objectForKey:@"format"] forKey:@"format"];
                    
                    
                    for (NSDictionary *dict in streams) {
                        
                        if ([[dict objectForKey:@"type"] isEqualToString:@"video"]) {
                            
                            [_playMetaDataDic setObject:[dict objectForKey:@"codec_name"] forKey:@"videoCodec"];
                            
                        }else if([[dict objectForKey:@"type"] isEqualToString:@"audio"]){
                            [_playMetaDataDic setObject:[dict objectForKey:@"codec_name"] forKey:@"audioCodec"];
                            
                        }
                        
                        
                    }
                    NSString *sourceIp = [newMediaMeta objectForKey:@"source_ip"];
                    if (_KSYLogClient) {
                        [_KSYLogClient insertNetStateInfoWithServerIp:sourceIp];
                    }
                    
                    ksymeta_unlock(rawMeta);
                    _mediaMeta = newMediaMeta;
                }
                
                
            }
            _isPreparedToPlay = YES;
            _loadState = MPMovieLoadStatePlayable | MPMovieLoadStatePlaythroughOK;
            if (_shouldAutoplay) {
                [self play];
            }
            break;
        }
        case FFP_MSG_COMPLETED: {
            //播放结束插入播放状态103
            if (_KSYLogClient) {
                [_KSYLogClient insertPlayStatePlayState:@"ok"];
                
            }
            _state = KSYPlayerStateCompleted;
            [self setScreenOn:NO];
            if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerCompleted:)]) {
                [_delegate mediaPlayerCompleted:self];
            }
            break;
        }
        case FFP_MSG_VIDEO_SIZE_CHANGED:
            NSLog(@"FFP_MSG_VIDEO_SIZE_CHANGED: %d, %d", avmsg->arg1, avmsg->arg2);
            if (avmsg->arg1 > 0)
                _videoWidth = avmsg->arg1;
            if (avmsg->arg2 > 0)
                _videoHeight = avmsg->arg2;
            break;
        case FFP_MSG_SAR_CHANGED:
            NSLog(@"FFP_MSG_SAR_CHANGED: %d, %d", avmsg->arg1, avmsg->arg2);
            if (avmsg->arg1 > 0)
                _sampleAspectRatioNumerator = avmsg->arg1;
            if (avmsg->arg2 > 0)
                _sampleAspectRatioDenominator = avmsg->arg2;
            break;
        case FFP_MSG_BUFFERING_START: {
            
            NSLog(@"FFP_MSG_BUFFERING_START:");
            if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerBuffing:)]) {
                [_delegate mediaPlayerBuffing:KSYPlayerBufferingStart];
            }
            _loadState = MPMovieLoadStateStalled;
            break;
        }
        case FFP_MSG_BUFFERING_END: {
            NSLog(@"FFP_MSG_BUFFERING_END:");
            if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerBuffing:)]) {
                [_delegate mediaPlayerBuffing:KSYPlayerBufferingEnd];
            }
            _loadState = MPMovieLoadStatePlayable | MPMovieLoadStatePlaythroughOK;
            break;
        }
        case FFP_MSG_BUFFERING_UPDATE:{
            _bufferingPosition = avmsg->arg1; //buffer的时长
            _bufferingProgress = avmsg->arg2;
            NSLog(@"FFP_MSG_BUFFERING_UPDATE: %i, %i",(int) _bufferingPosition, (int)_bufferingProgress);
            if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerBufferingPosition:)]) {
                [_delegate mediaPlayerBufferingPosition:_bufferingPosition];
            }
            break;
        }
        case FFP_MSG_BUFFERING_BYTES_UPDATE:{
            NSLog(@"FFP_MSG_BUFFERING_BYTES_UPDATE: %d", avmsg->arg1);
            
            break;
        }
        case FFP_MSG_BUFFERING_TIME_UPDATE:{
            _bufferingTime       = avmsg->arg1;
            NSLog(@"FFP_MSG_BUFFERING_TIME_UPDATE: %d", avmsg->arg1);
            break;
        }
        case FFP_MSG_PLAYBACK_STATE_CHANGED:{
            
            switch (avmsg->arg1) {
                case MP_STATE_IDLE:
                    _state  = KSYPlayerStateIdle;
                    break;
                case MP_STATE_INITIALIZED:
                    _state = KSYPlayerStateInitialized;
                    break;
                case MP_STATE_ASYNC_PREPARING:
                    _state = KSYPlayerStatePreparing;
                    break;
                case MP_STATE_PREPARED:
                    _state = KSYPlayerStatePrepared;
                    break;
                case MP_STATE_STARTED:
                    _state = KSYPlayerStatePlaying;
                    break;
                case MP_STATE_PAUSED:
                    _state = KSYPlayerStatePaused;
                    break;
                case MP_STATE_COMPLETED:
                    _state = KSYPlayerStateCompleted;
                    break;
                case MP_STATE_STOPPED:
                    _state = KSYPlayerStateStopped;
                    break;
                case MP_STATE_ERROR:
                    _state = KSYPlayerStateError;
                    break;
                    
                default:
                    break;
            }
            
        }
            break;
        case FFP_MSG_SEEK_COMPLETE: {
            NSLog(@"FFP_MSG_SEEK_COMPLETE:");
            
            if (_KSYLogClient) {
                [_KSYLogClient insertSeekBeginLog];
                [_KSYLogClient insertSeekEndLogWithMessage:@"SeekComplete success" state:@"ok"];
                
                
            }
            
            _seeking = NO;
            if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerSeekCompleted:)]) {
                [_delegate mediaPlayerSeekCompleted:self];
            }
            break;
        }
        case FFP_ERROR_SEEKUNREACHABLE: {
            NSLog(@"FFP_ERROR_SEEKUNREACHABLE:");
            if (_KSYLogClient) {
                [_KSYLogClient insertSeekEndLogWithMessage:[NSString stringWithFormat:@"%@",@(avmsg->arg1)] state:@"faile"];
                
            }
            //还要记录seek失败的状态“faile or ok”
        }
            
        case FFP_MSG_GETDRMKEY:{
            _drmVersion = avmsg->obj;
            
            if (!_drmVersion) {
                _drmVersion = "";
            }
            if (_delegate && [_delegate respondsToSelector:@selector(retiveDrmKey:player:)]) {
                [_delegate retiveDrmKey:[[NSString alloc] initWithUTF8String:_drmVersion] player:self];
            }
            break;
        }
        case FFP_MSG_SPEED:
        {
            
        }
            break;
        default:
            NSLog(@"unknown FFP_MSG_xxx(%d)", avmsg->what);
            break;
    }
    
    [_msgPool recycle:msg];
    if (_delegate && [_delegate respondsToSelector:@selector(mediaPlayerStateChanged:)]) {
        [_delegate mediaPlayerStateChanged:_state];
    }
    
    
}


+ (KSYPlayer *)sharedKSYPlayer
{
    static KSYPlayer *sharedKSYPlayerInstance = nil;
    static dispatch_once_t predicate;
    dispatch_once(&predicate, ^{
        sharedKSYPlayerInstance = [[self alloc] init];
    });
    return sharedKSYPlayerInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        
        ksymp_global_init();
        _isShutDownFinish = YES;
        
        
    }
    return self;
    
}
- (void)startWithMURL:(NSURL *)mUrl withOptions:(KSYFFOptions *)options allowLog:(BOOL)isAllow appIdentifier:(NSString *)appIdentifier
{
    [self startWithMURL:mUrl
            withOptions:options
    withSegmentResolver:nil
               allowLog:isAllow
          appIdentifier:appIdentifier];
}

- (void)startWithMURL:(NSURL *)mUrl
          withOptions:(KSYFFOptions *)options
  withSegmentResolver:(id<KSYMediaSegmentResolver>)segmentResolver
             allowLog:(BOOL)isallow
        appIdentifier:(NSString *)appIdentifier
{
    if (mUrl == nil)
        return;
    
    return [self startWithMURLString:[mUrl absoluteString]
                         withOptions:options
                 withSegmentResolver:segmentResolver
                            allowLog:isallow
                       appIdentifier:appIdentifier];
}

- (void)startWithMURLString:(NSString *)mUrlString
                withOptions:(KSYFFOptions *)options
        withSegmentResolver:(id<KSYMediaSegmentResolver>)segmentResolver
                   allowLog:(BOOL)isallow
              appIdentifier:(NSString *)appIdentifier

{
    if (mUrlString == nil) {
        return ;
    }
    
    self.mediaUrl = mUrlString;
    
    _testInter = 0;
    if (isallow) {
        _KSYLogClient = [[KSYLogClient alloc] initWithAppIdentifier:appIdentifier];
        NSTimer *sendTimer = [NSTimer scheduledTimerWithTimeInterval:3600 target:self selector:@selector(sendLog) userInfo:nil repeats:YES]; // **** 应该是 1小时 ＝ 3600秒 发一次
        [sendTimer setFireDate:[NSDate distantPast]]; // **** 立即执行
    }
    
    
}


- (void)setScreenOn:(BOOL)on
{
    [KSYMediaModule sharedModule].mediaModuleIdleTimerDisabled = on;
}

- (void)sendLog
{
    [_KSYLogClient sendLogData];
}
#pragma mark app state changed

- (void)playerSetUseLowLatencyWithBenable:(int)benable maxMs:(int)maxMs minMs:(int)minMs
{
    if (!_mediaPlayer) {
        return;
    }
    ksymp_set_use_low_latency(_mediaPlayer, benable, maxMs, minMs);
}
- (void)play
{
    @synchronized (self) {
        if (!_mediaPlayer) {
            return;
        }
        
        [self setScreenOn:_keepScreenOnWhilePlaying];
        ksymp_start(_mediaPlayer);
        NSLog(@"player play");
        
    }
    if (_isFirstScreen && _KSYLogClient) {
        _timeEnd = [[NSDate date]timeIntervalSince1970] * 1000;
        NSTimeInterval tiemPoor = _timeEnd - _timeBegin;
        [_KSYLogClient insertFirstScreenWithCacheBufferSize:_cacheBufferSize firstFrameTime:tiemPoor playMetaDic:_playMetaDataDic];
        
        
    }
    _isFirstScreen = NO;
    
}

- (void)pause
{
    @synchronized (self) {
        if (!_mediaPlayer) {
            return;
        }
        ksymp_pause(_mediaPlayer);
        NSLog(@"player pause");
    }
}

- (void)stop
{
    @synchronized (self) {
        if (!_mediaPlayer) {
            return;
        }
        [self setScreenOn:NO];
        ksymp_stop(_mediaPlayer);
        
        NSLog(@"player Stop");
    }
}

- (BOOL)isPlaying
{
    @synchronized (self) {
        if (!_mediaPlayer) {
            return NO;
        }
        return ksymp_is_playing(_mediaPlayer);
        NSLog(@"player isPlaying");
    }
}

- (void)prepareToPlay
{
    
    if (_isShutDownFinish) {
        NSLog(@"reday to prepare");
        @synchronized (self) {
            if (!_mediaPlayer) {
                
                
                _mediaMeta = [[NSDictionary alloc] init];
                _ffMrl = [[KSYFFMrl alloc] initWithMrl:self.mediaUrl];
                
                
                _controlStyle = MPMovieControlStyleNone;
                _scalingMode = MPMovieScalingModeAspectFit;
                
                _mediaPlayer = ksymp_ios_create(media_player_msg_loop);
                if (!_mediaPlayer) {
                    return;
                }
                _glView = [[KSYSDLGLView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
                NSLog(@"glview is %@",_glView);
                _videoView   = _glView;
                
                _msgPool = [[KSYFFMoviePlayerMessagePool alloc] init];
                
                ksymp_set_weak_thiz(_mediaPlayer, (__bridge_retained void *) self);
                
                ksymp_set_format_callback(_mediaPlayer, format_control_message, (__bridge void *) self);
                int chroma = SDL_FCC_I420;
                
                ksymp_ios_set_glview(_mediaPlayer, _glView);
                ksymp_set_overlay_format(_mediaPlayer, chroma);
                
                [[KSYAudioKit sharedInstance] setupAudioSession:self];
                
                _keepScreenOnWhilePlaying = YES;
                _isFirstScreen = NO;
                _timeBegin = 0;
                _timeEnd   = 0;
                _playMetaDataDic = [[NSMutableDictionary alloc] initWithCapacity:0];
                [self setScreenOn:YES];
                [self setScreenOn:_keepScreenOnWhilePlaying];
                ksymp_set_analyzeduration(_mediaPlayer, 300000);
                ksymp_set_data_source(_mediaPlayer, [_ffMrl.resolvedMrl UTF8String],NULL);
                ksymp_set_format_option(_mediaPlayer, "safe", 0);
                ksymp_prepare_async(_mediaPlayer);
                
                
            }
            
        }
        
        _timeBegin = [[NSDate date] timeIntervalSince1970] * 1000;
        _isFirstScreen = YES;
        
        
        _testInter = 0;
        
        if (_KSYLogClient) {
            [_KSYLogClient getTimeAndRandom];
            [_KSYLogClient senBaseLog];
            
            [_KSYLogClient insertPerformanceInfo];
            [_KSYLogClient insertPlayStatePlayState:@"ok"];
            
            if (_timer == nil) {
                _timer = [NSTimer scheduledTimerWithTimeInterval:_timeInterval > 0 ? _timeInterval:1 target:self selector:@selector(performanceInfoLog) userInfo:nil repeats:YES];
                
            }
            
        }
        
        
    }else {
        
        NSLog(@"start loop");
        
        _shutDownTimer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self selector:@selector(isFinishShutDown) userInfo:nil repeats:YES];
        
    }
    
    
    
}

- (void)isFinishShutDown
{
    NSLog(@"no finish");
    while (_isShutDownFinish) {
        NSLog(@"is finish");
        [self prepareToPlay];
        [_shutDownTimer invalidate];
        
    }
}
- (void)setTimeInterval:(NSTimeInterval)timeInterval
{
    if (_timer == nil) {
        _timer = [NSTimer scheduledTimerWithTimeInterval:timeInterval > 0 ? timeInterval:1 target:self selector:@selector(performanceInfoLog) userInfo:nil repeats:YES];
        
    }
    
}
- (void)performanceInfoLog
{   _testInter++;
    [_KSYLogClient insertPerformanceInfo];
    if (_testInter/10 == 0) {
        [_KSYLogClient insertSeekBeginLog];
        _isSeekBegin = YES;
    }
    if (_isSeekBegin == YES) {
        [_KSYLogClient insertSeekEndLogWithMessage:@"0" state:@"ok"];
        _isSeekBegin = NO;
    }
    
}
- (void)setVideoURL:(NSURL *)url
{
    [_ffMrl initialize:[url absoluteString]];
}

- (void)setCurrentPlaybackTime:(NSTimeInterval)aCurrentPlaybackTime
{
    NSLog(@"aCurrentPlaybackTime is %lf",aCurrentPlaybackTime);
    if (!_mediaPlayer)
        return;
    _state = KSYPlayerStateSeekingForward;
    _seeking = YES;
    ksymp_seek_to(_mediaPlayer, aCurrentPlaybackTime * 1000);
    if (_KSYLogClient) {
        [_KSYLogClient insertSeekBeginLog];
        
    }
    
}

- (NSTimeInterval)currentPlaybackTime
{
    if (!_mediaPlayer)
        return 0.0f;
    
    NSTimeInterval ret = ksymp_get_current_position(_mediaPlayer);
    return ret / 1000;
}

- (NSTimeInterval)duration
{
    if (!_mediaPlayer)
        return 0.0f;
    
    NSTimeInterval ret = ksymp_get_duration(_mediaPlayer);
    NSLog(@"duration is %lf",ret);
    return ret / 1000;
}

- (NSTimeInterval)playableDuration
{
    if (!_mediaPlayer)
        return 0.0f;
    
    NSTimeInterval ret = ksymp_get_playable_duration(_mediaPlayer);
    return ret / 1000;
}

- (void)setScalingMode:(MPMovieScalingMode)aScalingMode
{
    MPMovieScalingMode newScalingMode = aScalingMode;
    switch (aScalingMode) {
        case MPMovieScalingModeNone:
            [_videoView setContentMode:UIViewContentModeCenter];
            break;
        case MPMovieScalingModeAspectFit:
            [_videoView setContentMode:UIViewContentModeScaleAspectFit];
            break;
        case MPMovieScalingModeAspectFill:
            [_videoView setContentMode:UIViewContentModeScaleAspectFill];
            break;
        case MPMovieScalingModeFill:
            [_videoView setContentMode:UIViewContentModeScaleToFill];
            break;
        default:
            newScalingMode = _scalingMode;
    }
    _scalingMode = newScalingMode;
}

- (void)shutdown
{
    if (!_mediaPlayer) {
        return;
    }
    [self setScreenOn:NO];
    
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        if (!_mediaPlayer)
            return;
        _isShutDownFinish = NO;
        @synchronized (self) {
            ksymp_stop(_mediaPlayer);
            ksymp_dec_ref_p(&_mediaPlayer);
            
        }
        NSLog(@"shutdown");
        _isShutDownFinish = YES;
        
    });
    
}

- (void)reset
{
    ksymp_set_weak_thiz(_mediaPlayer, NULL);
}

- (void)fastForwardAtRate:(float)rate
{
    [self setRate:rate];
}

- (void)setRate:(float)value
{
    if (!_mediaPlayer) {
        return;
    }
    ksymp_set_videorate(_mediaPlayer, value);
    
}

- (KSYPlayerState)targetState
{
    if (!_mediaPlayer)
        return NO;
    
    KSYPlayerState mpState = KSYPlayerStateStopped;
    int playState = ksymp_get_state(_mediaPlayer);
    switch (playState) {
        case MP_STATE_STOPPED:
            mpState = KSYPlayerStateStopped;
            break;
        case MP_STATE_COMPLETED:
            mpState = KSYPlayerStateCompleted;
            break;
        case MP_STATE_ERROR:
            mpState = KSYPlayerStateError;
            break;
        case MP_STATE_IDLE:
            mpState = KSYPlayerStateIdle;
            break;
        case MP_STATE_INITIALIZED:
            mpState = KSYPlayerStateInitialized;
            break;
        case MP_STATE_ASYNC_PREPARING:
            mpState = KSYPlayerStatePreparing;
            break;
        case MP_STATE_PAUSED:
            mpState = KSYPlayerStatePaused;
            break;
        case MP_STATE_PREPARED:
            mpState = KSYPlayerStatePrepared;
            break;
        case MP_STATE_STARTED: {
            if (_seeking)
                mpState = KSYPlayerStateSeekingForward;
            else
                mpState = KSYPlayerStatePlaying;
            break;
        }
    }
    return mpState;
}

/**
 *  + - ＋
 *
 *  @param msec 向前时间
 */
- (void)seekTo:(long)msec
{
    if (!_mediaPlayer) {
        return;
    }
    _seeking = YES;
    _state = KSYPlayerStateSeekingForward;
    ksymp_seek_to(_mediaPlayer, msec);
    
    if (_KSYLogClient) {
        [_KSYLogClient insertSeekBeginLog];
        
    }
    
}

- (void)setKSYPlayerVolume:(float)volume
{
    MPMusicPlayerController *musicPlayer;
    musicPlayer = [MPMusicPlayerController applicationMusicPlayer];
    [musicPlayer setVolume:volume];
}

- (void)setKSYPlayerBrightness:(float)brightness
{
    [UIScreen mainScreen].brightness = brightness;
}

- (void)shutdownWaitStop:(KSYPlayer *) mySelf
{
    if (!_mediaPlayer)
        return;
    ksymp_stop(_mediaPlayer);
    [self performSelectorOnMainThread:@selector(shutdownClose:) withObject:self waitUntilDone:YES];
}

- (void)shutdownClose:(KSYPlayer *) mySelf
{
    if (!_mediaPlayer)
        return;
    
    ksymp_dec_ref_p(&_mediaPlayer);
    NSLog(@"shutdownCloss");
}

- (void)setAudioAmplify:(float)amplify
{
    if (!_mediaPlayer) {
        return;
    }
    ksymp_set_audioamplify(_mediaPlayer, amplify);
}

- (UIImage *)thumbnailImageAtCurrentTime
{
    if ([_videoView isKindOfClass:[KSYSDLGLView class]]) {
        KSYSDLGLView *glView = (KSYSDLGLView *)_videoView;
        return [glView snapshot];
    }
    return nil;
}

- (CGFloat)fpsAtOutput
{
    return _glView.fps;
}

#pragma mark Mark ---AudioSessionDelete
- (void)KSYAudioBeginInterruption
{
    if ([self isPlaying]) {
        [self pause];
    }
}

- (void)KSYAudioEndInterruption
{
    if ([self isPlaying]) {
        [self pause];
    }
}

- (void)getKS3DrmCEKWithURL:(NSURL *)cekURL
{
    //    NSURL *cekURL = [NSURL URLWithString:[NSString stringWithFormat:@"%@%@%@&cekver=%@",KS3CEKHOST,KS3CEKURL,_ffMrl.resolvedMrl,[[NSString alloc] initWithUTF8String:_drmVersion]]];
    NSURLRequest *cekRequest = [NSURLRequest requestWithURL:cekURL];
    NSURLConnection *cekConnection = [[NSURLConnection alloc] initWithRequest:cekRequest delegate:self startImmediately:YES];
    [cekConnection start];
}

- (BOOL)connection:(NSURLConnection *)connection canAuthenticateAgainstProtectionSpace:(NSURLProtectionSpace *)protectionSpace{
    return [protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust];
}


-(void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
    if (nil == _cekBody) {
        _cekBody = [NSMutableData data] ;
    }
    
    [_cekBody appendData:data];
    
}

- (void)connection:(NSURLConnection *)connection didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge {
    [challenge.sender useCredential:[NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust] forAuthenticationChallenge:challenge];
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    
    
}


- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    NSString *result = [[NSString alloc] initWithData:_cekBody  encoding:NSUTF8StringEncoding];
    char *cekStart = strstr([result UTF8String], [@"<Cek>" UTF8String]);
    char *cekEnd = strstr([result UTF8String], [@"</Cek>" UTF8String]);
    
    char *verStart = strstr([result UTF8String], [@"<Version>" UTF8String]);
    char *verEnd = strstr([result UTF8String], [@"</Version>" UTF8String]);
    if (cekStart && cekEnd && verStart && verEnd) {
        cekStart += 5;
        cekEnd[0] = 0;
        verStart += 9;
        verEnd[0] = 0;
        //        ksymp_set_drmkey(_mediaPlayer, verStart, cekStart);
        [self setDrmKey:[[NSString alloc] initWithUTF8String:verStart] cek:[[NSString alloc] initWithUTF8String:cekStart]];
    }
    
    
}

- (void)setAnalyzeduration:(int)analyzeduration
{
    if (_mediaPlayer) {
        //         ksymp_set_analyzeduration(_mediaPlayer,analyzeduration);
    }
    
}
- (void)setDrmKey:(NSString *)drmVersion cek:(NSString *)cek
{
    if (!_mediaPlayer) {
        return;
    }
    if (drmVersion && cek && _mediaPlayer) {
        ksymp_set_drmkey(_mediaPlayer, [drmVersion UTF8String], [cek UTF8String]);
    }
}
- (void)setRelativeFullURL:(DrmRelativeAllModel *)drmRelativeAllModel
{
    NSMutableString *urlS = [[NSMutableString alloc] init];
    [urlS appendString:[NSString stringWithFormat:@"%@://",KS3PlayerScheme]];
    [urlS appendString:drmRelativeAllModel.kscDrmHost];
    [urlS appendString:@"/"];
    [urlS appendString:drmRelativeAllModel.customName];
    [urlS appendString:@"/"];
    [urlS appendString:drmRelativeAllModel.drmMethod];
    [urlS appendString:@"?"];
    [urlS appendString:@"signature="];
    [urlS appendString:drmRelativeAllModel.signature];
    [urlS appendString:@"&accesskeyid="];
    [urlS appendString:drmRelativeAllModel.accessKeyId];
    [urlS appendString:@"&expire="];
    [urlS appendString:drmRelativeAllModel.expire];
    [urlS appendString:@"&nonce="];
    [urlS appendString:drmRelativeAllModel.nonce];
    //    [urlS appendString:[NSString stringWithFormat:@"&public=%d",drmRelativeAllModel.visible]];
    [urlS appendString:@"&resource="];
    [urlS appendString:_ffMrl.resolvedMrl];
    [urlS appendString:@"&version="];
    [urlS appendString:drmRelativeAllModel.cekVersion];
    [self getKS3DrmCEKWithURL:[NSURL URLWithString:urlS]];
    
}

+ (NSTimeInterval)getUTCFormateDate{
    
    NSDateComponents *comps = [[NSCalendar currentCalendar]
                               components:NSDayCalendarUnit | NSYearCalendarUnit | NSMonthCalendarUnit
                               fromDate:[NSDate date]];
    [comps setHour:0];
    [comps setMinute:0];
    [comps setSecond:[[NSTimeZone systemTimeZone] secondsFromGMT]];
    
    return [[[NSCalendar currentCalendar] dateFromComponents:comps] timeIntervalSince1970];
}

- (void)setRelativeFullURLWithSecretKey:(NSString *)secretKey drmRelativeModel:(DrmRelativeModel *)drmRelativeModel
{
    
    NSUInteger expire = [[NSDate date] timeIntervalSince1970] + 3600;
    drmRelativeModel.expire = [NSString stringWithFormat:@"%lu",(unsigned long)expire];
    NSString *signature = [KSYPlayerUtil ksyPlaySignatureWithsecretKey:secretKey httpVerb:@"GET" expire:drmRelativeModel.expire nonce:drmRelativeModel.expire];
    
    signature = [KSYPlayerUtil URLEncodedString:signature];
    
    NSMutableString *urlS = [[NSMutableString alloc] init];
    [urlS appendString:[NSString stringWithFormat:@"%@://",KS3PlayerScheme]];
    [urlS appendString:drmRelativeModel.kscDrmHost];
    [urlS appendString:@"/"];
    [urlS appendString:drmRelativeModel.customName];
    [urlS appendString:@"/"];
    [urlS appendString:drmRelativeModel.drmMethod];
    [urlS appendString:@"?"];
    [urlS appendString:@"signature="];
    [urlS appendString:signature];
    [urlS appendString:@"&accesskeyid="];
    [urlS appendString:drmRelativeModel.accessKeyId];
    [urlS appendString:@"&expire="];
    [urlS appendString:drmRelativeModel.expire];
    [urlS appendString:@"&nonce="];
    [urlS appendString:drmRelativeModel.expire];
    [urlS appendString:@"&resource="];
    [urlS appendString:_ffMrl.resolvedMrl];
    [urlS appendString:@"&version="];
    [urlS appendString:drmRelativeModel.cekVersion];
    
    [self getKS3DrmCEKWithURL:[NSURL URLWithString:urlS]];
    
    
    
}


- (void)setPlayerBuffersize:(int)size
{
    _cacheBufferSize = size;
    if (_mediaPlayer) {
        ksymp_set_buffersize(_mediaPlayer, size);
    }
}
- (void)saveVideoLocalPath:(NSString *)localpath
{
    if (_mediaPlayer) {
        ksymp_set_localdir(_mediaPlayer, [localpath cStringUsingEncoding:NSUTF8StringEncoding]);
    }
}




#pragma mark Mark ---ApplicationObservers
- (void)dealloc
{
    
    [self shutdown];
    ksymp_global_uninit();
    
    [_ffMrl removeTempFiles];
    [[KSYEventManager sharedManager] cancelCallToObject:self];
}
@end
