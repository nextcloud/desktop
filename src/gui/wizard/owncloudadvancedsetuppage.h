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

#ifndef MIRALL_OWNCLOUD_ADVANCED_SETUP_PAGE_H
#define MIRALL_OWNCLOUD_ADVANCED_SETUP_PAGE_H

#include <QWizard>

#include "ui_owncloudadvancedsetuppage.h"
#include "wizard/abstractcredswizardpage.h"
#include "wizard/owncloudwizardcommon.h"

class QProgressIndicator;

namespace OCC {

/**
 * @brief The OwncloudAdvancedSetupPage class
 * @ingroup gui
 */
class OwncloudAdvancedSetupPage : public AbstractWizardPage
{
    Q_OBJECT
public:
    OwncloudAdvancedSetupPage();

    bool isComplete() const override;
    void initializePage() override;
    int nextId() const override;
    bool validatePage() override;
    QStringList selectiveSyncBlacklist() const;
    bool manualFolderConfig() const;
    bool isConfirmBigFolderChecked() const;
    void setRemoteFolder(const QString &remoteFolder);
    void setMultipleFoldersExist(bool exist);
    void directoriesCreated();

public slots:
    void setErrorString(const QString &);

private slots:
    void slotSelectFolder();
    void slotSyncEverythingClicked();
    void slotSelectiveSyncClicked();
    void slotVirtualFileSyncClicked();
    void slotQuotaRetrieved(const QMap<QString, QString> &result);

private:
    void setRadioChecked(QRadioButton *radio);
    void updateStatus();
    void startSpinner();
    void stopSpinner();

    Ui_OwncloudAdvancedSetupPage _ui;
    bool _checking;
    bool _created;
    bool _localFolderValid;
    QProgressIndicator *_progressIndi;
    QStringList _selectiveSyncBlacklist;
};

} // namespace OCC

#endif
