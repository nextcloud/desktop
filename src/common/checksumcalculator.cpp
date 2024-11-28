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
#include "checksumcalculator.h"

#include <zlib.h>

#include <QCryptographicHash>
#include <QFile>
#include <QLoggingCategory>

namespace
{
constexpr qint64 bufSize = 500 * 1024;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcChecksumCalculator, "nextcloud.common.checksumcalculator", QtInfoMsg)

static QCryptographicHash::Algorithm algorithmTypeToQCryptoHashAlgorithm(ChecksumCalculator::AlgorithmType algorithmType)
{
    switch (algorithmType) {
    case ChecksumCalculator::AlgorithmType::Undefined:
    case ChecksumCalculator::AlgorithmType::Adler32:
        qCWarning(lcChecksumCalculator) << "Invalid algorithm type" << static_cast<int>(algorithmType);
        return static_cast<QCryptographicHash::Algorithm>(-1);
    case ChecksumCalculator::AlgorithmType::MD5:
        return QCryptographicHash::Algorithm::Md5;
    case ChecksumCalculator::AlgorithmType::SHA1:
        return QCryptographicHash::Algorithm::Sha1;
    case ChecksumCalculator::AlgorithmType::SHA256:
        return QCryptographicHash::Algorithm::Sha256;
    case ChecksumCalculator::AlgorithmType::SHA3_256:
        return QCryptographicHash::Algorithm::Sha3_256;
    }
    return static_cast<QCryptographicHash::Algorithm>(-1);
}

ChecksumCalculator::ChecksumCalculator(const QString &filePath, const QByteArray &checksumTypeName)
    : _device(new QFile(filePath))
{
    if (checksumTypeName == checkSumMD5C) {
        _algorithmType = AlgorithmType::MD5;
    } else if (checksumTypeName == checkSumSHA1C) {
        _algorithmType = AlgorithmType::SHA1;
    } else if (checksumTypeName == checkSumSHA2C) {
        _algorithmType = AlgorithmType::SHA256;
    } else if (checksumTypeName == checkSumSHA3C) {
        _algorithmType = AlgorithmType::SHA3_256;
    } else if (checksumTypeName == checkSumAdlerC) {
        _algorithmType = AlgorithmType::Adler32;
    }

    initChecksumAlgorithm();
}

ChecksumCalculator::~ChecksumCalculator()
{
    QMutexLocker locker(&_deviceMutex);
    if (_device && _device->isOpen()) {
        _device->close();
    }
}

QByteArray ChecksumCalculator::calculate()
{
    QByteArray result;

    if (!_isInitialized) {
        return result;
    }

    Q_ASSERT(!_device->isOpen());
    if (_device->isOpen()) {
        qCWarning(lcChecksumCalculator) << "Device already open. Ignoring.";
    }

    if (!_device->isOpen() && !_device->open(QIODevice::ReadOnly)) {
        if (auto file = qobject_cast<QFile *>(_device.data())) {
            qCWarning(lcChecksumCalculator) << "Could not open file" << file->fileName() << "for reading to compute a checksum" << file->errorString();
        } else {
            qCWarning(lcChecksumCalculator) << "Could not open device" << _device.data() << "for reading to compute a checksum" << _device->errorString();
        }
        return result;
    }

    for (;;) {
        QMutexLocker locker(&_deviceMutex);
        if (!_device->isOpen() || _device->atEnd()) {
            break;
        }
        const auto toRead = qMin(_device->bytesAvailable(), bufSize);
        if (toRead <= 0) {
            break;
        }
        QByteArray buf(toRead, Qt::Uninitialized);
        const auto sizeRead = _device->read(buf.data(), toRead);
        if (sizeRead <= 0) {
            break;
        }
        if (!addChunk(buf, sizeRead)) {
            break;
        }
    }

    {
        QMutexLocker locker(&_deviceMutex);
        if (!_device->isOpen()) {
            return result;
        }
    }

    if (_algorithmType == AlgorithmType::Adler32) {
        result = QByteArray::number(_adlerHash, 16);
    } else {
        Q_ASSERT(_cryptographicHash);
        if (_cryptographicHash) {
            result = _cryptographicHash->result().toHex();
        }
    }

    {
        QMutexLocker locker(&_deviceMutex);
        if (_device->isOpen()) {
            _device->close();
        }
    }

    return result;
}

void ChecksumCalculator::initChecksumAlgorithm()
{
    if (_algorithmType == AlgorithmType::Undefined) {
        qCWarning(lcChecksumCalculator) << "_algorithmType is Undefined, impossible to init Checksum Algorithm";
        return;
    }

    if (_algorithmType == AlgorithmType::Adler32) {
        _adlerHash = adler32(0L, Z_NULL, 0);
    } else {
        _cryptographicHash.reset(new QCryptographicHash(algorithmTypeToQCryptoHashAlgorithm(_algorithmType)));
    }

    _isInitialized = true;
}

bool ChecksumCalculator::addChunk(const QByteArray &chunk, const qint64 size)
{
    Q_ASSERT(_algorithmType != AlgorithmType::Undefined);
    if (_algorithmType == AlgorithmType::Undefined) {
        qCWarning(lcChecksumCalculator) << "_algorithmType is Undefined, impossible to add a chunk!";
        return false;
    }

    if (_algorithmType == AlgorithmType::Adler32) {
        _adlerHash = adler32(_adlerHash, (const Bytef *)chunk.data(), size);
        return true;
    } else {
        Q_ASSERT(_cryptographicHash);
        if (_cryptographicHash) {
            _cryptographicHash->addData(chunk);
            return true;
        }
    }
    return false;
}

}
