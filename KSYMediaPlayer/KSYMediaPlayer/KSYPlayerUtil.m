//
//  KSYPlayerUtil.m
//  KSYMediaPlayer
//
//  Created by JackWong on 15/5/8.
//  Copyright (c) 2015年 kingsoft. All rights reserved.
//

#import "KSYPlayerUtil.h"
#import <CommonCrypto/CommonHMAC.h>
#import <CommonCrypto/CommonDigest.h>
@implementation KSYPlayerUtil


+ (NSString *)URLEncodedString:(NSString *)str
{
    NSMutableString *output = [NSMutableString string];
    const unsigned char *source = (const unsigned char *)[str UTF8String];
    
    int sourceLen = (int)strlen((const char *)source);
    
    for (int i = 0; i < sourceLen; ++i) {
        
        const unsigned char thisChar = source[i];
        
        if (thisChar == ' '){
            
            [output appendString:@"+"];
            
        } else if (thisChar == '.' || thisChar == '-' || thisChar == '_' || thisChar == '~' ||
                   
                   (thisChar >= 'a' && thisChar <= 'z') ||
                   
                   (thisChar >= 'A' && thisChar <= 'Z') ||
                   
                   (thisChar >= '0' && thisChar <= '9')) {
            
            [output appendFormat:@"%c", thisChar];
            
        } else {
            
            [output appendFormat:@"%%%02X", thisChar];
            
        }
        
    }
    
    return output;
    
}


+ (NSString *)hexEncodeWithSecretKey:(NSString *)secretKey text:(NSString *)text
{
    const char *cKey  = [secretKey cStringUsingEncoding:NSUTF8StringEncoding];
    const char *cData = [text cStringUsingEncoding:NSUTF8StringEncoding];
    uint8_t cHMAC[CC_SHA1_DIGEST_LENGTH];
    CCHmac(kCCHmacAlgSHA1, cKey, strlen(cKey), cData, strlen(cData), cHMAC);
    NSData *HMAC = [[NSData alloc] initWithBytes:cHMAC length:CC_SHA1_DIGEST_LENGTH];
    NSString *strHash = [HMAC base64EncodedStringWithOptions:NSDataBase64Encoding64CharacterLineLength];
    return strHash;
}

+ (NSString *)ksyPlaySignatureWithsecretKey:(NSString *)secretKey
                                   httpVerb:(NSString *)httpVerb
                                     expire:(NSString *)expire
                                      nonce:(NSString *)nonce
{
    
    NSString *strHttpVerb = [httpVerb stringByAppendingString:@"\n"];
    expire = [expire stringByAppendingString:@"\n"];
    nonce = [NSString stringWithFormat:@"nonce=%@",nonce];
    NSString *strToSig = [strHttpVerb stringByAppendingString:expire];
    strToSig = [strToSig stringByAppendingString:nonce];
    strToSig = [self hexEncodeWithSecretKey:secretKey text:strToSig];
    return strToSig;
}
+ (NSUInteger)getTimeIntervalSinceNow:(NSTimeInterval)timeInterval
{
//    NSString* date;
//    NSDateFormatter * formatter = [[NSDateFormatter alloc ] init];
//    //[formatter setDateFormat:@"YYYY.MM.dd.hh.mm.ss"];
//    formatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US"];
//    [formatter setDateFormat:@"YYYY-MM-dd hh:mm:ss:SSS"];
//    date = [formatter stringFromDate:[NSDate dateWithTimeIntervalSinceNow:timeInterval]];
//    [formatter s];
//    NSString* timeLocal = [[NSString alloc] initWithFormat:@"%@", date];
    
    NSCalendar *calendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
    NSDate *sinceDate;
    NSDateComponents *comps = [[NSDateComponents alloc] init];
    NSInteger unitFlags = NSYearCalendarUnit | NSMonthCalendarUnit | NSDayCalendarUnit | NSWeekdayCalendarUnit |
    NSHourCalendarUnit | NSMinuteCalendarUnit | NSSecondCalendarUnit;
    sinceDate = [NSDate dateWithTimeIntervalSinceNow:timeInterval];
    comps = [calendar components:unitFlags fromDate:sinceDate];
//    week = [comps weekday];
//    month = [comps month];
//    day = [comps day];
//    hour = [comps hour];
//    min = [comps minute];
    NSUInteger sec = [comps second];
    


    return sec;
}
@end
