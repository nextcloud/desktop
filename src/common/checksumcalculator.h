/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
