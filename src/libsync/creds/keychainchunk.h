/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#ifndef KEYCHAINCHUNK_H
#define KEYCHAINCHUNK_H

#include "owncloudlib.h"

#include "accountfwd.h"

#include <qt6keychain/keychain.h>

#include <QObject>

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
class OWNCLOUDSYNC_EXPORT Job : public QObject
{
    Q_OBJECT
public:
    Job(QObject *parent = nullptr);

    ~Job() override;

    [[nodiscard]] QKeychain::Error error() const;
    [[nodiscard]] QString errorString() const;

    [[nodiscard]] QByteArray binaryData() const;
    [[nodiscard]] QString textData() const;

    [[nodiscard]] bool insecureFallback() const;

// If we use it but don't support insecure fallback, give us nice compilation errors ;p
#if defined(KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK)
    void setInsecureFallback(bool insecureFallback);
#endif

    /**
     * @return Whether this job autodeletes itself once finished() has been emitted. Default is true.
     * @see setAutoDelete()
     */
    [[nodiscard]] bool autoDelete() const;

    /**
     * Set whether this job should autodelete itself once finished() has been emitted.
     * @see autoDelete()
     */
    void setAutoDelete(bool autoDelete);

    /**
     * Set application name to be used to search the keychain
     */
    void setAppName(const QString &AppName);

#ifdef Q_OS_WIN
    /**
     * On Windows the credential keys aren't namespaced properly
     * by qtkeychain. To work around that we manually add namespacing
     * to the generated keys. See #6125.
     * It's safe to do that since the key format is changing for 2.4
     * anyway to include the account ids. That means old keys can be
     * migrated to new namespaced keys on windows for 2.4. 
     */
    void jobKeyPrependAppName(QString &key);
#endif

protected:
    QString _serviceName;
    Account *_account = nullptr;
    QString _key;
    QString _appName;
    bool _insecureFallback = false;
    bool _autoDelete = true;
    bool _keychainMigration = false;

    QKeychain::Error _error = QKeychain::NoError;
    QString _errorString;

    int _chunkCount = 0;
    QByteArray _chunkBuffer;
}; // class Job

/*
* @brief: Simple wrapper class for QKeychain::WritePasswordJob, splits too large keychain entry's data into chunks on Windows
*/
class OWNCLOUDSYNC_EXPORT WriteJob : public KeychainChunk::Job
{
    Q_OBJECT
public:
    WriteJob(Account *account, const QString &key, const QByteArray &data, QObject *parent = nullptr);
    WriteJob(const QString &key, const QByteArray &data, QObject *parent = nullptr);

    /**
     * Call this method to start the job (async).
     * You should connect some slot to the finished() signal first.
     *
     * @see QKeychain::Job::start()
     */
    void start();

    /**
     * Call this method to start the job synchronously.
     * Awaits completion with no need to connect some slot to the finished() signal first.
     *
     * @return Returns true on success (QKeychain::NoError).
    */
    bool exec();

signals:
    void finished(OCC::KeychainChunk::WriteJob *incomingJob);

private slots:
    void slotWriteJobDone(QKeychain::Job *incomingJob);
}; // class WriteJob

/*
* @brief: Simple wrapper class for QKeychain::ReadPasswordJob, splits too large keychain entry's data into chunks on Windows
*/
class OWNCLOUDSYNC_EXPORT ReadJob : public KeychainChunk::Job
{
    Q_OBJECT
public:
    ReadJob(Account *account, const QString &key, bool keychainMigration, QObject *parent = nullptr);
    ReadJob(const QString &key, QObject *parent = nullptr);

    /**
     * Call this method to start the job (async).
     * You should connect some slot to the finished() signal first.
     *
     * @see QKeychain::Job::start()
     */
    void start();

    /**
     * Call this method to start the job synchronously.
     * Awaits completion with no need to connect some slot to the finished() signal first.
     *
     * @return Returns true on success (QKeychain::NoError).
    */
    bool exec();

signals:
    void finished(OCC::KeychainChunk::ReadJob *incomingJob);

private slots:
    void slotReadJobDone(QKeychain::Job *incomingJob);

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
private:
    bool _retryOnKeyChainError = true; // true if we haven't done yet any reading from keychain
#endif
}; // class ReadJob

/*
* @brief: Simple wrapper class for QKeychain::DeletePasswordJob
*/
class OWNCLOUDSYNC_EXPORT DeleteJob : public KeychainChunk::Job
{
    Q_OBJECT
public:
    DeleteJob(Account *account, const QString &key, bool keychainMigration, QObject *parent = nullptr);
    DeleteJob(const QString &key, QObject *parent = nullptr);

    /**
     * Call this method to start the job (async).
     * You should connect some slot to the finished() signal first.
     *
     * @see QKeychain::Job::start()
     */
    void start();

    /**
     * Call this method to start the job synchronously.
     * Awaits completion with no need to connect some slot to the finished() signal first.
     *
     * @return Returns true on success (QKeychain::NoError).
    */
    bool exec();

signals:
    void finished(OCC::KeychainChunk::DeleteJob *incomingJob);

private slots:
    void slotDeleteJobDone(QKeychain::Job *incomingJob);
}; // class DeleteJob

} // namespace KeychainChunk

} // namespace OCC

#endif // KEYCHAINCHUNK_H
