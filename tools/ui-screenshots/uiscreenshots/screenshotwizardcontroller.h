/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTWIZARDCONTROLLER_H
#define SCREENSHOTWIZARDCONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace OCC {

/** @brief Supplies deterministic state through the contract referenced by the target account-wizard QML. */
class ScreenshotWizardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentStep MEMBER _currentStep NOTIFY currentStepChanged)
    Q_PROPERTY(QString serverUrl MEMBER _serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool serverUrlEditable MEMBER _serverUrlEditable CONSTANT)
    Q_PROPERTY(bool overrideServerSelectionRequired MEMBER _overrideServerSelectionRequired CONSTANT)
    Q_PROPERTY(bool startLoginFlowAutomatically MEMBER _startLoginFlowAutomatically CONSTANT)
    Q_PROPERTY(QStringList overrideServerNames MEMBER _overrideServerNames CONSTANT)
    Q_PROPERTY(int overrideServerIndex MEMBER _overrideServerIndex NOTIFY overrideServerSelectionChanged)
    Q_PROPERTY(bool busy MEMBER _busy CONSTANT)
    Q_PROPERTY(bool authPolling MEMBER _authPolling CONSTANT)
    Q_PROPERTY(QString errorText MEMBER _errorText CONSTANT)
    Q_PROPERTY(QUrl loginUrl MEMBER _loginUrl CONSTANT)
    Q_PROPERTY(QString authStatusText MEMBER _authStatusText CONSTANT)
    Q_PROPERTY(QString userDisplayName MEMBER _userDisplayName CONSTANT)
    Q_PROPERTY(QString serverDisplayName MEMBER _serverDisplayName CONSTANT)
    Q_PROPERTY(QUrl avatarUrl MEMBER _avatarUrl CONSTANT)
    Q_PROPERTY(QString syncEverythingDescription MEMBER _syncEverythingDescription CONSTANT)
    Q_PROPERTY(QString localSyncFolderDisplay MEMBER _localSyncFolderDisplay CONSTANT)
    Q_PROPERTY(QString localSyncFolderError MEMBER _localSyncFolderError CONSTANT)
    Q_PROPERTY(QString localSyncFolderFreeSpace MEMBER _localSyncFolderFreeSpace CONSTANT)
    Q_PROPERTY(bool localSyncFolderRequired MEMBER _localSyncFolderRequired CONSTANT)
    Q_PROPERTY(int syncMode MEMBER _syncMode NOTIFY syncModeChanged)
    Q_PROPERTY(bool canFinish MEMBER _canFinish CONSTANT)
    Q_PROPERTY(bool canUseVirtualFiles MEMBER _canUseVirtualFiles CONSTANT)
    Q_PROPERTY(bool isUsingFileProvider MEMBER _isUsingFileProvider CONSTANT)
    Q_PROPERTY(bool canUseClassicSync MEMBER _canUseClassicSync CONSTANT)
    Q_PROPERTY(bool hasAdvancedOptions MEMBER _hasAdvancedOptions CONSTANT)
    Q_PROPERTY(bool showLargeFolderConfirmation MEMBER _showLargeFolderConfirmation CONSTANT)
    Q_PROPERTY(bool askBeforeLargeFolders MEMBER _askBeforeLargeFolders NOTIFY askBeforeLargeFoldersChanged)
    Q_PROPERTY(int largeFolderThresholdMb MEMBER _largeFolderThresholdMb NOTIFY largeFolderThresholdMbChanged)
    Q_PROPERTY(bool showExternalStorageConfirmation MEMBER _showExternalStorageConfirmation CONSTANT)
    Q_PROPERTY(bool askBeforeExternalStorage MEMBER _askBeforeExternalStorage NOTIFY askBeforeExternalStorageChanged)
    Q_PROPERTY(bool proxySettingsAvailable MEMBER _proxySettingsAvailable CONSTANT)
    Q_PROPERTY(int proxyMode MEMBER _proxyMode NOTIFY proxySettingsChanged)
    Q_PROPERTY(int manualProxyType MEMBER _manualProxyType NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString proxyHost MEMBER _proxyHost NOTIFY proxySettingsChanged)
    Q_PROPERTY(int proxyPort MEMBER _proxyPort NOTIFY proxySettingsChanged)
    Q_PROPERTY(bool proxyAuthenticationRequired MEMBER _proxyAuthenticationRequired NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString proxyUser MEMBER _proxyUser NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString proxyPassword MEMBER _proxyPassword NOTIFY proxySettingsChanged)
    Q_PROPERTY(bool showProxyLocalhostWarning MEMBER _showProxyLocalhostWarning CONSTANT)
    Q_PROPERTY(QString basicAuthUser MEMBER _basicAuthUser NOTIFY basicAuthChanged)
    Q_PROPERTY(QString basicAuthPassword MEMBER _basicAuthPassword NOTIFY basicAuthChanged)
    Q_PROPERTY(bool basicAuthValid MEMBER _basicAuthValid CONSTANT)
    Q_PROPERTY(bool publicShareSetup MEMBER _publicShareSetup CONSTANT)
    Q_PROPERTY(QString appName MEMBER _appName CONSTANT)
    Q_PROPERTY(QString serverUrlPlaceholder MEMBER _serverUrlPlaceholder CONSTANT)
    Q_PROPERTY(QString clientCertificatePath MEMBER _clientCertificatePath NOTIFY clientCertificateChanged)
    Q_PROPERTY(QString clientCertificatePassword MEMBER _clientCertificatePassword NOTIFY clientCertificateChanged)
    Q_PROPERTY(QString clientCertificateError MEMBER _clientCertificateError NOTIFY clientCertificateChanged)
    Q_PROPERTY(bool clientCertificateValid MEMBER _clientCertificateValid NOTIFY clientCertificateChanged)

