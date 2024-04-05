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

#ifndef SOCKETAPISOCKET_OSX_H
#define SOCKETAPISOCKET_OSX_H

#include <QAbstractSocket>
#include <QIODevice>

class SocketApiServerPrivate;
class SocketApiSocketPrivate;

class SocketApiSocket : public QIODevice
{
    Q_OBJECT
public:
    SocketApiSocket(QObject *parent, SocketApiSocketPrivate *p);
    ~SocketApiSocket() override;

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;
    bool canReadLine() const override;

Q_SIGNALS:
    void disconnected();

private:
    // Use Qt's p-impl system to hide objective-c types from C++ code including this file
    Q_DECLARE_PRIVATE(SocketApiSocket)
    QScopedPointer<SocketApiSocketPrivate> d_ptr;
    friend class SocketApiServerPrivate;
};

class SocketApiServer : public QObject
{
    Q_OBJECT
public:
    SocketApiServer();
    ~SocketApiServer() override;

    void close();
    bool listen(const QString &name);
    SocketApiSocket *nextPendingConnection();

    static bool removeServer(const QString &) { return false; }

Q_SIGNALS:
    void newConnection();

private:
    Q_DECLARE_PRIVATE(SocketApiServer)
    QScopedPointer<SocketApiServerPrivate> d_ptr;
};

#endif // SOCKETAPISOCKET_OSX_H
