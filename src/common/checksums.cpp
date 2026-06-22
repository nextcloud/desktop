/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "config.h"
#include "filesystembase.h"
#include "common/checksums.h"
#include "checksumcalculator.h"
#include "asserts.h"

#include <QLoggingCategory>
#include <qtconcurrentrun.h>
#include <QCryptographicHash>

#ifdef ZLIB_FOUND
#include <zlib.h>
#endif

/** \file checksums.cpp
 *
 * \brief Computing and validating file checksums
 *
 * Overview
 * --------
 *
 * Checksums are used in two distinct ways during synchronization:
 *
 * - to guard uploads and downloads against data corruption
 *   (transmission checksum)
 * - to quickly check whether the content of a file has changed
 *   to avoid redundant uploads (content checksum)
 *
 * In principle both are independent and different checksumming
 * algorithms can be used. To avoid redundant computations, it can
 * make sense to use the same checksum algorithm though.
 *
 * Transmission Checksums
 * ----------------------
 *
 * The usage of transmission checksums is currently optional and needs
 * to be explicitly enabled by adding 'transmissionChecksum=TYPE' to
 * the '[General]' section of the config file.
 *
 * When enabled, the checksum will be calculated on upload and sent to
 * the server in the OC-Checksum header with the format 'TYPE:CHECKSUM'.
 *
 * On download, the header with the same name is read and if the
 * received data does not have the expected checksum, the download is
 * rejected.
 *
 * Transmission checksums guard a specific sync action and are not stored
 * in the database.
 *
 * Content Checksums
 * -----------------
 *
 * Sometimes the metadata of a local file changes while the content stays
 * unchanged. Content checksums allow the sync client to avoid uploading
 * the same data again by comparing the file's actual checksum to the
 * checksum stored in the database.
 *
 * Content checksums are not sent to the server.
 *
 * Checksum Algorithms
 * -------------------
 *
 * - Adler32 (requires zlib)
 * - MD5
 * - SHA1
 * - SHA256
 * - SHA3-256 (requires Qt 5.9)
 *
 */

