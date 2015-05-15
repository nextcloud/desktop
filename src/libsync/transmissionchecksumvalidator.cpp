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

#include <QtConcurrent>

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

QString TransmissionChecksumValidator::checksumType()
{
    return _checksumType;
}


void TransmissionChecksumValidator::uploadValidation( SyncFileItem *item )
{
    QString checksumType = _checksumType;
    if( checksumType.isEmpty() ) {
        ConfigFile cfg;
        checksumType = cfg.transmissionChecksum();
    }

    if( checksumType.isEmpty() || !item ) {
        // if there is no checksum defined, continue to upload
        emit validated();
    } else {
        _item = item;
        // Calculate the checksum in a different thread first.
        connect( &_watcher, SIGNAL(finished()),
                 this, SLOT(slotUploadChecksumCalculated()));
        if( checksumType == checkSumMD5C ) {
            item->_checksum = checkSumMD5C;
            item->_checksum += ":";
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcMd5Worker, _filePath));

        } else if( checksumType == checkSumSHA1C ) {
            item->_checksum = checkSumSHA1C;
            item->_checksum += ":";
            _watcher.setFuture(QtConcurrent::run( FileSystem::calcSha1Worker, _filePath));
        }
#ifdef ZLIB_FOUND
        else if( checksumType == checkSumAdlerC) {
            item->_checksum = checkSumAdlerC;
            item->_checksum += ":";
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcAdler32Worker, _filePath));
        }
#endif
        else {
            // for an unknown checksum, continue to upload
            emit validated();
        }
    }
}

void TransmissionChecksumValidator::slotUploadChecksumCalculated( )
{
    QByteArray checksum = _watcher.future().result();

    if( !checksum.isEmpty() ) {
        _item->_checksum.append(checksum);
    } else {
        _item->_checksum.clear();
    }

    emit validated();
}


void TransmissionChecksumValidator::downloadValidation( const QByteArray& checksumHeader )
{
    // if the incoming header is empty, there was no checksum header, and
    // no validation can happen. Just continue.
    if( checksumHeader.isEmpty() ) {
        emit validated();
        return;
    }

    bool ok = true;

    int indx = checksumHeader.indexOf(':');
    if( indx < 0 ) {
        qDebug() << "Checksum header malformed:" << checksumHeader;
        emit validated(); // show must go on - even not validated.
    }

    if( ok ) {
        const QByteArray type = checksumHeader.left(indx).toUpper();
        _expectedHash = checksumHeader.mid(indx+1);

        connect( &_watcher, SIGNAL(finished()), this, SLOT(slotDownloadChecksumCalculated()) );

        // start the calculation in different thread
        if( type == checkSumMD5C ) {
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcMd5Worker, _filePath));
        } else if( type == checkSumSHA1C ) {
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcSha1Worker, _filePath));
        }
#ifdef ZLIB_FOUND
        else if( type == checkSumAdlerUpperC ) {
            _watcher.setFuture(QtConcurrent::run(FileSystem::calcAdler32Worker, _filePath));
        }
#endif
        else {
            qDebug() << "Unknown checksum type" << type;
            emit validationFailed(tr("The checksum header was malformed."));
            return;
        }
    }
}

void TransmissionChecksumValidator::slotDownloadChecksumCalculated()
{
    const QByteArray hash = _watcher.future().result();

    if( hash != _expectedHash ) {
        emit validationFailed(tr("The file downloaded with a broken checksum, will be redownloaded."));
    } else {
        qDebug() << "Checksum checked and matching: " << _expectedHash;
        emit validated();
    }
}


}
