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

#include "wizard/owncloudwizardcommon.h"
#include "ui_owncloudadvancedsetuppage.h"

class QProgressIndicator;

namespace OCC {

/**
 * @brief The OwncloudAdvancedSetupPage class
 * @ingroup gui
 */
class OwncloudAdvancedSetupPage : public QWizardPage
{
    Q_OBJECT
public:
    OwncloudAdvancedSetupPage();

    bool isComplete() const override;
    void initializePage() override;
    int nextId() const override;
    bool validatePage() override;
    QString localFolder() const;
    QStringList selectiveSyncBlacklist() const;
    bool isConfirmBigFolderChecked() const;
    void setRemoteFolder(const QString &remoteFolder);
    void setMultipleFoldersExist(bool exist);
    void directoriesCreated();

signals:
    void createLocalAndRemoteFolders(const QString &, const QString &);

public slots:
    void setErrorString(const QString &);

private slots:
    void slotSelectFolder();
    void slotSyncEverythingClicked();
    void slotSelectiveSyncClicked();
    void slotQuotaRetrieved(const QVariantMap &result);

private:
    void setupCustomization();
    void updateStatus();
    bool dataChanged();
    void startSpinner();
    void stopSpinner();
    QUrl serverUrl() const;
    qint64 availableLocalSpace() const;
    QString checkLocalSpace(qint64 remoteSize) const;

    Ui_OwncloudAdvancedSetupPage _ui;
    bool _checking;
    bool _created;
    bool _localFolderValid;
    QProgressIndicator *_progressIndi;
    QString _remoteFolder;
    QStringList _selectiveSyncBlacklist;
    qint64 _rSize;
    qint64 _rSelectedSize;
};

} // namespace OCC

#endif
