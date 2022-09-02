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
#include "spacespage.h"
#include "graphapi/drives.h"
#include "ui_spacespage.h"

#include <QModelIndex>
#include <QTimer>

using namespace OCC;

SpacesPage::SpacesPage(AccountPtr accountPtr, QWidget *parent)
    : QWizardPage(parent)
    , _ui(new Ui::SpacesPage)
{
    _ui->setupUi(this);

    connect(_ui->widget, &Spaces::SpacesBrowser::selectionChanged, this, &QWizardPage::completeChanged);

    QTimer::singleShot(0, this, [this, accountPtr] {
        auto drive = new OCC::GraphApi::DrivesJob(accountPtr);

        connect(drive, &OCC::GraphApi::DrivesJob::finishedSignal, [drive, accountPtr, this] {
            QList<Spaces::Space> spaces;

            for (const auto &d : drive->drives()) {
                spaces.append(Spaces::Space::fromDrive(d));
            }

            _ui->widget->setItems(accountPtr, spaces);
        });

        drive->start();
    });
}

SpacesPage::~SpacesPage()
{
    delete _ui;
}

bool OCC::SpacesPage::isComplete() const
{
    return _ui->widget->selectedSpace().has_value();
}

std::optional<Spaces::Space> OCC::SpacesPage::selectedSpace() const
{
    const auto &selectedSpace = _ui->widget->selectedSpace();
    Q_ASSERT(selectedSpace.has_value());
    return selectedSpace;
}
