//
//  KSYAppInfoUnit.h
//  GetInfoDemo
//
//  Created by CuiZhibo on 15/8/24.
//  Copyright (c) 2015年 CuiZhibo. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface KSYAppInfoUnit : NSObject<UIWebViewDelegate>

@property (nonatomic, readonly) UIWebView *webView;
@property (nonatomic, copy) NSString  *userAgent;
/**
 *  e.g. @"iPhone", @"iPod touch"
 *
 *  @return 设备model
 */
+ (NSString *)deviceModel;

/**
 *  设备名称
 *
 *  @return 设备名称
 */
+ (NSString *)deviceName;

/**
 *  系统名
 *
 *  @return 系统名
 */
+ (NSString *)systemName ;

/**
 *  系统版本
 *
 *  @return 系统版本
 */
+ (NSString *)systemVersion;

+ (NSString *)systemDeviceTypeFormatted:(BOOL)formatted;
/**
 *  获取设备IP
 *
 *  @return 设备IP
 */
+ (NSString *)getDeviceIPAddress;
/**
 *  服务器IP
 *
 *  @param url 请求URL
 *
 *  @return 服务器IP
 */
+ (NSString*)getIPAddressByHostName:(NSURL*)url;
/**
 *  操作系统及版本
 *
 *  @return 操作系统及版本
 */
+ (NSString *)getSystemVersion;

//+ (NSString *)getManufacturer;
/**
 *  UUID
 *
 *  @return UUID
 */
+ (NSString *)getUniqueForDevice;

//+ (NSString *)getMobileNetworkInfo;
/**
 *  检测网络状况
 *
 *  @return 网络住状况
 */
+ (NSString*)checkNetworkType;
/**
 *  获得当前时间
 *
 *  @return 当前时间
 */
+ (NSString *)getCurrentTime;
/**
 *  获取cpu核数
 *
 *  @return cpu核数
 */
+ (unsigned int)getCpuCore;

/**
 *  获取cpu频率
 *
 *  @return cpu频率
 */
+ (NSUInteger) getCpuFrequency;
/**
 *  获取内存信息
 *
 *  @return 内存容量
 */
+ (NSUInteger)getMemory;
/**
 *  获取浏览器浏览器信息
 *
 *  @return 浏览器信息
 */
+ (NSString *)userAgentString;
/**
 *  CPU使用率
 *
 *  @return CPU使用率
 */
+ (float) cpu_usage;
/**
 *  当前APP所占内存大小
 *
 *  @return 占用的内存
 */
+ (double)usedMemory;
@end
