/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "filetagmodel.h"

#include "libsync/networkjobs.h"

Q_LOGGING_CATEGORY(lcFileTagModel, "nextcloud.gui.filetagmodel", QtInfoMsg)

namespace OCC {

FileTagModel::FileTagModel(const QString &serverRelativePath,
                           const AccountPtr &account,
                           QObject * const parent)
    : QAbstractListModel(parent)
    , _serverRelativePath(serverRelativePath)
    , _account(account)
{
    fetchFileTags();
}

int FileTagModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _tags.count();
}

QVariant FileTagModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return _tags.at(index.row());
    default:
        return QVariant();
    }
}

void FileTagModel::fetchFileTags()
{
    if (!_account || _serverRelativePath.isEmpty()) {
        qCDebug(lcFileTagModel) << "Cannot fetch filetags as account is null, or server relative path is empty";
        return;
    }

    qCDebug(lcFileTagModel) << "Starting fetch of filetags for file at:" << _serverRelativePath;

    const auto propfindJob = new PropfindJob(_account, _serverRelativePath, this);
    propfindJob->setProperties({ QByteArrayLiteral("http://nextcloud.org/ns:tags") });

    connect(propfindJob, &PropfindJob::result, this, &FileTagModel::processFileTagRequestFinished);
    connect(propfindJob, &PropfindJob::finishedWithError, this, &FileTagModel::processFileTagRequestFinishedWithError);

    propfindJob->start();
}

void FileTagModel::processFileTagRequestFinished(const QVariantMap &result)
{
    if (result.empty()) {
        qCDebug(lcFileTagModel) << "File tag fetch request finished successfully, but received empty results..."
                                << _serverRelativePath;
        return;
    }

    qCDebug(lcFileTagModel) << "File tag fetch request finished successfully, processing results..."
                            << _serverRelativePath;

    beginResetModel();
    _tags = result.value(QStringLiteral("tags")).toStringList();
    endResetModel();
}

void FileTagModel::processFileTagRequestFinishedWithError()
{
    qCWarning(lcFileTagModel) << "File tag fetch request job ended with error.";
}

}
