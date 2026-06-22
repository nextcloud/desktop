/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include <QDialog>
#include <QScopedPointer>

namespace OCC {

namespace Ui {
    class VfsDownloadErrorDialog;
}

class VfsDownloadErrorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VfsDownloadErrorDialog(const QString &fileName, const QString &errorMessage, QWidget *parent = nullptr);
    ~VfsDownloadErrorDialog() override;

private:
    QScopedPointer<Ui::VfsDownloadErrorDialog> _ui;
};
}
