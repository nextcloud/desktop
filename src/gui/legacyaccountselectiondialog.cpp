
/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "legacyaccountselectiondialog.h"

#ifndef DISABLE_ACCOUNT_MIGRATION

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace OCC
{

LegacyAccountSelectionDialog::LegacyAccountSelectionDialog(const QVector<AccountItem> &accounts, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Legacy import"));

    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Select the accounts to import from the legacy configuration:"), this));

    for (const auto &account : accounts) {
        auto checkbox = new QCheckBox(account.label, this);
        checkbox->setChecked(true);
        layout->addWidget(checkbox);
        _checkBoxes.insert(account.id, checkbox);
    }

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

QStringList LegacyAccountSelectionDialog::selectedAccountIds() const
{
    QStringList selectedAccount;
    for (auto it = _checkBoxes.constBegin(); it != _checkBoxes.constEnd(); ++it) {
        if (it.value()->isChecked()) {
            selectedAccount.push_back(it.key());
        }
    }
    return selectedAccount;
}

} // namespace OCC

#endif // DISABLE_ACCOUNT_MIGRATION

