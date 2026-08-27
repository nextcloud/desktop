/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "whitelabeltheme.h"

namespace OCC {

namespace WizardCommon {

    void setupCustomMedia(const QVariant &variant, QLabel *label)
    {
        if (!label)
            return;

        auto pix = variant.value<QPixmap>();
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
        // Used to dim whatever QPalette::Text the label inherited by 50% alpha - but this
        // runs from the page constructor (setupCustomization()), before the widget's
        // palette is reliably propagated to dark mode on every platform. A stale light-mode
        // text color dimmed to 50% renders as near-invisible dark-on-dark. Use the same
        // theme-aware muted color other wizard pages already use for secondary/hint text
        // instead - correct regardless of palette-propagation timing.
        auto palette = label->palette();
        palette.setColor(QPalette::Text, QColor(WLTheme.folderWizardPathColor()));
        label->setPalette(palette);
    }

} // ns WizardCommon

} // namespace OCC
