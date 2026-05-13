/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingspageutils.h"

#include <QCheckBox>
#include <QSizePolicy>
#include <QString>
#include <QWidget>

namespace OCC::SettingsPageUtils {

void configureSwitchRows(QWidget *parent)
{
    const auto switchStyle = QStringLiteral(
        "QCheckBox {"
        " padding: 8px 0px;"
        " border-bottom: 1px solid palette(mid);"
        " spacing: 8px;"
        "}"
        "QCheckBox::indicator {"
        " width: 34px;"
        " height: 18px;"
        " border-radius: 9px;"
        " background: palette(mid);"
        "}"
        "QCheckBox::indicator:checked {"
        " background: palette(highlight);"
        "}"
        "QCheckBox::indicator:disabled {"
        " background: palette(button);"
        "}"
    );

    const auto switchRows = parent->findChildren<QCheckBox *>();
    for (auto *switchRow : switchRows) {
        switchRow->setLayoutDirection(Qt::RightToLeft);
        switchRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        switchRow->setStyleSheet(switchStyle);
    }
}

} // namespace OCC::SettingsPageUtils
