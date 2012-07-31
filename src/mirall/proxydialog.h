/*
 * Copyright (C) by Thomas Mueller <thomas.mueller@tmit.eu>
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
#ifndef PROXYDIALOG_H
#define PROXYDIALOG_H

#include <QtCore>
#include <QDialog>

#include "ui_proxydialog.h"

namespace Mirall
{

    class ProxyDialog : public QDialog, public Ui::proxyDialog
{
Q_OBJECT

public:
    explicit ProxyDialog( QWidget* parent = 0 );
    ~ProxyDialog() {}

    void saveSettings();

private slots:
    void on_buttonBox_accepted();
    void on_manualProxyRadioButton_toggled(bool checked);
    void on_authRequiredcheckBox_stateChanged(int );
};

} // end namespace

#endif // PROXYDIALOG_H
