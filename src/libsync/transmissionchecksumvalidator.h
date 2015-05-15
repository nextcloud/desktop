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

#pragma once

#include <QObject>
#include <QByteArray>
#include <QFutureWatcher>

namespace OCC {
class SyncFileItem;

class TransmissionChecksumValidator : public QObject
{
    Q_OBJECT
public:
    explicit TransmissionChecksumValidator(const QString& filePath, QObject *parent = 0);

    void uploadValidation( SyncFileItem *item );
    void downloadValidation( const QByteArray& checksumHeader );

    void setChecksumType(const QByteArray &type );
    QString checksumType();

signals:
    void validated();
    void validationFailed( const QString& errMsg );

private slots:
    void slotUploadChecksumCalculated();
    void slotDownloadChecksumCalculated();

private:
    QByteArray    _checksumType;
    QByteArray    _expectedHash;
    QString       _filePath;
    SyncFileItem *_item;

    // watcher for the checksum calculation thread
    QFutureWatcher<QByteArray> _watcher;
};

}
