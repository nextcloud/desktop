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

#include "config.h"
#include "propagateupload.h"
#include "owncloudpropagator_p.h"
#include "networkjobs.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include "syncengine.h"
#include "propagateremotemove.h"
#include "propagateremotedelete.h"
#include "common/asserts.h"

#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>

extern "C" {
#include "libzsync/zsyncfile.h"
}

namespace OCC {

Q_LOGGING_CATEGORY(lcZsyncSeed, "sync.propagate.zsync.seed", QtInfoMsg)
Q_LOGGING_CATEGORY(lcZsyncGenerate, "sync.propagate.zsync.generate", QtInfoMsg)
Q_LOGGING_CATEGORY(lcZsyncGet, "sync.networkjob.zsync.get", QtInfoMsg)
Q_LOGGING_CATEGORY(lcZsyncPut, "sync.networkjob.zsync.put", QtInfoMsg)

bool isZsyncPropagationEnabled(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
{
    if (propagator->account()->capabilities().zsyncSupportedVersion() != "1.0") {
        qCInfo(lcPropagator) << "[zsync disabled] Lack of server support.";
        return false;
    }
    if (item->_remotePerm.hasPermission(RemotePermissions::IsMounted) || item->_remotePerm.hasPermission(RemotePermissions::IsMountedSub)) {
        qCInfo(lcPropagator) << "[zsync disabled] External storage not supported.";
        return false;
    }
    if (!propagator->syncOptions()._deltaSyncEnabled) {
        qCInfo(lcPropagator) << "[zsync disabled] Client configuration option.";
        return false;
    }
    if (item->_size < propagator->syncOptions()._deltaSyncMinFileSize) {
        qCInfo(lcPropagator) << "[zsync disabled] File size is smaller than minimum.";
        return false;
    }

    return true;
}

QUrl zsyncMetadataUrl(OwncloudPropagator *propagator, const QString &path)
{
    QUrlQuery urlQuery;
    QList<QPair<QString, QString>> QueryItems({ { "zsync", nullptr } });
    urlQuery.setQueryItems(QueryItems);
    return Utility::concatUrlPath(propagator->account()->davUrl(), propagator->_remoteFolder + path, urlQuery);
}

void ZsyncSeedRunnable::run()
{
    // Create a temporary file to use with zsync_begin()
    QTemporaryFile zsyncControlFile;
    zsyncControlFile.open();
    zsyncControlFile.write(_zsyncData.constData(), _zsyncData.size());
    zsyncControlFile.flush();

    int fileHandle = zsyncControlFile.handle();
    zsync_unique_ptr<FILE> f(fdopen(dup(fileHandle), "r"), [](FILE *f) {
        fclose(f);
    });
    zsyncControlFile.close();
    rewind(f.get());

    zsync_unique_ptr<struct zsync_state> zs(zsync_parse(f.get()), [](struct zsync_state *zs) {
        zsync_end(zs);
    });
    if (!zs) {
        QString errorString = tr("Unable to parse zsync file.");
        emit failedSignal(errorString);
        return;
    }

    QByteArray tmp_file;
    if (!_tmpFilePath.isEmpty()) {
        tmp_file = _tmpFilePath.toLocal8Bit();
    } else {
        QTemporaryFile tmpFile;
        tmpFile.open();
        tmp_file = tmpFile.fileName().toLocal8Bit();
        tmpFile.close();
    }

    const char *tfname = tmp_file;
    if (zsync_rename_file(zs.get(), tfname) != 0) {
        QString errorString = tr("Unable to rename temporary file.");
        emit failedSignal(errorString);
        return;
    }

    if (zsync_begin(zs.get(), f.get())) {
        QString errorString = tr("Unable to begin zsync.");
        emit failedSignal(errorString);
        return;
    }

    {
        /* Simple uncompressed file - open it */
        QFile file(_zsyncFilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QString errorString = tr("Unable to open file.");
            emit failedSignal(errorString);
            return;
        }

        /* Give the contents to libzsync to read, to find any content that
         * is part of the target file. */
        qCInfo(lcZsyncSeed) << "Reading seed file:" << _zsyncFilePath;
        int fileHandle = file.handle();
        zsync_unique_ptr<FILE> f(fdopen(dup(fileHandle), "r"), [](FILE *f) {
            fclose(f);
        });
        file.close();
        rewind(f.get());
        zsync_submit_source_file(zs.get(), f.get(), false, _type == ZsyncMode::download ? false : true);
    }

    emit finishedSignal(zs.release());
}

static void log_zsync_errors(const char *func, FILE *stream, void */*error_context*/)
{
    qCWarning(lcZsyncGenerate) << "Zsync error: " << func << ": " << strerror(ferror(stream));
}

void ZsyncGenerateRunnable::run()
{
    // Create a temporary file to use with zsync_begin()
    QTemporaryFile zsynctf, zsyncmeta;
    zsyncmeta.open();
    zsynctf.open();

    int metaHandle = zsyncmeta.handle();
    zsync_unique_ptr<FILE> meta(fdopen(dup(metaHandle), "w"), [](FILE *f) {
        fclose(f);
    });
    zsyncmeta.close();

    int tfHandle = zsynctf.handle();
    zsync_unique_ptr<FILE> tf(fdopen(dup(tfHandle), "w+"), [](FILE *f) {
        fclose(f);
    });
    zsynctf.close();

    /* Ensure that metadata file is not buffered, since we are using handles directly */
    setvbuf(meta.get(), NULL, _IONBF, 0);

    qCDebug(lcZsyncGenerate) << "Starting generation of:" << _file;

    QByteArray fileString = _file.toLocal8Bit();
    zsync_unique_ptr<FILE> in(fopen(fileString, "r"), [](FILE *f) {
        fclose(f);
    });
    if (!in) {
        QString error = QString(tr("Failed to open input file:")) + _file;
        FileSystem::remove(zsyncmeta.fileName());
        emit failedSignal(error);
        return;
    }

    zsync_unique_ptr<zsyncfile_state> state(zsyncfile_init(ZSYNC_BLOCKSIZE), [](zsyncfile_state *state) {
        zsyncfile_finish(&state);
    });
    state->stream_error = &log_zsync_errors;

    /* Read the input file and construct the checksum of the whole file, and
     * the per-block checksums */
    if (zsyncfile_read_stream_write_blocksums(in.get(), tf.get(), /*no_look_inside=*/1, state.get()) != 0) {
        QString error = QString(tr("Failed to write block sums:")) + _file;
        emit failedSignal(error);
        return;
    }

    // We don't care for the optimal checksum lengths computed by
    // compute_rsum_checksum_len() since we use blocks much larger
    // than the default (1 MB instead of 8 kB) and can just store the full
    // 24 bytes per block.
    int rsum_len = 8;
    int checksum_len = 16;

    if (zsyncfile_write(
            meta.get(), tf.get(),
            rsum_len, checksum_len,
            0, 0, 0, // recompress
            0, 0, // fname, mtime
            0, 0, // urls
            0, 0, // Uurls
            state.get())
        != 0) {
        QString error = QString(tr("Failed to write zsync metadata file:")) + _file;
        emit failedSignal(error);
        return;
    }

    qCDebug(lcZsyncGenerate) << "Done generation of:" << zsyncmeta.fileName();

    zsyncmeta.setAutoRemove(false);
    emit finishedSignal(zsyncmeta.fileName());
}
}
