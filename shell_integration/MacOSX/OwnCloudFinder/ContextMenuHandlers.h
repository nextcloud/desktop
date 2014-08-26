/**
 * Copyright (c) 2000-2012 Liferay, Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#import <Foundation/Foundation.h>

@interface NSObject (ContextMenuHandlers)

struct TFENodeVector;

+ (void)OCContextMenuHandlers_handleContextMenuCommon:(unsigned int)arg1 nodes:(const struct TFENodeVector*)arg2 event:(id)arg3 view:(id)arg4 browserController:(id)arg5 addPlugIns:(BOOL)arg6;
+ (void)OCContextMenuHandlers_handleContextMenuCommon:(unsigned int)arg1 nodes:(const struct TFENodeVector*)arg2 event:(id)arg3 view:(id)arg4 windowController:(id)arg5 addPlugIns:(BOOL)arg6;
+ (void)OCContextMenuHandlers_addViewSpecificStuffToMenu:(id)arg1 browserViewController:(id)arg2 context:(unsigned int)arg3;

- (void)OCContextMenuHandlers_configureWithNodes:(const struct TFENodeVector*)arg1 browserController:(id)arg2 container:(BOOL)arg3;
- (void)OCContextMenuHandlers_configureWithNodes:(const struct TFENodeVector*)arg1 windowController:(id)arg2 container:(BOOL)arg3;

@end