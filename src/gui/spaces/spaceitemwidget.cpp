/*
 * Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#include "spaceitemwidget.h"
#include "ui_spaceitemwidget.h"

#include "gui/guiutility.h"
#include "networkjobs.h"

namespace OCC::Spaces {

SpaceItemWidget::SpaceItemWidget(const AccountPtr &accountPtr, const Space &space, QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::SpaceItemWidget)
    , _space(space)
{
    _ui->setupUi(this);

    static const QSize ImageSizeC(128, 128);
    static const auto PlaceholderImageC = OCC::Utility::getCoreIcon(QStringLiteral("th-large")).pixmap(ImageSizeC);

    // clear the placeholder
    _ui->imageLabel->setText(QString());

    // we want to make sure all entries in the browser have the same height
    _ui->imageLabel->setMinimumSize(ImageSizeC);

    // showing the placeholder until the real image has been loaded (if available) should improve the UX
    _ui->imageLabel->setPixmap(PlaceholderImageC);

    if (!space.imageUrl().isEmpty()) {
        auto job = new OCC::SimpleNetworkJob(accountPtr, space.imageUrl(), {}, "GET", {}, {}, nullptr);

        connect(job, &OCC::SimpleNetworkJob::finishedSignal, this, [job, this] {
            QPixmap pixmap;
            qDebug() << "loading pixmap from space image:" << pixmap.loadFromData(job->reply()->readAll());
            const auto scaledPixmap = pixmap.scaled(ImageSizeC, Qt::KeepAspectRatio);
            _ui->imageLabel->setPixmap(scaledPixmap);
        });

        job->start();
    }

    _ui->titleLabel->setText(space.title());

    if (space.subtitle().isEmpty()) {
        _ui->subtitleLabel->hide();
    } else {
        _ui->subtitleLabel->setText(space.subtitle());
    }

    if (space.webUrl().isEmpty()) {
        _ui->openBrowserButton->setEnabled(false);
    }

    connect(_ui->openBrowserButton, &QPushButton::clicked, this, [this]() {
        Utility::openBrowser(_space.webUrl(), this);
    });

    connect(_ui->radioButton, &QRadioButton::clicked, this, &SpaceItemWidget::radioButtonClicked);
}

SpaceItemWidget::~SpaceItemWidget()
{
    delete _ui;
}

const Space &SpaceItemWidget::space() const
{
    return _space;
}

void SpaceItemWidget::setRadioButtonChecked(bool checked)
{
    _ui->radioButton->setChecked(checked);
}

} // OCC::Spaces
