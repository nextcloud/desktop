/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSPANELSTYLE_H
#define SETTINGSPANELSTYLE_H

class QWidget;
class QLabel;

namespace OCC::SettingsPanelStyle {

void apply(QWidget *root);
void applyManagedLabelStyle(QLabel *label);

} // namespace OCC::SettingsPanelStyle

#endif // SETTINGSPANELSTYLE_H
