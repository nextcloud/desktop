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
#include "nmcfolderwizardtargetpage.h"
#include "theme.h"


namespace OCC {

NMCFolderWizardTargetPage::NMCFolderWizardTargetPage()
    :Ui::FolderWizardTargetPage()
{
}

void NMCFolderWizardTargetPage::setDefaultSettings()
{
    warnFrame->setVisible(false);
    groupBox->setVisible(false);
}

void NMCFolderWizardTargetPage::setLayout()
{
    gridLayout_6->setMargin(0);

    QLabel *stepLabel = new QLabel();
    stepLabel->setText(QCoreApplication::translate("", "ADD_LIVE_BACKUP_HEADLINE"));
    stepLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    stepLabel->setStyleSheet("QLabel{color: black; font-size: 13px; font-weight: bold;}");

    gridLayout_6->addWidget(stepLabel, 0, 0, Qt::AlignTop | Qt::AlignLeft);

    QGridLayout *hLayout = new QGridLayout();

    QWidget *hLayoutWidget = new QWidget();
    hLayoutWidget->setLayout(hLayout);
    hLayoutWidget->setObjectName("whiteBackgroundLayout");
    hLayoutWidget->setStyleSheet("QWidget#whiteBackgroundLayout { background-color: white; border-radius: 4px;}");

    gridLayout_6->removeWidget(folderTreeWidget);
    gridLayout_6->removeWidget(refreshButton);
    gridLayout_6->removeWidget(addFolderButton);

    QLabel *textLabel2 = new QLabel();
    textLabel2->setText(QCoreApplication::translate("", "ADD_LIVE_BACKUP_PAGE2_DESCRIPTION"));
    textLabel2->setWordWrap(true);
    textLabel2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hLayout->addWidget(textLabel2, 0, 0, 1, 3);

    hLayout->addWidget(folderTreeWidget, 1, 0, 3, 1);

    addFolderButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hLayout->addWidget(addFolderButton, 1, 1, 1, 1);

    refreshButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    refreshButton->setFixedSize(addFolderButton->sizeHint());
    hLayout->addWidget(refreshButton, 2, 1, 1, 1);

    folderEntry->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hLayout->addWidget(folderEntry, 4, 0, 1, 3);

    gridLayout_6->addWidget(hLayoutWidget, 4, 0, 1, 3);
    gridLayout_6->addWidget(warnFrame, 5, 0, 1, 3 );
    warnFrame->setStyleSheet("border: 0px; border-radius: 4px; background-color: #fee2d0");
}

} // end namespace
