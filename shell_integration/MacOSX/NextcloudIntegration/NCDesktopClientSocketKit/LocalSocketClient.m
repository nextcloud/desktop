/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Foundation/Foundation.h>
#import <os/log.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>

#import "LocalSocketClient.h"

@interface LocalSocketClient()
{
    NSString* _socketPath;
    id<LineProcessor> _lineProcessor;

    int _sock;
    dispatch_queue_t _localSocketQueue;
    dispatch_source_t _readSource;
    dispatch_source_t _writeSource;
    NSMutableData* _inBuffer;
    NSMutableData* _outBuffer;
    os_log_t _log;
}
@end

static os_log_t getLocalSocketClientLogger(void) {
    static dispatch_once_t onceToken;
    static os_log_t logger = NULL;
    dispatch_once(&onceToken, ^{
        NSBundle *bundle = [NSBundle bundleForClass:[LocalSocketClient class]];
        NSString *subsystem = bundle.bundleIdentifier ?: @"NCDesktopClientSocketKit";
        logger = os_log_create(subsystem.UTF8String, "LocalSocketClient");
    });
    return logger;
}

@implementation LocalSocketClient

- (instancetype)initWithSocketPath:(NSString*)socketPath
                     lineProcessor:(id<LineProcessor>)lineProcessor
{
    self = [super init];

    if(self) {
        _log = getLocalSocketClientLogger();
        os_log_debug(_log, "Initiating local socket client pointing to %{public}@", socketPath);

        _socketPath = socketPath;
        _lineProcessor = lineProcessor;

        _sock = -1;
        _localSocketQueue = dispatch_queue_create("localSocketQueue", DISPATCH_QUEUE_SERIAL);

        _inBuffer = [NSMutableData data];
        _outBuffer = [NSMutableData data];
    }

    return self;
}

- (BOOL)isConnected
{
    os_log_debug(_log, "Checking is connected: %{public}@", _sock != -1 ? @"YES" : @"NO");
    return _sock != -1;
}

- (void)start
{
    if([self isConnected]) {
        os_log_debug(_log, "Socket client already connected. Not starting.");
        return;
    }

    struct sockaddr_un localSocketAddr;
    unsigned long socketPathByteCount = [_socketPath lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    int maxByteCount = sizeof(localSocketAddr.sun_path);

    if(socketPathByteCount >= maxByteCount) {
        os_log_error(_log, "Socket path '%{public}@' is too long: maximum socket path length is %i, this path is of length %lu", _socketPath, maxByteCount, socketPathByteCount);
        return;
    }

    os_log_debug(_log, "Opening local socket...");

    _sock = socket(AF_LOCAL, SOCK_STREAM, 0);

    if(_sock == -1) {
        os_log_error(_log, "Cannot open socket: '%{public}@'", [self strErr]);
        [self restart];
        return;
    }

    os_log_debug(_log, "Local socket opened. Connecting to '%{public}@' ...", _socketPath);

    localSocketAddr.sun_family = AF_LOCAL & 0xff;

    const char* pathBytes = [_socketPath UTF8String];
    strcpy(localSocketAddr.sun_path, pathBytes);

    int connectionStatus = connect(_sock, (struct sockaddr*)&localSocketAddr, sizeof(localSocketAddr));

    if(connectionStatus == -1) {
        os_log_error(_log, "Could not connect to '%{public}@': '%{public}@'", _socketPath, [self strErr]);
        [self restart];
        return;
    }

    int flags = fcntl(_sock, F_GETFL, 0);

    if(fcntl(_sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        os_log_error(_log, "Could not set socket to non-blocking mode: '%{public}@'", [self strErr]);
        [self restart];
        return;
    }

    os_log_debug(_log, "Connected to socket. Setting up dispatch sources...");

    _readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, _sock, 0, _localSocketQueue);
    dispatch_source_set_event_handler(_readSource, ^(void){ [self readFromSocket]; });
    dispatch_source_set_cancel_handler(_readSource, ^(void){
        self->_readSource = nil;
        [self closeConnection];
    });

    _writeSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_WRITE, _sock, 0, _localSocketQueue);
    dispatch_source_set_event_handler(_writeSource, ^(void){ [self writeToSocket]; });
    dispatch_source_set_cancel_handler(_writeSource, ^(void){
        self->_writeSource = nil;
        [self closeConnection];
    });

    // These dispatch sources are suspended upon creation.
    // We resume the writeSource when we actually have something to write, suspending it again once our outBuffer is empty.
    // We start the readSource now.

    os_log_debug(_log, "Starting to read from socket");

    dispatch_resume(_readSource);
}

