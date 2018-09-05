/*
 * Copyright (C) 2018 by AMCO
 * Copyright (C) 2018 by Jesús Deloya <jdeloya_ext@amco.mx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#import "fileManager.h"

#ifdef __cplusplus
extern "C" {
#endif
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
@interface ShavHelperManager : NSObject
+ (NSDictionary*)variantToNSDictionary:(QVariantMap)settings;
//+ (NSArray*)varianListToNSArray:(QVariantList)list;
+ (id)variantToNSObject:(QVariant)item;
+ (QVariant)NSObjectToVariant:(id)item;
+ (QVariantMap)NSDictionaryToVariantMap:(NSDictionary*)obj;
+ (QVariantList)NSArrayToVariantList:(NSArray*)array;
//+ (QString)applicationBundle;
+ (QVariantMap)NSErrorToVariantMap:(NSError*)error;
/*+ (QSize)CGSizeToQSize:(CGSize)size;
+ (CGSize)QSizeToCGSize:(QSize)size;
+ (QPoint)CGPointToQPoint:(CGPoint)point;
+ (CGPoint)QPointToCGPoint:(QPoint)point;
+ (QRect)CGRectToQRect:(CGRect)frame;
+ (CGRect)QRectToCGRect:(QRect)frame;*/
@end

@implementation ShavHelperManager
+ (NSDictionary*)variantToNSDictionary:(QVariantMap)settings {
    NSMutableDictionary* userInfoDic = nil;
    if(!settings.isEmpty()) {
        userInfoDic = [NSMutableDictionary dictionary];
        foreach(QString key, settings.keys()){
            QVariant item = settings[key];
            if(!item.isNull()) {
                if(item.type() == QVariant::String) {
                    NSString* value = item.toString().toNSString();
                    if(value) {
                        [userInfoDic setObject: value forKey: key.toNSString()];
                    }
                } else if(item.type() == QVariant::Bool || item.type() == QVariant::Int || item.type() == QVariant::Double ||
                          item.type() == QVariant::LongLong || item.type() == QVariant::UInt || item.type() == QVariant::ULongLong) {
                    if(item.type() == QVariant::Int) {
                        NSNumber* value = [NSNumber numberWithInt:item.toInt()];
                        if(value) {
                            [userInfoDic setObject: value forKey: key.toNSString()];
                        }
                    } else if(item.type() == QVariant::UInt) {
                        NSNumber* value = [NSNumber numberWithUnsignedInt:item.toUInt()];
                        if(value) {
                            [userInfoDic setObject: value forKey: key.toNSString()];
                        }
                    } else if(item.type() == QVariant::LongLong) {
                        NSNumber* value = [NSNumber numberWithLongLong:item.toLongLong()];
                        if(value) {
                            [userInfoDic setObject: value forKey: key.toNSString()];
                        }
                    } else if(item.type() == QVariant::ULongLong) {
                        NSNumber* value = [NSNumber numberWithUnsignedLongLong:item.toULongLong()];
                        if(value) {
                            [userInfoDic setObject: value forKey: key.toNSString()];
                        }
                    } else if(item.type() == QVariant::Double) {
                        NSNumber* value = [NSNumber numberWithDouble:item.toDouble()];
                        if(value) {
                            [userInfoDic setObject: value forKey: key.toNSString()];
                        }
                    } else if(item.type() == QVariant::Bool) {
                        BOOL flag = ((item.toBool() == true) ? YES : NO);
                        NSNumber* value = [NSNumber numberWithBool:flag];
                        if(value) {
                            [userInfoDic setObject: value forKey: key.toNSString()];
                        }
                    }
                } else if(item.type() == QVariant::Url) {
                    QUrl url = item.toUrl();
                    NSURL* urlPath = [NSURL URLWithString: (url.toString().toNSString())];
                    [userInfoDic setObject: urlPath forKey: key.toNSString()];
                } else if(item.canConvert<QVariantMap>()) {
                    NSDictionary* value = [ShavHelperManager variantToNSDictionary: (QVariantMap)item.toMap()];
                    if(value) {
                        [userInfoDic setObject: value forKey: key.toNSString()];
                    }
                } else if(item.type() == QVariant::DateTime) {
                    NSDate *date = item.toDateTime().toNSDate();
                    if(date) {
                        [userInfoDic setObject:date forKey:key.toNSString()];
                    }
                }
                else if(item.canConvert<QVariantList>()) {
                    NSArray* value = [ShavHelperManager varianListToNSArray: (QVariantList)item.toList()];
                    if(value) {
                        [userInfoDic setObject: value forKey: key.toNSString()];
                    }
                }
            }
        }
    }
    return userInfoDic;
}

