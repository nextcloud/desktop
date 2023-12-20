/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
