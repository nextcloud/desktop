/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QLabel>
#include <QPixmap>
#include <QVariant>
#include <QRadioButton>
#include <QAbstractButton>
#include <QCheckBox>
#include <QSpinBox>

#include "wizard/owncloudwizardcommon.h"
#include "theme.h"

namespace OCC {

namespace WizardCommon {

    void setupCustomMedia(const QVariant &variant, QLabel *label)
    {
        if (!label)
            return;

        QPixmap pix = variant.value<QPixmap>();
        if (!pix.isNull()) {
            label->setPixmap(pix);
            label->setAlignment(Qt::AlignTop | Qt::AlignRight);
            label->setVisible(true);
        } else {
            QString str = variant.toString();
            if (!str.isEmpty()) {
                label->setText(str);
                label->setTextFormat(Qt::RichText);
                label->setVisible(true);
                label->setOpenExternalLinks(true);
            }
        }
    }

    QString titleTemplate()
    {
        return QString::fromLatin1(R"(<font color="%1" size="5">)").arg(Theme::instance()->wizardHeaderTitleColor().name()) + QString::fromLatin1("%1</font>");
    }

    QString subTitleTemplate()
    {
        return QString::fromLatin1("<font color=\"%1\">").arg(Theme::instance()->wizardHeaderTitleColor().name()) + QString::fromLatin1("%1</font>");
    }

    void initErrorLabel(QLabel *errorLabel)
    {
        QString style = QLatin1String("border: 1px solid #eed3d7; border-radius: 5px; padding: 3px;"
                                      "background-color: #f2dede; color: #b94a48;");

        errorLabel->setStyleSheet(style);
        errorLabel->setWordWrap(true);
        auto sizePolicy = errorLabel->sizePolicy();
        sizePolicy.setRetainSizeWhenHidden(true);
        errorLabel->setSizePolicy(sizePolicy);
        errorLabel->setVisible(false);
    }

    void customizeHintLabel(QLabel *label)
    {
        auto palette = label->palette();
        QColor textColor = palette.color(QPalette::Text);
        textColor.setAlpha(128);
        palette.setColor(QPalette::Text, textColor);
        label->setPalette(palette);
    }

} // ns WizardCommon

} // namespace OCC
