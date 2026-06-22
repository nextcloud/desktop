/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>

namespace OCC {

namespace Ui {
class PasswordInputDialog;
}

class PasswordInputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordInputDialog(const QString &description, const QString &error, QWidget *parent = nullptr);
    ~PasswordInputDialog() override;

    [[nodiscard]] QString password() const;

private:
    std::unique_ptr<Ui::PasswordInputDialog> _ui;
};

}
