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

#include "nmcfolderwizardsourcepage.h"
#include "theme.h"

namespace OCC {

NMCFolderWizardSourcePage::NMCFolderWizardSourcePage()
    :FolderWizardSourcePage()
{

}

void NMCFolderWizardSourcePage::setDefaultSettings()
{
    groupBox->setVisible(false);
}

void NMCFolderWizardSourcePage::changeLayout()
{
    gridLayout_2->setMargin(0);

    QLabel *stepLabel = new QLabel();
    stepLabel->setText(QCoreApplication::translate("", "ADD_LIVE_BACKUP_HEADLINE"));
    stepLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stepLabel->setStyleSheet("QLabel{color: black; font-size: 13px; font-weight: bold;}");

    gridLayout_2->addWidget(stepLabel, 0, 0, Qt::AlignTop | Qt::AlignLeft);

    QWidget *mainLayoutWidget = new QWidget();
    mainLayoutWidget->setStyleSheet("");

    auto *whiteLayout = new QGridLayout;
    mainLayoutWidget->setObjectName("mainLayoutWidget");
    mainLayoutWidget->setStyleSheet("QWidget#mainLayoutWidget { background-color: white; border-radius: 4px;}");
    mainLayoutWidget->setLayout(whiteLayout);

    QLabel *textLabel = new QLabel();
    textLabel->setText(QCoreApplication::translate("", "ADD_LIVE_BACKUP_PAGE1_DESCRIPTION"));
    textLabel->setWordWrap(true);
    textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    whiteLayout->addWidget(textLabel, 0, 0);

    gridLayout_2->removeWidget(localFolderLineEdit);
    localFolderLineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    whiteLayout->addWidget(localFolderLineEdit, 1, 0);

    localFolderChooseBtn->setAutoDefault(true);
    localFolderChooseBtn->setDefault(true);
    localFolderChooseBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    //localFolderChooseBtn->setFixedHeight(10);
    whiteLayout->addWidget(localFolderChooseBtn, 1, 1);

    gridLayout_2->addWidget(mainLayoutWidget, 1, 0, 1, 3);

    gridLayout_2->removeWidget(warnLabel);
    gridLayout_2->addWidget(warnLabel, 2, 0, 1, 3);

    warnLabel->setStyleSheet("border: 0px; border-radius: 4px; background-color: #fee2d0");

    gridLayout_2->removeItem(verticalSpacer);
    gridLayout_2->addItem(verticalSpacer, 3, 0, 1, 3);
}

} // end namespace



