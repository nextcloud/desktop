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
    } else if (_maxTags <= 0) {
        return totalTags();
    } else {
        return qMin(totalTags(), maxTags());
    }
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
    propfindJob->setProperties({ QByteArrayLiteral("http://nextcloud.org/ns:tags"),
                                 QByteArrayLiteral("http://nextcloud.org/ns:system-tags") });

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
    _tags.clear();

    const auto normalTags = result.value(QStringLiteral("tags")).toStringList();
    const auto systemTags = result.value(QStringLiteral("system-tags")).toList();

    auto systemTagStringList = QStringList();
    for (const auto &systemTagMapVariant : systemTags) {
        const auto systemTagMap = systemTagMapVariant.toMap();
        const auto systemTag = systemTagMap.value(QStringLiteral("tag")).toString();
        systemTagStringList << systemTag;
    }

    _tags << normalTags << systemTagStringList;

    Q_EMIT totalTagsChanged();
    updateOverflowTagsString();
    endResetModel();
}

void FileTagModel::processFileTagRequestFinishedWithError()
{
    qCWarning(lcFileTagModel) << "File tag fetch request job ended with error.";
}

void FileTagModel::resetForNewFile()
{
    beginResetModel();
    _tags.clear();
    Q_EMIT totalTagsChanged();
    endResetModel();

    fetchFileTags();
}

QString FileTagModel::serverRelativePath() const
{
    return _serverRelativePath;
}

void FileTagModel::setServerRelativePath(const QString &serverRelativePath)
{
    if (_serverRelativePath == serverRelativePath) {
        return;
    }

    _serverRelativePath = serverRelativePath;
    Q_EMIT serverRelativePathChanged();

    resetForNewFile();
}

AccountPtr FileTagModel::account() const
{
    return _account;
}

void FileTagModel::setAccount(const AccountPtr &account)
{
    if (_account == account) {
        return;
    }

    _account = account;
    Q_EMIT accountChanged();

    resetForNewFile();
}

int FileTagModel::maxTags() const
{
    return _maxTags;
}

void FileTagModel::setMaxTags(const int maxTags)
{
    if (_maxTags == maxTags) {
        return;
    }

    const auto totalTagCount = totalTags();
    const auto tagsWillBeTruncated = maxTags < totalTagCount;

    // Summary tag will be after the last tag
    const auto notifyOverflowingTagsRemoved = tagsWillBeTruncated && totalTagCount > maxTags;

    if (notifyOverflowingTagsRemoved) {
        beginResetModel();
    }

    _maxTags = maxTags;

    if (notifyOverflowingTagsRemoved) {
        endResetModel();
    }

    Q_EMIT maxTagsChanged();
    updateOverflowTagsString();
}

QString FileTagModel::overflowTagsString() const
{
    return _overflowTagsString;
}

void FileTagModel::updateOverflowTagsString()
{
    if (totalTags() <= _maxTags) {
        _overflowTagsString = "";
    } else {
        const auto overflowingTags = _tags.mid(_maxTags + 1);
        _overflowTagsString = overflowingTags.join(", ");
    }

    qDebug() << _overflowTagsString;
    Q_EMIT overflowTagsStringChanged();
}

int FileTagModel::totalTags() const
{
    return _tags.count();
}

}
