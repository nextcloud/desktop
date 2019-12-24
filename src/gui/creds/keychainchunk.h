/*
 * Copyright (C) by Michael Schuster <michael@nextcloud.com>
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

#pragma once
#ifndef KEYCHAINCHUNK_H
#define KEYCHAINCHUNK_H

#include <QObject>
#include <keychain.h>
#include "accountfwd.h"

// We don't support insecure fallback
// #define KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK

namespace OCC {

namespace KeychainChunk {

/*
* Workaround for Windows:
*
* Split the keychain entry's data into chunks of 2048 bytes,
* to allow 4k (4096 bit) keys / large certs to be saved (see limits in webflowcredentials.h)
*/
static constexpr int ChunkSize = 2048;
static constexpr int MaxChunks = 10;

/*
 * @brief: Abstract base class for KeychainChunk jobs.
 */
class Job : public QObject {
    Q_OBJECT
public:
    Job(QObject *parent = nullptr);

    const QKeychain::Error error() const {
        return _error;
    }
    const QString errorString() const {
        return _errorString;
    }

    QByteArray binaryData() const {
        return _chunkBuffer;
    }

    const bool insecureFallback() const {
        return _insecureFallback;
    }

// If we use it but don't support insecure fallback, give us nice compilation errors ;p
#if defined(KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK)
    void setInsecureFallback(const bool &insecureFallback)
    {
        _insecureFallback = insecureFallback;
    }
#endif

protected:
    QString _serviceName;
    Account *_account;
    QString _key;
    bool _insecureFallback = false;
    bool _keychainMigration = false;

    QKeychain::Error _error = QKeychain::NoError;
    QString _errorString;

    int _chunkCount = 0;
    QByteArray _chunkBuffer;
}; // class Job

/*
* @brief: Simple wrapper class for QKeychain::WritePasswordJob, splits too large keychain entry's data into chunks on Windows
*/
class WriteJob : public KeychainChunk::Job {
    Q_OBJECT
public:
    WriteJob(Account *account, const QString &key, const QByteArray &data, QObject *parent = nullptr);
    void start();

signals:
    void finished(KeychainChunk::WriteJob *incomingJob);

private slots:
    void slotWriteJobDone(QKeychain::Job *incomingJob);
}; // class WriteJob

/*
* @brief: Simple wrapper class for QKeychain::ReadPasswordJob, splits too large keychain entry's data into chunks on Windows
*/
class ReadJob : public KeychainChunk::Job {
    Q_OBJECT
public:
    ReadJob(Account *account, const QString &key, const bool &keychainMigration, QObject *parent = nullptr);
    void start();

signals:
    void finished(KeychainChunk::ReadJob *incomingJob);

private slots:
    void slotReadJobDone(QKeychain::Job *incomingJob);
}; // class ReadJob

} // namespace KeychainChunk

} // namespace OCC

#endif // KEYCHAINCHUNK_H
