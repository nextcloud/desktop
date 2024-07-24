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
#include "gui/qmlutils.h"

#include "libsync/account.h"
#include "libsync/graphapi/space.h"

#include <QSortFilterProxyModel>
#include <QWidget>

namespace Ui {
class SpacesBrowser;
}

namespace OCC::Spaces {
class SpacesModel;

class SpacesBrowser : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QSortFilterProxyModel *model MEMBER _sortModel READ model CONSTANT)
    Q_PROPERTY(GraphApi::Space *currentSpace MEMBER _currentSpace READ currentSpace NOTIFY currentSpaceChanged)
    QML_ELEMENT
    QML_UNCREATABLE("C++ only")
    OC_DECLARE_WIDGET_FOCUS
public:
    explicit SpacesBrowser(QWidget *parent = nullptr);
    ~SpacesBrowser();

    void setAccount(OCC::AccountPtr acc);

    GraphApi::Space *currentSpace();

    QSortFilterProxyModel *model();

Q_SIGNALS:
    void currentSpaceChanged(GraphApi::Space *space);

private:
    ::Ui::SpacesBrowser *ui;

    OCC::AccountPtr _acc;
    SpacesModel *_model;
    QSortFilterProxyModel *_sortModel;
    GraphApi::Space *_currentSpace = nullptr;
};

}
