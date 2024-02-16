/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "folderwizard.h"
#include "folderwizard_p.h"

#include "folderwizardlocalpath.h"
#include "folderwizardremotepath.h"
#include "folderwizardselectivesync.h"

#include "spacespage.h"

#include "account.h"
#include "common/asserts.h"
#include "gui/application.h"
#include "gui/settingsdialog.h"
#include "theme.h"

#include "gui/accountstate.h"
#include "gui/folderman.h"
#include "gui/spaces/spacesmodel.h"

#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QUrl>

#include <stdlib.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderWizard, "gui.folderwizard", QtInfoMsg)

QString FolderWizardPrivate::formatWarnings(const QStringList &warnings, bool isError)
{
    QString ret;
    if (warnings.count() == 1) {
        ret = isError ? QCoreApplication::translate("FolderWizard", "<b>Error:</b> %1").arg(warnings.first()) : QCoreApplication::translate("FolderWizard", "<b>Warning:</b> %1").arg(warnings.first());
    } else if (warnings.count() > 1) {
        QStringList w2;
        for (const auto &warning : warnings) {
            w2.append(QStringLiteral("<li>%1</li>").arg(warning));
        }
        ret = isError ? QCoreApplication::translate("FolderWizard", "<b>Error:</b><ul>%1</ul>").arg(w2.join(QString()))
                      : QCoreApplication::translate("FolderWizard", "<b>Warning:</b><ul>%1</ul>").arg(w2.join(QString()));
    }

    return ret;
}

QString FolderWizardPrivate::defaultSyncRoot() const
{
    if (!_account->account()->hasDefaultSyncRoot()) {
        auto folderType = _account->supportsSpaces() ? FolderMan::NewFolderType::SpacesSyncRoot : FolderMan::NewFolderType::OC10SyncRoot;
        return FolderMan::suggestSyncFolder(_account->account()->url(), _account->account()->davDisplayName(), folderType);
    } else {
        return _account->account()->defaultSyncRoot();
    }
}

FolderWizardPrivate::FolderWizardPrivate(FolderWizard *q, const AccountStatePtr &account)
    : q_ptr(q)
    , _account(account)
    , _folderWizardSourcePage(new FolderWizardLocalPath(this))
    , _folderWizardSelectiveSyncPage(new FolderWizardSelectiveSync(this))
{
    if (account->supportsSpaces()) {
        _spacesPage = new SpacesPage(account->account(), q);
        q->setPage(FolderWizard::Page_Space, _spacesPage);
        _spacesPage->installEventFilter(q);
    }
    q->setPage(FolderWizard::Page_Source, _folderWizardSourcePage);
    _folderWizardSourcePage->installEventFilter(q);

    // for now spaces are meant to be synced as a whole
    if (!_account->supportsSpaces() && !Theme::instance()->singleSyncFolder()) {
        _folderWizardTargetPage = new FolderWizardRemotePath(this);
        q->setPage(FolderWizard::Page_Target, _folderWizardTargetPage);
        _folderWizardTargetPage->installEventFilter(q);
    }

    q->setPage(FolderWizard::Page_SelectiveSync, _folderWizardSelectiveSyncPage);
}

QString FolderWizardPrivate::initialLocalPath() const
{
    if (_account->supportsSpaces()) {
        return FolderMan::findGoodPathForNewSyncFolder(
            defaultSyncRoot(), _spacesPage->selectedSpaceData(Spaces::SpacesModel::Columns::Name).toString(), FolderMan::NewFolderType::SpacesSyncRoot);
    }

    // Split default sync root:
    const QFileInfo path(defaultSyncRoot());
    return FolderMan::findGoodPathForNewSyncFolder(path.path(), path.fileName(), FolderMan::NewFolderType::OC10SyncRoot);
}

QString FolderWizardPrivate::remotePath() const
{
    return _folderWizardTargetPage ? _folderWizardTargetPage->targetPath() : QString();
}

uint32_t FolderWizardPrivate::priority() const
{
    if (_account->supportsSpaces()) {
        return _spacesPage->selectedSpaceData(Spaces::SpacesModel::Columns::Priority).toUInt();
    }
    return 0;
}

