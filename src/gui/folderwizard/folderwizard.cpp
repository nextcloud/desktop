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
#include "configfile.h"
#include "creds/abstractcredentials.h"
#include "gui/application.h"
#include "gui/askexperimentalvirtualfilesfeaturemessagebox.h"
#include "gui/settingsdialog.h"
#include "networkjobs.h"
#include "theme.h"

#include "gui/accountstate.h"
#include "gui/folderman.h"
#include "gui/selectivesyncdialog.h"
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
    if (!_account->hasDefaultSyncRoot()) {
        return FolderMan::suggestSyncFolder(_account->url(), _account->davDisplayName());
    } else {
        return _account->defaultSyncRoot();
    }
}

FolderWizardPrivate::FolderWizardPrivate(FolderWizard *q, const AccountPtr &account)
    : q_ptr(q)
    , _account(account)
    , _folderWizardSourcePage(new FolderWizardLocalPath(account))
    , _folderWizardSelectiveSyncPage(new FolderWizardSelectiveSync(account))
{
    if (account->capabilities().spacesSupport().enabled) {
        _spacesPage = new SpacesPage(account, q);
        q->setPage(FolderWizard::Page_Space, _spacesPage);
        _spacesPage->installEventFilter(q);
    }
    q->setPage(FolderWizard::Page_Source, _folderWizardSourcePage);
    _folderWizardSourcePage->installEventFilter(q);
    // for now spaces are meant to be synced as a whole
    if (!account->capabilities().spacesSupport().enabled && !Theme::instance()->singleSyncFolder()) {
        _folderWizardTargetPage = new FolderWizardRemotePath(account);
        q->setPage(FolderWizard::Page_Target, _folderWizardTargetPage);
        _folderWizardTargetPage->installEventFilter(q);
    }
    if (!account->capabilities().spacesSupport().enabled) {
        // TODO: add spaces support to selective sync
        q->setPage(FolderWizard::Page_SelectiveSync, _folderWizardSelectiveSyncPage);
    }
}

QString FolderWizardPrivate::initialLocalPath() const
{
    QString defaultPath = defaultSyncRoot();
    if (_account->capabilities().spacesSupport().enabled) {
        defaultPath += QLatin1Char('/') + _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::Name).toString();
    };
    return FolderMan::instance()->findGoodPathForNewSyncFolder(defaultPath);
}

QUrl FolderWizardPrivate::davUrl() const
{
    if (_account->capabilities().spacesSupport().enabled) {
        auto url = _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::WebDavUrl).toUrl();
        if (!url.path().endsWith(QLatin1Char('/'))) {
            url.setPath(url.path() + QLatin1Char('/'));
        }
        return url;
    }
    return _account->davUrl();
}

QString FolderWizardPrivate::displayName() const
{
    if (_account->capabilities().spacesSupport().enabled) {
        return _spacesPage->selectedSpace(Spaces::SpacesModel::Columns::Name).toString();
    };
    return QString();
}

bool FolderWizardPrivate::useVirtualFiles() const
{
    const auto mode = bestAvailableVfsMode();
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

FolderWizard::FolderWizard(const AccountPtr &account, QWidget *parent, Qt::WindowFlags flags)
    : QWizard(parent, flags)
    , d_ptr(new FolderWizardPrivate(this, account))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
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
    if (!d->_account->hasDefaultSyncRoot()) {
        if (FileSystem::isChildPathOf(localPath, d->defaultSyncRoot())) {
            d->_account->setDefaultSyncRoot(d->defaultSyncRoot());
        }
    }
    return {
        d->davUrl(),
        localPath,
        d->_folderWizardTargetPage ? d->_folderWizardTargetPage->targetPath() : QString(),
        d->displayName(),
        d->useVirtualFiles(),
        d->_folderWizardSelectiveSyncPage ? d->_folderWizardSelectiveSyncPage->selectiveSyncBlackList() : QStringList()
    };
}

} // end namespace
