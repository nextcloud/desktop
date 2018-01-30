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
#include <cmath>
#include <cstring>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <arpa/inet.h>
#endif

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

/**
 * Exit with IO-related error message
 */
int ZsyncGenerateRunnable::stream_error(const char *func, FILE *stream)
{
    QString error = QString(func) + QString(": ") + QString(strerror(ferror(stream)));
    emit failedSignal(error);
    return -1;
}

/**
 * Copy the full block checksums from their temporary store file to the .zsync,
 * stripping the hashes down to the desired lengths specified by the last 2
 * parameters.
 */
int ZsyncGenerateRunnable::fcopy_hashes(FILE *fin, FILE *fout, size_t rsum_bytes, size_t hash_bytes)
{
    unsigned char buf[CHECKSUM_SIZE + 4];
    size_t len;

    while ((len = fread(buf, 1, sizeof(buf), fin)) > 0) {
        /* write trailing rsum_bytes of the rsum (trailing because the second part of the rsum is more useful in practice for hashing), and leading checksum_bytes of the checksum */
        if (fwrite(buf + 4 - rsum_bytes, 1, rsum_bytes, fout) < rsum_bytes)
            break;
        if (fwrite(buf + 4, 1, hash_bytes, fout) < hash_bytes)
            break;
    }
    if (ferror(fin)) {
        return stream_error("fread", fin);
    }
    if (ferror(fout)) {
        return stream_error("fwrite", fout);
    }

    return 0;
}

/**
 * Given one block of data, calculate the checksums for this block and write
 * them (as raw bytes) to the given output stream
 */
int ZsyncGenerateRunnable::write_block_sums(unsigned char *buf, size_t got, FILE *f)
{
    struct rsum r;
    unsigned char checksum[CHECKSUM_SIZE];

    /* Pad for our checksum, if this is a short last block  */
    if (got < _blocksize)
        memset(buf + got, 0, _blocksize - got);

    /* Do rsum and checksum, and convert to network endian */
    r = rcksum_calc_rsum_block(buf, _blocksize);
    rcksum_calc_checksum(&checksum[0], buf, _blocksize);
    r.a = htons(r.a);
    r.b = htons(r.b);

    /* Write them raw to the stream */
    if (fwrite(&r, sizeof r, 1, f) != 1)
        return stream_error("fwrite", f);
    if (fwrite(checksum, sizeof checksum, 1, f) != 1)
        return stream_error("fwrite", f);

    return 0;
}

/**
 * Reads the data stream and writes to the zsync stream the blocksums for the
 * given data. No compression handling.
 */
int ZsyncGenerateRunnable::read_stream_write_blocksums(FILE *fin, FILE *fout)
{
    unsigned char *buf = (unsigned char *)malloc(_blocksize);

    if (!buf) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    while (!feof(fin)) {
        int got = fread(buf, 1, _blocksize, fin);

        if (got > 0) {
            /* The SHA-1 sum, unlike our internal block-based sums, is on the whole file and nothing else - no padding */
            SHA1Update(&_shactx, buf, got);

            write_block_sums(buf, got, fout);
            _len += got;
        } else {
            if (ferror(fin))
                return stream_error("fread", fin);
        }
    }
    free(buf);
    return 0;
}

void ZsyncGenerateRunnable::run()
{
    // Create a temporary file to use with zsync_begin()
    QTemporaryFile zsynctf, zsyncmeta;
    zsyncmeta.open();
    zsyncmeta.setAutoRemove(false);
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

    int rsum_len, checksum_len, seq_matches;
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

    /* Read the input file and construct the checksum of the whole file, and
     * the per-block checksums */
    SHA1Init(&_shactx);
    if (read_stream_write_blocksums(in.get(), tf.get())) {
        QString error = QString(tr("Failed to write block sums:")) + _file;
        FileSystem::remove(zsyncmeta.fileName());
        emit failedSignal(error);
        return;
    }

    { /* Decide how long a rsum hash and checksum hash per block we need for this file */
        seq_matches = 1;
        rsum_len = ceil(((log(_len) + log(_blocksize)) / log(2) - 8.6) / 8);
        /* For large files, the optimum weak checksum size can be more than
         * what we have available. Switch to seq_matches for this case. */
        if (rsum_len > 4) {
            /* seq_matches > 1 in theory would reduce the amount of rsum_len
             * needed, since we get effectively rsum_len*seq_matches required
             * to match before a strong checksum is calculated. In practice,
             * consecutive blocks in the file can be highly correlated, so we
             * want to keep the maximum available rsum_len as well. */
            // XXX: disabled: this seems to cause unmatched blocks at end of
            //      files with sizes which are unaligned to blocksize
            // seq_matches = 2;
            rsum_len = 4;
        }

        /* min lengths of rsums to store */
        rsum_len = max(2, rsum_len);

        /* Now the checksum length; min of two calculations */
        checksum_len = max(ceil(
                               (20 + (log(_len) + log(1 + _len / _blocksize)) / log(2))
                               / seq_matches / 8),
            ceil((20 + log(1 + _len / _blocksize) / log(2)) / 8));

        /* Keep checksum_len within 4-16 bytes */
        checksum_len = min(16, max(4, checksum_len));
    }

    /* Okay, start writing the zsync file */
    fprintf(meta.get(), "zsync: 0.6.3\n");
    fprintf(meta.get(), "Blocksize: %lu\n", _blocksize);
    fprintf(meta.get(), "Length: %lu\n", _len);
    fprintf(meta.get(), "Hash-Lengths: %d,%d,%d\n", seq_matches, rsum_len,
        checksum_len);

    { /* Write out SHA1 checksum of the entire file */
        unsigned char digest[SHA1_DIGEST_LENGTH];
        unsigned int i;

        fputs("SHA-1: ", meta.get());

        SHA1Final(digest, &_shactx);

        for (i = 0; i < sizeof digest; i++)
            fprintf(meta.get(), "%02x", digest[i]);
        fputc('\n', meta.get());
    }

    /* End of headers */
    fputc('\n', meta.get());

    /* Now copy the actual block hashes to the .zsync */
    rewind(tf.get());
    if (fcopy_hashes(tf.get(), meta.get(), rsum_len, checksum_len)) {
        QString error = QString(tr("Failed to copy hashes:")) + _file;
        FileSystem::remove(zsyncmeta.fileName());
        emit failedSignal(error);
        return;
    }

    qCDebug(lcZsyncGenerate) << "Done generation of:" << zsyncmeta.fileName();

    emit finishedSignal(zsyncmeta.fileName());
}
}