QUrl FolderWizardPrivate::davUrl() const
{
    if (_account->supportsSpaces()) {
        auto url = _spacesPage->selectedSpaceData(Spaces::SpacesModel::Columns::WebDavUrl).toUrl();
        if (!url.path().endsWith(QLatin1Char('/'))) {
            url.setPath(url.path() + QLatin1Char('/'));
        }
        return url;
    }
    return _account->account()->davUrl();
}

QString FolderWizardPrivate::spaceId() const
{
    if (_account->supportsSpaces()) {
        return _spacesPage->selectedSpaceData(Spaces::SpacesModel::Columns::SpaceId).toString();
    }
    return {};
}

QString FolderWizardPrivate::displayName() const
{
    if (_account->supportsSpaces()) {
        return _spacesPage->selectedSpaceData(Spaces::SpacesModel::Columns::Name).toString();
    }
    return QString();
}

const AccountStatePtr &FolderWizardPrivate::accountState()
{
    return _account;
}

bool FolderWizardPrivate::useVirtualFiles() const
{
    const auto mode = VfsPluginManager::instance().bestAvailableVfsMode();
    const bool useVirtualFiles = (Theme::instance()->forceVirtualFilesOption() && mode == Vfs::WindowsCfApi) || (_folderWizardSelectiveSyncPage->useVirtualFiles());
    if (useVirtualFiles) {
        const auto availability = Vfs::checkAvailability(initialLocalPath(), mode);
        if (!availability) {
            auto msg = new QMessageBox(QMessageBox::Warning, FolderWizard::tr("Virtual files are not available for the selected folder"), availability.error(), QMessageBox::Ok, ocApp()->gui()->settingsDialog());
            msg->setAttribute(Qt::WA_DeleteOnClose);
            msg->open();
            return false;
        }
    }
    return useVirtualFiles;
}

FolderWizard::FolderWizard(const AccountStatePtr &account, QWidget *parent)
    : QWizard(parent)
    , d_ptr(new FolderWizardPrivate(this, account))
{
    setWindowTitle(tr("Add Folder Sync Connection"));
    setOptions(QWizard::CancelButtonOnLeft);
    setButtonText(QWizard::FinishButton, tr("Add Sync Connection"));
    setWizardStyle(QWizard::ModernStyle);
}

FolderWizard::~FolderWizard()
{
}

bool FolderWizard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        // Workaround QTBUG-3396:  forces QWizardPrivate::updateLayout()
        QTimer::singleShot(0, this, [this] { setTitleFormat(titleFormat()); });
    }
    return QWizard::eventFilter(watched, event);
}

void FolderWizard::resizeEvent(QResizeEvent *event)
{
    QWizard::resizeEvent(event);

    // workaround for QTBUG-22819: when the error label word wrap, the minimum height is not adjusted
    if (auto page = currentPage()) {
        int hfw = page->heightForWidth(page->width());
        if (page->height() < hfw) {
            page->setMinimumSize(page->minimumSizeHint().width(), hfw);
            setTitleFormat(titleFormat()); // And another workaround for QTBUG-3396
        }
    }
}

FolderWizard::Result FolderWizard::result()
{
    Q_D(FolderWizard);

    const QString localPath = d->_folderWizardSourcePage->localPath();
    if (!d->_account->account()->hasDefaultSyncRoot()) {
        if (FileSystem::isChildPathOf(localPath, d->defaultSyncRoot())) {
            d->_account->account()->setDefaultSyncRoot(d->defaultSyncRoot());
            if (!QFileInfo::exists(d->defaultSyncRoot())) {
                OC_ASSERT(QDir().mkpath(d->defaultSyncRoot()));
            }
        }
    }

    return {
        d->davUrl(), //
        d->spaceId(), //
        localPath, //
        d->remotePath(), //
        d->displayName(), //
        d->useVirtualFiles(), //
        d->priority(), //
        d->_folderWizardSelectiveSyncPage ? d->_folderWizardSelectiveSyncPage->selectiveSyncBlackList() : QSet<QString>{} //
    };
}

} // end namespace
