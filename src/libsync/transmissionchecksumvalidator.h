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
#include "accountfwd.h"

#include <QObject>
#include <QByteArray>
#include <QFutureWatcher>

namespace OCC {

/// Creates a checksum header from type and value.
QByteArray makeChecksumHeader(const QByteArray& checksumType, const QByteArray& checksum);

/// Parses a checksum header
bool parseChecksumHeader(const QByteArray& header, QByteArray* type, QByteArray* checksum);

/**
 * Computes the checksum of a file.
 * \ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ComputeChecksum : public QObject
{
    Q_OBJECT
public:
    explicit ComputeChecksum(QObject* parent = 0);

    /**
     * By default the checksum type is read from the config file, but can be overridden
     * with this method.
     */
    void setChecksumType(const QByteArray& type);

    QByteArray checksumType() const;

    /**
     * Computes the checksum for the given file path.
     *
     * done() is emitted when the calculation finishes.
     */
    void start(const QString& filePath);

signals:
    void done(const QByteArray& checksumType, const QByteArray& checksum);

private slots:
    void slotCalculationDone();

private:
    QByteArray _checksumType;

    // watcher for the checksum calculation thread
    QFutureWatcher<QByteArray> _watcher;
};

/**
 * Checks whether a file's checksum matches the expected value.
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ValidateChecksumHeader : public QObject
{
    Q_OBJECT
public:
    explicit ValidateChecksumHeader(QObject *parent = 0);

    /**
     * Check a file's actual checksum against the provided checksumHeader
     *
     * If no checksum is there, or if a correct checksum is there, the signal validated()
     * will be emitted. In case of any kind of error, the signal validationFailed() will
     * be emitted.
     */
    void start(const QString& filePath, const QByteArray& checksumHeader);

signals:
    void validated(const QByteArray& checksumHeader);
    void validationFailed( const QString& errMsg );

private slots:
    void slotChecksumCalculated(const QByteArray& checksumType, const QByteArray& checksum);

private:
    QByteArray _expectedChecksumType;
    QByteArray _expectedChecksum;
};

}
