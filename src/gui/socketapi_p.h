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

    explicit SocketListener(QIODevice *socket)
        : socket(socket)
    {
    }

    void sendMessage(const QString &message, bool doWait = false) const;

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
    ListenerClosure(CallbackFunction callback)
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
    SocketApiJob(const QString &jobId, SocketListener *socketListener, const QJsonObject &arguments)
        : _jobId(jobId)
        , _socketListener(socketListener)
        , _arguments(arguments)
    {
    }

    void resolve(const QString &response = QString())
    {
        _socketListener->sendMessage(QLatin1String("RESOLVE|") + _jobId + '|' + response);
    }

    void resolve(const QJsonObject &response) { resolve(QJsonDocument{ response }.toJson()); }

    const QJsonObject &arguments() { return _arguments; }

    void reject(const QString &response)
    {
        _socketListener->sendMessage(QLatin1String("REJECT|") + _jobId + '|' + response);
    }

private:
    QString _jobId;
    SocketListener *_socketListener;
    QJsonObject _arguments;
};
}

Q_DECLARE_METATYPE(OCC::SocketListener *)

#endif // SOCKETAPI_P_H