+ (id)variantToNSObject:(QVariant)item {
    id res = nil;
    if(!item.isNull()) {
        if(item.type() == QVariant::String) {
            res = item.toString().toNSString();
        } else if(item.type() == QVariant::Bool || item.type() == QVariant::Int || item.type() == QVariant::Double ||
                  item.type() == QVariant::LongLong || item.type() == QVariant::UInt || item.type() == QVariant::ULongLong) {
            if(item.type() == QVariant::Int) {
                res = [NSNumber numberWithInt:item.toInt()];
            } else if(item.type() == QVariant::UInt) {
                res = [NSNumber numberWithUnsignedInt:item.toUInt()];
            } else if(item.type() == QVariant::LongLong) {
                res = [NSNumber numberWithLongLong:item.toLongLong()];
            } else if(item.type() == QVariant::ULongLong) {
                res = [NSNumber numberWithUnsignedLongLong:item.toULongLong()];
            } else if(item.type() == QVariant::Double) {
                res = [NSNumber numberWithDouble:item.toDouble()];
            } else if(item.type() == QVariant::Bool) {
                BOOL flag = ((item.toBool() == true) ? YES : NO);
                res = [NSNumber numberWithBool:flag];
            }
        } else if(item.type() == QVariant::Url) {
            QUrl url = item.toUrl();
            res = [NSURL URLWithString: (url.toString().toNSString())];
        } else if(item.canConvert<QVariantMap>()) {
            res = [ShavHelperManager variantToNSDictionary: (QVariantMap)item.toMap()];
        } /*else if(item.canConvert<QVariantList>()) {
            res = [ShavHelperManager varianListToNSArray: (QVariantList)item.toList()];
        }*/
    }
    return res;
}

+ (QVariant)NSObjectToVariant:(id)item {
    QVariant res;
    res.clear();
    if(item) {
        if([item isKindOfClass: [NSDictionary class]]) {
            res = QVariant([ShavHelperManager NSDictionaryToVariantMap: (NSDictionary*)item]);
        } /*else if([item isKindOfClass: [NSArray class]]) {
            res = QVariant([ShavHelperManager NSArrayToVariantList: (NSArray*)item]);
        }*/ else if([item isKindOfClass: [NSNumber class]]) {
            if((NSNumber*)item == (void*)kCFBooleanFalse || (NSNumber*)item == (void*)kCFBooleanTrue) {
                BOOL value = [(NSNumber*)item boolValue];
                res = QVariant(((value == YES) ? true : false));
            } else if(strcmp([item objCType], @encode(int)) == 0) {
                int value = [(NSNumber*)item intValue];
                res = QVariant(value);
            } else if(strcmp([item objCType], @encode(double)) == 0) {
                double value = [(NSNumber*)item doubleValue];
                res = QVariant(value);
            } else if(strcmp([item objCType], @encode(float)) == 0) {
                float value = [(NSNumber*)item floatValue];
                res = QVariant(value);
            } else if(strcmp([item objCType], @encode(uint)) == 0) {
                uint value = [(NSNumber*)item unsignedIntValue];
                res = QVariant(value);
            } else if(strcmp([item objCType], @encode(long long)) == 0) {
                long long value = [(NSNumber*)item longLongValue];
                res = QVariant(value);
            }
        } else if([item isKindOfClass: [NSString class]]) {
            res = QVariant(QString::fromNSString((NSString*)item));
        } else if([item isKindOfClass:[NSURL class]]) {
            NSURL* url = (NSURL*)item;
            res = QVariant(QUrl(QString::fromNSString(url.absoluteString)));
        }
    }
    return res;
}

