/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ignorelisttablewidget.h"
#include "ui_ignorelisttablewidget.h"

#include "folderman.h"

#include <QFile>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovider.h"
#include "macOS/fileproviderxpc.h"
#endif

namespace OCC {

static constexpr int patternCol = 0;
static constexpr int deletableCol = 1;
static constexpr int readOnlyRows = 3;

Q_LOGGING_CATEGORY(lcIgnoreListTableWidget, "nextcloud.gui.ignorelisttablewidget", QtInfoMsg)

IgnoreListTableWidget::IgnoreListTableWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::IgnoreListTableWidget)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->setupUi(this);

    ui->descriptionLabel->setText(tr("Files or folders matching a pattern will not be synchronized.\n\n"
                                     "Items where deletion is allowed will be deleted if they prevent a "
                                     "directory from being removed. "
                                     "This is useful for meta data."));

    ui->removePushButton->setEnabled(false);
    connect(ui->tableWidget, &QTableWidget::itemSelectionChanged,
            this, &IgnoreListTableWidget::slotItemSelectionChanged);
    connect(ui->removePushButton, &QAbstractButton::clicked,
            this, &IgnoreListTableWidget::slotRemoveCurrentItem);
    connect(ui->addPushButton, &QAbstractButton::clicked,
            this, &IgnoreListTableWidget::slotAddPattern);
    connect(ui->removeAllPushButton, &QAbstractButton::clicked,
            this, &IgnoreListTableWidget::slotRemoveAllItems);

    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(patternCol, QHeaderView::Stretch);
    ui->tableWidget->verticalHeader()->setVisible(false);
}

IgnoreListTableWidget::~IgnoreListTableWidget()
{
    delete ui;
}

void IgnoreListTableWidget::slotItemSelectionChanged()
{
    const auto item = ui->tableWidget->currentItem();
    if (!item) {
        ui->removePushButton->setEnabled(false);
        return;
    }

    const auto enable = item->flags() & Qt::ItemIsEnabled;
    ui->removePushButton->setEnabled(enable);
}

void IgnoreListTableWidget::slotRemoveCurrentItem()
{
    ui->tableWidget->removeRow(ui->tableWidget->currentRow());
    if(ui->tableWidget->rowCount() == readOnlyRows)
        ui->removeAllPushButton->setEnabled(false);
}

void IgnoreListTableWidget::slotRemoveAllItems()
{
    ui->tableWidget->setRowCount(0);
}

void IgnoreListTableWidget::slotWriteIgnoreFile(const QString &file)
{
    QFile ignores(file);

    if (!ignores.open(QIODevice::WriteOnly)) {
        qCWarning(lcIgnoreListTableWidget).nospace() << "failed to write ignore list"
            << " file=" << file
            << " errorString=" << ignores.errorString();
        QMessageBox::warning(this,
                             tr("Could not open file"),
                             tr("Cannot write changes to \"%1\".").arg(file));

        ignores.close();
        return;
    }

    // rewrite the whole file since the user can also remove system patterns
    ignores.resize(0);

    for (auto row = 0; row < ui->tableWidget->rowCount(); ++row) {
        const auto patternItem = ui->tableWidget->item(row, patternCol);
        if (!(patternItem->flags() & Qt::ItemIsEnabled)) {
            // skip read-only patterns
            continue;
        }
        const auto deletableItem = ui->tableWidget->item(row, deletableCol);

        QByteArray prefix;
        if (deletableItem && deletableItem->checkState() == Qt::Checked) {
            prefix = "]";
        } else if (patternItem->text().startsWith('#')) {
            prefix = "\\";
        }
        ignores.write(prefix + patternItem->text().toUtf8() + '\n');
    }
    ignores.close(); // close the file before reloading stuff.

    const auto folderMan = FolderMan::instance();

    // We need to force a remote discovery after a change of the ignore list.
    // Otherwise we would not download the files/directories that are no longer
    // ignored (because the remote etag did not change)   (issue #3172)
    for (const auto folder : std::as_const(folderMan->map())) {
        folder->journalDb()->forceRemoteDiscoveryNextSync();
        folderMan->scheduleFolder(folder);
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    Mac::FileProvider::instance()->xpc()->setIgnoreList();
#endif
}

void IgnoreListTableWidget::slotAddPattern()
{
    auto okClicked = false;
    const auto pattern = QInputDialog::getText(this,
                                               tr("Add Ignore Pattern"),
                                               tr("Add a new ignore pattern:"),
                                               QLineEdit::Normal,
                                               {},
                                               &okClicked);

    if (!okClicked || pattern.isEmpty()) {
        return;
    }

    addPattern(pattern, false, false);
    ui->tableWidget->scrollToBottom();
}

void IgnoreListTableWidget::readIgnoreFile(const QString &file, const bool readOnly)
{
    QFile ignores(file);

    if (ignores.open(QIODevice::ReadOnly)) {
        while (!ignores.atEnd()) {
            auto line = QString::fromUtf8(ignores.readLine());
            line.chop(1);
            if (line == QStringLiteral("\\#*#")) {
                continue;
            }

            if (!line.isEmpty() && !line.startsWith("#")) {
                auto deletable = false;
                if (line.startsWith(']')) {
                    deletable = true;
                    line = line.mid(1);
                }
                addPattern(line, deletable, readOnly);
            }
        }
    }
}

int IgnoreListTableWidget::addPattern(const QString &pattern, const bool deletable, const bool readOnly)
{
    const auto newRow = ui->tableWidget->rowCount();
    ui->tableWidget->setRowCount(newRow + 1);

    const auto patternItem = new QTableWidgetItem;
    patternItem->setText(pattern);
    ui->tableWidget->setItem(newRow, patternCol, patternItem);

    const auto deletableItem = new QTableWidgetItem;
    deletableItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    deletableItem->setCheckState(deletable ? Qt::Checked : Qt::Unchecked);
    ui->tableWidget->setItem(newRow, deletableCol, deletableItem);

    if (readOnly) {
        patternItem->setFlags(patternItem->flags() ^ Qt::ItemIsEnabled);
        deletableItem->setFlags(deletableItem->flags() ^ Qt::ItemIsEnabled);
    }

    ui->removeAllPushButton->setEnabled(true);

    return newRow;
}

} // namespace OCC
