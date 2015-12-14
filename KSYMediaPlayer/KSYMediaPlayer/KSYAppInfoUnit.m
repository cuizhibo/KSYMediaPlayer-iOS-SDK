//
//  KSYAppInfoUnit.m
//  GetInfoDemo
//
//  Created by CuiZhibo on 15/8/24.
//  Copyright (c) 2015年 CuiZhibo. All rights reserved.
//

#import "KSYAppInfoUnit.h"
#import <sys/utsname.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <CoreTelephony/CTCarrier.h>
//#import "KSYAuthUtils.h"
#include <sys/socket.h>
#import <sys/sysctl.h>
#import <mach/mach.h>

@implementation KSYAppInfoUnit

+ (KSYAppInfoUnit *)sharedInfoUnit
{
    static KSYAppInfoUnit *sharedInfoUnitInstance = nil;
    static dispatch_once_t onceObj;
    dispatch_once(&onceObj, ^{
        sharedInfoUnitInstance = [[self alloc] init];
    });
    return sharedInfoUnitInstance;
}

// 设备model
+ (NSString *)deviceModel
{
    // Get the device model
    if ([[UIDevice currentDevice] respondsToSelector:@selector(model)]) {
        // Make a string for the device model
        NSString *deviceModel = [[UIDevice currentDevice] model];
        // Set the output to the device model
        return deviceModel;
    } else {
        // Device model not found
        return @"";
    }
}

// 设备名称
+ (NSString *)deviceName {
    // Get the current device name
    if ([[UIDevice currentDevice] respondsToSelector:@selector(name)]) {
        // Make a string for the device name
        NSString *deviceName = [[UIDevice currentDevice] name];
        // Set the output to the device name
        return deviceName;
    } else {
        // Device name not found
        return @"";
    }
}

// 系统名
+ (NSString *)systemName {
    // Get the current system name
    if ([[UIDevice currentDevice] respondsToSelector:@selector(systemName)]) {
        // Make a string for the system name
        NSString *systemName = [[UIDevice currentDevice] systemName];
        // Set the output to the system name
        return systemName;
    } else {
        // System name not found
        return @"";
    }
}

// 系统版本
+ (NSString *)systemVersion {
    // Get the current system version
    if ([[UIDevice currentDevice] respondsToSelector:@selector(systemVersion)]) {
        // Make a string for the system version
        NSString *systemVersion = [[UIDevice currentDevice] systemVersion];
        // Set the output to the system version
        return systemVersion;
    } else {
        // System version not found
        return @"";
    }
}