+ (QVariantMap)NSDictionaryToVariantMap:(NSDictionary*)obj {
    QVariantMap res;
    res.clear();
    if(obj && [obj isKindOfClass: [NSDictionary class]]) {
        for(NSEnumerator* key in [obj keyEnumerator]) {
            id item = [obj objectForKey: key];
            QString keyString = QString::fromNSString((NSString*)key);
            if([item isKindOfClass: [NSDictionary class]]) {
                res[keyString] = [ShavHelperManager NSDictionaryToVariantMap: (NSDictionary*)item];
            } /*else if([item isKindOfClass: [NSArray class]]) {
                res[keyString] = [ShavHelperManager NSArrayToVariantList: (NSArray*)item];
            }*/ else if([item isKindOfClass: [NSNumber class]]) {
                if((NSNumber*)item == (void*)kCFBooleanFalse || (NSNumber*)item == (void*)kCFBooleanTrue) {
                    BOOL value = [(NSNumber*)item boolValue];
                    res[keyString] = QVariant(((value == YES) ? true : false));
                } else if(strcmp([item objCType], @encode(int)) == 0) {
                    int value = [(NSNumber*)item intValue];
                    res[keyString] = QVariant(value);
                } else if(strcmp([item objCType], @encode(double)) == 0) {
                    double value = [(NSNumber*)item doubleValue];
                    res[keyString] = QVariant(value);
                } else if(strcmp([item objCType], @encode(float)) == 0) {
                    float value = [(NSNumber*)item floatValue];
                    res[keyString] = QVariant(value);
                } else if(strcmp([item objCType], @encode(uint)) == 0) {
                    uint value = [(NSNumber*)item unsignedIntValue];
                    res[keyString] = QVariant(value);
                } else if(strcmp([item objCType], @encode(long long)) == 0) {
                    long long value = [(NSNumber*)item longLongValue];
                    res[keyString] = QVariant(value);
                }
            } else if([item isKindOfClass: [NSString class]]) {
                res[keyString] = QString::fromNSString((NSString*)item);
            } else if([item isKindOfClass:[NSURL class]]) {
                NSURL* url = (NSURL*)item;
                res[keyString] = QVariant(QUrl(QString::fromNSString(url.absoluteString)));
            } else if([item isKindOfClass:[NSDate class]]) {
                QDateTime date = QDateTime::fromNSDate((NSDate *)item);
                res[keyString] = date;
            }
        }
    }
    return res;
}

+ (QVariantMap)NSErrorToVariantMap:(NSError*)error {
    QVariantMap map;
    map.clear();
    if(error) {
        map["code"] = (int)(error.code);
        map["domain"] = QString::fromNSString(error.domain);
        map["localizedDescription"] = QString::fromNSString(error.localizedDescription);
        map["localizedFailureReason"] = QString::fromNSString(error.localizedFailureReason);
        map["localizedRecoverySuggestion"] = QString::fromNSString(error.localizedRecoverySuggestion);
        if(error.userInfo != nil) {
            map["userInfo"] = [ShavHelperManager NSDictionaryToVariantMap: error.userInfo];
        }
    }
    
    return map;
}

+ (QVariantList)NSArrayToVariantList:(NSArray*)array {
    QVariantList list;
    list.clear();
    if(array) {
        for(id item in array) {
            NSLog(@"%@",item);
            if([item isKindOfClass: [NSDictionary class]]) {
                list.append([ShavHelperManager NSDictionaryToVariantMap: (NSDictionary*)item]);
            } else if([item isKindOfClass: [NSArray class]]) {
                list.append([ShavHelperManager NSArrayToVariantList: (NSArray*)item]);
            } else if([item isKindOfClass: [NSNumber class]]) {
                if((NSNumber*)item == (void*)kCFBooleanFalse || (NSNumber*)item == (void*)kCFBooleanTrue) {
                    BOOL value = [(NSNumber*)item boolValue];
                    list.append(QVariant(((value == YES) ? true : false)));
                } else if(strcmp([item objCType], @encode(int)) == 0) {
                    int value = [(NSNumber*)item intValue];
                    list.append(QVariant(value));
                } else if(strcmp([item objCType], @encode(double)) == 0) {
                    double value = [(NSNumber*)item doubleValue];
                    list.append(QVariant(value));
                } else if(strcmp([item objCType], @encode(float)) == 0) {
                    float value = [(NSNumber*)item floatValue];
                    list.append(QVariant(value));
                } else if(strcmp([item objCType], @encode(uint)) == 0) {
                    uint value = [(NSNumber*)item unsignedIntValue];
                    list.append(QVariant(value));
                } else if(strcmp([item objCType], @encode(long long)) == 0) {
                    long long value = [(NSNumber*)item longLongValue];
                    list.append(QVariant(value));
                }
            } else if([item isKindOfClass: [NSString class]]) {
                list.append(QString::fromNSString((NSString*)item));
            } else if([item isKindOfClass:[NSURL class]]) {
                NSURL* url = (NSURL*)item;
                list.append(QVariant(QUrl(QString::fromNSString(url.absoluteString))));
            }
        }
    }
    return list;
}

@end

