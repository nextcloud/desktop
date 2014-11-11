//
//  FinishedIconCache.m
//  OwnCloudFinder
//
//  Created by Markus Goetz on 01/10/14.
//
//

#import "FinishedIconCache.h"


@interface FinishedIconCacheItem : NSObject
@property (nonatomic, strong) NSImage *icon;
@property (nonatomic) NSTimeInterval maxAge;
@end

@implementation FinishedIconCacheItem
@synthesize icon;
@synthesize maxAge;
- (void)dealloc {
	//NSLog(@"RELEASE %@ %@", self, self.icon);
	if (self.icon) {
		[self->icon release];
	}
	[super dealloc];
}
@end

@implementation FinishedIconCache

static FinishedIconCache* sharedInstance = nil;

- init
{
	self = [super init];
	if (self)
	{
		_cache = [[NSCache alloc] init];
		_cache.totalCostLimit = (2880 * 1800); // mbp15 screen size
		_hits = 0;
		_misses = 0;
	}
	return self;
}

- (void)dealloc
{
	[_cache dealloc];
	[super dealloc];
}

+ (FinishedIconCache*)sharedInstance
{
	@synchronized(self)
	{
		if (sharedInstance == nil)
		{
			sharedInstance = [[self alloc] init];
		}
	}
	return sharedInstance;
}


- (NSImage*)getIcon:(NSString*)fileName overlayIconIndex:(int)idx width:(float)w height:(float)h
{
	NSString *cacheKey = [NSString stringWithFormat:@"%@--%d--%f%f", fileName, idx, w,h];
	FinishedIconCacheItem *item = [_cache objectForKey:cacheKey];
	if (item) {
		if (item.maxAge > [[NSDate date] timeIntervalSinceReferenceDate]) {
			_hits++;
			return item.icon;
		}
	}
	_misses++;
	return NULL;
}

- (void)registerIcon:(NSImage*)icon withFileName:(NSString*)fileName overlayIconIndex:(int)idx width:(float)w height:(float)h
{
	NSString *cacheKey = [NSString stringWithFormat:@"%@--%d--%f%f", fileName, idx, w, h];
	FinishedIconCacheItem *item = [[FinishedIconCacheItem alloc] init];
	item.icon = icon;
	// max age between 1 sec and 5 sec
	item.maxAge = [[NSDate date] timeIntervalSinceReferenceDate] + 1.0 + 4.0*((double)arc4random() / 0x100000000);
	[_cache setObject:item forKey:cacheKey cost:w*h];
	[item release];
	//NSLog(@"CACHE hit/miss ratio: %f", (float)_hits/(float)_misses);
}

@end
