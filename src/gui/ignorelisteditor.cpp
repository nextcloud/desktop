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

#include "configfile.h"

#include "ignorelisteditor.h"
#include "ui_ignorelisteditor.h"

#include <QFile>
#include <QDir>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QInputDialog>

namespace OCC {

IgnoreListEditor::IgnoreListEditor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::IgnoreListEditor)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);

    ui->descriptionLabel->setText(tr("Files or directories matching a pattern will not be synchronized.\n\n"
                                     "Checked items will also be deleted if they prevent a directory from "
                                     "being removed. This is useful for meta data."));

    ConfigFile cfgFile;
    readIgnoreFile(cfgFile.excludeFile(ConfigFile::SystemScope), true);
    readIgnoreFile(cfgFile.excludeFile(ConfigFile::UserScope), false);

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
    item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);
    item->setCheckState(Qt::Unchecked);
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
    ConfigFile cfgFile;
    QString ignoreFile = cfgFile.excludeFile(ConfigFile::UserScope);
    QFile ignores(ignoreFile);
    if (ignores.open(QIODevice::WriteOnly)) {
        for(int i = 0; i < ui->listWidget->count(); ++i) {
            QListWidgetItem *item = ui->listWidget->item(i);
            if (item->flags() & Qt::ItemIsEnabled) {
                QByteArray prepend;
                if (item->checkState() == Qt::Checked) {
                    prepend = "]";
                }
                ignores.write(prepend+item->text().toUtf8()+'\n');
            }
        }
    } else {
        QMessageBox::warning(this, tr("Could not open file"),
                             tr("Cannot write changes to '%1'.").arg(ignoreFile));
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

    QListWidgetItem *item = new QListWidgetItem;
    setupItemFlags(item);
    if (pattern.startsWith("]")) {
        pattern = pattern.mid(1);
        item->setCheckState(Qt::Checked);
    }
    item->setText(pattern);
    ui->listWidget->addItem(item);
    ui->listWidget->scrollToItem(item);
}

void IgnoreListEditor::slotEditPattern(QListWidgetItem *item)
{
    if (!(item->flags() & Qt::ItemIsEnabled))
        return;

    QString pattern = QInputDialog::getText(this, tr("Edit Ignore Pattern"),
                                            tr("Edit ignore pattern:"),
                                            QLineEdit::Normal, item->text());
    if (!pattern.isEmpty()) {
        item->setText(pattern);
    }
}

void IgnoreListEditor::readIgnoreFile(const QString &file, bool readOnly)
{

    ConfigFile cfgFile;
    const QString disabledTip(tr("This entry is provided by the system at '%1' "
                                 "and cannot be modified in this view.")
            .arg(QDir::toNativeSeparators(cfgFile.excludeFile(ConfigFile::SystemScope))));

    QFile ignores(file);
    if (ignores.open(QIODevice::ReadOnly)) {
        while (!ignores.atEnd()) {
            QString line = QString::fromUtf8(ignores.readLine());
            line.chop(1);
            if (!line.isEmpty() && !line.startsWith("#")) {
                QListWidgetItem *item = new QListWidgetItem;
                setupItemFlags(item);
                if (line.startsWith("]")) {
                    line = line.mid(1);
                    item->setCheckState(Qt::Checked);
                }
                item->setText(line);
                if (readOnly) {
                    item->setFlags(item->flags() ^ Qt::ItemIsEnabled);
                    item->setToolTip(disabledTip);
                }
                ui->listWidget->addItem(item);
            }
        }
    }
}

} // namespace OCC