// System Device Type (iPhone1,0) (Formatted = iPhone 1)
+ (NSString *)systemDeviceTypeFormatted:(BOOL)formatted {
    // Set up a Device Type String
    NSString *DeviceType;
    
    // Check if it should be formatted
    if (formatted) {
        // Formatted
        @try {
            // Set up a new Device Type String
            NSString *NewDeviceType;
            // Set up a struct
            struct utsname DT;
            // Get the system information
            uname(&DT);
            // Set the device type to the machine type
            DeviceType = [NSString stringWithFormat:@"%s", DT.machine];
            
            if ([DeviceType isEqualToString:@"i386"])
                NewDeviceType = @"iPhone Simulator";
            else if ([DeviceType isEqualToString:@"x86_64"])
                NewDeviceType = @"iPhone Simulator";
            else if ([DeviceType isEqualToString:@"iPhone1,1"])
                NewDeviceType = @"iPhone";
            else if ([DeviceType isEqualToString:@"iPhone1,2"])
                NewDeviceType = @"iPhone 3G";
            else if ([DeviceType isEqualToString:@"iPhone2,1"])
                NewDeviceType = @"iPhone 3GS";
            else if ([DeviceType isEqualToString:@"iPhone3,1"])
                NewDeviceType = @"iPhone 4";
            else if ([DeviceType isEqualToString:@"iPhone4,1"])
                NewDeviceType = @"iPhone 4S";
            else if ([DeviceType isEqualToString:@"iPhone5,1"])
                NewDeviceType = @"iPhone 5(GSM)";
            else if ([DeviceType isEqualToString:@"iPhone5,2"])
                NewDeviceType = @"iPhone 5(GSM+CDMA)";
            else if ([DeviceType isEqualToString:@"iPhone5,3"])
                NewDeviceType = @"iPhone 5c(GSM)";
            else if ([DeviceType isEqualToString:@"iPhone5,4"])
                NewDeviceType = @"iPhone 5c(GSM+CDMA)";
            else if ([DeviceType isEqualToString:@"iPhone6,1"])
                NewDeviceType = @"iPhone 5s(GSM)";
            else if ([DeviceType isEqualToString:@"iPhone6,2"])
                NewDeviceType = @"iPhone 5s(GSM+CDMA)";
            else if ([DeviceType isEqualToString:@"iPhone7,1"])
                NewDeviceType = @"iPhone 6 Plus";
            else if ([DeviceType isEqualToString:@"iPhone7,2"])
                NewDeviceType = @"iPhone 6";
            else if ([DeviceType isEqualToString:@"iPod1,1"])
                NewDeviceType = @"iPod Touch 1G";
            else if ([DeviceType isEqualToString:@"iPod2,1"])
                NewDeviceType = @"iPod Touch 2G";
            else if ([DeviceType isEqualToString:@"iPod3,1"])
                NewDeviceType = @"iPod Touch 3G";
            else if ([DeviceType isEqualToString:@"iPod4,1"])
                NewDeviceType = @"iPod Touch 4G";
            else if ([DeviceType isEqualToString:@"iPod5,1"])
                NewDeviceType = @"iPod Touch 5G";
            else if ([DeviceType isEqualToString:@"iPad1,1"])
                NewDeviceType = @"iPad";
            else if ([DeviceType isEqualToString:@"iPad2,1"])
                NewDeviceType = @"iPad 2(WiFi)";
            else if ([DeviceType isEqualToString:@"iPad2,2"])
                NewDeviceType = @"iPad 2(GSM)";
            else if ([DeviceType isEqualToString:@"iPad2,3"])
                NewDeviceType = @"iPad 2(CDMA)";
            else if ([DeviceType isEqualToString:@"iPad2,4"])
                NewDeviceType = @"iPad 2(WiFi + New Chip)";
            else if ([DeviceType isEqualToString:@"iPad2,5"])
                NewDeviceType = @"iPad mini(WiFi)";
            else if ([DeviceType isEqualToString:@"iPad2,6"])
                NewDeviceType = @"iPad mini(GSM)";
            else if ([DeviceType isEqualToString:@"iPad2,7"])
                NewDeviceType = @"iPad mini(GSM+CDMA)";
            else if ([DeviceType isEqualToString:@"iPad3,1"])
                NewDeviceType = @"iPad 3(WiFi)";
            else if ([DeviceType isEqualToString:@"iPad3,2"])
                NewDeviceType = @"iPad 3(GSM+CDMA)";
            else if ([DeviceType isEqualToString:@"iPad3,3"])
                NewDeviceType = @"iPad 3(GSM)";
            else if ([DeviceType isEqualToString:@"iPad3,4"])
                NewDeviceType = @"iPad 4(WiFi)";
            else if ([DeviceType isEqualToString:@"iPad3,5"])
                NewDeviceType = @"iPad 4(GSM)";
            else if ([DeviceType isEqualToString:@"iPad3,6"])
                NewDeviceType = @"iPad 4(GSM+CDMA)";
            else if ([DeviceType isEqualToString:@"iPad3,3"])
                NewDeviceType = @"New iPad";
            else if ([DeviceType isEqualToString:@"iPad4,1"])
                NewDeviceType = @"iPad Air(WiFi)";
            else if ([DeviceType isEqualToString:@"iPad4,2"])
                NewDeviceType = @"iPad Air(Cellular)";
            else if ([DeviceType isEqualToString:@"iPad4,4"])
                NewDeviceType = @"iPad mini 2(WiFi)";
            else if ([DeviceType isEqualToString:@"iPad4,5"])
                NewDeviceType = @"iPad mini 2(Cellular)";
            else if ([DeviceType hasPrefix:@"iPad"])
                NewDeviceType = @"iPad";
            
            // Return the new device type
            return NewDeviceType;
        }
        @catch (NSException *exception) {
            // Error
            return @"";
        }
    } else {
        // Unformatted
        @try {
            // Set up a struct
            struct utsname DT;
            // Get the system information
            uname(&DT);
            // Set the device type to the machine type
            DeviceType = [NSString stringWithFormat:@"%s", DT.machine];
            
            // Return the device type
            return DeviceType;
        }
        @catch (NSException *exception) {
            // Error
            return nil;
        }
    }
}