namespace OCC {

Q_LOGGING_CATEGORY(lcChecksums, "nextcloud.sync.checksums", QtInfoMsg)

#define BUFSIZE qint64(500 * 1024) // 500 KiB

static QByteArray calcCryptoHash(const QByteArray &data, QCryptographicHash::Algorithm algo)
{
    if (data.isEmpty()) {
        return {};
    }
    QCryptographicHash crypto(algo);
    crypto.addData(data);
    return crypto.result().toHex();
}

QByteArray calcSha256(const QByteArray &data)
{
    return calcCryptoHash(data, QCryptographicHash::Sha256);
}

QByteArray makeChecksumHeader(const QByteArray &checksumType, const QByteArray &checksum)
{
    if (checksumType.isEmpty() || checksum.isEmpty())
        return QByteArray();
    QByteArray header = checksumType;
    header.append(':');
    header.append(checksum);
    return header;
}

QByteArray findBestChecksum(const QByteArray &_checksums)
{
    if (_checksums.isEmpty()) {
        return {};
    }
    const auto checksums = QString::fromUtf8(_checksums);
    int i = 0;
    // The order of the searches here defines the preference ordering.
    if (-1 != (i = checksums.indexOf(QLatin1String("SHA3-256:"), 0, Qt::CaseInsensitive))
        || -1 != (i = checksums.indexOf(QLatin1String("SHA256:"), 0, Qt::CaseInsensitive))
        || -1 != (i = checksums.indexOf(QLatin1String("SHA1:"), 0, Qt::CaseInsensitive))
        || -1 != (i = checksums.indexOf(QLatin1String("MD5:"), 0, Qt::CaseInsensitive))
        || -1 != (i = checksums.indexOf(QLatin1String("ADLER32:"), 0, Qt::CaseInsensitive))) {
        // Now i is the start of the best checksum
        // Grab it until the next space or end of xml or end of string.
        int end = _checksums.indexOf(' ', i);
        // workaround for https://github.com/owncloud/core/pull/38304
        if (end == -1) {
            end = _checksums.indexOf('<', i);
        }
        return _checksums.mid(i, end - i);
    }
    qCWarning(lcChecksums) << "Failed to parse" << _checksums;
    return {};
}

bool parseChecksumHeader(const QByteArray &header, QByteArray *type, QByteArray *checksum)
{
    if (header.isEmpty()) {
        type->clear();
        checksum->clear();
        return true;
    }

    const auto idx = header.indexOf(':');
    if (idx < 0) {
        return false;
    }

    *type = header.left(idx);
    *checksum = header.mid(idx + 1);
    return true;
}


QByteArray parseChecksumHeaderType(const QByteArray &header)
{
    const auto idx = header.indexOf(':');
    if (idx < 0) {
        return QByteArray();
    }
    return header.left(idx);
}

bool uploadChecksumEnabled()
{
    static bool enabled = qEnvironmentVariableIsEmpty("OWNCLOUD_DISABLE_CHECKSUM_UPLOAD");
    return enabled;
}

static bool checksumComputationEnabled()
{
    static bool enabled = qEnvironmentVariableIsEmpty("OWNCLOUD_DISABLE_CHECKSUM_COMPUTATIONS");
    return enabled;
}

ComputeChecksum::ComputeChecksum(QObject *parent)
    : QObject(parent)
{
}

ComputeChecksum::~ComputeChecksum()
{
    // let the checksum calculation complete before deleting the checksumCalculator instance
    _watcher.waitForFinished();
}

void ComputeChecksum::setChecksumType(const QByteArray &type)
{
    _checksumType = type;
}

QByteArray ComputeChecksum::checksumType() const
{
    return _checksumType;
}

void ComputeChecksum::start(const QString &filePath)
{
    qCDebug(lcChecksums) << "Computing" << checksumType() << "checksum of" << filePath << "in a thread";
    startImpl(filePath);
}

void ComputeChecksum::startImpl(const QString &filePath)
{
    connect(&_watcher, &QFutureWatcherBase::finished,
        this, &ComputeChecksum::slotCalculationDone,
        Qt::UniqueConnection);

    _checksumCalculator.reset(new ChecksumCalculator(filePath, _checksumType));
    _watcher.setFuture(QtConcurrent::run([this]() {
        if (!_checksumCalculator) {
            qCDebug(lcChecksums) << "checksumCalculator instance was destroyed before calculation started, returning empty checksum";
            return QByteArray();
        }

        return _checksumCalculator->calculate();
    }));
}

QByteArray ComputeChecksum::computeNowOnFile(const QString &filePath, const QByteArray &checksumType)
{
    return computeNow(filePath, checksumType);
}

QByteArray ComputeChecksum::computeNow(const QString &filePath, const QByteArray &checksumType)
{
    if (!checksumComputationEnabled()) {
        qCWarning(lcChecksums) << "Checksum computation disabled by environment variable";
        return QByteArray();
    }

    ChecksumCalculator checksumCalculator(filePath, checksumType);
    return checksumCalculator.calculate();
}

void ComputeChecksum::slotCalculationDone()
{
    QByteArray checksum = _watcher.future().result();
    if (!checksum.isNull()) {
        emit done(_checksumType, checksum);
    } else {
        emit done(QByteArray(), QByteArray());
    }
}


ValidateChecksumHeader::ValidateChecksumHeader(QObject *parent)
    : QObject(parent)
{
}

ComputeChecksum *ValidateChecksumHeader::prepareStart(const QByteArray &checksumHeader)
{
    // If the incoming header is empty no validation can happen. Just continue.
    if (checksumHeader.isEmpty()) {
        emit validated(QByteArray(), QByteArray());
        return nullptr;
    }

    if (!parseChecksumHeader(checksumHeader, &_expectedChecksumType, &_expectedChecksum)) {
        qCWarning(lcChecksums) << "Checksum header malformed:" << checksumHeader;
        emit validationFailed(tr("The checksum header is malformed."), _calculatedChecksumType, _calculatedChecksum, ChecksumHeaderMalformed);
        return nullptr;
    }

    auto calculator = new ComputeChecksum(this);
    calculator->setChecksumType(_expectedChecksumType);
    connect(calculator, &ComputeChecksum::done,
        this, &ValidateChecksumHeader::slotChecksumCalculated);
    return calculator;
}

void ValidateChecksumHeader::start(const QString &filePath, const QByteArray &checksumHeader)
{
    if (auto calculator = prepareStart(checksumHeader))
        calculator->start(filePath);
}

QByteArray ValidateChecksumHeader::calculatedChecksumType() const
{
    return _calculatedChecksumType;
}

QByteArray ValidateChecksumHeader::calculatedChecksum() const
{
    return _calculatedChecksum;
}

void ValidateChecksumHeader::slotChecksumCalculated(const QByteArray &checksumType,
    const QByteArray &checksum)
{
    _calculatedChecksumType = checksumType;
    _calculatedChecksum = checksum;

    if (checksumType != _expectedChecksumType) {
        emit validationFailed(tr("The checksum header contained an unknown checksum type \"%1\"").arg(QString::fromLatin1(_expectedChecksumType)),
            _calculatedChecksumType, _calculatedChecksum, ChecksumTypeUnknown);
        return;
    }
    if (checksum != _expectedChecksum) {
        emit validationFailed(tr(R"(The downloaded file does not match the checksum, it will be resumed. "%1" != "%2")").arg(QString::fromUtf8(_expectedChecksum), QString::fromUtf8(checksum)),
            _calculatedChecksumType, _calculatedChecksum, ChecksumMismatch);
        return;
    }
    emit validated(checksumType, checksum);
}

CSyncChecksumHook::CSyncChecksumHook() = default;

QByteArray CSyncChecksumHook::hook(const QByteArray &path, const QByteArray &otherChecksumHeader, void * /*this_obj*/)
{
    QByteArray type = parseChecksumHeaderType(QByteArray(otherChecksumHeader));
    if (type.isEmpty())
        return nullptr;

    qCInfo(lcChecksums) << "Computing" << type << "checksum of" << path << "in the csync hook";
    QByteArray checksum = ComputeChecksum::computeNowOnFile(QString::fromUtf8(path), type);
    if (checksum.isNull()) {
        qCWarning(lcChecksums) << "Failed to compute checksum" << type << "for" << path;
        return nullptr;
    }

    return makeChecksumHeader(type, checksum);
}

}
