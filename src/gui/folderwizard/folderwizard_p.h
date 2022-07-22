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

#pragma once

#include "folderwizard.h"

#include "libsync/accountfwd.h"

#include <QCoreApplication>
#include <QStringList>

namespace OCC {
Q_DECLARE_LOGGING_CATEGORY(lcFolderWizard);

class FolderWizardPrivate
{
public:
    FolderWizardPrivate(FolderWizard *q, const AccountStatePtr &account);
    static QString formatWarnings(const QStringList &warnings, bool isError = false);

    QString initialLocalPath() const;

    QString defaultSyncRoot() const;

    QUrl davUrl() const;
    bool useVirtualFiles() const;
    QString displayName() const;

    const AccountStatePtr &accountState();

private:
    Q_DECLARE_PUBLIC(FolderWizard);
    FolderWizard *q_ptr;

    AccountStatePtr _account;
    class SpacesPage *_spacesPage;
    class FolderWizardLocalPath *_folderWizardSourcePage = nullptr;
    class FolderWizardRemotePath *_folderWizardTargetPage = nullptr;
    class FolderWizardSelectiveSync *_folderWizardSelectiveSyncPage = nullptr;
};


class FolderWizardPage : public QWizardPage
{
    Q_OBJECT
public:
    FolderWizardPage(FolderWizardPrivate *parent)
        : QWizardPage(nullptr)
        , _parent(parent)
    {
    }

protected:
    inline FolderWizardPrivate *folderWizardPrivate() const
    {
        return _parent;
    }

private:
    FolderWizardPrivate *_parent;
};
}
