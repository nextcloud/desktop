/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#pragma once

#include "abstractsetupwizardpage.h"
#include "gui/qmlutils.h"

namespace OCC {
class OAuth;
namespace Wizard {

    class OAuthCredentialsSetupWizardPage : public AbstractSetupWizardPage
    {
        Q_OBJECT
        OC_DECLARE_WIDGET_FOCUS

    public:
        explicit OAuthCredentialsSetupWizardPage(OAuth *oauth, const QUrl &serverUrl, QWidget *parent = nullptr);

        bool validateInput() override;

    Q_SIGNALS:
        void openBrowserButtonPushed(const QUrl &url);
    };

}
}
