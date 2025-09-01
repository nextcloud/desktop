/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudpropagator.h"
#include <QFile>

namespace OCC {

/**
 * Tags for checksum header.
 * It's here for being shared between Upload- and Download Job
 */
constexpr auto checkSumHeaderC = "OC-Checksum";
constexpr auto contentMd5HeaderC = "Content-MD5";
constexpr auto checksumRecalculateOnServerHeaderC = "X-Recalculate-Hash";

/**
 * @brief Declaration of the other propagation jobs
 * @ingroup libsync
 */
class PropagateLocalRemove : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateLocalRemove(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }

    void willDeleteItemToClientTrashBin(const QString &itemFilePath)
    {
        _deleteToClientTrashBin.insert(itemFilePath);
    }

    void start() override;

private:
    bool removeRecursively(const QString &path);

    QSet<QString> _deleteToClientTrashBin;

    bool _moveToTrash = false;
};

/**
 * @brief The PropagateLocalMkdir class
 * @ingroup libsync
 */
class PropagateLocalMkdir : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateLocalMkdir(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() override;

    /**
     * Whether an existing file with the same name may be deleted before
     * creating the directory.
     *
     * Default: false.
     */
    void setDeleteExistingFile(bool enabled);

private:
    void startLocalMkdir();
    void startDemanglingName(const QString &parentPath);

    bool _deleteExistingFile = false;
};

/**
 * @brief The PropagateLocalRename class
 * @ingroup libsync
 */
class PropagateLocalRename : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateLocalRename(OwncloudPropagator *propagator, const SyncFileItemPtr &item);
    void start() override;
    [[nodiscard]] JobParallelism parallelism() const override { return _item->isDirectory() ? WaitForFinished : FullParallelism; }

private:
    bool deleteOldDbRecord(const QString &fileName);

};
}
