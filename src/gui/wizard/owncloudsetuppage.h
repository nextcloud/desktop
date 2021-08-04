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

#include <QScopedPointer>
#include <QWizard>

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

class QLabel;
class QVariant;
class QProgressIndicator;
class QButtonGroup;
class Ui_OwncloudSetupPage;

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

    bool isComplete() const override;
    void initializePage() override;
    int nextId() const override;
    void setServerUrl(const QString &);
    void setAllowPasswordStorage(bool);
    bool validatePage() override;
    QString url() const;
    QString localFolder() const;
    void setRemoteFolder(const QString &remoteFolder);
    void setMultipleFoldersExist(bool exist);
    void setAuthType();

public slots:
    void setErrorString(const QString &);
    void startSpinner();
    void stopSpinner();

protected slots:
    void slotUrlChanged(const QString &);
    void slotUrlEditFinished();

signals:
    void determineAuthType(const QString &);

private:
    QScopedPointer<Ui_OwncloudSetupPage> _ui;

    QString _oCUrl;
    QString _ocUser;
    bool _authTypeKnown;
    bool _checking;
    bool _multipleFoldersExist;

    QProgressIndicator *_progressIndi;
    QButtonGroup *_selectiveSyncButtons;
    QString _remoteFolder;
    OwncloudWizard *_ocWizard;
};

} // namespace OCC

#endif
