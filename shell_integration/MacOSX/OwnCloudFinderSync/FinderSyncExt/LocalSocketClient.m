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

#import <Foundation/Foundation.h>
#import "LocalSocketClient.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>

@implementation LocalSocketClient

- (instancetype)init:(NSString*)socketPath lineProcessor:(LineProcessor*)lineProcessor
{
    NSLog(@"Initiating local socket client.");
    self = [super init];
    
    if(self) {
        self.socketPath = socketPath;
        self.lineProcessor = lineProcessor;
        
        self.sock = -1;
        self.localSocketQueue = dispatch_queue_create("localSocketQueue", DISPATCH_QUEUE_SERIAL);
        
        self.inBuffer = [NSMutableData data];
        self.outBuffer = [NSMutableData data];
    }
        
    return self;
}

- (BOOL)isConnected
{
    NSLog(@"Checking is connected: %@", self.sock != -1 ? @"YES" : @"NO");
    return self.sock != -1;
}

- (void)start
{
    if([self isConnected]) {
        NSLog(@"Socket client already connected. Not starting.");
        return;
    }
    
    struct sockaddr_un localSocketAddr;
    unsigned long socketPathByteCount = [self.socketPath lengthOfBytesUsingEncoding:NSUTF8StringEncoding]; // add 1 for the NUL terminator char
    int maxByteCount = sizeof(localSocketAddr.sun_path);
    
    if(socketPathByteCount > maxByteCount) {
        // LOG THAT THE SOCKET PATH IS TOO LONG HERE
        NSLog(@"Socket path '%@' is too long: maximum socket path length is %i, this path is of length %lu", self.socketPath, maxByteCount, socketPathByteCount);
        return;
    }
    
    NSLog(@"Opening local socket...");
    
    // LOG THAT THE SOCKET IS BEING OPENED HERE
    self.sock = socket(AF_LOCAL, SOCK_STREAM, 0);
    
    if(self.sock == -1) {
        NSLog(@"Cannot open socket: '%@'", [self strErr]);
        [self restart];
        return;
    }
    
    NSLog(@"Local socket opened. Connecting to '%@' ...", self.socketPath);
    
    localSocketAddr.sun_family = AF_LOCAL & 0xff;
    
    const char* pathBytes = [self.socketPath UTF8String];
    strcpy(localSocketAddr.sun_path, pathBytes);
    
    int connectionStatus = connect(self.sock, (struct sockaddr*)&localSocketAddr, sizeof(localSocketAddr));
    
    if(connectionStatus == -1) {
        NSLog(@"Could not connect to '%@': '%@'", self.socketPath, [self strErr]);
        [self restart];
        return;
    }
    
    int flags = fcntl(self.sock, F_GETFL, 0);
    
    if(fcntl(self.sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        NSLog(@"Could not set socket to non-blocking mode: '%@'", [self strErr]);
        [self restart];
        return;
    }
    
    NSLog(@"Connected to socket. Setting up dispatch sources...");
    
    self.readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, self.sock, 0, self.localSocketQueue);
    dispatch_source_set_event_handler(self.readSource, ^(void){ [self readFromSocket]; });
    dispatch_source_set_cancel_handler(self.readSource, ^(void){
        self.readSource = nil;
        [self closeConnection];
    });
    
    self.writeSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, self.sock, 0, self.localSocketQueue);
    dispatch_source_set_event_handler(self.writeSource, ^(void){ [self writeToSocket]; });
    dispatch_source_set_cancel_handler(self.writeSource, ^(void){
        self.writeSource = nil;
        [self closeConnection];
    });
    
    // These dispatch sources are suspended upon creation.
    // We resume the writeSource when we actually have something to write, suspending it again once our outBuffer is empty.
    // We start the readSource now.
    
    NSLog(@"Starting to read from socket");
    
    dispatch_resume(self.readSource);
    [self askOnSocket:@"" query:@"GET_STRINGS"];
}

- (void)restart
{
    NSLog(@"Restarting connection to socket.");
    [self closeConnection];
    dispatch_async(dispatch_get_main_queue(), ^(void){
        [NSTimer scheduledTimerWithTimeInterval:5 repeats:NO block:^(NSTimer* timer) {
            [self start];
        }];
    });
}

- (void)closeConnection
{
    NSLog(@"Closing connection.");
    
    if(self.readSource) {
        // Since dispatch_source_cancel works asynchronously, if we deallocate the dispatch source here then we can
        // cause a crash. So instead we strongly hold a reference to the read source and deallocate it asynchronously
        // with the handler.
        __block dispatch_source_t previousReadSource = self.readSource;
        dispatch_source_set_cancel_handler(self.readSource, ^{
            previousReadSource = nil;
        });
        dispatch_source_cancel(self.readSource);
        // The readSource is still alive due to the other reference and will be deallocated by the cancel handler
        self.readSource = nil;
    }
    
    if(self.writeSource) {
        // Same deal with the write source
        __block dispatch_source_t previousWriteSource = self.writeSource;
        dispatch_source_set_cancel_handler(self.writeSource, ^{
            previousWriteSource = nil;
        });
        dispatch_source_cancel(self.writeSource);
        self.writeSource = nil;
    }

    [self.inBuffer setLength:0];
    [self.outBuffer setLength: 0];
    
    if(self.sock != -1) {
        close(self.sock);
        self.sock = -1;
    }
}

