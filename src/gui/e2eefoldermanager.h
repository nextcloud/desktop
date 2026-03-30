// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QMap>
#include "account.h"
#include "accountmanager.h"

namespace OCC {

class Folder;

/**
 * @brief Stateless bridge between E2E encryption and folder management
 *
 * This class acts as a mediator that:
 * - Listens to E2E initialization signals from all accounts
 * - Coordinates folder restoration when E2E becomes ready
 * - Keeps E2E concerns separate from FolderMan's core responsibilities
 *
 * @ingroup gui
 */
class E2EFolderManager : public QObject
{
    Q_OBJECT

public:
    static E2EFolderManager *instance();
    ~E2EFolderManager() override;

private slots:
    void slotE2eInitializationFinished();
    void slotAccountAdded(AccountState *accountState);

private:
    E2EFolderManager(QObject *parent = nullptr);

    void connectE2eSignals(const AccountPtr &account);
    void restoreE2eFoldersForAccount(const AccountPtr &account);
    void folderTerminateSyncAndUpdateBlackList(const QStringList &blackList, OCC::Folder *folder, const QStringList &foldersToRemoveFromBlacklist);

    QMap<QString, QMetaObject::Connection> _folderConnections;
    static E2EFolderManager *_instance;
};

} // namespace OCC
