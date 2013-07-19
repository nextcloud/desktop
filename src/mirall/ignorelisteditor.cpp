/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "mirall/mirallconfigfile.h"

#include "ignorelisteditor.h"
#include "ui_ignorelisteditor.h"

#include <QFile>
#include <QDir>
#include <QListWidget>
#include <QListWidgetItem>
#include <QColorGroup>
#include <QMessageBox>
#include <QInputDialog>

namespace Mirall {

IgnoreListEditor::IgnoreListEditor(QDialog *parent) :
    QDialog(parent),
    ui(new Ui::IgnoreListEditor)
{
    ui->setupUi(this);

    ui->descriptionLabel->setText(tr("Files matching the following patterns will not be synchronized:"));

    MirallConfigFile cfgFile;
    readIgnoreFile(cfgFile.excludeFile(MirallConfigFile::SystemScope), true);
    readIgnoreFile(cfgFile.excludeFile(MirallConfigFile::UserScope), false);

    connect(this, SIGNAL(accepted()), SLOT(slotUpdateLocalIgnoreList()));
    ui->removePushButton->setEnabled(false);
    connect(ui->listWidget, SIGNAL(itemSelectionChanged()), SLOT(slotItemSelectionChanged()));
    connect(ui->listWidget, SIGNAL(itemActivated(QListWidgetItem*)), SLOT(slotItemChanged(QListWidgetItem*)));
    connect(ui->removePushButton, SIGNAL(clicked()), SLOT(slotRemoveCurrentItem()));
    connect(ui->addPushButton, SIGNAL(clicked()), SLOT(slotAddPattern()));
    connect(ui->listWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(slotEditPattern(QListWidgetItem*)));
}

static void setupItemFlags(QListWidgetItem* item)
{
    item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
}

IgnoreListEditor::~IgnoreListEditor()
{
    delete ui;
}

void IgnoreListEditor::slotItemSelectionChanged()
{
    QListWidgetItem *item = ui->listWidget->currentItem();
    if (!item) {
        ui->removePushButton->setEnabled(false);
        return;
    }

    bool enable = item->flags() & Qt::ItemIsEnabled;
    ui->removePushButton->setEnabled(enable);
}

void IgnoreListEditor::slotRemoveCurrentItem()
{
    delete ui->listWidget->currentItem();
}

void IgnoreListEditor::slotUpdateLocalIgnoreList()
{
    MirallConfigFile cfgFile;
    QString ignoreFile = cfgFile.excludeFile(MirallConfigFile::UserScope);
    QFile ignores(ignoreFile);
    if (ignores.open(QIODevice::WriteOnly)) {
        for(int i = 0; i < ui->listWidget->count(); ++i) {
            QListWidgetItem *item = ui->listWidget->item(i);
            if (item->flags() & Qt::ItemIsEnabled) {
                ignores.write(item->text().toUtf8()+'\n');
            }
        }
    } else {
        QMessageBox::warning(this, tr("Could not open file"),
                             tr("Cannot write changes to '%1'.").arg(ignoreFile));
    }
}

void IgnoreListEditor::slotAddPattern()
{
    QString pattern = QInputDialog::getText(this, tr("Add Ignore Pattern"), tr("Add a new ignore pattern:"));
    QListWidgetItem *item = new QListWidgetItem(pattern);
    setupItemFlags(item);
    ui->listWidget->addItem(item);
    ui->listWidget->scrollToItem(item);
}

void IgnoreListEditor::slotEditPattern(QListWidgetItem *item)
{
    if (!(item->flags() & Qt::ItemIsEnabled))
        return;

    QString pattern = QInputDialog::getText(this, tr("Add Ignore Pattern"),
                                            tr("Add a new ignore pattern:"),
                                            QLineEdit::Normal, item->text());
    if (!pattern.isEmpty()) {
        item->setText(pattern);
    }
}

void IgnoreListEditor::readIgnoreFile(const QString &file, bool readOnly)
{

    MirallConfigFile cfgFile;
    const QString disabledTip(tr("This entry is provided by the system at '%1' "
                                 "and cannot be modified in this view.")
            .arg(QDir::toNativeSeparators(cfgFile.excludeFile(MirallConfigFile::SystemScope))));

    QFile ignores(file);
    if (ignores.open(QIODevice::ReadOnly)) {
        while (!ignores.atEnd()) {
            QString line = QString::fromUtf8(ignores.readLine());
            line.chop(1);
            if (!line.isEmpty() && !line.startsWith("#")) {
                QListWidgetItem *item = new QListWidgetItem(line);
                if (readOnly) {
                    setupItemFlags(item);
                    item->setFlags(item->flags() ^ Qt::ItemIsEnabled);
                    item->setToolTip(disabledTip);
                }
                ui->listWidget->addItem(item);
            }
        }
    }
}

} // namespace Mirall
