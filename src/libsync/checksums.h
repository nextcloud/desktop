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

class SyncJournalDb;

/// Creates a checksum header from type and value.
QByteArray makeChecksumHeader(const QByteArray &checksumType, const QByteArray &checksum);

/// Parses a checksum header
bool parseChecksumHeader(const QByteArray &header, QByteArray *type, QByteArray *checksum);

/// Convenience for getting the type from a checksum header, null if none
QByteArray parseChecksumHeaderType(const QByteArray &header);

/// Checks OWNCLOUD_DISABLE_CHECKSUM_UPLOAD
bool uploadChecksumEnabled();

/// Checks OWNCLOUD_CONTENT_CHECKSUM_TYPE (default: SHA1)
QByteArray contentChecksumType();


/**
 * Computes the checksum of a file.
 * \ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ComputeChecksum : public QObject
{
    Q_OBJECT
public:
    explicit ComputeChecksum(QObject *parent = 0);

    /**
     * Sets the checksum type to be used. The default is empty.
     */
    void setChecksumType(const QByteArray &type);

    QByteArray checksumType() const;

    /**
     * Computes the checksum for the given file path.
     *
     * done() is emitted when the calculation finishes.
     */
    void start(const QString &filePath);

    /**
     * Computes the checksum synchronously.
     */
    static QByteArray computeNow(const QString &filePath, const QByteArray &checksumType);

signals:
    void done(const QByteArray &checksumType, const QByteArray &checksum);

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
    void start(const QString &filePath, const QByteArray &checksumHeader);

signals:
    void validated(const QByteArray &checksumType, const QByteArray &checksum);
    void validationFailed(const QString &errMsg);

private slots:
    void slotChecksumCalculated(const QByteArray &checksumType, const QByteArray &checksum);

private:
    QByteArray _expectedChecksumType;
    QByteArray _expectedChecksum;
};

/**
 * Hooks checksum computations into csync.
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT CSyncChecksumHook : public QObject
{
    Q_OBJECT
public:
    explicit CSyncChecksumHook();

    /**
     * Returns the checksum value for \a path that is comparable to \a otherChecksum.
     *
     * Called from csync, where a instance of CSyncChecksumHook has
     * to be set as userdata.
     * The return value will be owned by csync.
     */
    static const char *hook(const char *path, const char *otherChecksumHeader, void *this_obj);
};
}
