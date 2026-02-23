/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileactionsmodel.h"
#include "networkjobs.h"
#include "account.h"
#include "folderman.h"
#include "common/utility.h"
#include "tray/activitydata.h"

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
        return QVariant::fromValue<QueryList>(_fileActions.at(row).params);
    case FileActionResponseLabelRole:
        return _response.label;
    case FileActionResponseUrlRole:
        return _response.url;
    default:
        return QVariant();
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
    roles[FileActionResponseLabelRole] = "responseLabel";
    roles[FileActionResponseUrlRole] = "responseUrl";

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

void FileActionsModel::setFileId(const QByteArray &fileId)
{
    if (fileId == _fileId) {
        return;
    }

    _fileId = fileId;

    if (_accountState && !_localPath.isEmpty()) {
        parseEndpoints();
    }
    
    Q_EMIT fileChanged();
}

QString FileActionsModel::remoteItemPath() const
{
    return _remoteItemPath;
}

void FileActionsModel::setRemoteItemPath(const QString &remoteItemPath)
{
    if (remoteItemPath == _remoteItemPath) {
        return;
    }
    _remoteItemPath = remoteItemPath;
    Q_EMIT fileChanged();
}

void FileActionsModel::setupFileProperties()
{
    qCDebug(lcFileActions) << "Setting up file properties for:" << _localPath;

    const auto folderForPath = FolderMan::instance()->folderForPath(_localPath);

    // Declare once so it's available after the if-else clause.
    QMimeDatabase::MatchMode mimeMatchMode;

    if (folderForPath) { // Synchronization folders
        qCDebug(lcFileActions) << "Found synchronization folder for" << _localPath;
        _filePath = _localPath.mid(folderForPath->cleanPath().length() + 1);

        SyncJournalFileRecord fileRecord;

        if (!folderForPath->journalDb()->getFileRecord(_filePath, &fileRecord)) {
            qCWarning(lcFileActions) << "Invalid file record for path:" << _localPath;
            return;
        }

        _fileId = fileRecord._fileId;

        // Decide match mode based on whether this is a virtual file
        mimeMatchMode = fileRecord.isVirtualFile() ? QMimeDatabase::MatchExtension
                                                   : QMimeDatabase::MatchDefault;
    } else { // Virtual file systems
        qCDebug(lcFileActions) << "Did not find synchronization folder for" << _localPath;
        // In this case, _fileId should already be initialized with a value from the calling code.
        _filePath = _localPath;
        // When we don't have a sync folder, use extension matching
        mimeMatchMode = QMimeDatabase::MatchExtension;
    }

    QMimeDatabase mimeDb;
    const auto mimeType = mimeDb.mimeTypeForFile(_localPath, mimeMatchMode);
    _mimeType = mimeType;
    _fileIcon = _accountUrl + Activity::relativeServerFileTypeIconPath(_mimeType);
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
    auto resetActions = [this](const ActionList &actions) {
        beginResetModel();
        _fileActions = actions;
        endResetModel();
        Q_EMIT fileActionModelChanged();
    };

    if (!_accountState) {
        qCWarning(lcFileActions) << "No account state available for" << _localPath;
        resetActions({});
        return;
    }

    if (!_accountState->isConnected()) {
        qCWarning(lcFileActions) << "The account is not connected"
                                 << _accountUrl;
        setResponse({ tr("Your account is offline %1.", "account url").arg(_accountUrl),
                     _accountUrl });
        resetActions({});
        return;
    }

    if (_fileId.isEmpty()) {
        qCWarning(lcFileActions) << "The file id is empty, not initialized"
                                 << _localPath;
        setResponse({ tr("The file ID is empty for %1.", "file name").arg(_localPath),
                     _accountUrl });
        return;
    }

    if (!_mimeType.isValid()) {
        qCWarning(lcFileActions) << "The mime type found for the file is not valid"
                                 << _localPath;
        setResponse({ tr("The file type for %1 is not valid.", "file name").arg(_localPath),
                     _accountUrl });
        resetActions({});
        return;
    }

    const auto contextMenuList = _accountState->account()->capabilities().fileActionsByMimeType(_mimeType);
    if (contextMenuList.isEmpty()) {
        qCWarning(lcFileActions) << "contextMenuByMimeType is empty, nothing was returned by capabilities"
                                 << _localPath;

        //: TRANSLATOR Placeholder contains file MIME type
        setResponse({ tr("No file actions were returned by the server for %1 files.", "file mimetype")
                         .arg(_mimeType.filterString()),
                     _accountUrl });
        resetActions({});
        return;
    }

    ActionList actions;
    for (const auto &contextMenu : contextMenuList) {
        QueryList queryList;
        const auto paramsMap = contextMenu.value("params").toMap();
        for (auto param = paramsMap.cbegin(), end = paramsMap.cend(); param != end; ++param) {
            const auto name = param.key();
            QByteArray value;
            if (name == fileIdC) {
                value = _fileId;
            }

            if (param.key() == filePathC) {
                value = _filePath.toUtf8();
            }

            if (!value.isEmpty()) {
                queryList.append( QueryItem{ name, value } );
            }
        }

        actions.append({ parseIcon(contextMenu.value("icon").toString()),
                        contextMenu.value("name").toString(),
                        contextMenu.value("url").toString(),
                        contextMenu.value("method").toString(),
                        queryList });
    }

    resetActions(actions);

    qCDebug(lcFileActions) << "File" << _localPath << "has"
                           << actions.size()
                           << "actions available.";
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
        qCWarning(lcFileActions) << "No account state for"
                                 << _localPath;
        return;
    }

    const auto requesturl = parseUrl(_fileActions.at(row).url);
    auto job = new JsonApiJob(_accountState->account(),
                                requesturl,
                                this);
    connect(job, &JsonApiJob::jsonReceived,
            this, &FileActionsModel::processRequest);
    for (const auto &param : std::as_const(_fileActions.at(row).params)) {
        QUrlQuery query;
        query.addQueryItem(param.name, param.value);
        job->addQueryParams(query);
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
    const auto successMessage = tr("%1 done.", "file action success message").arg(fileAction);

    QString remoteFolderPath;
    if (folderForPath) {
        remoteFolderPath = _accountUrl + folderForPath->remotePath();
    } else if (!_remoteItemPath.isEmpty()) {
        remoteFolderPath = _accountUrl + _remoteItemPath;
    } else {
        qCWarning(lcFileActions) << "Failed to find folder for path and no remote item path available:" << _localPath;
        return;
    }

    if (root.empty()) {
        setResponse({ successMessage, remoteFolderPath });
        return;
    }

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

