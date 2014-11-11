//
//  FinishedIconCache.h
//  OwnCloudFinder
//
//  Created by Markus Goetz on 01/10/14.
//
//

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

@interface FinishedIconCache : NSObject {
	NSCache *_cache;
	long long _hits;
	long long _misses;
}

+ (FinishedIconCache*)sharedInstance;

- (NSImage*)getIcon:(NSString*)fileName overlayIconIndex:(int)idx width:(float)w height:(float)h;
- (void)registerIcon:(NSImage*)icon withFileName:(NSString*)fileName overlayIconIndex:(int)idx width:(float)w height:(float)h;


@end
