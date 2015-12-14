//
//  KSYLogClient.m
//  KSYMediaPlayer
//
//  Created by CuiZhibo on 15/8/26.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import "KSYLogClient.h"
#import "KSYReachability.h"
#import "KSYGzipUtillity.h"
#import <CoreData/CoreData.h>
#import "KSYLogEntity.h"
#import "KSYAppInfoUnit.h"
#define kTableName @"KSYLogEntity"
#define kDataRow 120

@interface KSYLogClient ()

@property (readonly, strong, nonatomic) NSManagedObjectContext *managedObjectContext;
@property (readonly, strong, nonatomic) NSManagedObjectModel *managedObjectModel;
@property (readonly, strong, nonatomic) NSPersistentStoreCoordinator *persistentStoreCoordinator;
@property (nonatomic, copy) NSString *romString;
@property (nonatomic, copy) NSString *url;
@property (nonatomic, copy) NSString *pushZipUrl;

@end

@implementation KSYLogClient

@synthesize managedObjectContext = _managedObjectContext;
@synthesize managedObjectModel = _managedObjectModel;
@synthesize persistentStoreCoordinator = _persistentStoreCoordinator;


- (instancetype)initWithAppIdentifier:(NSString *)appIdentifier
{
    self = [super init];
    if (self)
    {
        if ([appIdentifier hasPrefix:@"/"]) {
            
            appIdentifier = [appIdentifier substringFromIndex:1];
        }else if ([appIdentifier hasSuffix:@"/"]){
            appIdentifier = [appIdentifier substringToIndex:appIdentifier.length - 1];
        }
        if (appIdentifier == nil) {
            appIdentifier = @"unknowappname";
        }
        appIdentifier = [appIdentifier lowercaseString];
        NSLog(@"appIdentifier is %@",appIdentifier);
        self.url = [NSString stringWithFormat:@"http://logcloud.ksyun.com/push/single/%@",appIdentifier];
        self.pushZipUrl = [NSString stringWithFormat:@"http://logcloud.ksyun.com/push/gzip/%@",appIdentifier];
        self.memory = [KSYAppInfoUnit getMemory];
    }
    return self;
}

- (void)saveContext
{
    NSError *error = nil;
    NSManagedObjectContext *managedObjectContext = self.managedObjectContext;
    if (managedObjectContext != nil) {
        if ([managedObjectContext hasChanges] && ![managedObjectContext save:&error]) {
            
            NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
            abort();
        }
    }
}



- (NSManagedObjectContext *)managedObjectContext
{
    if (_managedObjectContext != nil) {
        return _managedObjectContext;
    }
    
    NSPersistentStoreCoordinator *coordinator = [self persistentStoreCoordinator];
    if (coordinator != nil) {
        _managedObjectContext = [[NSManagedObjectContext alloc] init];
        [_managedObjectContext setPersistentStoreCoordinator:coordinator];
    }
    return _managedObjectContext;
}

- (NSManagedObjectModel *)managedObjectModel
{
    
    if (_managedObjectModel != nil) {
        return _managedObjectModel;
    }
    
    
    NSString *staticLibraryBundlePath = [[NSBundle mainBundle] pathForResource:@"KSYMediaResource" ofType:@"bundle"];
    
    NSBundle *bundle = [NSBundle bundleWithPath:staticLibraryBundlePath];
    
    NSURL *modelURL = [bundle URLForResource:@"KSYPlayerLog" withExtension:@"momd"];
    
    NSLog(@"bundle path is %@",staticLibraryBundlePath);
    NSLog(@"bundle is %@modelURL is %@",bundle,modelURL);
    
    _managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:modelURL];
    NSLog(@"_managedObjectModel is %@",_managedObjectModel);
    return _managedObjectModel;
    
    
}

