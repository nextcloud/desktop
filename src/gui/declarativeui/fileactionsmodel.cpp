/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileactionsmodel.h"
#include "networkjobs.h"
#include "account.h"
#include "folderman.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcFileActions, "nextcloud.gui.fileactions", QtInfoMsg)

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
    _accountUrl = _accountState->account()->url().toString();
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

    Q_EMIT fileChanged();
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
        qCWarning(lcFileActions) << "Invalid file record for path:" << _localPath;
        return;
    }

    _fileId = fileRecord._fileId;

    const auto mimeMatchMode = fileRecord.isVirtualFile() ? QMimeDatabase::MatchExtension
                                                          : QMimeDatabase::MatchDefault;
    QMimeDatabase mimeDb;
    const auto mimeType = mimeDb.mimeTypeForFile(_localPath, mimeMatchMode);
    _mimeType = mimeType;

    // TODO: display an icon for each mimeType
    _fileIcon = "";
}

QMimeType FileActionsModel::mimeType() const
{
    return _mimeType;
}

QString FileActionsModel::fileIcon() const
{
    return _fileIcon;
}

QString FileActionsModel::responseLabel() const
{
    return _response.label;
}

void FileActionsModel::setResponseLabel(const QString &label)
{
    _response.label = label;
}

QString FileActionsModel::responseUrl() const
{
    return _response.url;
}

void FileActionsModel::setResponseUrl(const QString &url)
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
        qCWarning(lcFileActions) << "The account is not connected" << _accountUrl;
        setResponse({ tr("Your account is offline %1.", "account url").arg(_accountUrl), _accountUrl });
        return;
    }

    if (_fileId.isEmpty()) {
        qCWarning(lcFileActions) << "The file id is empty, not initialized" << _localPath;
        setResponse({ tr("The file id is empty for %1.", "file name").arg(_localPath), _accountUrl });
        return;
    }

    if (!_mimeType.isValid()) {
        qCWarning(lcFileActions) << "The mime type found for the file is not valid" << _localPath;
        setResponse({ tr("The file type for %1 is not valid.", "file name").arg(_localPath), _accountUrl });
        return;
    }

    const auto contextMenuList = _accountState->account()->capabilities().contextMenuByMimeType(_mimeType);
    //const QList<QVariantMap> contextMenuList;
    if (contextMenuList.isEmpty()) {
        qCWarning(lcFileActions) << "contextMenuByMimeType is empty, nothing was returned by capabilities" << _localPath;
        setResponse({ tr("No file actions were returned by the server for %1 files.", "file mymetype").arg(_mimeType.filterString()), _accountUrl });
        return;
    }

    for (const auto &contextMenu : contextMenuList) {
        _fileActions.append({ parseIcon(contextMenu.value("icon").toString()),
                             contextMenu.value("name").toString(),
                             contextMenu.value("url").toString(),
                             contextMenu.value("method").toString(),
                             contextMenu.value("params").toStringList() });
    }

    qCDebug(lcFileActions) << "File" << _localPath << "has" << _fileActions.size() << "actions available.";
    Q_EMIT fileActionModelChanged();
}

QString FileActionsModel::parseUrl(const QString &url) const
{
    auto parsedUrl = url;
    parsedUrl.replace(fileIdUrlC, _fileId);
    return parsedUrl;
}

QString FileActionsModel::parseIcon(const QString &icon) const
{
    if (icon.isEmpty()) {
        return QStringLiteral("image://svgimage-custom-color/convert_to_text.svg/");
    }

    return _accountUrl + icon;
}

void FileActionsModel::createRequest(const int row)
{
    if (!_accountState) {
        qCWarning(lcFileActions) << "No account state for" << _localPath;
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
    if (!params.isEmpty()) {
        job->addQueryParams(params);
    }
    const auto verb = job->stringToVerb(_fileActions.at(row).method);
    job->setVerb(verb);
    job->setProperty(rowC, row);
    job->start();
}

void FileActionsModel::processRequest(const QJsonDocument &json, int statusCode)
{
    const auto row = sender()->property(rowC).toInt();
    const auto fileAction = _fileActions.at(row).name;
    const auto errorMessage = tr("%1 did not succeed, please try again later. "
                                 "If you need help, contact your server administrator.",
                                 "file action error message").arg(fileAction);
    if (statusCode != 200) {
        qCWarning(lcFileActions) << "File action did not succeed for" << _localPath;
        setResponse({ errorMessage, _accountUrl });
        return;
    }

    const auto root = json.object().value(QStringLiteral("root")).toObject();
    const auto folderForPath = FolderMan::instance()->folderForPath(_localPath);
    const auto remoteFolderPath = _accountUrl + folderForPath->remotePath();
    const auto successMessage = tr("%1 done.", "file action success message").arg(fileAction);
    if (root.empty()) {
        setResponse({ successMessage, remoteFolderPath });
        return;
    }

    const auto orientation = root.value(QStringLiteral("orientation")).toString();
    const auto rows = root.value(QStringLiteral("rows")).toArray();
    if (rows.empty()) {
        setResponse({ successMessage, remoteFolderPath });
        return;
    }

    for (const auto &rowValue : rows) {
        const auto row = rowValue.toObject();
        const auto children = row.value("children").toArray();
        for (const auto &childValue : children) {
            const auto child = childValue.toObject();
            setResponse({ child.value(QStringLiteral("element")).toString(),
                         _accountUrl + child.value(QStringLiteral("url")).toString() });
        }
    }
}

} // namespace OCC