- (void)restart
{
    os_log_debug(_log, "Restarting connection to socket.");
    [self closeConnection];
    dispatch_async(dispatch_get_main_queue(), ^(void){
        [NSTimer scheduledTimerWithTimeInterval:5 repeats:NO block:^(NSTimer* timer) {
            [self start];
        }];
    });
}

- (void)closeConnection
{
    os_log_debug(_log, "Closing connection.");

    if(_readSource) {
        // Since dispatch_source_cancel works asynchronously, if we deallocate the dispatch source here then we can
        // cause a crash. So instead we strongly hold a reference to the read source and deallocate it asynchronously
        // with the handler.
        __block dispatch_source_t previousReadSource = _readSource;
        dispatch_source_set_cancel_handler(_readSource, ^{
            previousReadSource = nil;
        });
        dispatch_source_cancel(_readSource);
        // The readSource is still alive due to the other reference and will be deallocated by the cancel handler
        _readSource = nil;
    }

    if(_writeSource) {
        // Same deal with the write source
        __block dispatch_source_t previousWriteSource = _writeSource;
        dispatch_source_set_cancel_handler(_writeSource, ^{
            previousWriteSource = nil;
        });
        dispatch_source_cancel(_writeSource);
        _writeSource = nil;
    }

    [_inBuffer setLength:0];
    [_outBuffer setLength: 0];

    if(_sock != -1) {
        close(_sock);
        _sock = -1;
    }
}

- (NSString*)strErr
{
    int err = errno;
    const char *errStr = strerror(err);
    NSString *errorStr = [NSString stringWithUTF8String:errStr];

    if([errorStr length] > 0) {
        return errorStr;
    } else {
        return [NSString stringWithFormat:@"Unknown error code: %i", err];
    }
}

- (void)sendMessage:(NSString *)message
{
    dispatch_async(_localSocketQueue, ^(void) {
        if(![self isConnected]) {
            return;
        }

        BOOL writeSourceIsSuspended = [self->_outBuffer length] == 0;

        [self->_outBuffer appendData:[message dataUsingEncoding:NSUTF8StringEncoding]];

        os_log_debug(self->_log, "Writing to out buffer: '%{public}@'", message);
        os_log_debug(self->_log, "Out buffer now %li bytes", [self->_outBuffer length]);

        if(writeSourceIsSuspended) {
            os_log_debug(self->_log, "Resuming write dispatch source.");
            dispatch_resume(self->_writeSource);
        }
    });
}

- (void)askOnSocket:(NSString *)path query:(NSString *)verb
{
    NSString *line = [NSString stringWithFormat:@"%@:%@\n", verb, path];
    [self sendMessage:line];
}

