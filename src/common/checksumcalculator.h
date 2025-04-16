/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ocsynclib.h"
#include "config.h"
#include "checksumconsts.h"

#include <QObject>
#include <QByteArray>
#include <QFutureWatcher>
#include <QMutex>
#include <QScopedPointer>

class QCryptographicHash;

namespace OCC {
class OCSYNC_EXPORT ChecksumCalculator
{
    Q_DISABLE_COPY(ChecksumCalculator)

public:
    enum class AlgorithmType {
        Undefined = -1,
        MD5,
        SHA1,
        SHA256,
        SHA3_256,
        Adler32,
    };

    ChecksumCalculator(const QString &filePath, const QByteArray &checksumTypeName);
    ~ChecksumCalculator();
    [[nodiscard]] QByteArray calculate();

private:
    void initChecksumAlgorithm();
    bool addChunk(const QByteArray &chunk, const qint64 size);
    QScopedPointer<QIODevice> _device;
    QScopedPointer<QCryptographicHash> _cryptographicHash;
    unsigned int _adlerHash = 0;
    bool _isInitialized = false;
    AlgorithmType _algorithmType = AlgorithmType::Undefined;
    QMutex _deviceMutex;
};
}
