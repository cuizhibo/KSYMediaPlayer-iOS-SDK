//
//  KSYLogEntity.h
//  KSYMediaPlayer
//
//  Created by CuiZhibo on 15/8/26.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>


@interface KSYLogEntity : NSManagedObject

@property (nonatomic, retain) NSString * seekBeginJson;
@property (nonatomic, retain) NSString * seekEndJson;
@property (nonatomic, retain) NSString * basicsJson;
@property (nonatomic, retain) NSString * networkJson;
@property (nonatomic, retain) NSString * performanceJson;
@property (nonatomic, retain) NSString * firstScreenJson;
@property (nonatomic, retain) NSString * seekStateJson;
@property (nonatomic, retain) NSString * seekDetailJson;
@property (nonatomic, retain) NSString * playStateJson;

@end