const QString FileManager::FMFileType = QString::fromNSString(NSFileType);
const QString FileManager::FMFileTypeDirectory = QString::fromNSString(NSFileTypeDirectory);
const QString FileManager::FMFileTypeRegular =QString::fromNSString(NSFileTypeRegular);
const QString FileManager::FMFileTypeSymbolicLink =QString::fromNSString(NSFileTypeSymbolicLink);
const QString FileManager::FMFileTypeSocket =QString::fromNSString(NSFileTypeSocket);
const QString FileManager::FMFileTypeCharacterSpecial =QString::fromNSString(NSFileTypeCharacterSpecial);
const QString FileManager::FMFileTypeBlockSpecial =QString::fromNSString(NSFileTypeBlockSpecial);
const QString FileManager::FMFileTypeUnknown =QString::fromNSString(NSFileTypeUnknown);
const QString FileManager::FMFileSize =QString::fromNSString(NSFileSize);
const QString FileManager::FMFileModificationDate =QString::fromNSString(NSFileModificationDate);
const QString FileManager::FMFileReferenceCount =QString::fromNSString(NSFileReferenceCount);
const QString FileManager::FMFileDeviceIdentifier =QString::fromNSString(NSFileDeviceIdentifier);
const QString FileManager::FMFileOwnerAccountName =QString::fromNSString(NSFileOwnerAccountName);
const QString FileManager::FMFileGroupOwnerAccountName =QString::fromNSString(NSFileGroupOwnerAccountName);
const QString FileManager::FMFilePosixPermissions =QString::fromNSString(NSFilePosixPermissions);
const QString FileManager::FMFileSystemNumber =QString::fromNSString(NSFileSystemNumber);
const QString FileManager::FMFileSystemFileNumber =QString::fromNSString(NSFileSystemFileNumber);
const QString FileManager::FMFileExtensionHidden =QString::fromNSString(NSFileExtensionHidden);
const QString FileManager::FMFileHFSCreatorCode =QString::fromNSString(NSFileHFSCreatorCode);
const QString FileManager::FMFileHFSTypeCode =QString::fromNSString(NSFileHFSTypeCode);
const QString FileManager::FMFileImmutable =QString::fromNSString(NSFileImmutable);
const QString FileManager::FMFileAppendOnly =QString::fromNSString(NSFileAppendOnly);
const QString FileManager::FMFileCreationDate =QString::fromNSString(NSFileCreationDate);
const QString FileManager::FMFileOwnerAccountID =QString::fromNSString(NSFileOwnerAccountID);
const QString FileManager::FMFileGroupOwnerAccountID =QString::fromNSString(NSFileGroupOwnerAccountID);
const QString FileManager::FMFileBusy =QString::fromNSString(NSFileBusy);

const QString FileManager::FMFileSystemSize =QString::fromNSString(NSFileSystemSize);
const QString FileManager::FMFileSystemFreeSize =QString::fromNSString(NSFileSystemFreeSize);
const QString FileManager::FMFileSystemNodes =QString::fromNSString(NSFileSystemNodes);
const QString FileManager::FMFileSystemFreeNodes =QString::fromNSString(NSFileSystemFreeNodes);
    
const QString FileManager::FMPOSIXErrorDomain = QString::fromNSString(NSPOSIXErrorDomain);

QVariantMap* FileManager::attributesOfItemAtPath(QString path, QVariantMap &error)
{
    NSString* p = path.toNSString();
    NSError *nsError = nil;
    QVariantMap* qAttribs = nullptr;
    NSDictionary* attribs =
    [[NSFileManager defaultManager] attributesOfItemAtPath:p error:&nsError];
    if(nsError)
        error = [ShavHelperManager NSErrorToVariantMap:nsError];
    else
        qAttribs = new QVariantMap([ShavHelperManager NSDictionaryToVariantMap:attribs]);
    return qAttribs;
}
    
bool FileManager::createDirectory(QString path, QVariantMap attributes, QVariantMap &error)
{
    NSDictionary *attrib = [ShavHelperManager variantToNSDictionary:attributes];
    NSError *nsError = nil;
    bool retval = [[NSFileManager defaultManager] createDirectoryAtPath:path.toNSString()
                                     withIntermediateDirectories:NO
                                                      attributes:attrib
                                                                  error:&nsError]?true:false;
    if(nsError)
        error = [ShavHelperManager NSErrorToVariantMap:nsError];
    return retval;
}

