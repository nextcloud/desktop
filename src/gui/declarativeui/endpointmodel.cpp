/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "endpointmodel.h"
#include "networkjobs.h"
#include "account.h"
#include "folderman.h"

namespace OCC {

EndpointModel::EndpointModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QVariant EndpointModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    const auto row = index.row();
    switch (role) {
    case EndpointIconRole:
        return _endpoints.at(row).icon; // deck.svg
    case EndpointNameRole:
        return _endpoints.at(row).name; // Convert file
    case EndpointUrlRole:
        return _endpoints.at(row).url; // /ocs/v2.php/apps/declarativetest/newDeckBoard
    case EndpointMethodRole:
        return _endpoints.at(row).method; // GET
    case EndpointParamsRole:
        return _endpoints.at(row).params; // filePath
    }

    return {};
}

int EndpointModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _endpoints.size();
}

QHash<int, QByteArray> EndpointModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[EndpointIconRole] = "icon";
    roles[EndpointNameRole] = "name";
    roles[EndpointUrlRole] = "url";
    roles[EndpointMethodRole] = "method";
    roles[EndpointParamsRole] = "params";

    return roles;
}

AccountState *EndpointModel::accountState() const
{
    return _accountState;
}

void EndpointModel::setAccountState(AccountState *accountState)
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

QString EndpointModel::localPath() const
{
    return _localPath;
}


void EndpointModel::setLocalPath(const QString &localPath)
{
    if (localPath.isEmpty()) {
        return;
    }

    if (localPath == _localPath) {
        return;
    }

    _localPath = localPath;

    setFileId();
    setMimeType();
    parseEndpoints();

    Q_EMIT localPathChanged();
}

QByteArray EndpointModel::fileId() const
{
    return _fileId;
}

void EndpointModel::setFileId()
{
    const auto folderForPath = FolderMan::instance()->folderForPath(_localPath);
    const auto file = _localPath.mid(folderForPath->cleanPath().length() + 1);
    SyncJournalFileRecord fileRecord;
    if (!folderForPath->journalDb()->getFileRecord(file, &fileRecord)) {
        qDebug() << "Invalid file record for path:" << _localPath;
        return;
    }

    _fileId = fileRecord._fileId;
}

QMimeType EndpointModel::mimeType() const
{
    return _mimeType;
}

void EndpointModel::setMimeType()
{
    const auto folderForPath = FolderMan::instance()->folderForPath(_localPath);
    const auto file = _localPath.mid(folderForPath->cleanPath().length() + 1);
    SyncJournalFileRecord fileRecord;
    if (!folderForPath->journalDb()->getFileRecord(file, &fileRecord)) {
        qDebug() << "Invalid file record for path:" << _localPath;
        return;
    }

    const auto mimeMatchMode = fileRecord.isVirtualFile() ? QMimeDatabase::MatchExtension
                                                          : QMimeDatabase::MatchDefault;
    QMimeDatabase mimeDb;
    const auto mimeType = mimeDb.mimeTypeForFile(_localPath, mimeMatchMode);
    _mimeType = mimeType;
}

QString EndpointModel::label() const
{
    return _response.label;
}

void EndpointModel::setLabel(const QString &label)
{
    _response.label = label;
}

QString EndpointModel::url() const
{
    return _response.url;
}

void EndpointModel::setUrl(const QString &url)
{
    _response.url = url;
}

void EndpointModel::setResponse(const Response &response)
{
    _response = response;
    Q_EMIT responseChanged();
}

void EndpointModel::parseEndpoints()
{
    if (!_accountState->isConnected()) {
        return;
    }

    const auto contextMenuList = _accountState->account()->capabilities().contextMenuByMimeType(_mimeType);
    for (const auto &contextMenu : contextMenuList) {
        _endpoints.append({_accountState->account()->url().toString()
                               + contextMenu.value("icon").toString(),
                           contextMenu.value("name").toString(),
                           contextMenu.value("url").toString(),
                           contextMenu.value("method").toString(),
                           contextMenu.value("params").toString()});
    }

    Q_EMIT endpointModelChanged();
}

QString EndpointModel::parseUrl(const QString &url) const
{
    auto unparsedUrl = url;
    const auto fileIdParam = QStringLiteral("{fileId}");
    const auto parsedUrl = unparsedUrl.replace(QRegularExpression(fileIdParam), _fileId);
    return parsedUrl;
}

void EndpointModel::createRequest(const int row)
{
    if (!_accountState) {
        return;
    }

    const auto requesturl = parseUrl(_endpoints.at(row).url);
    auto job = new JsonApiJob(_accountState->account(),
                                requesturl,
                                this);
    connect(job, &JsonApiJob::jsonReceived,
            this, &EndpointModel::processRequest);
    QUrlQuery params;
    params.addQueryItem(_endpoints.at(row).params, _fileId);
    job->addQueryParams(params);
    const auto verb = job->stringToVerb(_endpoints.at(row).method);
    job->setVerb(verb);
    job->start();
}

void EndpointModel::processRequest(const QJsonDocument &json)
{
    const auto root = json.object().value(QStringLiteral("root")).toObject();
    if (root.empty()) {
        return;
    }
    const auto orientation = root.value(QStringLiteral("orientation")).toString();
    const auto rows = root.value(QStringLiteral("rows")).toArray();
    if (rows.empty()) {
        return;
    }

    for (const auto &rowValue : rows) {
        const auto row = rowValue.toObject();
        const auto children = row.value("children").toArray();

        for (const auto &childValue : children) {
            const auto child = childValue.toObject();
            _response.label = child.value(QStringLiteral("element")).toString();
            _response.url = _accountState->account()->url().toString() +
                child.value(QStringLiteral("url")).toString();
        }
    }

    Q_EMIT responseChanged();
}

} // namespace OCC
