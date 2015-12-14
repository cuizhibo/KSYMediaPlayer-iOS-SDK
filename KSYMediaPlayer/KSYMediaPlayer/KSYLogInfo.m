//
//  KSYLogInfo.m
//  GetInfoDemo
//
//  Created by CuiZhibo on 15/8/24.
//  Copyright (c) 2015年 CuiZhibo. All rights reserved.
//

#import "KSYLogInfo.h"

@implementation KSYLogInfo

+ (KSYLogInfo *)sharedLogInfo
{
    static KSYLogInfo *sharedInfoInstance = nil;
    static dispatch_once_t  onceObj;
    dispatch_once(&onceObj, ^{
        sharedInfoInstance = [[self alloc] init];

    });
    return sharedInfoInstance;
}



@end
