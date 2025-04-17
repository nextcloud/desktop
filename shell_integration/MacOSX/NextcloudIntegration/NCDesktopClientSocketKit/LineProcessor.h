/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LineProcessor_h
#define LineProcessor_h

@protocol LineProcessor<NSObject>

- (void)process:(NSString*)line;

@end

#endif /* LineProcessor_h */