public:
    /** @brief Creates a fictional controller initially displaying the server page. */
    explicit ScreenshotWizardController(QObject *parent = nullptr);

    /** @brief Sets the production wizard step before the window is created. */
    void setCurrentStepForScreenshot(int step);
    /** @brief Sets the production sync mode before the window is created. */
    void setSyncModeForScreenshot(int syncMode);

    /** @brief No-op server submission. */
    Q_INVOKABLE void submitServerUrl();
    /** @brief No-op Basic Auth submission. */
    Q_INVOKABLE void submitBasicAuth();
    /** @brief No-op browser-login action. */
    Q_INVOKABLE void openBrowserLogin();
    /** @brief No-op login-link copy action. */
    Q_INVOKABLE void copyLoginLink();
    /** @brief No-op signup action. */
    Q_INVOKABLE void openSignup();
    /** @brief No-op self-hosting guide action. */
    Q_INVOKABLE void openSelfHostedServerGuide();
    /** @brief Opens the real QML proxy dialog through the root window connection. */
    Q_INVOKABLE void openProxySettings();
    /** @brief Finishes the fictional wizard as rejected. */
    Q_INVOKABLE void cancel();
    /** @brief Moves the fictional controller to the preceding step. */
    Q_INVOKABLE void goBack();
    /** @brief Finishes the fictional wizard as accepted. */
    Q_INVOKABLE void finish();
    /** @brief Finishes without configuring a local folder. */
    Q_INVOKABLE void skipFolderConfiguration();
    /** @brief Selects one of the production sync-mode enum values. */
    Q_INVOKABLE void setSyncMode(int syncMode);
    /** @brief No-op local-folder chooser. */
    Q_INVOKABLE void chooseLocalSyncFolder();
    /** @brief Selects the production selective-sync mode. */
    Q_INVOKABLE void openSelectiveSync();
    /** @brief Opens the real QML advanced-options dialog through the root connection. */
    Q_INVOKABLE void openAdvancedOptions();
    /** @brief Updates the in-memory large-folder confirmation choice. */
    Q_INVOKABLE void setAskBeforeLargeFolders(bool ask);
    /** @brief Updates the in-memory large-folder threshold. */
    Q_INVOKABLE void setLargeFolderThresholdMb(int thresholdMb);
    /** @brief Updates the in-memory external-storage confirmation choice. */
    Q_INVOKABLE void setAskBeforeExternalStorage(bool ask);
    /** @brief Opens the real QML certificate dialog through the root connection. */
    Q_INVOKABLE void chooseClientCertificate();
    /** @brief Returns false because no certificate is configured. */
    Q_INVOKABLE bool submitClientCertificate();
    /** @brief Clears the in-memory certificate fields. */
    Q_INVOKABLE void clearClientCertificateInput();
    /** @brief No-op insecure retry action. */
    Q_INVOKABLE void retrySecureConnectionWithoutTls();
    /** @brief Opens the certificate dialog for a secure-connection retry. */
    Q_INVOKABLE void useClientCertificateForSecureConnection();

