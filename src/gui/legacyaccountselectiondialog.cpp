
/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "legacyaccountselectiondialog.h"

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

    for (const auto &acc : accounts) {
        auto cb = new QCheckBox(acc.label, this);
        cb->setChecked(true);
        layout->addWidget(cb);
        _checkBoxes.insert(acc.id, cb);
    }

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

QStringList LegacyAccountSelectionDialog::selectedAccountIds() const
{
    QStringList res;
    for (auto it = _checkBoxes.constBegin(); it != _checkBoxes.constEnd(); ++it) {
        if (it.value()->isChecked()) {
            res.push_back(it.key());
        }
    }
    return res;
}

} // namespace OCC

