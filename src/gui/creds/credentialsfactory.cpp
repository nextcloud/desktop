/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>
#include <QString>

#include "creds/credentialsfactory.h"
#include "creds/httpcredentialsgui.h"
#include "creds/dummycredentials.h"
#include "creds/webflowcredentials.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcGuiCredentials, "nextcloud.gui.credentials", QtInfoMsg)

namespace CredentialsFactory {

    AbstractCredentials *create(const QString &type)
    {
        // empty string might happen for old version of configuration
        if (type == "http" || type == "") {
            return new HttpCredentialsGui;
        } else if (type == "dummy") {
            return new DummyCredentials;
        } else if (type == "webflow") {
            return new WebFlowCredentials;
        } else {
            qCWarning(lcGuiCredentials, "Unknown credentials type: %s", qPrintable(type));
            return new DummyCredentials;
        }
    }

} // ns CredentialsFactory

} // namespace OCC