- (NSPersistentStoreCoordinator *)persistentStoreCoordinator
{
    if (_persistentStoreCoordinator != nil) {
        return _persistentStoreCoordinator;
    }
    
    NSURL *storeURL = [[self applicationDocumentsDirectory] URLByAppendingPathComponent:@"KSYPlayerLog.sqlite"];
    NSLog(@"StoreURL: %@", storeURL);
    
    NSError *error = nil;
    
    _persistentStoreCoordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:[self managedObjectModel]];
    if (![_persistentStoreCoordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:storeURL options:nil error:&error]) {
        NSLog(@"Unresolved error %@, %@", error, [error userInfo]);
        abort();
    }
    
    return _persistentStoreCoordinator;
}

#pragma mark - Application's Documents directory

- (NSURL *)applicationDocumentsDirectory
{
    return [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask] lastObject];
}

#pragma mark - CRUD


- (void)deleteLog:(NSArray *)arrData {
    NSManagedObjectContext *context = [self managedObjectContext];
    for (NSManagedObject *obj in arrData) {
        [context deleteObject:obj];
    }
    NSError *error = nil;
    if ([context save:&error] == NO) {
        NSLog(@"KSYPlayer SDK 删除日志失败: %@", error);
    }
}

- (void)updateData:(NSString*)request_id  withIsLook:(NSString*)islook {
    NSManagedObjectContext *context = [self managedObjectContext];
    
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"request_id like[cd] %@", request_id];
    
    //首先你需要建立一个request
    NSFetchRequest * request = [[NSFetchRequest alloc] init];
    [request setEntity:[NSEntityDescription entityForName:kTableName inManagedObjectContext:context]];
    [request setPredicate:predicate];//这里相当于sqlite中的查询条件，具体格式参考苹果文档
    
    NSError *error = nil;
    
    //保存
    if ([context save:&error]) {
        //更新成功
        NSLog(@"KSYPlayer SDK 日志记录更新：成功！");
    }
}

- (NSMutableArray*)selectData:(int)pageSize andOffset:(NSInteger)currentPage
{
    NSManagedObjectContext *context = [self managedObjectContext];
    
    
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    
    [fetchRequest setFetchLimit:pageSize];
    [fetchRequest setFetchOffset:currentPage];
    
    NSEntityDescription *entity = [NSEntityDescription entityForName:kTableName inManagedObjectContext:context];
    [fetchRequest setEntity:entity];
    NSError *error;
    NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
    NSMutableArray *resultArray = [NSMutableArray array];
    
    for (KSYLogEntity *log in fetchedObjects) {
        
        [resultArray addObject:log];
        
    }
    return resultArray;
}

- (NSInteger)dataCount {
    NSManagedObjectContext *context = [self managedObjectContext];
    NSFetchRequest *fetchRequest = [[NSFetchRequest alloc] init];
    NSEntityDescription *entity = [NSEntityDescription entityForName:kTableName inManagedObjectContext:context];
    [fetchRequest setEntity:entity];
    NSError *error;
    NSLog(@"context is %@,\nentity is %@",context,entity);
    if (entity != nil) {
        NSArray *fetchedObjects = [context executeFetchRequest:fetchRequest error:&error];
        NSLog(@"fetchedObjects.count is %@",@(fetchedObjects.count));
        return fetchedObjects.count;
        
    }
    return 0;
}

- (void)deleteFirstRecord {
    NSArray *arrData = [self selectData:1 andOffset:0];
    [self deleteLog:arrData];
}

#pragma mark - Send data

- (void)sendLogData {
    NSInteger count = [self dataCount] / kDataRow + 1;
    if ([self dataCount] == 0) {
        count = 0;
    }
    for (NSInteger i = 0; i < count; i ++) {
        NSArray *arrData = [self selectData:kDataRow andOffset:i * kDataRow];
        if ([self isWifi] == YES) {
            [self sendOnce:arrData];
        }
        else {
            break;
        }
    }
}



