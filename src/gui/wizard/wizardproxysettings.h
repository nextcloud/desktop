/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>

#include "ui_proxysettings.h"

class WizardProxySettings : public QDialog
{
    Q_OBJECT
public:
    explicit WizardProxySettings(QWidget *parent = nullptr);

private:
    Ui_ProxySettings _ui{};

};
