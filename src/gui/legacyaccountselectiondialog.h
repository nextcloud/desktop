
/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#if !DISABLE_ACCOUNT_MIGRATION

#include <QDialog>
#include <QMap>
#include <QStringList>

class QCheckBox;

namespace OCC
{

class LegacyAccountSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    struct AccountItem {
        QString id;
        QString label;
    };

    explicit LegacyAccountSelectionDialog(const QVector<AccountItem> &accounts, QWidget *parent = nullptr);

    [[nodiscard]] QStringList selectedAccountIds() const;

private:
    QMap<QString, QCheckBox *> _checkBoxes;
};

} // namespace OCC

#endif // !DISABLE_ACCOUNT_MIGRATION

