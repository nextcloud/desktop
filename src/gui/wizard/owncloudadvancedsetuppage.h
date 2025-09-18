/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_OWNCLOUD_ADVANCED_SETUP_PAGE_H
#define MIRALL_OWNCLOUD_ADVANCED_SETUP_PAGE_H

#include <QWizard>

#include "wizard/owncloudwizardcommon.h"
#include "ui_owncloudadvancedsetuppage.h"
#include "elidedlabel.h"

class QProgressIndicator;
class QNetworkReply;

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

    [[nodiscard]] bool isComplete() const override;
    void initializePage() override;
    [[nodiscard]] int nextId() const override;
    bool validatePage() override;
    [[nodiscard]] QString localFolder() const;
    [[nodiscard]] QStringList selectiveSyncBlacklist() const;
    [[nodiscard]] bool useVirtualFileSync() const;
    [[nodiscard]] bool isConfirmBigFolderChecked() const;
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
    void slotQuotaRetrievedWithError(QNetworkReply *reply);

private:
    void setRadioChecked(QRadioButton *radio);

#ifdef BUILD_FILE_PROVIDER_MODULE
    void updateMacOsFileProviderRelatedViews();
#endif

    void setupCustomization();
    void updateStatus();
    bool dataChanged();
    void startSpinner();
    void stopSpinner();
    [[nodiscard]] QUrl serverUrl() const;
    [[nodiscard]] qint64 availableLocalSpace() const;
    [[nodiscard]] QString checkLocalSpace(qint64 remoteSize) const;
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

    Ui_OwncloudAdvancedSetupPage _ui{};
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