- (NSString*)strErr
{
    int err = errno;
    const char *errStr = strerror(err);
    NSString *errorStr = [NSString stringWithUTF8String:errStr];
    
    if([errorStr length] == 0) {
        return errorStr;
    } else {
        return [NSString stringWithFormat:@"Unknown error code: %i", err];
    }
}

- (void)askOnSocket:(NSString *)path query:(NSString *)verb
{
    NSString *line = [NSString stringWithFormat:@"%@:%@\n", verb, path];
    dispatch_async(self.localSocketQueue, ^(void) {
        if(![self isConnected]) {
            return;
        }
        
        BOOL writeSourceIsSuspended = [self.outBuffer length] == 0;
        
        [self.outBuffer appendData:[line dataUsingEncoding:NSUTF8StringEncoding]];
        
        NSLog(@"Writing to out buffer: '%@'", line);
        NSLog(@"Out buffer now %li bytes", [self.outBuffer length]);
        
        if(writeSourceIsSuspended) {
            NSLog(@"Resuming write dispatch source.");
            dispatch_resume(self.writeSource);
        }
    });
}

- (void)writeToSocket
{
    if(![self isConnected]) {
        return;
    }
    
    if([self.outBuffer length] == 0) {
        NSLog(@"Empty out buffer, suspending write dispatch source.");
        dispatch_suspend(self.writeSource);
        return;
    }
    
    NSLog(@"About to write %li bytes from outbuffer to socket.", [self.outBuffer length]);
    
    long bytesWritten = write(self.sock, [self.outBuffer bytes], [self.outBuffer length]);
    char lineWritten[[self.outBuffer length]];
    memcpy(lineWritten, [self.outBuffer bytes], [self.outBuffer length]);
    NSLog(@"Wrote %li bytes to socket. Line written was: '%@'", bytesWritten, [NSString stringWithUTF8String:lineWritten]);
    
    if(bytesWritten == 0) {
        // 0 means we reached "end of file" and thus the socket was closed. So let's restart it
        NSLog(@"Socket was closed. Restarting...");
        [self restart];
    } else if(bytesWritten == -1) {
        int err = errno; // Make copy before it gets nuked by something else
        
        if(err == EAGAIN || err == EWOULDBLOCK)  {
            // No free space in the OS' buffer, nothing to do here
            NSLog(@"No free space in OS buffer. Ending write.");
            return;
        } else {
            NSLog(@"Error writing to local socket: '%@'", [self strErr]);
            [self restart];
        }
    } else if(bytesWritten > 0) {
        [self.outBuffer replaceBytesInRange:NSMakeRange(0, bytesWritten) withBytes:NULL length:0];
        
        NSLog(@"Out buffer cleared. Now count is %li bytes.", [self.outBuffer length]);
        
        if([self.outBuffer length] == 0) {
            NSLog(@"Out buffer has been emptied, suspending write dispatch source.");
            dispatch_suspend(self.writeSource);
        }
    }
}

- (void)askForIcon:(NSString*)path isDirectory:(BOOL)isDirectory;
{
    NSLog(@"Asking for icon.");
    
    NSString *verb;
    if(isDirectory) {
        verb = @"RETRIEVE_FOLDER_STATUS";
    } else {
        verb = @"RETRIEVE_FILE_STATUS";
    }
    
    [self askOnSocket:path query:verb];
}

- (void)readFromSocket
{
    if(![self isConnected]) {
        return;
    }
    
    NSLog(@"Reading from socket.");
    
    int bufferLength = BUF_SIZE / 2;
    char buffer[bufferLength];
    
    while(true) {
        long bytesRead = read(self.sock, buffer, bufferLength);
        
        NSLog(@"Read %li bytes from socket.", bytesRead);
        
        if(bytesRead == 0) {
            // 0 means we reached "end of file" and thus the socket was closed. So let's restart it
            NSLog(@"Socket was closed. Restarting...");
            [self restart];
            return;
        } else if(bytesRead == -1) {
            int err = errno;
            if(err == EAGAIN) {
                NSLog(@"No error and no data. Stopping.");
                return; // No error, no data, so let's stop
            } else {
                NSLog(@"Error reading from local socket: '%@'", [self strErr]);
                [self closeConnection];
                return;
            }
        } else {
            [self.inBuffer appendBytes:buffer length:bytesRead];
            [self processInBuffer];
        }
    }
}

- (void)processInBuffer
{
    NSLog(@"Processing in buffer. In buffer length %li", [self.inBuffer length]);
    UInt8 separator[] = {0xa}; // Byte value for "\n"
    while(true) {
        NSRange firstSeparatorIndex = [self.inBuffer rangeOfData:[NSData dataWithBytes:separator length:1] options:0 range:NSMakeRange(0, [self.inBuffer length])];
        
        if(firstSeparatorIndex.location == NSNotFound) {
            NSLog(@"No separator found. Stopping.");
            return; // No separator, nope out
        } else {
            unsigned char *buffer = [self.inBuffer mutableBytes];
            buffer[firstSeparatorIndex.location] = 0; // Add NULL terminator, so we can use C string methods
            
            NSString *newLine = [NSString stringWithUTF8String:[self.inBuffer bytes]];

            [self.inBuffer replaceBytesInRange:NSMakeRange(0, firstSeparatorIndex.location + 1) withBytes:NULL length:0];
            [self.lineProcessor process:newLine];
        }
    }
}
    
@end
