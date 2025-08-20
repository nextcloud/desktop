/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_OWNCLOUD_SETUP_PAGE_H
#define MIRALL_OWNCLOUD_SETUP_PAGE_H

#include <QWizard>

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

#include "../addcertificatedialog.h"
#include "wizard/owncloudconnectionmethoddialog.h"
#include "wizard/wizardproxysettings.h"

#include "ui_owncloudsetupnocredspage.h"

#include "config.h"

class QLabel;
class QVariant;
class QProgressIndicator;

namespace OCC {

/**
 * @brief The OwncloudSetupPage class
 * @ingroup gui
 */
class OwncloudSetupPage : public QWizardPage
{
    Q_OBJECT
public:
    OwncloudSetupPage(QWidget *parent = nullptr);
    ~OwncloudSetupPage() override;

    [[nodiscard]] bool isComplete() const override;
    void initializePage() override;
    [[nodiscard]] int nextId() const override;
    void setServerUrl(const QString &);
    void setAllowPasswordStorage(bool);
    bool validatePage() override;
    [[nodiscard]] QString url() const;
    [[nodiscard]] QString localFolder() const;
    void setRemoteFolder(const QString &remoteFolder);
    void setMultipleFoldersExist(bool exist);
    void setAuthType(DetermineAuthTypeJob::AuthType type);

public slots:
    void setErrorString(const QString &, bool retryHTTPonly);
    void startSpinner();
    void stopSpinner();
    void slotCertificateAccepted();
    void slotStyleChanged();

protected slots:
    void slotUrlChanged(const QString &);
    void slotUrlEditFinished();

    void slotSetProxySettings();

    void setupCustomization();

signals:
    void determineAuthType(const QString &);

private:
    void setLogo();
    void customizeStyle();
    void setupServerAddressDescriptionLabel();

    Ui_OwncloudSetupPage _ui{};

    QString _oCUrl;
    QString _ocUser;
    bool _authTypeKnown = false;
    bool _checking = false;
    DetermineAuthTypeJob::AuthType _authType = DetermineAuthTypeJob::Basic;

    QProgressIndicator *_progressIndi;
    OwncloudWizard *_ocWizard;
    AddCertificateDialog *addCertDial = nullptr;

    WizardProxySettings *_proxySettingsDialog = nullptr;

    // Grab the forceLoginV2-setting from the wizard
    bool useFlow2 = _ocWizard->useFlow2();
};

} // namespace OCC

#endif
