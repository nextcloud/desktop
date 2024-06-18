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
#include "spacesbrowser.h"
#include "ui_spacesbrowser.h"

#include "spacesmodel.h"

#include "gui/accountmanager.h"
#include "gui/spaces/spaceimageprovider.h"
#include "resources/resources.h"

#include <QMenu>
#include <QQmlContext>
#include <QSortFilterProxyModel>

using namespace OCC;
using namespace OCC::Spaces;

SpacesBrowser::SpacesBrowser(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SpacesBrowser)
{
    ui->setupUi(this);
    _model = new SpacesModel(this);


    _sortModel = new QSortFilterProxyModel(this);
    _sortModel->setFilterRole(static_cast<int>(SpacesModel::Roles::Enabled));
    _sortModel->setSourceModel(_model);
    _sortModel->setSortRole(static_cast<int>(SpacesModel::Roles::Priority));
    _sortModel->sort(0, Qt::DescendingOrder);

    QmlUtils::initQuickWidget(ui->quickWidget, QUrl(QStringLiteral("qrc:/qt/qml/org/ownCloud/gui/spaces/qml/SpacesView.qml")), this);
}

SpacesBrowser::~SpacesBrowser()
{
    delete ui;
}

void SpacesBrowser::setAccount(OCC::AccountPtr acc)
{
    _acc = acc;
    if (acc) {
        _model->setSpacesManager(acc->spacesManager());
        ui->quickWidget->engine()->addImageProvider(QStringLiteral("space"), new Spaces::SpaceImageProvider(acc));
    }
}

GraphApi::Space *SpacesBrowser::currentSpace()
{
    return _currentSpace;
}

QSortFilterProxyModel *SpacesBrowser::model()
{
    return _sortModel;
}