// 获取设备IP
+ (NSString *)getDeviceIPAddress
{
    @try {
        NSString *address = @"an error occurred when obtaining ip address";
        struct ifaddrs *interfaces = NULL;
        struct ifaddrs *temp_addr = NULL;
        int success = 0;
        
        success = getifaddrs(&interfaces);
        
        if (success == 0) { // 0 表示获取成功
            
            temp_addr = interfaces;
            while (temp_addr != NULL) {
                if( temp_addr->ifa_addr->sa_family == AF_INET) {
                    // Check if interface is en0 which is the wifi connection on the iPhone
                    if ([[NSString stringWithUTF8String:temp_addr->ifa_name] isEqualToString:@"en0"]) {
                        // Get NSString from C String
                        address = [NSString stringWithUTF8String:inet_ntoa(((struct sockaddr_in *)temp_addr->ifa_addr)->sin_addr)];
                    }
                }
                
                temp_addr = temp_addr->ifa_next;
            }
        }
        
        freeifaddrs(interfaces);
        return address;
        
    }
    @catch (NSException *exception) {
        return @"";
    }
    @finally {
        
    }
}

// 服务器IP
+ (NSString *)getIPAddressByHostName:(NSURL*)url
{
    struct hostent *remoteHostEnt = gethostbyname([[url host] UTF8String]);
    // Get address info from host entry
    if (remoteHostEnt == nil) {
        return @"";
    }
    struct in_addr *remoteInAddr = (struct in_addr *) remoteHostEnt->h_addr_list[0];
    // Convert numeric addr to ASCII string
    char *sRemoteInAddr = inet_ntoa(*remoteInAddr);
    // hostIP
    NSString* hostIP = [NSString stringWithUTF8String:sRemoteInAddr];
    return hostIP;
    
    
}

// 操作系统及版本
+ (NSString *)getSystemVersion
{
    NSString *systemVersion = [[UIDevice currentDevice] systemVersion];
    return [@"IOS" stringByAppendingString:[NSString stringWithFormat:@"%@",systemVersion]] ;
}


+ (NSString *)getManufacturer
{
    return @"Apple Inc";
}

// UUID
+ (NSString *)getUniqueForDevice
{
    @try {
        NSString *idfv = [[[UIDevice currentDevice] identifierForVendor] UUIDString];
        return idfv;
        
    }
    @catch (NSException *exception) {
        return @"";
    }
    @finally {
        
    }
    
}
//+ (NSString *)getMobileNetworkInfo
//{
//    @try {
//        CTTelephonyNetworkInfo *netWorkInfo = [[CTTelephonyNetworkInfo alloc] init];
//        CTCarrier *carrier = netWorkInfo.subscriberCellularProvider;
//        return carrier.carrierName;
//    }
//    @catch (NSException *exception) {
//        return @"";
//    }
//    @finally {
//        
//    }
//}

// 检测网络状况
+ (NSString*)checkNetworkType
{
    NSArray *subviews = [[[[UIApplication sharedApplication] valueForKey:@"statusBar"] valueForKey:@"foregroundView"]subviews];
    NSNumber *dataNetworkItemView = nil;
    
    for (id subview in subviews)
    {
        if([subview isKindOfClass:[NSClassFromString(@"UIStatusBarDataNetworkItemView") class]])
        {
            dataNetworkItemView = subview;
            break;
        }
    }
    
    switch ([[dataNetworkItemView valueForKey:@"dataNetworkType"]integerValue])
    {
        case 0: default:
        {
            return @"Unknown";
            break;
        }
        case 1:
        {
            return @"2G";
            break;
        }
        case 2:
        {
            return @"3G";
            break;
        }
        case 3:
        {
            return @"4G";
            break;
        }
        case 4:
        {
            return @"LTE";
            break;
        }
        case 5:
        {
            return @"Wifi";
            break;
        }
    }
}

// 获得当前时间
+ (NSString *)getCurrentTime
{
    @try {
        
        UInt64 recordTime = [[NSDate date] timeIntervalSince1970]*1000;
        
        NSString *time = [NSString stringWithFormat:@"%@",@(recordTime)];
        return time;
    }
    @catch (NSException *exception) {
        return @"";
    }
    
}

// 获取cpu核数
+ (unsigned int)getCpuCore
{
    unsigned int ncpu;
    size_t len = sizeof(ncpu);
    sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);
    return ncpu;
}

+ (NSUInteger) getSysInfo: (uint) typeSpecifier
{
    size_t size = sizeof(int);
    int results;
    int mib[2] = {CTL_HW, typeSpecifier};
    sysctl(mib, 2, &results, &size, NULL, 0);
    return (NSUInteger) results;
}

// 获取CPU频率
+ (NSUInteger)getCpuFrequency
{
    return [self getSysInfo:HW_CPU_FREQ];
}

// 获取内存信息
+ (NSUInteger)getMemory
{
    return [self getSysInfo:HW_USERMEM];
}

