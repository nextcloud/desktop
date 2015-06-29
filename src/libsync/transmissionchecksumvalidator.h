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

#include "owncloudlib.h"

#include <QObject>
#include <QByteArray>
#include <QFutureWatcher>

namespace OCC {

/**
 * @brief The TransmissionChecksumValidator class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT TransmissionChecksumValidator : public QObject
{
    Q_OBJECT
public:
    explicit TransmissionChecksumValidator(const QString& filePath, QObject *parent = 0);

    /**
     * method to prepare a checksum for transmission and save it to the _checksum
     * member of the SyncFileItem *item.
     * The kind of requested checksum is taken from config. No need to set from outside.
     *
     * In any case of processing (checksum set, no checksum required and also unusual error)
     * the object will emit the signal validated(). The item->_checksum is than either
     * set to a proper value or empty.
     */
    void uploadValidation();

    /**
     * method to verify the checksum coming with requests in a checksum header. The required
     * checksum method is read from config.
     *
     * If no checksum is there, or if a correct checksum is there, the signal validated()
     * will be emitted. In case of any kind of error, the signal validationFailed() will
     * be emitted.
     */
    void downloadValidation( const QByteArray& checksumHeader );

    // This is only used in test cases (by now). This class reads the required
    // test case from the config file.
    void setChecksumType(const QByteArray &type );
    QString checksumType() const;

signals:
    void validated(const QByteArray& checksum);
    void validationFailed( const QString& errMsg );

private slots:
    void slotUploadChecksumCalculated();
    void slotDownloadChecksumCalculated();

private:
    QByteArray    _checksumType;
    QByteArray    _expectedHash;
    QByteArray    _checksumHeader;

    QString       _filePath;

    // watcher for the checksum calculation thread
    QFutureWatcher<QByteArray> _watcher;
};

}
