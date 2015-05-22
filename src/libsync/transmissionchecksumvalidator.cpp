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
#include "configfile.h"

#include <qtconcurrentrun.h>

namespace OCC {

TransmissionChecksumValidator::TransmissionChecksumValidator(const QString& filePath, QObject *parent)
  :QObject(parent),
    _filePath(filePath)
{

}

void TransmissionChecksumValidator::setChecksumType( const QByteArray& type )
{
    _checksumType = type;
}

QString TransmissionChecksumValidator::checksumType() const
{
    QString checksumType = _checksumType;
    if( checksumType.isEmpty() ) {
        ConfigFile cfg;
        checksumType = cfg.transmissionChecksum();
    }

    return checksumType;
}

void TransmissionChecksumValidator::uploadValidation()
{
    const QString csType = checksumType();

    if( csType.isEmpty() ) {
        // if there is no checksum defined, continue to upload
        emit validated(QByteArray());
    } else {
        // Calculate the checksum in a different thread first.

        connect( &_watcher, SIGNAL(finished()),
                 this, SLOT(slotUploadChecksumCalculated()));
        if( csType == checkSumMD5C ) {
            _checksumHeader = checkSumMD5C;
            _checksumHeader += ":";
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcMd5, _filePath));

        } else if( csType == checkSumSHA1C ) {
            _checksumHeader = checkSumSHA1C;
            _checksumHeader += ":";
            _watcher.setFuture(QtConcurrent::run( FileSystem::calcSha1, _filePath));
        }
#ifdef ZLIB_FOUND
        else if( csType == checkSumAdlerC) {
            _checksumHeader = checkSumAdlerC;
            _checksumHeader += ":";
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcAdler32, _filePath));
        }
#endif
        else {
            // for an unknown checksum, continue to upload
            emit validated(QByteArray());
        }
    }
}

void TransmissionChecksumValidator::slotUploadChecksumCalculated( )
{
    QByteArray checksum = _watcher.future().result();

    if( !checksum.isEmpty() ) {
        checksum.prepend( _checksumHeader );
    }

    emit validated(checksum);
}


void TransmissionChecksumValidator::downloadValidation( const QByteArray& checksumHeader )
{
    // if the incoming header is empty, there was no checksum header, and
    // no validation can happen. Just continue.
    const QString csType = checksumType();

    // for empty checksum type, everything is valid.
    if( csType.isEmpty() ) {
        emit validated(QByteArray());
        return;
    }

    int indx = checksumHeader.indexOf(':');
    if( indx < 0 ) {
        qDebug() << "Checksum header malformed:" << checksumHeader;
        emit validationFailed(tr("The checksum header is malformed.")); // show must go on - even not validated.
        return;
    }

    const QByteArray type = checksumHeader.left(indx).toUpper();
    _expectedHash = checksumHeader.mid(indx+1);

    connect( &_watcher, SIGNAL(finished()), this, SLOT(slotDownloadChecksumCalculated()) );

    // start the calculation in different thread
    if( type == checkSumMD5C ) {
        _watcher.setFuture(QtConcurrent::run(FileSystem::calcMd5, _filePath));
    } else if( type == checkSumSHA1C ) {
        _watcher.setFuture(QtConcurrent::run(FileSystem::calcSha1, _filePath));
    }
#ifdef ZLIB_FOUND
    else if( type == checkSumAdlerUpperC ) {
        _watcher.setFuture(QtConcurrent::run(FileSystem::calcAdler32, _filePath));
    }
#endif
    else {
        qDebug() << "Unknown checksum type" << type;
        emit validationFailed(tr("The checksum header is malformed."));
        return;
    }
}

void TransmissionChecksumValidator::slotDownloadChecksumCalculated()
{
    const QByteArray hash = _watcher.future().result();

    if( hash != _expectedHash ) {
        emit validationFailed(tr("The downloaded file does not match the checksum, it will be resumed."));
    } else {
        // qDebug() << "Checksum checked and matching: " << _expectedHash;
        emit validated(hash);
    }
}


}