- (void)createHttpRequest {
    _webView = [[UIWebView alloc] init];
    _webView.delegate = self;
    [_webView loadRequest:[NSURLRequest requestWithURL:
                           [NSURL URLWithString:@"http://kss.ksyun.com"]]];
    NSLog(@"%@", [KSYAppInfoUnit userAgentString]);
}

//获取浏览器信息
+ (NSString *)userAgentString
{
    KSYAppInfoUnit *unit = [KSYAppInfoUnit sharedInfoUnit];
    while (unit.userAgent == nil)
    {
        NSLog(@"%@", @"in while");
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]];
    }
    return unit.userAgent;
}

- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
{
    if (webView == _webView) {
        self.userAgent = [request valueForHTTPHeaderField:@"User-Agent"];
        return NO;
    }
    return YES;
}

//CPU使用率
+ (float) cpu_usage
{
    kern_return_t kr;
    task_info_data_t tinfo;
    mach_msg_type_number_t task_info_count;
    
    task_info_count = TASK_INFO_MAX;
    kr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    
    task_basic_info_t      basic_info;
    thread_array_t         thread_list;
    mach_msg_type_number_t thread_count;
    
    thread_info_data_t     thinfo;
    mach_msg_type_number_t thread_info_count;
    
    thread_basic_info_t basic_info_th;
    uint32_t stat_thread = 0; // Mach threads
    
    basic_info = (task_basic_info_t)tinfo;
    
    // get threads in the task
    kr = task_threads(mach_task_self(), &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return -1;
    }
    if (thread_count > 0)
        stat_thread += thread_count;
    
    long tot_sec = 0;
    long tot_usec = 0;
    float tot_cpu = 0;
    int j;
    
    for (j = 0; j < thread_count; j++)
    {
        thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[j], THREAD_BASIC_INFO,
                         (thread_info_t)thinfo, &thread_info_count);
        if (kr != KERN_SUCCESS) {
            return -1;
        }
        
        basic_info_th = (thread_basic_info_t)thinfo;
        
        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            tot_sec = tot_sec + basic_info_th->user_time.seconds + basic_info_th->system_time.seconds;
            tot_usec = tot_usec + basic_info_th->system_time.microseconds + basic_info_th->system_time.microseconds;
            tot_cpu = tot_cpu + basic_info_th->cpu_usage / (float)TH_USAGE_SCALE * 100.0;
        }
        
    } // for each thread
    
    kr = vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
    assert(kr == KERN_SUCCESS);
    
    return tot_cpu;
}

// 获取当前任务所占用的内存（单位：MB）
+ (double)usedMemory
{
    task_basic_info_data_t taskInfo;
    mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;
    kern_return_t kernReturn = task_info(mach_task_self(),
                                         TASK_BASIC_INFO,
                                         (task_info_t)&taskInfo,
                                         &infoCount);
    
    if (kernReturn != KERN_SUCCESS
        ) {
        return NSNotFound;
    }
    
    return taskInfo.resident_size / 1024.0 / 1024.0;
}

+ (NSString*)dictionaryToJson:(NSDictionary *)dic

{
    
    NSError *parseError = nil;
    
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dic options:NSJSONWritingPrettyPrinted error:&parseError];
    
    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    
}

+ (NSDictionary *)getBaseDict
{
    NSString *uuidStr = [KSYAppInfoUnit getUniqueForDevice];
    NSString *dateStr = [KSYAppInfoUnit getCurrentTime];

    NSMutableDictionary *baseDict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:uuidStr,@"_id",dateStr,@"date", nil];
    return baseDict;

}
+ (NSString *)getSeekBeginJsonStringWithType:(NSString *)type field:(NSString *)field
{

    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[KSYAppInfoUnit getBaseDict]];
    [jsonDict setObject:type forKey:@"type"];
    [jsonDict setObject:field forKey:@"field"];
    
    return [KSYAppInfoUnit dictionaryToJson:jsonDict];

}

+ (NSString *)getSeekEndJsonStringWithType:(NSString *)type field:(NSString *)field
{
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[KSYAppInfoUnit getBaseDict]];
    [jsonDict setObject:type forKey:@"type"];
    [jsonDict setObject:field forKey:@"field"];
    
    return [KSYAppInfoUnit dictionaryToJson:jsonDict];

}

+ (NSString *)getSystemTimeZone
{
    NSTimeZone *zone = [NSTimeZone timeZoneForSecondsFromGMT:[NSTimeZone systemTimeZone].secondsFromGMT];
    return zone.name;
    
}
@end
