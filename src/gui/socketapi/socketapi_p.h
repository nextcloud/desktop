/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
        hashBits.setBit((hash & 0xFFFF) % NumBits); // NOLINT it's uint all the way and the modulo puts us back in the 0..1023 range
        hashBits.setBit((hash >> 16) % NumBits); // NOLINT
    }
    [[nodiscard]] bool isHashMaybeStored(uint hash) const
    {
        return hashBits.testBit((hash & 0xFFFF) % NumBits) // NOLINT
            && hashBits.testBit((hash >> 16) % NumBits); // NOLINT
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

}

Q_DECLARE_METATYPE(OCC::SocketListener *)

#endif // SOCKETAPI_P_H
