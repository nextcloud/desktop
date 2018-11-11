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

#ifndef MIRALL_OWNCLOUD_SETUP_PAGE_H
#define MIRALL_OWNCLOUD_SETUP_PAGE_H

#include <QWizard>

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

#include "../addcertificatedialog.h"
#include "wizard/owncloudconnectionmethoddialog.h"

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
    ~OwncloudSetupPage();

    bool isComplete() const Q_DECL_OVERRIDE;
    void initializePage() Q_DECL_OVERRIDE;
    int nextId() const Q_DECL_OVERRIDE;
    void setServerUrl(const QString &);
    void setAllowPasswordStorage(bool);
    bool validatePage() Q_DECL_OVERRIDE;
    QString url() const;
    QString localFolder() const;
    void setRemoteFolder(const QString &remoteFolder);
    void setMultipleFoldersExist(bool exist);
    void setAuthType(DetermineAuthTypeJob::AuthType type);

public slots:
    void setErrorString(const QString &, bool retryHTTPonly);
    void startSpinner();
    void stopSpinner();
    void slotCertificateAccepted();

protected slots:
    void slotUrlChanged(const QString &);
    void slotUrlEditFinished();
#ifdef WITH_PROVIDERS
    void slotLogin();
    void slotGotoProviderList();
#endif

    void setupCustomization();

signals:
    void determineAuthType(const QString &);

private:
    bool urlHasChanged();

    Ui_OwncloudSetupPage _ui;

    QString _oCUrl;
    QString _ocUser;
    bool _authTypeKnown;
    bool _checking;
    bool _multipleFoldersExist;
    DetermineAuthTypeJob::AuthType _authType;

    QProgressIndicator *_progressIndi;
    QButtonGroup *_selectiveSyncButtons;
    QString _remoteFolder;
    AddCertificateDialog *addCertDial;
    OwncloudWizard *_ocWizard;
};

} // namespace OCC

#endif
