/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#pragma once

#include "account.h"

#include <QWidget>

namespace Ui {
class SpacesBrowser;
}

namespace OCC::Spaces {
class SpacesModel;

class SpacesBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit SpacesBrowser(QWidget *parent = nullptr);
    ~SpacesBrowser();

    void setAccount(OCC::AccountPtr acc);

    QModelIndex currentSpace();

Q_SIGNALS:
    void selectionChanged();

private:
    Ui::SpacesBrowser *ui;

    OCC::AccountPtr _acc;
    SpacesModel *_model;
};

}