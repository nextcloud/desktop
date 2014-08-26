//
// LNVersionComparisonProtocol.h
// Sparkle
//
// Created by Andy Matuschak on 12/21/07.
// Copyright 2007 Andy Matuschak. All rights reserved.
//

#ifndef LNVERSIONCOMPARISONPROTOCOL_H
#define LNVERSIONCOMPARISONPROTOCOL_H

/*!
    @protocol
    @abstract    Implement this protocol to provide version comparison facilities for Sparkle.
 */
@protocol LNVersionComparison

/*!
    @method
    @abstract   An abstract method to compare two version strings.
    @discussion Should return NSOrderedAscending if b > a, NSOrderedDescending if b < a, and NSOrderedSame if they are equivalent.
 */
-(NSComparisonResult)compareVersion:(NSString*)versionA toVersion:(NSString*)versionB;

@end

#endif
