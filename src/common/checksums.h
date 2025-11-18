/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "ocsynclib.h"
#include "config.h"

#include <QObject>
#include <QByteArray>
#include <QFutureWatcher>

#include <memory>

class QFile;

namespace OCC {

class ChecksumCalculator;
class SyncJournalDb;

/**
 * Returns the highest-quality checksum in a 'checksums'
 * property retrieved from the server.
 *
 * Example: "ADLER32:1231 SHA1:ab124124 MD5:2131affa21"
 *       -> "SHA1:ab124124"
 */
OCSYNC_EXPORT QByteArray findBestChecksum(const QByteArray &checksums);


/// Creates a checksum header from type and value.
OCSYNC_EXPORT QByteArray makeChecksumHeader(const QByteArray &checksumType, const QByteArray &checksum);

/// Parses a checksum header
OCSYNC_EXPORT bool parseChecksumHeader(const QByteArray &header, QByteArray *type, QByteArray *checksum);

/// Convenience for getting the type from a checksum header, null if none
OCSYNC_EXPORT QByteArray parseChecksumHeaderType(const QByteArray &header);

/// Checks OWNCLOUD_DISABLE_CHECKSUM_UPLOAD
OCSYNC_EXPORT bool uploadChecksumEnabled();

OCSYNC_EXPORT QByteArray calcSha256(const QByteArray &data);

/**
 * Computes the checksum of a file.
 * \ingroup libsync
 */
class OCSYNC_EXPORT ComputeChecksum : public QObject
{
    Q_OBJECT
public:
    explicit ComputeChecksum(QObject *parent = nullptr);
    ~ComputeChecksum() override;

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

    /**
     * Computes the checksum synchronously on file. Convenience wrapper for computeNow().
     */
    static QByteArray computeNowOnFile(const QString &filePath, const QByteArray &checksumType);

signals:
    void done(const QByteArray &checksumType, const QByteArray &checksum);

private slots:
    void slotCalculationDone();

private:
    void startImpl(const QString &filePath);

    QByteArray _checksumType;

    // watcher for the checksum calculation thread
    QFutureWatcher<QByteArray> _watcher;

    QScopedPointer<ChecksumCalculator> _checksumCalculator;
};

/**
 * Checks whether a file's checksum matches the expected value.
 * @ingroup libsync
 */
class OCSYNC_EXPORT ValidateChecksumHeader : public QObject
{
    Q_OBJECT
public:
    enum FailureReason {
        Success,
        ChecksumHeaderMalformed,
        ChecksumTypeUnknown,
        ChecksumMismatch,
    };
    Q_ENUM(FailureReason)

    explicit ValidateChecksumHeader(QObject *parent = nullptr);

    /**
     * Check a file's actual checksum against the provided checksumHeader
     *
     * If no checksum is there, or if a correct checksum is there, the signal validated()
     * will be emitted. In case of any kind of error, the signal validationFailed() will
     * be emitted.
     */
    void start(const QString &filePath, const QByteArray &checksumHeader);

    [[nodiscard]] QByteArray calculatedChecksumType() const;
    [[nodiscard]] QByteArray calculatedChecksum() const;

signals:
    void validated(const QByteArray &checksumType, const QByteArray &checksum);
    void validationFailed(const QString &errMsg, const QByteArray &calculatedChecksumType,
        const QByteArray &calculatedChecksum, const OCC::ValidateChecksumHeader::FailureReason reason);

private slots:
    void slotChecksumCalculated(const QByteArray &checksumType, const QByteArray &checksum);

private:
    ComputeChecksum *prepareStart(const QByteArray &checksumHeader);

    QByteArray _expectedChecksumType;
    QByteArray _expectedChecksum;

    QByteArray _calculatedChecksumType;
    QByteArray _calculatedChecksum;
};

/**
 * Hooks checksum computations into csync.
 * @ingroup libsync
 */
class OCSYNC_EXPORT CSyncChecksumHook : public QObject
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
    static QByteArray hook(const QByteArray &path, const QByteArray &otherChecksumHeader, void *this_obj);
};
}
