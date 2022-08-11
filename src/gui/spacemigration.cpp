/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "spacemigration.h"

#include "common/version.h"
#include "gui/accountmanager.h"
#include "gui/folderman.h"
#include "libsync/account.h"
#include "libsync/graphapi/drives.h"

#include <QJsonArray>

Q_LOGGING_CATEGORY(lcMigration, "gui.migration.spaces", QtInfoMsg)

using namespace OCC;

SpaceMigration::SpaceMigration(const AccountStatePtr &account, const QString &path, QObject *parent)
    : QObject(parent)
    , _accountState(account)
    , _path(path)
{
}

void SpaceMigration::start()
{
    FolderMan::instance()->setSyncEnabled(false);

    QJsonArray folders;
    for (auto *f : FolderMan::instance()->folders()) {
        // same account and uses the legacy dav url
        // already migrated folders are ignored
        if (f->accountState()->account() == _accountState->account() && f->webDavUrl() == _accountState->account()->davUrl()) {
            folders.append(f->remotePath());
            _migrationFolders.append(f);
        }
    }
    const QJsonObject obj{{QStringLiteral("version"), Version::version().toString()}, {QStringLiteral("user"), _accountState->account()->davUser()},
        {QStringLiteral("remotefolder"), folders}};
    auto job = new JsonJob(_accountState->account(), _accountState->account()->url(), _path, QByteArrayLiteral("POST"), obj);
    job->setParent(this);
    connect(job, &JsonJob::finishedSignal, this, [job, this] {
        const int status = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200 && job->parseError().error == QJsonParseError::NoError) {
            migrate(job->data().value(QStringLiteral("folders")).toObject());
        } else {
            if (job->parseError().error != QJsonParseError::NoError) {
                qCInfo(lcMigration) << job->parseError().errorString();
            }
            Q_EMIT finished();
        }
    });
    job->start();
}

void SpaceMigration::migrate(const QJsonObject &folders)
{
    auto drivesJob = new GraphApi::Drives(_accountState->account(), this);
    connect(drivesJob, &GraphApi::Drives::finishedSignal, [drivesJob, folders, this] {
        const auto drives = drivesJob->drives();
        for (auto &folder : _migrationFolders) {
            if (folder) {
                const auto obj = folders.value(folder->remotePath()).toObject();
                const QString error = obj.value(QStringLiteral("error")).toString();
                if (error.isEmpty()) {
                    const QString newPath = obj.value(QStringLiteral("path")).toString();
                    const QString space_id = obj.value(QStringLiteral("space_id")).toString();

                    const auto it = std::find_if(drives.cbegin(), drives.cend(), [space_id](auto it) { return it.getId() == space_id; });

                    if (it != drives.cend()) {
                        qCDebug(lcMigration) << "Migrating:" << folder->path() << "davUrl:" << folder->_definition._webDavUrl << "->"
                                             << it->getRoot().getWebDavUrl() << "remotPath:" << folder->_definition._targetPath << "->" << newPath;
                        folder->_definition._webDavUrl = QUrl(it->getRoot().getWebDavUrl());
                        folder->_definition._targetPath = newPath;
                        folder->saveToSettings();
                    }
                } else {
                    qCInfo(lcMigration) << "No migration of" << folder->remotePath() << "reason:" << error;
                }
            }
        }
        _accountState->_supportsSpaces = true;
        AccountManager::instance()->save();
        FolderMan::instance()->setSyncEnabled(true);
        Q_EMIT finished();
    });
    drivesJob->start();
}
