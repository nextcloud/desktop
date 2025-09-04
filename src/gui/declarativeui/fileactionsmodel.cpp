/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileactionsmodel.h"
#include "networkjobs.h"
#include "account.h"
#include "folderman.h"

namespace OCC {

FileActionsModel::FileActionsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QVariant FileActionsModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    const auto row = index.row();
    switch (role) {
    case FileActionIconRole:
        return _fileActions.at(row).icon; // deck.svg
    case FileActionNameRole:
        return _fileActions.at(row).name; // Convert file
    case FileActionUrlRole:
        return _fileActions.at(row).url; // /ocs/v2.php/apps/declarativetest/newDeckBoard
    case FileActionMethodRole:
        return _fileActions.at(row).method; // GET
    case FileActionParamsRole:
        return _fileActions.at(row).params; // filePath
    }

    return {};
}

int FileActionsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _fileActions.size();
}

QHash<int, QByteArray> FileActionsModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[FileActionIconRole] = "icon";
    roles[FileActionNameRole] = "name";
    roles[FileActionUrlRole] = "url";
    roles[FileActionMethodRole] = "method";
    roles[FileActionParamsRole] = "params";

    return roles;
}

AccountState *FileActionsModel::accountState() const
{
    return _accountState;
}

void FileActionsModel::setAccountState(AccountState *accountState)
{
    if (accountState == nullptr) {
        return;
    }

    if (accountState == _accountState) {
        return;
    }

    _accountState = accountState;
    Q_EMIT accountStateChanged();
}

QString FileActionsModel::localPath() const
{
    return _localPath;
}


void FileActionsModel::setLocalPath(const QString &localPath)
{
    if (localPath.isEmpty()) {
        return;
    }

    if (localPath == _localPath) {
        return;
    }

    _localPath = localPath;

    setupFileProperties();
    parseEndpoints();

    Q_EMIT localPathChanged();
}

QByteArray FileActionsModel::fileId() const
{
    return _fileId;
}

void FileActionsModel::setupFileProperties()
{
    const auto folderForPath = FolderMan::instance()->folderForPath(_localPath);
    _filePath = _localPath.mid(folderForPath->cleanPath().length() + 1);
    SyncJournalFileRecord fileRecord;
    if (!folderForPath->journalDb()->getFileRecord(_filePath, &fileRecord)) {
        qDebug() << "Invalid file record for path:" << _localPath;
        return;
    }

    _fileId = fileRecord._fileId;

    const auto mimeMatchMode = fileRecord.isVirtualFile() ? QMimeDatabase::MatchExtension
                                                          : QMimeDatabase::MatchDefault;
    QMimeDatabase mimeDb;
    const auto mimeType = mimeDb.mimeTypeForFile(_localPath, mimeMatchMode);
    _mimeType = mimeType;
}

QMimeType FileActionsModel::mimeType() const
{
    return _mimeType;
}

QString FileActionsModel::label() const
{
    return _response.label;
}

void FileActionsModel::setLabel(const QString &label)
{
    _response.label = label;
}

QString FileActionsModel::url() const
{
    return _response.url;
}

void FileActionsModel::setUrl(const QString &url)
{
    _response.url = url;
}

void FileActionsModel::setResponse(const Response &response)
{
    _response = response;
    Q_EMIT responseChanged();
}

void FileActionsModel::parseEndpoints()
{
    if (!_accountState->isConnected()) {
        return;
    }

    if (_fileId.isEmpty()) {
        return;
    }

    if (!_mimeType.isValid()) {
        return;
    }

    const auto contextMenuList = _accountState->account()->capabilities().contextMenuByMimeType(_mimeType);
    for (const auto &contextMenu : contextMenuList) {
        _fileActions.append({_accountState->account()->url().toString()
                               + contextMenu.value("icon").toString(),
                           contextMenu.value("name").toString(),
                           contextMenu.value("url").toString(),
                           contextMenu.value("method").toString(),
                           contextMenu.value("params").toStringList()});
    }

    Q_EMIT fileActionModelChanged();
}

QString FileActionsModel::parseUrl(const QString &url) const
{
    auto unparsedUrl = url;
    const auto parsedUrl = unparsedUrl.replace(QRegularExpression(fileIdUrlC), _fileId);
    return parsedUrl;
}

void FileActionsModel::createRequest(const int row)
{
    if (!_accountState) {
        return;
    }

    const auto requesturl = parseUrl(_fileActions.at(row).url);
    auto job = new JsonApiJob(_accountState->account(),
                                requesturl,
                                this);
    connect(job, &JsonApiJob::jsonReceived,
            this, &FileActionsModel::processRequest);
    QUrlQuery params;
    for (const auto &param : _fileActions.at(row).params) {
        if (param == fileIdC) {
            params.addQueryItem(param, _fileId);
        }

        if (param == filePathC) {
            params.addQueryItem(param, _filePath);
        }
    }
    job->addQueryParams(params);
    const auto verb = job->stringToVerb(_fileActions.at(row).method);
    job->setVerb(verb);
    job->start();
}

void FileActionsModel::processRequest(const QJsonDocument &json, int statusCode)
{
    Q_UNUSED(json)
    auto message = tr("File action succeded, access your instance for the result.");
    if (statusCode != 200) {
        message = tr("File action did not succeed, access your instance for details.");
        return;
    }

    _response.label = message;
    _response.url = _accountState->account()->url().toString();

    Q_EMIT responseChanged();
}

} // namespace OCC
