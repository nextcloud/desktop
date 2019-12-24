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

#include "account.h"
#include "keychainchunk.h"
#include "theme.h"
#include "networkjobs.h"
#include "configfile.h"
#include "creds/abstractcredentials.h"

using namespace QKeychain;

namespace OCC {

Q_LOGGING_CATEGORY(lcKeychainChunk, "nextcloud.sync.credentials.keychainchunk", QtInfoMsg)

namespace KeychainChunk {

#if defined(KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK)
static void addSettingsToJob(Account *account, QKeychain::Job *job)
{
    Q_UNUSED(account)
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());
    settings->setParent(job); // make the job parent to make setting deleted properly
    job->setSettings(settings.release());
}
#endif

/*
* Job
*/
Job::Job(QObject *parent)
    : QObject(parent)
{
    _serviceName = Theme::instance()->appName();
}

/*
* WriteJob
*/
WriteJob::WriteJob(Account *account, const QString &key, const QByteArray &data, QObject *parent)
    : Job(parent)
{
    _account = account;
    _key = key;

    // Windows workaround: Split the private key into chunks of 2048 bytes,
    // to allow 4k (4096 bit) keys to be saved (obey Windows's limits)
    _chunkBuffer = data;
    _chunkCount = 0;
}

void WriteJob::start()
{
    slotWriteJobDone(nullptr);
}

void WriteJob::slotWriteJobDone(QKeychain::Job *incomingJob)
{
    QKeychain::WritePasswordJob *writeJob = static_cast<QKeychain::WritePasswordJob *>(incomingJob);

    // errors?
    if (writeJob) {
        _error = writeJob->error();
        _errorString = writeJob->errorString();

        if (writeJob->error() != NoError) {
            qCWarning(lcKeychainChunk) << "Error while writing" << writeJob->key() << "chunk" << writeJob->errorString();
            _chunkBuffer.clear();
        }
    }

    // write a chunk if there is any in the buffer
    if (!_chunkBuffer.isEmpty()) {
#if defined(Q_OS_WIN)
        // Windows workaround: Split the data into chunks of 2048 bytes,
        // to allow 4k (4096 bit) keys to be saved (obey Windows's limits)
        auto chunk = _chunkBuffer.left(KeychainChunk::ChunkSize);

        _chunkBuffer = _chunkBuffer.right(_chunkBuffer.size() - chunk.size());
#else
        // write full data in one chunk on non-Windows, as usual
        auto chunk = _chunkBuffer;

        _chunkBuffer.clear();
#endif
        auto index = (_chunkCount++);

        // keep the limit
        if (_chunkCount > KeychainChunk::MaxChunks) {
            qCWarning(lcKeychainChunk) << "Maximum chunk count exceeded while writing" << writeJob->key() << "chunk" << QString::number(index) << "cutting off after" << QString::number(KeychainChunk::MaxChunks) << "chunks";

            writeJob->deleteLater();

            _chunkBuffer.clear();

            emit finished(this);
            return;
        }

        const QString kck = AbstractCredentials::keychainKey(
            _account->url().toString(),
            _key + (index > 0 ? (QString(".") + QString::number(index)) : QString()),
            _account->id());

        QKeychain::WritePasswordJob *job = new QKeychain::WritePasswordJob(_serviceName);
#if defined(KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK)
        addSettingsToJob(_account, job);
#endif
        job->setInsecureFallback(_insecureFallback);
        connect(job, &QKeychain::Job::finished, this, &KeychainChunk::WriteJob::slotWriteJobDone);
        // only add the key's (sub)"index" after the first element, to stay compatible with older versions and non-Windows
        job->setKey(kck);
        job->setBinaryData(chunk);
        job->start();

        chunk.clear();
    } else {
        emit finished(this);
    }

    writeJob->deleteLater();
}

/*
* ReadJob
*/
ReadJob::ReadJob(Account *account, const QString &key, const bool &keychainMigration, QObject *parent)
    : Job(parent)
{
    _account = account;
    _key = key;

    _keychainMigration = keychainMigration;

    _chunkCount = 0;
    _chunkBuffer.clear();
}

void ReadJob::start()
{
    _chunkCount = 0;
    _chunkBuffer.clear();

    const QString kck = AbstractCredentials::keychainKey(
        _account->url().toString(),
        _key,
        _keychainMigration ? QString() : _account->id());

    QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(_serviceName);
#if defined(KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK)
    addSettingsToJob(_account, job);
#endif
    job->setInsecureFallback(_insecureFallback);
    job->setKey(kck);
    connect(job, &QKeychain::Job::finished, this, &KeychainChunk::ReadJob::slotReadJobDone);
    job->start();
}

void ReadJob::slotReadJobDone(QKeychain::Job *incomingJob)
{
    // Errors or next chunk?
    QKeychain::ReadPasswordJob *readJob = static_cast<QKeychain::ReadPasswordJob *>(incomingJob);

    if (readJob) {
        if (readJob->error() == NoError && readJob->binaryData().length() > 0) {
            _chunkBuffer.append(readJob->binaryData());
            _chunkCount++;

#if defined(Q_OS_WIN)
            // try to fetch next chunk
            if (_chunkCount < KeychainChunk::MaxChunks) {
                const QString kck = AbstractCredentials::keychainKey(
                    _account->url().toString(),
                    _key + QString(".") + QString::number(_chunkCount),
                    _keychainMigration ? QString() : _account->id());

                QKeychain::ReadPasswordJob *job = new QKeychain::ReadPasswordJob(_serviceName);
#if defined(KEYCHAINCHUNK_ENABLE_INSECURE_FALLBACK)
                addSettingsToJob(_account, job);
#endif
                job->setInsecureFallback(_insecureFallback);
                job->setKey(kck);
                connect(job, &QKeychain::Job::finished, this, &KeychainChunk::ReadJob::slotReadJobDone);
                job->start();

                readJob->deleteLater();
                return;
            } else {
                qCWarning(lcKeychainChunk) << "Maximum chunk count for" << readJob->key() << "reached, ignoring after" << KeychainChunk::MaxChunks;
            }
#endif
        } else {
            if (readJob->error() != QKeychain::Error::EntryNotFound ||
                ((readJob->error() == QKeychain::Error::EntryNotFound) && _chunkCount == 0)) {
                _error = readJob->error();
                _errorString = readJob->errorString();
                qCWarning(lcKeychainChunk) << "Unable to read" << readJob->key() << "chunk" << QString::number(_chunkCount) << readJob->errorString();
            }
        }

        readJob->deleteLater();
    }

    emit finished(this);
}

} // namespace KeychainChunk

} // namespace OCC