- (void)writeToSocket
{
    if(![self isConnected]) {
        return;
    }

    if([_outBuffer length] == 0) {
        os_log_debug(_log, "Empty out buffer, suspending write dispatch source.");
        dispatch_suspend(_writeSource);
        return;
    }

    os_log_debug(_log, "About to write %li bytes from outbuffer to socket.", [_outBuffer length]);

    long bytesWritten = write(_sock, [_outBuffer bytes], [_outBuffer length]);
    
    if(bytesWritten > 0) {
        NSString *lineWritten = [[NSString alloc] initWithBytes:[_outBuffer bytes] 
                                                          length:bytesWritten 
                                                        encoding:NSUTF8StringEncoding];
        os_log_debug(_log, "Wrote %li bytes to socket. Line written was: '%{public}@'", bytesWritten, lineWritten ?: @"<invalid UTF-8>");
    } else {
        os_log_debug(_log, "Wrote %li bytes to socket.", bytesWritten);
    }

    if(bytesWritten == 0) {
        // 0 means we reached "end of file" and thus the socket was closed. So let's restart it
        os_log_debug(_log, "Socket was closed. Restarting...");
        [self restart];
    } else if(bytesWritten == -1) {
        int err = errno; // Make copy before it gets nuked by something else

        if(err == EAGAIN || err == EWOULDBLOCK)  {
            // No free space in the OS' buffer, nothing to do here
            os_log_debug(_log, "No free space in OS buffer. Ending write.");
            return;
        } else {
            os_log_error(_log, "Error writing to local socket: '%{public}@'", [self strErr]);
            [self restart];
        }
    } else if(bytesWritten > 0) {
        [_outBuffer replaceBytesInRange:NSMakeRange(0, bytesWritten) withBytes:NULL length:0];

        os_log_debug(_log, "Out buffer cleared. Now count is %li bytes.", [_outBuffer length]);

        if([_outBuffer length] == 0) {
            os_log_debug(_log, "Out buffer has been emptied, suspending write dispatch source.");
            dispatch_suspend(_writeSource);
        }
    }
}

- (void)askForIcon:(NSString*)path isDirectory:(BOOL)isDirectory;
{
    os_log_debug(_log, "Asking for icon.");

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

    os_log_debug(_log, "Reading from socket.");

    int bufferLength = BUF_SIZE / 2;
    char buffer[bufferLength];

    while(true) {
        long bytesRead = read(_sock, buffer, bufferLength);

        os_log_debug(_log, "Read %li bytes from socket.", bytesRead);

        if(bytesRead == 0) {
            // 0 means we reached "end of file" and thus the socket was closed. So let's restart it
            os_log_debug(_log, "Socket was closed. Restarting...");
            [self restart];
            return;
        } else if(bytesRead == -1) {
            int err = errno;
            if(err == EAGAIN) {
                os_log_debug(_log, "No error and no data. Stopping.");
                return; // No error, no data, so let's stop
            } else {
                os_log_error(_log, "Error reading from local socket: '%{public}@'", [self strErr]);
                [self closeConnection];
                return;
            }
        } else {
            [_inBuffer appendBytes:buffer length:bytesRead];
            [self processInBuffer];
        }
    }
}

- (void)processInBuffer
{
    os_log_debug(_log, "Processing in buffer. In buffer length %li", _inBuffer.length);

    static const UInt8 separator[] = {0xa}; // Byte value for "\n"
    static const char terminator[] = {0};
    NSData * const separatorData = [NSData dataWithBytes:separator length:1];

    while(_inBuffer.length > 0) {
        const NSUInteger inBufferLength = _inBuffer.length;
        const NSRange inBufferLengthRange = NSMakeRange(0, inBufferLength);
        const NSRange firstSeparatorIndex = [_inBuffer rangeOfData:separatorData
                                                           options:0
                                                             range:inBufferLengthRange];

        NSUInteger nullTerminatorIndex = NSUIntegerMax;

        // Add NULL terminator, so we can use C string methods
        if (firstSeparatorIndex.location == NSNotFound) {
            os_log_debug(_log, "No separator found. Creating new buffer with space for null terminator.");

            [_inBuffer appendBytes:terminator length:1];
            nullTerminatorIndex = inBufferLength;
        } else {
            nullTerminatorIndex = firstSeparatorIndex.location;
            [_inBuffer replaceBytesInRange:NSMakeRange(nullTerminatorIndex, 1) withBytes:terminator];
        }

        NSAssert(nullTerminatorIndex != NSUIntegerMax, @"Null terminator index should be valid.");

        NSString * const newLine = [NSString stringWithUTF8String:_inBuffer.bytes];
        const NSRange nullTerminatorRange = NSMakeRange(0, nullTerminatorIndex + 1);

        [_inBuffer replaceBytesInRange:nullTerminatorRange withBytes:NULL length:0];
        [_lineProcessor process:newLine];
    }

    os_log_debug(_log, "Finished processing inBuffer");
}

@end
