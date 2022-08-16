/*
 * Copyright (C) by Dominik Schmidt <dev@dominik-schmidt.de>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef SOCKETAPI_P_H
#define SOCKETAPI_P_H

#include <functional>
#include <QBitArray>
#include <QPointer>

#include <QJsonDocument>
#include <QJsonObject>

#include <memory>
#include <QTimer>

namespace OCC {

class BloomFilter
{
    // Initialize with m=1024 bits and k=2 (high and low 16 bits of a qHash).
    // For a client navigating in less than 100 directories, this gives us a probability less than
    // (1-e^(-2*100/1024))^2 = 0.03147872136 false positives.
    const static int NumBits = 1024;

public:
    BloomFilter()
        : hashBits(NumBits)
    {
    }

    void storeHash(uint hash)
    {
        hashBits.setBit((hash & 0xFFFF) % NumBits);
        hashBits.setBit((hash >> 16) % NumBits);
    }
    bool isHashMaybeStored(uint hash) const
    {
        return hashBits.testBit((hash & 0xFFFF) % NumBits)
            && hashBits.testBit((hash >> 16) % NumBits);
    }

private:
    QBitArray hashBits;
};

class SocketListener
{
public:
    QPointer<QIODevice> socket;

    explicit SocketListener(QIODevice *_socket)
        : socket(_socket)
    {
    }

    void sendMessage(const QString &message, bool doWait = false) const;
    void sendWarning(const QString &message, bool doWait = false) const
    {
        sendMessage(QStringLiteral("WARNING:") + message, doWait);
    }
    void sendError(const QString &message, bool doWait = false) const
    {
        sendMessage(QStringLiteral("ERROR:") + message, doWait);
    }

    void sendMessageIfDirectoryMonitored(const QString &message, uint systemDirectoryHash) const
    {
        if (_monitoredDirectoriesBloomFilter.isHashMaybeStored(systemDirectoryHash))
            sendMessage(message, false);
    }

    void registerMonitoredDirectory(uint systemDirectoryHash)
    {
        _monitoredDirectoriesBloomFilter.storeHash(systemDirectoryHash);
    }

private:
    BloomFilter _monitoredDirectoriesBloomFilter;
};

class ListenerClosure : public QObject
{
    Q_OBJECT
public:
    using CallbackFunction = std::function<void()>;
    ListenerClosure(const CallbackFunction &callback)
        : callback_(callback)
    {
    }

public slots:
    void closureSlot()
    {
        callback_();
        deleteLater();
    }

private:
    CallbackFunction callback_;
};

class SocketApiJob : public QObject
{
    Q_OBJECT
public:
    explicit SocketApiJob(const QString &jobId, const QSharedPointer<SocketListener> &socketListener, const QJsonObject &arguments)
        : _jobId(jobId)
        , _socketListener(socketListener)
        , _arguments(arguments)
    {
    }

    void resolve(const QString &response = QString());

    void resolve(const QJsonObject &response);

    const QJsonObject &arguments() { return _arguments; }

    void reject(const QString &response);

protected:
    QString _jobId;
    QSharedPointer<SocketListener> _socketListener;
    QJsonObject _arguments;
};

class SocketApiJobV2 : public QObject
{
    Q_OBJECT
public:
    explicit SocketApiJobV2(const QSharedPointer<SocketListener> &socketListener, const QString &command, const QJsonObject &arguments);

    void success(const QJsonObject &response) const;
    void failure(const QString &error) const;

    const QJsonObject &arguments() const { return _arguments; }
    QString command() const { return _command; }

    QString warning() const;
    void setWarning(const QString &warning);

Q_SIGNALS:
    void finished() const;

private:
    void doFinish(const QJsonObject &obj) const;

    QSharedPointer<SocketListener> _socketListener;
    const QString _command;
    QString _jobId;
    QJsonObject _arguments;
    QString _warning;
};
}

Q_DECLARE_METATYPE(OCC::SocketListener *)

#endif // SOCKETAPI_P_H
