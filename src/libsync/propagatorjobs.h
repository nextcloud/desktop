/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include "owncloudpropagator.h"
#include <QFile>

namespace OCC {

/**
 * Tags for checksum header.
 * It's here for being shared between Upload- and Download Job
 */
static const char checkSumHeaderC[] = "OC-Checksum";

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
    void start() Q_DECL_OVERRIDE;

private:
    bool removeRecursively(const QString &path);
    QString _error;
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
        , _deleteExistingFile(false)
    {
    }
    void start() Q_DECL_OVERRIDE;

    /**
     * Whether an existing file with the same name may be deleted before
     * creating the directory.
     *
     * Default: false.
     */
    void setDeleteExistingFile(bool enabled);

private:
    bool _deleteExistingFile;
};

/**
 * @brief The PropagateLocalRename class
 * @ingroup libsync
 */
class PropagateLocalRename : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateLocalRename(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() Q_DECL_OVERRIDE;
    JobParallelism parallelism() Q_DECL_OVERRIDE { return _item->isDirectory() ? WaitForFinished : FullParallelism; }
};
}
