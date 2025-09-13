// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 Hannah von Reth <h.vonreth@opencloud.eu>

#pragma once

#include "owncloudlib.h"

#include "common/syncjournalfilerecord.h"

#include <QIODevice>
#include <QObject>

namespace OCC
{
class Vfs;
class GETFileJob;

class OWNCLOUDSYNC_EXPORT HydrationJob : public QObject
{
    Q_OBJECT
public:
    explicit HydrationJob(Vfs *vfs, const QByteArray &fileId, std::unique_ptr<QIODevice> &&device, QObject *parent);

    void start();
    void abort();

    // In case the device to write to is a file, it can be passed here to the result slots
    void setTargetFile(const QString &fileName);
    [[nodiscard]] QString targetFileName() const;

    [[nodiscard]] Vfs *vfs() const;

    [[nodiscard]] SyncJournalFileRecord record() const;

    [[nodiscard]] QByteArray fileId() const
    {
        return _fileId;
    }

Q_SIGNALS:
    void finished();
    void error(const QString &error);

private:
    Vfs *_vfs = nullptr;
    QByteArray _fileId;
    std::unique_ptr<QIODevice> _device;
    QString _fileName;
    SyncJournalFileRecord _record;
    GETFileJob *_job = nullptr;
};
}
