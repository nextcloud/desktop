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

#include "nmcgui/nmcnetworksettings.h"
#include "networksettings.h"
#include "ui_networksettings.h"


namespace OCC {

NMCNetworkSettings::NMCNetworkSettings(QWidget *parent)
    : NetworkSettings(parent)
{
    setLayout();
}

void NMCNetworkSettings::setLayout()
{
    //Fix Layouts
    //Proxy settings
    getUi()->proxyGroupBox->setTitle("");
    getUi()->proxyGroupBox->layout()->removeWidget(getUi()->manualProxyRadioButton);
    getUi()->proxyGroupBox->layout()->removeWidget(getUi()->noProxyRadioButton);
    getUi()->proxyGroupBox->layout()->removeWidget(getUi()->systemProxyRadioButton);
    getUi()->proxyGroupBox->layout()->removeItem(getUi()->horizontalLayout_7);
    getUi()->proxyGroupBox->layout()->removeItem(getUi()->horizontalSpacer_2);
    getUi()->proxyGroupBox->layout()->setContentsMargins(16,16,16,16);
    getUi()->proxyGroupBox->setStyleSheet("QGroupBox { background-color: white; border-radius: 4px; }");

    QGridLayout *proxyLayout = static_cast<QGridLayout *>(getUi()->proxyGroupBox->layout());
    auto proxyLabel = new QLabel(QCoreApplication::translate("", "PROXY_SETTINGS"));
    proxyLabel->setStyleSheet("QLabel{font-size: 12px; font-weight: bold;}");

    proxyLayout->addWidget(proxyLabel, 0, 0 );
    proxyLayout->addItem(new QSpacerItem(1,8, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
    proxyLayout->addWidget(getUi()->noProxyRadioButton, 2, 0 );
    proxyLayout->addWidget(getUi()->systemProxyRadioButton, 3, 0 );
    proxyLayout->addWidget(getUi()->manualProxyRadioButton, 4, 0 );
    proxyLayout->addLayout(getUi()->horizontalLayout_7, 5, 0);

    //Remove the spacer, so the elements can expand.
    getUi()->horizontalSpacer->changeSize(0,0, QSizePolicy::Fixed, QSizePolicy::Fixed);

    //DownloadBox
    getUi()->verticalSpacer_2->changeSize(0,0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    getUi()->downloadBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    getUi()->horizontalLayout_3->setSpacing(8);
    getUi()->downloadBox->setTitle("");
    getUi()->downloadBox->layout()->removeWidget(getUi()->noDownloadLimitRadioButton);
    getUi()->downloadBox->layout()->removeWidget(getUi()->autoDownloadLimitRadioButton);
    getUi()->downloadBox->layout()->removeWidget(getUi()->downloadLimitRadioButton);
    getUi()->downloadBox->layout()->removeItem(getUi()->horizontalLayout_3);
    getUi()->downloadBox->layout()->setContentsMargins(16,16,16,16);
    getUi()->downloadBox->setStyleSheet("QGroupBox { background-color: white; border-radius: 4px; }");

    QGridLayout *downLayout = static_cast<QGridLayout *>(getUi()->downloadBox->layout());

    auto downLabel = new QLabel(QCoreApplication::translate("", "DOWNLOAD_BANDWIDTH"));
    downLabel->setStyleSheet("QLabel{font-size: 12px; font-weight: bold;}");
    downLayout->addWidget(downLabel, 0, 0 );
    downLayout->addItem(new QSpacerItem(1,8, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
    downLayout->addWidget(getUi()->noDownloadLimitRadioButton, 2, 0 );
    downLayout->addWidget(getUi()->autoDownloadLimitRadioButton, 3, 0 );
    downLayout->addWidget(getUi()->downloadLimitRadioButton, 4, 0 );
    downLayout->addItem(getUi()->horizontalLayout_3, 4, 1);

    getUi()->downloadLimitRadioButton->setFixedHeight(getUi()->downloadSpinBox->height());

    //UploadBox
    getUi()->uploadBox->layout()->removeItem(getUi()->horizontalLayout_4);
    static_cast<QGridLayout *>(getUi()->uploadBox->layout())->addItem(getUi()->horizontalLayout_4, 2, 1);

    getUi()->verticalSpacer_3->changeSize(0,0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    getUi()->uploadBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    getUi()->horizontalLayout_4->setSpacing(8);
    getUi()->uploadBox->setTitle("");
    getUi()->uploadBox->layout()->removeWidget(getUi()->noUploadLimitRadioButton);
    getUi()->uploadBox->layout()->removeWidget(getUi()->autoUploadLimitRadioButton);
    getUi()->uploadBox->layout()->removeWidget(getUi()->uploadLimitRadioButton);
    getUi()->uploadBox->layout()->removeItem(getUi()->horizontalLayout_4);
    getUi()->uploadBox->layout()->setContentsMargins(16,16,16,16);
    getUi()->uploadBox->setStyleSheet("QGroupBox { background-color: white; border-radius: 4px; }");

    QGridLayout *upLayout = static_cast<QGridLayout *>(getUi()->uploadBox->layout());

    auto uploadLabel = new QLabel(QCoreApplication::translate("", "UPLOAD_BANDWIDTH"));
    uploadLabel->setStyleSheet("QLabel{font-size: 12px; font-weight: bold;}");
    upLayout->addWidget(uploadLabel, 0, 0 );
    upLayout->addItem(new QSpacerItem(1,8, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0);
    upLayout->addWidget(getUi()->noUploadLimitRadioButton, 2, 0 );
    upLayout->addWidget(getUi()->autoUploadLimitRadioButton, 3, 0 );
    upLayout->addWidget(getUi()->uploadLimitRadioButton, 4, 0 );
    upLayout->addItem(getUi()->horizontalLayout_4, 4, 1);

    getUi()->uploadLimitRadioButton->setFixedHeight(getUi()->uploadSpinBox->height());
}

} // namespace OCC