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
#include "folderwizardlocalpath.h"
#include "ui_folderwizardsourcepage.h"

#include "folderwizard.h"
#include "folderwizard_p.h"

#include "gui/folderman.h"

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

using namespace OCC;

FolderWizardLocalPath::FolderWizardLocalPath(const AccountPtr &account, QWidget *parent)
    : QWizardPage(parent)
    , _ui(new Ui_FolderWizardSourcePage)
    , _account(account)
{
    _ui->setupUi(this);
    registerField(QLatin1String("sourceFolder*"), _ui->localFolderLineEdit);
    connect(_ui->localFolderChooseBtn, &QAbstractButton::clicked, this, &FolderWizardLocalPath::slotChooseLocalFolder);
    _ui->localFolderChooseBtn->setToolTip(tr("Click to select a local folder to sync."));

    _ui->localFolderLineEdit->setToolTip(tr("Enter the path to the local folder."));

    _ui->warnLabel->setTextFormat(Qt::RichText);
    _ui->warnLabel->hide();
}

FolderWizardLocalPath::~FolderWizardLocalPath()
{
    delete _ui;
}

void FolderWizardLocalPath::initializePage()
{
    _ui->warnLabel->hide();
    _ui->localFolderLineEdit->setText(QDir::toNativeSeparators(static_cast<FolderWizard *>(wizard())->destination()));
}

void FolderWizardLocalPath::cleanupPage()
{
    _ui->warnLabel->hide();
}

bool FolderWizardLocalPath::isComplete() const
{
    QString errorStr = FolderMan::instance()->checkPathValidityForNewFolder(
        QDir::fromNativeSeparators(_ui->localFolderLineEdit->text()));


    bool isOk = errorStr.isEmpty();
    QStringList warnStrings;
    if (!isOk) {
        warnStrings << errorStr;
    }

    _ui->warnLabel->setWordWrap(true);
    if (isOk) {
        _ui->warnLabel->hide();
        _ui->warnLabel->clear();
    } else {
        _ui->warnLabel->show();
        QString warnings = FolderWiardPrivate::formatWarnings(warnStrings);
        _ui->warnLabel->setText(warnings);
    }
    return isOk;
}

void FolderWizardLocalPath::slotChooseLocalFolder()
{
    QString sf = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir d(sf);

    // open the first entry of the home dir. Otherwise the dir picker comes
    // up with the closed home dir icon, stupid Qt default...
    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
        QDir::DirsFirst | QDir::Name);

    if (dirs.count() > 0)
        sf += QLatin1Char('/') + dirs.at(0); // Take the first dir in home dir.

    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select the local folder"),
        sf);
    if (!dir.isEmpty()) {
        // set the last directory component name as alias
        _ui->localFolderLineEdit->setText(QDir::toNativeSeparators(dir));
    }
    emit completeChanged();
}
