//
//  KSYLogClient.h
//  KSYMediaPlayer
//
//  Created by CuiZhibo on 15/8/26.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface KSYLogClient : NSObject

@property (nonatomic, assign)NSInteger  memory;
/**
 *  初始化日志打点工具
 *
 *  @param appIdentifier 开发者定义的应用唯一标识，必须是由小写字母组成的字符串，不能包含特殊字符
 *
 *  @return 打点工具
 */
- (instancetype)initWithAppIdentifier:(NSString *)appIdentifier;

//开头发送一条101信息
- (void)senBaseLog;
//发送日志信息
- (void)sendLogData;
//生成随机数
- (void)getTimeAndRandom;
//首屏插入
- (void)insertFirstScreenWithCacheBufferSize:(NSInteger)cacheBufferSize firstFrameTime:(NSTimeInterval)firstFrameTime playMetaDic:(NSDictionary *)playMetaDataDic;
//基础信息插入
- (void)insertBaseLogWithPlayMetaData:(NSString *)playMetaData;
//网络信息插入
- (void)insertNetStateInfoWithServerIp:(NSString *)serverIp;
//性能指标插入
- (void)insertPerformanceInfo;
//播放状态记录
- (void)insertPlayStatePlayState:(NSString *)playState;
//seek 开始
- (void)insertSeekBeginLog;
//seek 结束
- (void)insertSeekEndLogWithMessage:(NSString *)message state:(NSString *)state;


#pragma mark- 用户自定义打点数据
/**
 *  开发者自定义日志信息
 *
 *  @param paramDic 开发者手机的日志信息
 *  @param field    日志类型，由开发者自定义
 *  @param type     记录类型，只可以传 “201”，“202”，“203”，其他的不识别
 */
- (void)userInsertOneRecordLogWithParam:(NSDictionary *)paramDic field:(NSString *)field type:(NSString *)type;

@end