- (NSData *)generateStrLogWithDataCollection:(NSArray *)arrData {
    NSMutableArray *arrSpecial = [[NSMutableArray alloc] initWithCapacity:0];
    NSMutableString *postString = [[NSMutableString alloc] initWithCapacity:0];
    for (NSInteger i = 0; i < arrData.count; i ++) {
        KSYLogEntity *entity = arrData[i];
        if (entity.performanceJson != nil) {
            [postString appendString:entity.performanceJson];
            [postString appendString:@"\n"];
            
        }else if (entity.firstScreenJson != nil){
            [postString appendString:entity.firstScreenJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.playStateJson != nil){
            [arrSpecial addObject:entity.playStateJson];
            [postString appendString:entity.playStateJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.seekBeginJson != nil){
            [arrSpecial addObject:entity.seekBeginJson];
            [postString appendString:entity.seekBeginJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.seekEndJson != nil){
            [arrSpecial addObject:entity.seekEndJson];
            [postString appendString:entity.seekEndJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.seekStateJson != nil){
            [arrSpecial addObject:entity.seekStateJson];
            [postString appendString:entity.seekStateJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.seekDetailJson != nil){
            [arrSpecial addObject:entity.seekDetailJson];
            [postString appendString:entity.seekDetailJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.networkJson != nil){
            [arrSpecial addObject:entity.networkJson];
            [postString appendString:entity.networkJson];
            [postString appendString:@"\n"];
            
            
        }else if (entity.basicsJson != nil){
            [arrSpecial addObject:entity.basicsJson];
            [postString appendString:entity.basicsJson];
            [postString appendString:@"\n"];
            
            
        }
    }
    NSLog(@"postString is %@",postString);
    NSData *data = [postString dataUsingEncoding:NSUTF8StringEncoding];
    
    NSData *gzipData = [KSYGzipUtillity gzipData:data];
    
    return gzipData;
}


- (void)sendOnce:(NSArray *)arrData {
    NSLog(@"arrData.count is %@",@(arrData.count));
    NSData *dataLog = [self generateStrLogWithDataCollection:arrData];
    NSLog(@"dataLog.length is %@",@(dataLog.length));
    NSMutableURLRequest *urlRequest = [[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:self.pushZipUrl]];
    [urlRequest setHTTPMethod:@"POST"];
    [urlRequest setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [urlRequest setValue:@"gzip, deflate" forHTTPHeaderField:@"Accept-Encoding"]; // **** for gzip
    [urlRequest setHTTPBody:dataLog];
    [NSURLConnection sendAsynchronousRequest:urlRequest queue:[NSOperationQueue currentQueue] completionHandler:^(NSURLResponse *response, NSData *data, NSError *connectionError) {
        NSInteger responseCode = [(NSHTTPURLResponse *)response statusCode];
        NSString *responseString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        NSLog(@"sendOnce responseString is %@",responseString);
        NSLog(@"Log send request code: ----- %ld",(long)responseCode);
        if (responseCode == 200) {
            NSLog(@"success 111");
            [self deleteLog:arrData]; // **** 发送成功就删除
        }
    }];
}


//开头发送一条101信息
- (void)senBaseLog
{
    
    NSString *gmt = [KSYAppInfoUnit getSystemTimeZone];
    
    NSString *cpu = [NSString stringWithFormat:@"%@ * %@",@([KSYAppInfoUnit getCpuCore]),@([KSYAppInfoUnit getCpuFrequency])];
    //    NSString *userAgent = [KSYAppInfoUnit userAgentString];
    //    NSInteger memory = [KSYAppInfoUnit getMemory];
    NSNumber *memoryNum = [NSNumber numberWithInteger:self.memory];
    NSString *device  = [KSYAppInfoUnit getUniqueForDevice];
    NSString *date = [KSYAppInfoUnit getCurrentTime];
    //    NSString *system = [KSYAppInfoUnit systemName];
    NSString *core = [KSYAppInfoUnit systemVersion];
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"101" forKey:@"type"];
    [jsonDict setObject:cpu forKey:@"cpu"];
    [jsonDict setObject:gmt forKey:@"gmt"];
    [jsonDict setObject:@"Safair" forKey:@"userAgent"];
    [jsonDict setObject:memoryNum forKey:@"memory"];
    [jsonDict setObject:device forKey:@"device"];
    [jsonDict setObject:date forKey:@"date"];
    [jsonDict setObject:@"iOS" forKey:@"system"];
    [jsonDict setObject:core forKey:@"core"];
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    
    NSMutableURLRequest *urlRequest = [[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:self.url]];
    [urlRequest setHTTPMethod:@"POST"];
    [urlRequest setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [urlRequest setHTTPBody:[jsonString dataUsingEncoding:NSUTF8StringEncoding]];
    [NSURLConnection sendAsynchronousRequest:urlRequest queue:[NSOperationQueue currentQueue] completionHandler:^(NSURLResponse *response, NSData *data, NSError *connectionError) {
        NSInteger responseCode = [(NSHTTPURLResponse *)response statusCode];
        NSString *responseString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        NSLog(@"send one 101Log responseString is %@",responseString);
        NSLog(@"Log send request code: ----- %ld",(long)responseCode);
        if (responseCode == 200) {
            NSLog(@"success 111");
        }
    }];
    
}

#pragma mark- 收集日志
- (BOOL)isWifi {
    KSYReachability *r = [KSYReachability reachabilityWithHostName:@"www.apple.com"];
    NetworkStatus status = r.currentReachabilityStatus;
    if (status == ReachableViaWiFi) {
        return YES;
    }
    return NO;
}


- (NSString*)dictionaryToJson:(NSDictionary *)dic

{
    NSError *parseError = nil;
    
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dic options:NSJSONWritingPrettyPrinted error:&parseError];
    
    
    NSString *string = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    string = [string stringByReplacingOccurrencesOfString:@"\n" withString:@""];
    
    return string;
    
}

//随机数
- (void)getTimeAndRandom{
    int iRandom=arc4random();
    if (iRandom<0) {
        iRandom=-iRandom;
    }
    
    NSDateFormatter *tFormat = [[NSDateFormatter alloc] init];
    [tFormat setDateFormat:@"yyyyMMddHHmmss"];
    NSString *tResult=[NSString stringWithFormat:@"%@%d",[tFormat stringFromDate:[NSDate date]],iRandom];
    self.romString = tResult;
    NSLog(@"随机数：%@",tResult);
}

//基本字典
- (NSDictionary *)getBaseDict
{
    NSString *dateStr = [KSYAppInfoUnit getCurrentTime];
    //    NSString *gmt = [KSYAppInfoUnit getSystemTimeZone];
    NSString *uuid  = [KSYAppInfoUnit getUniqueForDevice];
    NSMutableDictionary *baseDict = [[NSMutableDictionary alloc] initWithObjectsAndKeys:self.romString,@"_id",dateStr,@"date",uuid,@"uuid", nil];
    return baseDict;
    
}

//获得数据库实体
- (KSYLogEntity *)getLogEntity
{
    NSManagedObjectContext *context = [self managedObjectContext];
    KSYLogEntity *logEntity = [NSEntityDescription insertNewObjectForEntityForName:kTableName inManagedObjectContext:context];
    return logEntity;
}

//seek 开始
- (void)insertSeekBeginLog
{
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"201" forKey:@"type"];
    [jsonDict setObject:@"seek" forKey:@"field"];
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].seekBeginJson = jsonString;
    
}

//seek 结束
- (void)insertSeekEndLogWithMessage:(NSString *)message state:(NSString *)state
{
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"203" forKey:@"type"];
    [jsonDict setObject:@"seek" forKey:@"field"];
    [jsonDict setObject:message forKey:@"message"];
    [jsonDict setObject:state forKey:@"status"];
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].seekBeginJson = jsonString;
    
}

//首屏插入
- (void)insertFirstScreenWithCacheBufferSize:(NSInteger)cacheBufferSize firstFrameTime:(NSTimeInterval)firstFrameTime playMetaDic:(NSDictionary *)playMetaDataDic
{
    
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"102" forKey:@"type"];
    [jsonDict setObject:@(cacheBufferSize) forKey:@"cacheBufferSize"];
    [jsonDict setObject:@(firstFrameTime) forKey:@"firstFrameTime"];
    
    if ([playMetaDataDic objectForKey:@"playType"] != nil) {
        [jsonDict setObject:[playMetaDataDic objectForKey:@"playType"] forKey:@"playType"];
        
    }
    if ([playMetaDataDic objectForKey:@"protocol"] != nil) {
        [jsonDict setObject:[playMetaDataDic objectForKey:@"protocol"] forKey:@"protocol"];
        
    }
    if ([playMetaDataDic objectForKey:@"format"] != nil) {
        [jsonDict setObject:[playMetaDataDic objectForKey:@"format"] forKey:@"format"];
        
    }
    if ([playMetaDataDic objectForKey:@"audioCodec"] != nil) {
        [jsonDict setObject:[playMetaDataDic objectForKey:@"audioCodec"] forKey:@"audioCodec"];
        
    }
    if ([playMetaDataDic objectForKey:@"videoCodec"] != nil) {
        [jsonDict setObject:[playMetaDataDic objectForKey:@"videoCodec"] forKey:@"videoCodec"];
        
    }
    
    
    
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].firstScreenJson = jsonString;
}


