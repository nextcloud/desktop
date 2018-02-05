/*
 * Copyright (C) by Ahmed Ammar <ahmed.a.ammar@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#include <QLoggingCategory>
#include <QTemporaryFile>
#include <QRunnable>
#include <QThreadPool>

extern "C" {
#include "librcksum/rcksum.h"
#include "libzsync/zmap.h"
#include "libzsync/sha1.h"
#include "libzsync/zsync.h"
}

#define ZSYNC_BLOCKSIZE (1 * 1024 * 1024) // must be power of 2

namespace OCC {
Q_DECLARE_LOGGING_CATEGORY(lcZsyncPut)
Q_DECLARE_LOGGING_CATEGORY(lcZsyncGet)

enum class ZsyncMode { download,
    upload };

template <typename T>
using zsync_unique_ptr = std::unique_ptr<T, std::function<void(T *)>>;

/**
 * @ingroup libsync
 *
 * Helper function to know if we are allowed to attempt using zsync from configuration/command-line options.
 *
 */
bool isZsyncPropagationEnabled(OwncloudPropagator *propagator, const SyncFileItemPtr &item);

/**
 * @ingroup libsync
 *
 * Helper function to get zsync metadata Url.
 *
 */
QUrl zsyncMetadataUrl(OwncloudPropagator *propagator, const QString &path);

/**
 * @ingroup libsync
 *
 * Helper runnable to 'seed' the zsync_state by providing the downloaded metadata and seed file.
 * This is needed for both upload and download since they both must seed the zsync_state to know which
 * ranges to upload/download.
 *
 */
class ZsyncSeedRunnable : public QObject, public QRunnable
{
    Q_OBJECT
    QByteArray _zsyncData;
    QString _zsyncFilePath;
    QString _tmpFilePath;
    ZsyncMode _type;

public:
    explicit ZsyncSeedRunnable(QByteArray &zsyncData, QString path, ZsyncMode type, QString tmpFilePath = nullptr)
        : _zsyncData(zsyncData)
        , _zsyncFilePath(path)
        , _tmpFilePath(tmpFilePath)
        , _type(type){};

    void run();

signals:
    void finishedSignal(void *zs);
    void failedSignal(const QString &errorString);
};

/**
 * @ingroup libsync
 *
 * Helper runnable to generate zsync metadata file when uploading.
 * Takes an input file path and returns a zsync metadata file path finsihed.
 *
 */
class ZsyncGenerateRunnable : public QObject, public QRunnable
{
    Q_OBJECT
    const QString _file;

public:
    explicit ZsyncGenerateRunnable(const QString &file)
        : _file(file){};

    void run();

signals:
    void finishedSignal(const QString &generatedFileName);
    void failedSignal(const QString &errorString);
};
}
