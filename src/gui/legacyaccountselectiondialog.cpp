
/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "legacyaccountselectiondialog.h"
#include "whitelabeltheme.h"
#include "buttonStyle.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

namespace OCC
{

LegacyAccountSelectionDialog::LegacyAccountSelectionDialog(const QVector<AccountItem> &accounts, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Legacy import"));
    setPalette(QPalette(QPalette::Window, WLTheme.dialogBackgroundColor()));

    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Select the accounts to import from the legacy configuration:"), this));

    for (const auto &account : accounts) {
        auto checkbox = new QCheckBox(account.label, this);
        checkbox->setChecked(true);
        layout->addWidget(checkbox);
        _checkBoxes.insert(account.id, checkbox);
    }

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setProperty("buttonStyle", QVariant::fromValue(ButtonStyleName::Primary));
    buttonBox->button(QDialogButtonBox::Cancel)->setProperty("buttonStyle", QVariant::fromValue(ButtonStyleName::Secondary));


    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    customizeStyle();
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

void LegacyAccountSelectionDialog::customizeStyle()
{
    this->setStyleSheet(
        QStringLiteral("QDialog { background-color: %1; }"
                       " QLabel { %2 }"
                       " QCheckBox { color: %3; %2 }")
            .arg(
                WLTheme.dialogBackgroundColor(),
                WLTheme.fontConfigurationCss(
                    WLTheme.settingsFont(),
                    WLTheme.settingsTextSize(),
                    WLTheme.settingsTextWeight(),
                    WLTheme.titleColor()
                ),
                WLTheme.black()
            )
    ); 
}

} // namespace OCC