//基础信息插入
- (void)insertBaseLogWithPlayMetaData:(NSString *)playMetaData
{
    NSNumber *cpuNumber = [NSNumber numberWithUnsignedLong:[KSYAppInfoUnit getCpuCore] * [KSYAppInfoUnit getCpuFrequency]];
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"101" forKey:@"type"];
    [jsonDict setObject:playMetaData forKey:@"playMetaData"];
    [jsonDict setObject:cpuNumber forKey:@"cpu"];
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].basicsJson = jsonString;
}

//网络信息插入
- (void)insertNetStateInfoWithServerIp:(NSString *)serverIp
{
    NSString *network = [KSYAppInfoUnit checkNetworkType];
    NSString *deviceIp = [KSYAppInfoUnit getDeviceIPAddress];
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"200" forKey:@"type"];
    [jsonDict setObject:@"net" forKey:@"field"];
    [jsonDict setObject:network forKey:@"network"];
    [jsonDict setObject:deviceIp forKey:@"deviceIp"];
    if (serverIp != nil) {
        [jsonDict setObject:serverIp forKey:@"serverIp"];
        
    }
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].firstScreenJson = jsonString;
    
}

//性能
- (void)insertPerformanceInfo
{
    NSNumber *cpuUsageNumber = [NSNumber numberWithFloat:[KSYAppInfoUnit cpu_usage]];
    NSNumber *memoryUsageNumber = [NSNumber numberWithDouble:[KSYAppInfoUnit usedMemory]];
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"200" forKey:@"type"];
    [jsonDict setObject:@"usage" forKey:@"field"];
    [jsonDict setObject:cpuUsageNumber forKey:@"cpu_usage"];
    [jsonDict setObject:memoryUsageNumber forKey:@"memory_usage"];
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].performanceJson = jsonString;
}

//播放状态
- (void)insertPlayStatePlayState:(NSString *)playState
{
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] initWithDictionary:[self getBaseDict]];
    [jsonDict setObject:@"103" forKey:@"type"];
    if (playState != nil) {
        [jsonDict setObject:playState forKey:@"playStatus"];
        
    }
    NSString *jsonString = [self dictionaryToJson:jsonDict];
    [self getLogEntity].playStateJson = jsonString;
}

#pragma mark- 用户自定义打点数据

- (void)userInsertOneRecordLogWithParam:(NSDictionary *)paramDic field:(NSString *)field type:(NSString *)type
{
    NSString *dateStr = [KSYAppInfoUnit getCurrentTime];
    NSMutableDictionary *dic = [[NSMutableDictionary alloc] initWithDictionary:paramDic];
    [dic setObject:dateStr forKey:@"data"];
    [dic setObject:type forKey:@"type"];
    [dic setObject:field forKey:@"field"];
    
}

@end