bool FileManager::createFileAtPath(QString path, QVariantMap attributes, int flags, QVariant &userData, QVariantMap &error)
{
    mode_t mode = attributes.value(FMFilePosixPermissions).toLongLong();
    int fd = open(path.toLatin1().data(), flags, mode);
    if ( fd < 0 ) {
        NSError *nserror = [NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil];
        if(nserror)
            error = [ShavHelperManager NSErrorToVariantMap:nserror];
        return false;
    }
    
    userData = fd;
    return true;
}

bool FileManager::createFileAtPath(QString path, QVariantMap attributes, QVariant &userData, QVariantMap &error)
{
    return this->createFileAtPath(path, attributes, (O_RDWR | O_CREAT | O_EXCL), userData, error);
}
    
bool FileManager::removeItemAtPath(QString path, QVariantMap &error)
{
    // NOTE: If removeDirectoryAtPath is commented out, then this may be called
    // with a directory, in which case NSFileManager will recursively remove all
    // subdirectories. So be careful!
    NSError *nserror=nil;
    bool retval = [[NSFileManager defaultManager] removeItemAtPath:path.toNSString() error:&nserror]?true:false;
    if(nserror)
        error = [ShavHelperManager NSErrorToVariantMap:nserror];
    return retval;
}
    
bool FileManager::createSymbolicLinkAtPath(QString path, QString otherPath, QVariantMap &error)
{
    NSError *nserror=nil;
    bool retval = [[NSFileManager defaultManager] createSymbolicLinkAtPath:path.toNSString()
                                                       withDestinationPath:otherPath.toNSString()
                                                                     error:&nserror];
    if(nserror)
        error = [ShavHelperManager NSErrorToVariantMap:nserror];
    
    return retval;
}
    
QString FileManager::destinationOfSymbolicLinkAtPath(QString path, QVariantMap &error)
{
    NSError *nserror=nil;
    NSString *retval = [[NSFileManager defaultManager] destinationOfSymbolicLinkAtPath:path.toNSString()
                                                                     error:&nserror];
    if(nserror)
        error = [ShavHelperManager NSErrorToVariantMap:nserror];
    
    return QString::fromNSString(retval);
}
    
QVariantMap FileManager::attributesOfFileSystemForPath(QString path, QVariantMap &error)
{
    NSError *nsError;
    QVariantMap qAttribs;
    NSDictionary* d =
    [[NSFileManager defaultManager] attributesOfFileSystemForPath:path.toNSString() error:&nsError];
    if (d) {
        NSMutableDictionary* attribs = [NSMutableDictionary dictionaryWithDictionary:d];
        [attribs setObject:[NSNumber numberWithBool:YES]
                    forKey:@"kGMUserFileSystemVolumeSupportsExtendedDatesKey"];
        
        NSURL *URL = [NSURL fileURLWithPath:path.toNSString() isDirectory:YES];
        NSNumber *supportsCaseSensitiveNames = nil;
        [URL getResourceValue:&supportsCaseSensitiveNames
                       forKey:NSURLVolumeSupportsCaseSensitiveNamesKey
                        error:NULL];
        if (supportsCaseSensitiveNames == nil) {
            supportsCaseSensitiveNames = [NSNumber numberWithBool:YES];
        }
        [attribs setObject:supportsCaseSensitiveNames
                    forKey:@"kGMUserFileSystemVolumeSupportsCaseSensitiveNamesKey"];
        
        qAttribs = [ShavHelperManager NSDictionaryToVariantMap:attribs];
        return qAttribs;
    }
    error = [ShavHelperManager NSErrorToVariantMap:nsError];
    return qAttribs;
}
    
bool FileManager::setAttributes(QVariantMap attributes, QString path, QVariantMap &error)
{
    NSError *nserror = nil;
    NSDictionary *dict = [ShavHelperManager variantToNSDictionary:attributes];
    bool retval = [[NSFileManager defaultManager] setAttributes:dict
                                                   ofItemAtPath:path.toNSString()
                                                          error:&nserror]?true:false;
    if(nserror)
    {
        error = [ShavHelperManager NSErrorToVariantMap:nserror];
    }
    return retval;
}
    
QStringList FileManager::contentsOfDirectoryAtPath (QString path, QVariantMap &error)
{
    NSError *errorns = nil;
    NSString* p = path.toNSString();
    NSArray *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:p error:&errorns];
    QVariantList retlist = [ShavHelperManager NSArrayToVariantList:contents];
    QStringList retval;
    foreach(QVariant value, retlist)
        retval.append(value.toString());
    if(errorns)
        error = [ShavHelperManager NSErrorToVariantMap:errorns];
    return retval;
}
    
#ifdef __cplusplus
}
#endif
