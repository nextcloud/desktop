/*
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

#ifndef MIRALL_OWNCLOUD_SHIBBOLETH_CREDS_PAGE_H
#define MIRALL_OWNCLOUD_SHIBBOLETH_CREDS_PAGE_H

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>
#include <QPointer>

#include "wizard/abstractcredswizardpage.h"
#include "accountfwd.h"

namespace OCC {

class ShibbolethWebView;

/**
 * @brief The OwncloudShibbolethCredsPage class
 * @ingroup gui
 */
class OwncloudShibbolethCredsPage : public AbstractCredentialsWizardPage
{
    Q_OBJECT
public:
    OwncloudShibbolethCredsPage();

    AbstractCredentials *getCredentials() const Q_DECL_OVERRIDE;

    void initializePage() Q_DECL_OVERRIDE;
    int nextId() const Q_DECL_OVERRIDE;
    void setConnected();

Q_SIGNALS:
    void connectToOCUrl(const QString &);

public Q_SLOTS:
    void setVisible(bool visible) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void slotShibbolethCookieReceived(const QNetworkCookie &);
    void slotBrowserRejected();

private:
    void setupBrowser();

    QPointer<ShibbolethWebView> _browser;
    bool _afterInitialSetup;
    QNetworkCookie _cookie;
};

} // namespace OCC

#endif
