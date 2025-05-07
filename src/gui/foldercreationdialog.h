/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FOLDERCREATIONDIALOG_H
#define FOLDERCREATIONDIALOG_H

#include <QDialog>

namespace OCC {

namespace Ui {
class FolderCreationDialog;
}

class FolderCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FolderCreationDialog(const QString &destination, QWidget *parent = nullptr);
    ~FolderCreationDialog() override;

signals:
    void folderCreated(const QString &fullFolderPath);

private slots:
    void accept() override;

    void slotNewFolderNameEditTextEdited();

private:
    Ui::FolderCreationDialog *ui;

    QString _destination;
};

}

#endif // FOLDERCREATIONDIALOG_H