signals:
    /** @brief Notifies QML that the current production step changed. */
    void currentStepChanged();
    /** @brief Notifies QML that the server URL changed. */
    void serverUrlChanged();
    /** @brief Notifies QML that override-server selection changed. */
    void overrideServerSelectionChanged();
    /** @brief Notifies QML that the sync mode changed. */
    void syncModeChanged();
    /** @brief Notifies QML that large-folder confirmation changed. */
    void askBeforeLargeFoldersChanged();
    /** @brief Notifies QML that the large-folder threshold changed. */
    void largeFolderThresholdMbChanged();
    /** @brief Notifies QML that external-storage confirmation changed. */
    void askBeforeExternalStorageChanged();
    /** @brief Notifies QML that proxy fixture fields changed. */
    void proxySettingsChanged();
    /** @brief Notifies QML that Basic Auth fixture fields changed. */
    void basicAuthChanged();
    /** @brief Notifies QML that certificate fixture fields changed. */
    void clientCertificateChanged();
    /** @brief Mirrors production wizard completion. */
    void finished(int result);
    /** @brief Requests the eagerly created advanced-options dialog. */
    void advancedOptionsRequested();
    /** @brief Requests the eagerly created proxy-settings dialog. */
    void proxySettingsRequested();
    /** @brief Requests the eagerly created certificate dialog. */
    void clientCertificateDialogRequested();
    /** @brief Mirrors the production secure-connection failure surface. */
    void secureConnectionFailed(const QString &host, bool retryHttpOnly);

private:
    Q_DISABLE_COPY_MOVE(ScreenshotWizardController)

    int _currentStep = 0;
    QString _serverUrl = QStringLiteral("https://cloud.example.com");
    bool _serverUrlEditable = true;
    bool _overrideServerSelectionRequired = false;
    bool _startLoginFlowAutomatically = false;
    QStringList _overrideServerNames;
    int _overrideServerIndex = -1;
    bool _busy = false;
    bool _authPolling = false;
    QString _errorText;
    QUrl _loginUrl = QUrl(QStringLiteral("https://cloud.example.com/login/flow"));
    QString _authStatusText;
    QString _userDisplayName = QStringLiteral("Alex Morgan");
    QString _serverDisplayName = QUrl(QStringLiteral("https://cloud.example.com")).host();
    QUrl _avatarUrl = QUrl(QStringLiteral("qrc:/client/theme/colored/Nextcloud-icon-128.png"));
    QString _syncEverythingDescription;
    QString _localSyncFolderDisplay = QStringLiteral("~/Nextcloud");
    QString _localSyncFolderError;
    QString _localSyncFolderFreeSpace = QStringLiteral("128 GB free");
    bool _localSyncFolderRequired = false;
    int _syncMode = 2;
    bool _canFinish = true;
    bool _canUseVirtualFiles = true;
    bool _isUsingFileProvider = true;
    bool _canUseClassicSync = true;
    bool _hasAdvancedOptions = true;
    bool _showLargeFolderConfirmation = true;
    bool _askBeforeLargeFolders = true;
    int _largeFolderThresholdMb = 500;
    bool _showExternalStorageConfirmation = true;
    bool _askBeforeExternalStorage = true;
    bool _proxySettingsAvailable = true;
    int _proxyMode = 0;
    int _manualProxyType = 0;
    QString _proxyHost = QStringLiteral("proxy.example.com");
    int _proxyPort = 8080;
    bool _proxyAuthenticationRequired = false;
    QString _proxyUser;
    QString _proxyPassword;
    bool _showProxyLocalhostWarning = false;
    QString _basicAuthUser = QStringLiteral("alex");
    QString _basicAuthPassword;
    bool _basicAuthValid = true;
    bool _publicShareSetup = false;
    QString _appName = QStringLiteral("Nextcloud");
    QString _serverUrlPlaceholder = QStringLiteral("https://cloud.example.com");
    QString _clientCertificatePath;
    QString _clientCertificatePassword;
    QString _clientCertificateError;
    bool _clientCertificateValid = false;
};

}

#endif // SCREENSHOTWIZARDCONTROLLER_H
