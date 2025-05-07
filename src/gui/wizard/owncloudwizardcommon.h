/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_OWNCLOUD_WIZARD_COMMON_H
#define MIRALL_OWNCLOUD_WIZARD_COMMON_H

#include "config.h"

#include <QString>

class QVariant;
class QLabel;
class QRadioButton;
class QSpinBox;
class QCheckBox;
class QAbstractButton;

namespace OCC {

namespace WizardCommon {

    void setupCustomMedia(const QVariant &variant, QLabel *label);
    QString titleTemplate();
    QString subTitleTemplate();
    void initErrorLabel(QLabel *errorLabel);
    void customizeHintLabel(QLabel *label);

    enum SyncMode {
        SelectiveMode,
        BoxMode
    };

    enum Pages {
        Page_Welcome,
        Page_ServerSetup,
        Page_HttpCreds,
        Page_Flow2AuthCreds,
#ifdef WITH_WEBENGINE
        Page_WebView,
#endif // WITH_WEBENGINE
        Page_TermsOfService,
        Page_AdvancedSetup,
    };

} // ns WizardCommon

} // namespace OCC

#endif // MIRALL_OWNCLOUD_WIZARD_COMMON_H
