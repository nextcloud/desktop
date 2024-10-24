/*
 * Copyright (C) by Eugen Fischer
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
#include "nmcgui/nmcselectivesyncdialog.h"
#include "QtWidgets/qapplication.h"
#include "QtWidgets/qboxlayout.h"
#include "QtWidgets/qlabel.h"


namespace OCC {

NMCSelectiveSyncWidget::NMCSelectiveSyncWidget(AccountPtr account, QWidget *parent)
    : SelectiveSyncWidget(account, parent)
{
    _layout->removeWidget(_folderTree);
    _layout->removeWidget(_header);

    _header->setText(QCoreApplication::translate("", "ADD_LIVE_BACKUP_PAGE3_DESCRIPTION"));

    QHBoxLayout *hLayout = new QHBoxLayout();
    hLayout->setSpacing(8);

    QLabel *icon = new QLabel(this);
    icon->setFixedSize(18, 18);
    icon->setPixmap(QIcon(QLatin1String(":/client/theme/NMCIcons/applicationLogo.svg")).pixmap(18, 18));
    hLayout->addWidget(icon);

    QLabel *stepLabel = new QLabel(this);
    stepLabel->setText(QCoreApplication::translate("", "ADD_LIVE_BACKUP_HEADLINE"));
    stepLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stepLabel->setStyleSheet("QLabel{color: black; font-size: 13px; font-weight: bold;}");
    hLayout->addWidget(stepLabel);

    hLayout->setContentsMargins(0,0,0,0);

    QWidget *hContainer = new QWidget(this);
    hContainer->setLayout(hLayout);

    _layout->addWidget(hContainer);

    QVBoxLayout *vLayout = new QVBoxLayout();
    vLayout->setSpacing(8);

    vLayout->addWidget(_header);
    vLayout->addWidget(_folderTree);
    _folderTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFrame *vContainer = new QFrame(this);
    vContainer->setLayout(vLayout);
    vContainer->setObjectName("whiteBackgroundLayout");
    vContainer->setStyleSheet("QFrame#whiteBackgroundLayout { background-color: white; border-radius: 4px;}");

    _layout->addWidget(vContainer);
}

}
