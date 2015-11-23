/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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
#include "config.h"
#include "filesystem.h"
#include "transmissionchecksumvalidator.h"
#include "syncfileitem.h"
#include "propagatorjobs.h"
#include "account.h"

#include <qtconcurrentrun.h>

namespace OCC {

QByteArray makeChecksumHeader(const QByteArray& checksumType, const QByteArray& checksum)
{
    QByteArray header = checksumType;
    header.append(':');
    header.append(checksum);
    return header;
}

bool parseChecksumHeader(const QByteArray& header, QByteArray* type, QByteArray* checksum)
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

bool uploadChecksumEnabled()
{
    static bool enabled = qgetenv("OWNCLOUD_DISABLE_CHECKSUM_UPLOAD").isEmpty();
    return enabled;
}

bool downloadChecksumEnabled()
{
    static bool enabled = qgetenv("OWNCLOUD_DISABLE_CHECKSUM_DOWNLOAD").isEmpty();
    return enabled;
}

ComputeChecksum::ComputeChecksum(QObject* parent)
    : QObject(parent)
{
}

void ComputeChecksum::setChecksumType(const QByteArray& type)
{
    _checksumType = type;
}

QByteArray ComputeChecksum::checksumType() const
{
    return _checksumType;
}

void ComputeChecksum::start(const QString& filePath)
{
    // Calculate the checksum in a different thread first.
    connect( &_watcher, SIGNAL(finished()),
             this, SLOT(slotCalculationDone()),
             Qt::UniqueConnection );
    _watcher.setFuture(QtConcurrent::run(ComputeChecksum::computeNow, filePath, checksumType()));
}

QByteArray ComputeChecksum::computeNow(const QString& filePath, const QByteArray& checksumType)
{
    if( checksumType == checkSumMD5C ) {
        return FileSystem::calcMd5(filePath);
    } else if( checksumType == checkSumSHA1C ) {
        return FileSystem::calcSha1(filePath);
    }
#ifdef ZLIB_FOUND
    else if( checksumType == checkSumAdlerC) {
        return FileSystem::calcAdler32(filePath);
    }
#endif
    // for an unknown checksum or no checksum, we're done right now
    if( !checksumType.isEmpty() ) {
        qDebug() << "Unknown checksum type:" << checksumType;
    }
    return QByteArray();
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

void ValidateChecksumHeader::start(const QString& filePath, const QByteArray& checksumHeader)
{
    // If the incoming header is empty no validation can happen. Just continue.
    if( checksumHeader.isEmpty() ) {
        emit validated(QByteArray(), QByteArray());
        return;
    }

    if( !parseChecksumHeader(checksumHeader, &_expectedChecksumType, &_expectedChecksum) ) {
        qDebug() << "Checksum header malformed:" << checksumHeader;
        emit validationFailed(tr("The checksum header is malformed."));
        return;
    }

    auto calculator = new ComputeChecksum(this);
    calculator->setChecksumType(_expectedChecksumType);
    connect(calculator, SIGNAL(done(QByteArray,QByteArray)),
            SLOT(slotChecksumCalculated(QByteArray,QByteArray)));
    calculator->start(filePath);
}

void ValidateChecksumHeader::slotChecksumCalculated(const QByteArray& checksumType,
                                                    const QByteArray& checksum)
{
    if( checksumType != _expectedChecksumType ) {
        emit validationFailed(tr("The checksum header contained an unknown checksum type '%1'").arg(
                                  QString::fromLatin1(_expectedChecksumType)));
        return;
    }
    if( checksum != _expectedChecksum ) {
        emit validationFailed(tr("The downloaded file does not match the checksum, it will be resumed."));
        return;
    }
    emit validated(checksumType, checksum);
}

CSyncChecksumHook::CSyncChecksumHook(SyncJournalDb *journal)
    : _journal(journal)
{
}

bool CSyncChecksumHook::hook(const char* path, uint32_t checksumTypeId, const char* checksum, void *this_obj)
{
    CSyncChecksumHook* checksumHook = static_cast<CSyncChecksumHook*>(this_obj);
    return checksumHook->check(QString::fromUtf8(path), checksumTypeId, QByteArray(checksum));
}

bool CSyncChecksumHook::check(const QString& path, int checksumTypeId, const QByteArray& checksum)
{
    QByteArray checksumType = _journal->getChecksumType(checksumTypeId);
    if (checksumType.isEmpty()) {
        qDebug() << "Checksum type" << checksumTypeId << "not found";
        return false;
    }

    QByteArray newChecksum = ComputeChecksum::computeNow(path, checksumType);
    if (newChecksum.isNull()) {
        qDebug() << "Failed to compute checksum" << checksumType << "for" << path;
        return false;
    }
    return newChecksum == checksum;
}


}
