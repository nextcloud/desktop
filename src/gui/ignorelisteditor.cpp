/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"

#include "folderman.h"
#include "generalsettings.h"
#include "ignorelisteditor.h"
#include "ignorelisttablewidget.h"
#include "ui_ignorelisteditor.h"

#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>

namespace OCC {

IgnoreListEditor::IgnoreListEditor(QWidget *parent)
    : QDialog{parent}
    , ui{new Ui::IgnoreListEditor}
    , _ignoreListType{IgnoreListType::Global}
{
    ConfigFile cfgFile;
    _ignoreFile = cfgFile.excludeFile(ConfigFile::Scope::UserScope);
    setupUi();
}

IgnoreListEditor::IgnoreListEditor(const QString &ignoreFile, QWidget *parent)
    : QDialog{parent}
    , ui{new Ui::IgnoreListEditor}
    , _ignoreFile{ignoreFile}
    , _ignoreListType{IgnoreListType::Folder}
{
    setupUi();
    ui->groupboxGlobalIgnoreSettings->hide();
}

IgnoreListEditor::~IgnoreListEditor()
{
    delete ui;
}

bool IgnoreListEditor::ignoreHiddenFiles() const
{
    return !ui->syncHiddenFilesCheckBox->isChecked();
}

void IgnoreListEditor::slotSaveIgnoreList()
{
    // TODO: this will tell the file provider extension a different set of files to globally ignore
    // when called from the local editor -- not good!
    ui->ignoreTableWidget->slotWriteIgnoreFile(_ignoreFile);

    if (_ignoreListType != Global) {
        return;
    }

    /* handle the hidden file checkbox for the global ignore list editor */

    /* the ignoreHiddenFiles flag is a folder specific setting, but for now, it is
    * handled globally. Save it to every folder that is defined.
    * TODO this can now be fixed, simply attach this IgnoreListEditor to top-level account
    * settings
    */
    FolderMan::instance()->setIgnoreHiddenFiles(ignoreHiddenFiles());
}

void IgnoreListEditor::slotRestoreDefaults(QAbstractButton *button)
{
    if(ui->buttonBox->buttonRole(button) != QDialogButtonBox::ResetRole) {
        return;
    }

    ui->ignoreTableWidget->slotRemoveAllItems();

    setupTableReadOnlyItems();

    if (_ignoreListType == Global) {
        ConfigFile cfgFile;
        ui->ignoreTableWidget->readIgnoreFile(cfgFile.excludeFile(ConfigFile::SystemScope), false);
        return;
    }

    ui->ignoreTableWidget->readIgnoreFile(_ignoreFile);
}

void IgnoreListEditor::setupUi()
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);

    setupTableReadOnlyItems();
    ui->ignoreTableWidget->readIgnoreFile(_ignoreFile);

    connect(this, &QDialog::accepted, this, &IgnoreListEditor::slotSaveIgnoreList);
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &IgnoreListEditor::slotRestoreDefaults);

    ui->syncHiddenFilesCheckBox->setChecked(!FolderMan::instance()->ignoreHiddenFiles());
}

void IgnoreListEditor::setupTableReadOnlyItems()
{
    if (_ignoreListType != Global) {
        return;
    }

    ui->ignoreTableWidget->addPattern(".csync_journal.db*", /*deletable=*/false, /*readonly=*/true);
    ui->ignoreTableWidget->addPattern("._sync_*.db*", /*deletable=*/false, /*readonly=*/true);
    ui->ignoreTableWidget->addPattern(".sync_*.db*", /*deletable=*/false, /*readonly=*/true);
}

} // namespace OCC
