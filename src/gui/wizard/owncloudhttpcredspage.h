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

#ifndef MIRALL_OWNCLOUD_HTTP_CREDS_PAGE_H
#define MIRALL_OWNCLOUD_HTTP_CREDS_PAGE_H

#include "wizard/abstractcredswizardpage.h"
#include "wizard/owncloudwizard.h"

#include "ui_owncloudhttpcredspage.h"

class QProgressIndicator;

namespace OCC {

/**
 * @brief The OwncloudHttpCredsPage class
 */
class OwncloudHttpCredsPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    OwncloudHttpCredsPage(QWidget *parent);

    AbstractCredentials *getCredentials() const override;

    void initializePage() override;
    void cleanupPage() override;
    bool validatePage() override;
    int nextId() const override;
    void setConnected();
    void setErrorString(const QString &err);

Q_SIGNALS:
    void connectToOCUrl(const QString &);

private:
    void startSpinner();
    void stopSpinner();
    void setupCustomization();

    Ui_OwncloudHttpCredsPage _ui;
    bool _connected;
    QProgressIndicator *_progressIndi;
    OwncloudWizard *_ocWizard;
};

} // namespace OCC

#endif
