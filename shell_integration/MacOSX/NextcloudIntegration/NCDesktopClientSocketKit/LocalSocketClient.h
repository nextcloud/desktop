/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#import <NCDesktopClientSocketKit/LineProcessor.h>

#ifndef LocalSocketClient_h
#define LocalSocketClient_h
#define BUF_SIZE 4096

/// Class handling asynchronous communication with a server over a local UNIX socket.
///
/// The implementation uses a `DispatchQueue` and `DispatchSource`s to handle asynchronous communication and thread
/// safety. The delegate that handles the line-decoding is **not invoked on the UI thread**, but the (random) thread associated
/// with the `DispatchQueue`.
///
/// If any UI work needs to be done, the `LineProcessor` class dispatches this work on the main queue (so the UI thread) itself.
///
/// Other than the `init(withSocketPath:, lineProcessor)` and the `start()` method, all work is done "on the dispatch
/// queue". The `localSocketQueue` is a serial dispatch queue (so a maximum of 1, and only 1, task is run at any
/// moment), which guarantees safe access to instance variables. Both `askOnSocket(_:, query:)` and
/// `askForIcon(_:, isDirectory:)` will internally dispatch the work on the `DispatchQueue`.
///
/// Sending and receiving data to and from the socket, is handled by two `DispatchSource`s. These will run an event
/// handler when data can be read from resp. written to the socket. These handlers will also be run on the
/// `DispatchQueue`.

@interface LocalSocketClient : NSObject

- (instancetype)initWithSocketPath:(NSString*)socketPath
                     lineProcessor:(id<LineProcessor>)lineProcessor;

@property (readonly) BOOL isConnected;

- (void)start;
- (void)restart;
- (void)closeConnection;

- (void)sendMessage:(NSString*)message;
- (void)askOnSocket:(NSString*)path
              query:(NSString*)verb;
- (void)askForIcon:(NSString*)path
       isDirectory:(BOOL)isDirectory;

@end
#endif /* LocalSocketClient_h */
