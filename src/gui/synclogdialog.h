/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 * Copyright (C) 2015 by Klaas Freitag <freitag@owncloud.com>
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

#ifndef SyncLogDialog_H
#define SyncLogDialog_H

#include "protocolwidget.h"

#include <QDialog>

namespace OCC {


namespace Ui {
    class SyncLogDialog;
}


/**
 * @brief The SyncLogDialog class
 * @ingroup gui
 */
class SyncLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncLogDialog(QWidget *parent = 0);
    ~SyncLogDialog();

private slots:

private:
    Ui::SyncLogDialog *_ui;
};
}

#endif // SyncLogDialog_H
