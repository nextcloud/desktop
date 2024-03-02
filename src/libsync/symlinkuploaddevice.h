/*
 * Copyright (C) by Tamino Bauknecht <dev@tb6.eu>
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

#include "propagateupload.h"

namespace OCC {

/**
 * @brief Symlink specialization of an UploadDevice
 * @ingroup libsync
 */
class SymLinkUploadDevice : public UploadDevice
{
    Q_OBJECT

public:
    SymLinkUploadDevice(const QString &fileName, qint64 start, qint64 size, BandwidthManager *bwm);

    bool open(QIODevice::OpenMode mode) override;

    qint64 readData(char *data, qint64 maxlen) override;
    bool seek(qint64 pos) override;

protected:
    QByteArray _symlinkContent;
};
}
