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
#include "elidedlabel.h"

class QProgressIndicator;

namespace OCC {

class OwncloudWizard;

/**
 * @brief The OwncloudAdvancedSetupPage class
 * @ingroup gui
 */
class OwncloudAdvancedSetupPage : public QWizardPage
{
    Q_OBJECT
public:
    OwncloudAdvancedSetupPage(OwncloudWizard *wizard);

    bool isComplete() const override;
    void initializePage() override;
    int nextId() const override;
    bool validatePage() override;
    QString localFolder() const;
    QStringList selectiveSyncBlacklist() const;
    bool useVirtualFileSync() const;
    bool isConfirmBigFolderChecked() const;
    void setRemoteFolder(const QString &remoteFolder);
    void setMultipleFoldersExist(bool exist);
    void directoriesCreated();

signals:
    void createLocalAndRemoteFolders(const QString &, const QString &);

public slots:
    void setErrorString(const QString &);
    void slotStyleChanged();

private slots:
    void slotSelectFolder();
    void slotSyncEverythingClicked();
    void slotSelectiveSyncClicked();
    void slotVirtualFileSyncClicked();
    void slotQuotaRetrieved(const QVariantMap &result);

private:
    void setRadioChecked(QRadioButton *radio);

    void setupCustomization();
    void updateStatus();
    bool dataChanged();
    void startSpinner();
    void stopSpinner();
    QUrl serverUrl() const;
    qint64 availableLocalSpace() const;
    QString checkLocalSpace(qint64 remoteSize) const;
    void customizeStyle();
    void setServerAddressLabelUrl(const QUrl &url);
    void styleSyncLogo();
    void styleLocalFolderLabel();
    void setResolutionGuiVisible(bool value);
    void setupResoultionWidget();
    void fetchUserAvatar();
    void setUserInformation();

    // TODO: remove when UX decision is made
    void refreshVirtualFilesAvailibility(const QString &path);

    Ui_OwncloudAdvancedSetupPage _ui;
    bool _checking = false;
    bool _created = false;
    bool _localFolderValid = false;
    QProgressIndicator *_progressIndi;
    QString _remoteFolder;
    QString _localPath;
    QStringList _selectiveSyncBlacklist;
    qint64 _rSize = -1;
    qint64 _rSelectedSize = -1;
    OwncloudWizard *_ocWizard;
    QScopedPointer<ElidedLabel> _filePathLabel;
};

} // namespace OCC

#endif
