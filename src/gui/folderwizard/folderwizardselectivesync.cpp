/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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
#include "folderwizardselectivesync.h"

#include "folderwizard.h"
#include "folderwizard_p.h"

#include "gui/application.h"
#include "gui/askexperimentalvirtualfilesfeaturemessagebox.h"
#include "gui/selectivesyncwidget.h"

#include "libsync/theme.h"

#include "common/vfs.h"
#include "gui/settingsdialog.h"

#include <QCheckBox>
#include <QVBoxLayout>


using namespace OCC;

FolderWizardSelectiveSync::FolderWizardSelectiveSync(FolderWizardPrivate *parent)
    : FolderWizardPage(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    _selectiveSync = new SelectiveSyncWidget(folderWizardPrivate()->accountState()->account(), this);
    layout->addWidget(_selectiveSync);

    if (!Theme::instance()->forceVirtualFilesOption() && Theme::instance()->showVirtualFilesOption()
        && VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::WindowsCfApi) {
        _virtualFilesCheckBox = new QCheckBox(tr("Use virtual files instead of downloading content immediately"));
        connect(_virtualFilesCheckBox, &QCheckBox::clicked, this, &FolderWizardSelectiveSync::virtualFilesCheckboxClicked);
        connect(_virtualFilesCheckBox, &QCheckBox::stateChanged, this, [this](int state) {
            _selectiveSync->setEnabled(state == Qt::Unchecked);
        });
        _virtualFilesCheckBox->setChecked(true);
        layout->addWidget(_virtualFilesCheckBox);
    }
}

FolderWizardSelectiveSync::~FolderWizardSelectiveSync()
{
}


void FolderWizardSelectiveSync::initializePage()
{
    QString targetPath = static_cast<FolderWizard *>(wizard())->d_func()->remotePath();
    QString alias = QFileInfo(targetPath).fileName();
    if (alias.isEmpty())
        alias = Theme::instance()->appName();
    _selectiveSync->setDavUrl(dynamic_cast<FolderWizard *>(wizard())->d_func()->davUrl());
    _selectiveSync->setFolderInfo(targetPath, alias);
    QWizardPage::initializePage();
}

bool FolderWizardSelectiveSync::validatePage()
{
    if (!useVirtualFiles()) {
        _selectiveSyncBlackList = _selectiveSync->createBlackList();
    }
    return true;
}

bool FolderWizardSelectiveSync::useVirtualFiles() const
{
    return _virtualFilesCheckBox && _virtualFilesCheckBox->isChecked();
}

void FolderWizardSelectiveSync::virtualFilesCheckboxClicked()
{
    // The click has already had an effect on the box, so if it's
    // checked it was newly activated.
    if (_virtualFilesCheckBox->isChecked()) {
        auto *messageBox = new AskExperimentalVirtualFilesFeatureMessageBox(ocApp()->gui()->settingsDialog());

        messageBox->setAttribute(Qt::WA_DeleteOnClose);

        connect(messageBox, &AskExperimentalVirtualFilesFeatureMessageBox::rejected, this, [this]() {
            _virtualFilesCheckBox->setChecked(false);
        });

        // no need to show the message box on Windows
        // as a little shortcut, we just re-use the message box's accept handler
        if (VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::WindowsCfApi) {
            Q_EMIT messageBox->accepted();
        } else {
            messageBox->show();
            ocApp()->gui()->raiseDialog(messageBox);
        }
    }
}

const QSet<QString> &FolderWizardSelectiveSync::selectiveSyncBlackList() const
{
    return _selectiveSyncBlackList;
}
