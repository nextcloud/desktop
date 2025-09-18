/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

    [[nodiscard]] AbstractCredentials *getCredentials() const override;

    void initializePage() override;
    void cleanupPage() override;
    bool validatePage() override;
    void setConnected();
    void setErrorString(const QString &err);

Q_SIGNALS:
    void connectToOCUrl(const QString &);

public slots:
    void slotStyleChanged();

private:
    void startSpinner();
    void stopSpinner();
    void setupCustomization();
    void customizeStyle();

    Ui_OwncloudHttpCredsPage _ui;
    bool _connected = false;
    QProgressIndicator *_progressIndi;
    OwncloudWizard *_ocWizard;
};

} // namespace OCC

#endif
