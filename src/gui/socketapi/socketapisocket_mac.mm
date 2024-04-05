/*
 * Copyright (C) by Jocelyn Turcotte <jturcotte@woboq.com>
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

#include "socketapisocket_mac.h"
#import <Cocoa/Cocoa.h>

@protocol ChannelProtocol <NSObject>

- (void)sendMessage:(NSData *)msg;

@end

@protocol RemoteEndProtocol <NSObject, ChannelProtocol>

- (void)registerTransmitter:(id)tx;

@end

@interface LocalEnd : NSObject <ChannelProtocol>

@property (atomic) SocketApiSocketPrivate *wrapper;

- (instancetype)initWithWrapper:(SocketApiSocketPrivate *)wrapper;

@end

@interface Server : NSObject

@property (atomic) SocketApiServerPrivate *wrapper;

- (instancetype)initWithWrapper:(SocketApiServerPrivate *)wrapper;
- (void)registerClient:(NSDistantObject<RemoteEndProtocol> *)remoteEnd;

@end

class SocketApiSocketPrivate
{
public:
    SocketApiSocket *q_ptr;

    SocketApiSocketPrivate(NSDistantObject<ChannelProtocol> *remoteEnd);
    ~SocketApiSocketPrivate();

    // release remoteEnd
    void disconnectRemote();

    NSDistantObject<ChannelProtocol> *remoteEnd;
    LocalEnd *localEnd;
    QByteArray inBuffer;
    bool isRemoteDisconnected = false;
};

class SocketApiServerPrivate
{
public:
    SocketApiServer *q_ptr;

    SocketApiServerPrivate();
    ~SocketApiServerPrivate();

    QList<SocketApiSocket *> pendingConnections;
    NSConnection *connection;
    Server *server;
};


@implementation LocalEnd

@synthesize wrapper = _wrapper;

- (instancetype)initWithWrapper:(SocketApiSocketPrivate *)wrapper
{
    self = [super init];
    self.wrapper = wrapper;
    return self;
}

- (void)sendMessage:(NSData *)msg
{
    if (self.wrapper) {
        self.wrapper->inBuffer += QByteArray::fromRawNSData(msg);
        Q_EMIT self.wrapper->q_ptr->readyRead();
    }
}

- (void)connectionDidDie:(NSNotification *)notification
{
    // The NSConnectionDidDieNotification docs say to disconnect from NSConnection here
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    if (self.wrapper) {
        self.wrapper->disconnectRemote();
        Q_EMIT self.wrapper->q_ptr->disconnected();
    }
}
@end

@implementation Server

@synthesize wrapper = _wrapper;

- (instancetype)initWithWrapper:(SocketApiServerPrivate *)wrapper
{
    self = [super init];
    self.wrapper = wrapper;
    return self;
}

- (void)registerClient:(NSDistantObject<RemoteEndProtocol> *)remoteEnd
{
    // This saves a few mach messages that would otherwise be needed to query the interface
    [remoteEnd setProtocolForProxy:@protocol(RemoteEndProtocol)];

    SocketApiServer *server = self.wrapper->q_ptr;
    SocketApiSocketPrivate *socketPrivate = new SocketApiSocketPrivate(remoteEnd);
    SocketApiSocket *socket = new SocketApiSocket(server, socketPrivate);
    self.wrapper->pendingConnections.append(socket);
    Q_EMIT server->newConnection();

    [remoteEnd registerTransmitter:socketPrivate->localEnd];
}
@end


SocketApiSocket::SocketApiSocket(QObject *parent, SocketApiSocketPrivate *p)
    : QIODevice(parent)
    , d_ptr(p)
{
    Q_D(SocketApiSocket);
    d->q_ptr = this;
    open(ReadWrite);
}

SocketApiSocket::~SocketApiSocket()
{
}

qint64 SocketApiSocket::readData(char *data, qint64 maxlen)
{
    Q_D(SocketApiSocket);
    qint64 len = std::min(maxlen, static_cast<qint64>(d->inBuffer.size()));
    if (len < 0 || len > std::numeric_limits<int>::max()) {
        return -1;
    }

    memcpy(data, d->inBuffer.constData(), static_cast<size_t>(len));
    d->inBuffer.remove(0, static_cast<int>(len));
    return len;
}

qint64 SocketApiSocket::writeData(const char *data, qint64 len)
{
    Q_D(SocketApiSocket);
    if (d->isRemoteDisconnected) {
        return -1;
    }

    if (len < std::numeric_limits<NSUInteger>::min() || len > std::numeric_limits<NSUInteger>::max()) {
        return -1;
    }

    @try {
        // FIXME: The NSConnection will make this block unless the function is marked as "oneway"
        // in the protocol. This isn't async and reduces our performances but this currectly avoids
        // a Mach queue deadlock during requests bursts of the legacy OwnCloudFinder extension.
        // Since FinderSync already runs in a separate process, blocking isn't too critical.
        NSData *payload = QByteArray::fromRawData(data, static_cast<int>(len)).toRawNSData();
        [d->remoteEnd sendMessage:payload];
        return len;
    } @catch (NSException *) {
        // connectionDidDie can be notified too late, also interpret any sending exception as a disconnection.
        d->disconnectRemote();
        Q_EMIT disconnected();
        return -1;
    }
}

qint64 SocketApiSocket::bytesAvailable() const
{
    Q_D(const SocketApiSocket);
    return d->inBuffer.size() + QIODevice::bytesAvailable();
}

bool SocketApiSocket::canReadLine() const
{
    Q_D(const SocketApiSocket);
    return d->inBuffer.indexOf('\n', int(pos())) != -1 || QIODevice::canReadLine();
}

SocketApiSocketPrivate::SocketApiSocketPrivate(NSDistantObject<ChannelProtocol> *remoteEnd)
    : remoteEnd(remoteEnd)
    , localEnd([[LocalEnd alloc] initWithWrapper:this])
{
    [remoteEnd retain];
    // (Ab)use our objective-c object just to catch the notification
    [[NSNotificationCenter defaultCenter] addObserver:localEnd
                                             selector:@selector(connectionDidDie:)
                                                 name:NSConnectionDidDieNotification
                                               object:[remoteEnd connectionForProxy]];
}

SocketApiSocketPrivate::~SocketApiSocketPrivate()
{
    disconnectRemote();

    // The DO vended localEnd might still be referenced by the connection
    localEnd.wrapper = nil;
    [localEnd release];
}

void SocketApiSocketPrivate::disconnectRemote()
{
    if (isRemoteDisconnected)
        return;
    isRemoteDisconnected = true;

    [remoteEnd release];
}

SocketApiServer::SocketApiServer()
    : d_ptr(new SocketApiServerPrivate)
{
    Q_D(SocketApiServer);
    d->q_ptr = this;
}

SocketApiServer::~SocketApiServer()
{
}

void SocketApiServer::close()
{
    // Assume we'll be destroyed right after
}

bool SocketApiServer::listen(const QString &name)
{
    Q_D(SocketApiServer);
    // Set the name of the root object
    return [d->connection registerName:name.toNSString()];
}

SocketApiSocket *SocketApiServer::nextPendingConnection()
{
    Q_D(SocketApiServer);
    return d->pendingConnections.takeFirst();
}

SocketApiServerPrivate::SocketApiServerPrivate()
{
    // Create the connection and server object to vend over Disributed Objects
    connection = [[NSConnection alloc] init];
    server = [[Server alloc] initWithWrapper:this];
    [connection setRootObject:server];
}

SocketApiServerPrivate::~SocketApiServerPrivate()
{
    [connection release];
    server.wrapper = nil;
    [server release];
}
