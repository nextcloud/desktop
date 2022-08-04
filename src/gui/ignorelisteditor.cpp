/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "ignorelisteditor.h"
#include "ui_ignorelisteditor.h"

#include "configfile.h"
#include "gui/folderman.h"
#include "gui/guiutility.h"

#include <QFile>
#include <QDir>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QInputDialog>

namespace OCC {

static const int patternCol = 0;
static const int deletableCol = 1;
static const int skippedLinesRole = Qt::UserRole;
static const int isGlobalRole = Qt::UserRole + 1;

IgnoreListEditor::IgnoreListEditor(QWidget *parent)
    : QDialog(parent, Qt::Sheet)
    , ui(new Ui::IgnoreListEditor)
{
    Utility::setModal(this);
    ui->setupUi(this);

    ConfigFile cfgFile;
    readOnlyTooltip = tr("This entry is provided by the system at '%1' "
                         "and cannot be modified in this view.")
                          .arg(QDir::toNativeSeparators(cfgFile.excludeFile(ConfigFile::SystemScope)));

    addPattern(".csync_journal.db*", /*deletable=*/false, /*readonly=*/true, /*global=*/true);
    addPattern("._sync_*.db*", /*deletable=*/false, /*readonly=*/true, /*global=*/true);
    addPattern(".sync_*.db*", /*deletable=*/false, /*readonly=*/true, /*global=*/true);
    readIgnoreFile(cfgFile.excludeFile(ConfigFile::SystemScope), /*global=*/true);
    readIgnoreFile(cfgFile.excludeFile(ConfigFile::UserScope), /*global=*/false);

    connect(this, &QDialog::accepted, this, &IgnoreListEditor::slotUpdateLocalIgnoreList);
    ui->removePushButton->setEnabled(false);
    connect(ui->tableWidget, &QTableWidget::itemSelectionChanged, this, &IgnoreListEditor::slotItemSelectionChanged);
    connect(ui->removePushButton, &QAbstractButton::clicked, this, &IgnoreListEditor::slotRemoveCurrentItem);
    connect(ui->addPushButton, &QAbstractButton::clicked, this, &IgnoreListEditor::slotAddPattern);

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(patternCol, QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setVisible(false);
}

IgnoreListEditor::~IgnoreListEditor()
{
    delete ui;
}

void IgnoreListEditor::slotItemSelectionChanged()
{
    QTableWidgetItem *item = ui->tableWidget->currentItem();
    if (!item) {
        ui->removePushButton->setEnabled(false);
        return;
    }

    bool enable = item->flags() & Qt::ItemIsEnabled;
    ui->removePushButton->setEnabled(enable);
}

void IgnoreListEditor::slotRemoveCurrentItem()
{
    ui->tableWidget->removeRow(ui->tableWidget->currentRow());
}

void IgnoreListEditor::slotUpdateLocalIgnoreList()
{
    ConfigFile cfgFile;
    QString ignoreFile = cfgFile.excludeFile(ConfigFile::UserScope);
    QFile ignores(ignoreFile);
    if (ignores.open(QIODevice::WriteOnly)) {
        for (int row = 0; row < ui->tableWidget->rowCount(); ++row) {
            QTableWidgetItem *patternItem = ui->tableWidget->item(row, patternCol);
            QTableWidgetItem *deletableItem = ui->tableWidget->item(row, deletableCol);

            if (patternItem->data(isGlobalRole).toBool())
                continue;

            const auto &skippedLines = patternItem->data(skippedLinesRole).toStringList();
            for (const auto &line : skippedLines)
                ignores.write(line.toUtf8() + '\n');

            QByteArray prepend;
            if (deletableItem->checkState() == Qt::Checked) {
                prepend = "]";
            } else if (patternItem->text().startsWith('#')) {
                prepend = "\\";
            }
            ignores.write(prepend + patternItem->text().toUtf8() + '\n');
        }
    } else {
        QMessageBox::warning(this, tr("Could not open file"),
            tr("Cannot write changes to '%1'.").arg(ignoreFile));
    }
    ignores.close(); //close the file before reloading stuff.

    FolderMan *folderMan = FolderMan::instance();

    // We need to force a remote discovery after a change of the ignore list.
    // Otherwise we would not download the files/directories that are no longer
    // ignored (because the remote etag did not change)   (issue #3172)
    for (auto *folder : folderMan->folders()) {
        if (folder->isReady()) {
            folder->journalDb()->forceRemoteDiscoveryNextSync();
            folder->reloadExcludes();
            folder->slotNextSyncFullLocalDiscovery();
            folderMan->scheduleFolder(folder);
        }
    }
}

void IgnoreListEditor::slotAddPattern()
{
    bool okClicked;
    QString pattern = QInputDialog::getText(this, tr("Add Ignore Pattern"),
        tr("Add a new ignore pattern:"),
        QLineEdit::Normal, QString(), &okClicked);

    if (!okClicked || pattern.isEmpty())
        return;

    addPattern(pattern, /*deletable=*/false, /*readonly=*/false, /*global=*/false);
    ui->tableWidget->scrollToBottom();
}

void IgnoreListEditor::readIgnoreFile(const QString &file, bool global)
{
    QFile ignores(file);
    if (!ignores.open(QIODevice::ReadOnly))
        return;

    QStringList skippedLines;
    bool readonly = global; // global ignores default to read-only

    while (!ignores.atEnd()) {
        QString line = QString::fromUtf8(ignores.readLine());
        line.chop(1);

        // Collect empty lines and comments, we want to preserve them
        if (line.isEmpty() || line.startsWith("#")) {
            skippedLines.append(line);
            // A directive that prohibits editing in the ui
            if (line == "#!readonly")
                readonly = true;
            continue;
        }

        bool deletable = false;
        if (line.startsWith(']')) {
            deletable = true;
            line = line.mid(1);
        }

        // Add and reset
        addPattern(line, deletable, readonly, global, skippedLines);
        skippedLines.clear();
        readonly = global;
    }
}

int IgnoreListEditor::addPattern(const QString &pattern, bool deletable, bool readOnly, bool global, const QStringList &skippedLines)
{
    int newRow = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(newRow + 1);

    QTableWidgetItem *patternItem = new QTableWidgetItem;
    patternItem->setText(pattern);
    patternItem->setData(skippedLinesRole, skippedLines);
    patternItem->setData(isGlobalRole, global);
    ui->tableWidget->setItem(newRow, patternCol, patternItem);

    QTableWidgetItem *deletableItem = new QTableWidgetItem;
    deletableItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    deletableItem->setCheckState(deletable ? Qt::Checked : Qt::Unchecked);
    ui->tableWidget->setItem(newRow, deletableCol, deletableItem);

    if (readOnly) {
        patternItem->setFlags(patternItem->flags() ^ Qt::ItemIsEnabled);
        patternItem->setToolTip(readOnlyTooltip);
        deletableItem->setFlags(deletableItem->flags() ^ Qt::ItemIsEnabled);
    }

    return newRow;
}

} // namespace OCC
