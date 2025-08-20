/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wizardproxysettings.h"

WizardProxySettings::WizardProxySettings(QWidget *parent)
    : QDialog(parent)
{
    _ui.setupUi(this);
    setWindowModality(Qt::WindowModal);
}
