//
//  KSYLogInfo.h
//  GetInfoDemo
//
//  Created by CuiZhibo on 15/8/24.
//  Copyright (c) 2015年 CuiZhibo. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface KSYLogInfo : NSObject

+ (KSYLogInfo *)sharedLogInfo;
/**
 *  CPU信息（CPU信息，核心数及主频）
 */
@property (nonatomic, copy)NSString     *cpu;
/**
 *  内存信息
 */
@property (nonatomic, copy)NSString     *memory;
/**
 *  内核版本
 */
@property (nonatomic, copy)NSString     *core;
/**
 *  设备型号
 */
@property (nonatomic, copy)NSString     *device;
/**
 *  设备编号
 */
@property (nonatomic, copy)NSString     *uuid;
/**
 *  网络环境（2G、3G、4G、Wifi）
 */
@property (nonatomic, copy)NSString     *net;
/**
 *  当前时间
 */
@property (nonatomic, copy)NSString     *currentDate;
/**
 *  浏览器信息
 */
@property (nonatomic, copy)NSString     *userAgent;
/**
 *  当前设备IP
 */
@property (nonatomic, copy)NSString     *deviceIp;
/**
 *  服务器IP
 */
@property (nonatomic, copy)NSString     *serverIp;
/**
 *  CPU使用率
 */
@property (nonatomic, copy)NSString     *cpuUsage;
/**
 *  内存占用
 */
@property (nonatomic, copy)NSString     *memoryUsage;
/**
 *  播放首屏时间
 */
@property (nonatomic, copy)NSString     *firstFrameTime;
/**
 *  播放区缓存大小
 */
@property (nonatomic, copy)NSString     *cacheBufferSize;
/**
 *  Seek 开始时间
 */
@property (nonatomic, copy)NSString     *seekBegin;
/**
 *  Seek 结束时间
 */
@property (nonatomic, copy)NSString     *seekEnd;
/**
 *  Seek 状态记录
 */
@property (nonatomic, copy)NSString     *seekStatus;
/**
 *  Seek 状态详情
 */
@property (nonatomic, copy)NSString     *seekMessage;
/**
 * 播放失败记录
 */
@property (nonatomic, copy)NSString     *playStatus;
/**
 *  直播点播、协议、格式、编码记录
 */
@property (nonatomic, copy)NSString     *playMetaData;
@end
