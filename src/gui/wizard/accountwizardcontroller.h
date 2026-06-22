/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ACCOUNTWIZARDCONTROLLER_H
#define ACCOUNTWIZARDCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QNetworkProxy>
#include <QStringList>
#include <QSslCertificate>
#include <QSslKey>
#include <QUrl>

#include <memory>

#include "accountfwd.h"
#include "creds/flow2auth.h"
#include "networkjobs.h"

class QNetworkReply;

namespace OCC {

class SelectiveSyncDialog;
class AccountState;

/**
 * Backend for the QML account wizard.
 *
 * The controller owns the account setup state and keeps credentials, network
 * jobs and authentication details in C++. QML is intentionally limited to
 * rendering these properties and invoking high-level actions.
 */
class AccountWizardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Step currentStep READ currentStep NOTIFY currentStepChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(bool serverUrlEditable READ serverUrlEditable NOTIFY serverUrlEditableChanged)
    Q_PROPERTY(bool overrideServerSelectionRequired READ overrideServerSelectionRequired NOTIFY overrideServerSelectionChanged)
    Q_PROPERTY(bool startLoginFlowAutomatically READ startLoginFlowAutomatically CONSTANT)
    Q_PROPERTY(QStringList overrideServerNames READ overrideServerNames NOTIFY overrideServerSelectionChanged)
    Q_PROPERTY(int overrideServerIndex READ overrideServerIndex WRITE setOverrideServerIndex NOTIFY overrideServerSelectionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool authPolling READ authPolling NOTIFY authPollingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QUrl loginUrl READ loginUrl NOTIFY loginUrlChanged)
    Q_PROPERTY(QString authStatusText READ authStatusText NOTIFY authStatusTextChanged)
    Q_PROPERTY(QString userDisplayName READ userDisplayName NOTIFY userDisplayNameChanged)
    Q_PROPERTY(QString serverDisplayName READ serverDisplayName NOTIFY serverDisplayNameChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(QString syncEverythingDescription READ syncEverythingDescription NOTIFY syncEverythingDescriptionChanged)
    Q_PROPERTY(QString localSyncFolder READ localSyncFolder NOTIFY localSyncFolderChanged)
    Q_PROPERTY(QString localSyncFolderDisplay READ localSyncFolderDisplay NOTIFY localSyncFolderChanged)
    Q_PROPERTY(QString localSyncFolderError READ localSyncFolderError NOTIFY localSyncFolderErrorChanged)
    Q_PROPERTY(QString localSyncFolderFreeSpace READ localSyncFolderFreeSpace NOTIFY localSyncFolderFreeSpaceChanged)
    Q_PROPERTY(bool localSyncFolderRequired READ localSyncFolderRequired NOTIFY localSyncFolderRequiredChanged)
    Q_PROPERTY(SyncMode syncMode READ syncMode NOTIFY syncModeChanged)
    Q_PROPERTY(bool canFinish READ canFinish NOTIFY canFinishChanged)
    Q_PROPERTY(bool canUseVirtualFiles READ canUseVirtualFiles CONSTANT)
    Q_PROPERTY(bool isUsingFileProvider READ isUsingFileProvider CONSTANT)
    Q_PROPERTY(bool canUseClassicSync READ canUseClassicSync CONSTANT)
    Q_PROPERTY(bool needsSyncOptions READ needsSyncOptions NOTIFY needsSyncOptionsChanged)
    Q_PROPERTY(bool canSkipFolderConfiguration READ canSkipFolderConfiguration CONSTANT)
    Q_PROPERTY(bool hasAdvancedOptions READ hasAdvancedOptions CONSTANT)
    Q_PROPERTY(bool showLargeFolderConfirmation READ showLargeFolderConfirmation CONSTANT)
    Q_PROPERTY(bool askBeforeLargeFolders READ askBeforeLargeFolders NOTIFY askBeforeLargeFoldersChanged)
    Q_PROPERTY(int largeFolderThresholdMb READ largeFolderThresholdMb NOTIFY largeFolderThresholdMbChanged)
    Q_PROPERTY(bool showExternalStorageConfirmation READ showExternalStorageConfirmation CONSTANT)
    Q_PROPERTY(bool askBeforeExternalStorage READ askBeforeExternalStorage NOTIFY askBeforeExternalStorageChanged)
    Q_PROPERTY(bool proxySettingsAvailable READ proxySettingsAvailable CONSTANT)
    Q_PROPERTY(int proxyMode READ proxyMode WRITE setProxyMode NOTIFY proxySettingsChanged)
    Q_PROPERTY(int manualProxyType READ manualProxyType WRITE setManualProxyType NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString proxyHost READ proxyHost WRITE setProxyHost NOTIFY proxySettingsChanged)
    Q_PROPERTY(int proxyPort READ proxyPort WRITE setProxyPort NOTIFY proxySettingsChanged)
    Q_PROPERTY(bool proxyAuthenticationRequired READ proxyAuthenticationRequired WRITE setProxyAuthenticationRequired NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString proxyUser READ proxyUser WRITE setProxyUser NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString proxyPassword READ proxyPassword WRITE setProxyPassword NOTIFY proxySettingsChanged)
    Q_PROPERTY(bool proxySettingsValid READ proxySettingsValid NOTIFY proxySettingsChanged)
    Q_PROPERTY(bool showProxyLocalhostWarning READ showProxyLocalhostWarning NOTIFY proxySettingsChanged)
    Q_PROPERTY(QString basicAuthUser READ basicAuthUser WRITE setBasicAuthUser NOTIFY basicAuthChanged)
    Q_PROPERTY(QString basicAuthPassword READ basicAuthPassword WRITE setBasicAuthPassword NOTIFY basicAuthChanged)
    Q_PROPERTY(bool basicAuthValid READ basicAuthValid NOTIFY basicAuthChanged)
    Q_PROPERTY(bool publicShareSetup READ publicShareSetup NOTIFY publicShareSetupChanged)
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString serverUrlPlaceholder READ serverUrlPlaceholder CONSTANT)
    Q_PROPERTY(QString clientCertificatePath READ clientCertificatePath NOTIFY clientCertificateChanged)
    Q_PROPERTY(QString clientCertificatePassword READ clientCertificatePassword WRITE setClientCertificatePassword NOTIFY clientCertificateChanged)
    Q_PROPERTY(QString clientCertificateError READ clientCertificateError NOTIFY clientCertificateChanged)
    Q_PROPERTY(bool clientCertificateValid READ clientCertificateValid NOTIFY clientCertificateChanged)

public:
    enum Step {
        ServerStep = 0,
        BrowserAuthStep,
        BasicAuthStep,
        SyncOptionsStep,
        CompletedStep
    };
    Q_ENUM(Step)

    enum SyncMode {
        SyncEverything = 0,
        SelectiveSync,
        VirtualFiles
    };
    Q_ENUM(SyncMode)

    explicit AccountWizardController(QObject *parent = nullptr);
    ~AccountWizardController() override;

    [[nodiscard]] Step currentStep() const;
    [[nodiscard]] QString serverUrl() const;
    void setServerUrl(const QString &serverUrl);
    [[nodiscard]] bool serverUrlEditable() const;
    [[nodiscard]] bool overrideServerSelectionRequired() const;
    [[nodiscard]] bool startLoginFlowAutomatically() const;
    [[nodiscard]] QStringList overrideServerNames() const;
    [[nodiscard]] int overrideServerIndex() const;
    void setOverrideServerIndex(int index);
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool authPolling() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QUrl loginUrl() const;
    [[nodiscard]] QString authStatusText() const;
    [[nodiscard]] QString userDisplayName() const;
    [[nodiscard]] QString serverDisplayName() const;
    [[nodiscard]] QString avatarUrl() const;
    [[nodiscard]] QString syncEverythingDescription() const;
    [[nodiscard]] QString localSyncFolder() const;
    [[nodiscard]] QString localSyncFolderDisplay() const;
    [[nodiscard]] QString localSyncFolderError() const;
    [[nodiscard]] QString localSyncFolderFreeSpace() const;
    [[nodiscard]] bool localSyncFolderRequired() const;
    [[nodiscard]] SyncMode syncMode() const;
    [[nodiscard]] bool canFinish() const;
    [[nodiscard]] bool canUseVirtualFiles() const;
    [[nodiscard]] bool isUsingFileProvider() const;
    [[nodiscard]] bool canUseClassicSync() const;
    [[nodiscard]] bool needsSyncOptions() const;
    [[nodiscard]] bool canSkipFolderConfiguration() const;
    [[nodiscard]] bool hasAdvancedOptions() const;
    [[nodiscard]] bool showLargeFolderConfirmation() const;
    [[nodiscard]] bool askBeforeLargeFolders() const;
    [[nodiscard]] int largeFolderThresholdMb() const;
    [[nodiscard]] bool showExternalStorageConfirmation() const;
    [[nodiscard]] bool askBeforeExternalStorage() const;
    [[nodiscard]] bool proxySettingsAvailable() const;
    [[nodiscard]] int proxyMode() const;
    void setProxyMode(int proxyMode);
    [[nodiscard]] int manualProxyType() const;
    void setManualProxyType(int manualProxyType);
    [[nodiscard]] QString proxyHost() const;
    void setProxyHost(const QString &proxyHost);
    [[nodiscard]] int proxyPort() const;
    void setProxyPort(int proxyPort);
    [[nodiscard]] bool proxyAuthenticationRequired() const;
    void setProxyAuthenticationRequired(bool proxyAuthenticationRequired);
    [[nodiscard]] QString proxyUser() const;
    void setProxyUser(const QString &proxyUser);
    [[nodiscard]] QString proxyPassword() const;
    void setProxyPassword(const QString &proxyPassword);
    [[nodiscard]] bool proxySettingsValid() const;
    [[nodiscard]] bool showProxyLocalhostWarning() const;
    [[nodiscard]] QString basicAuthUser() const;
    void setBasicAuthUser(const QString &user);
    [[nodiscard]] QString basicAuthPassword() const;
    void setBasicAuthPassword(const QString &password);
    [[nodiscard]] bool basicAuthValid() const;
    [[nodiscard]] bool publicShareSetup() const;
    [[nodiscard]] QString appName() const;
    [[nodiscard]] QString serverUrlPlaceholder() const;
    [[nodiscard]] QString clientCertificatePath() const;
    [[nodiscard]] QString clientCertificatePassword() const;
    void setClientCertificatePassword(const QString &password);
    [[nodiscard]] QString clientCertificateError() const;
    [[nodiscard]] bool clientCertificateValid() const;

    [[nodiscard]] static QString normalizeServerUrlInput(const QString &serverUrl, const QString &davPath = {});

    Q_INVOKABLE void submitServerUrl();
    Q_INVOKABLE void submitBasicAuth();
    Q_INVOKABLE void openBrowserLogin();
    Q_INVOKABLE void copyLoginLink();
    Q_INVOKABLE void openSignup();
    Q_INVOKABLE void openSelfHostedServerGuide();
    Q_INVOKABLE void openProxySettings();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void finish();
    Q_INVOKABLE void skipFolderConfiguration();
    Q_INVOKABLE void setSyncMode(int syncMode);
    Q_INVOKABLE void chooseLocalSyncFolder();
    Q_INVOKABLE void openSelectiveSync();
    Q_INVOKABLE void openAdvancedOptions();
    Q_INVOKABLE void setAskBeforeLargeFolders(bool ask);
    Q_INVOKABLE void setLargeFolderThresholdMb(int thresholdMb);
    Q_INVOKABLE void setAskBeforeExternalStorage(bool ask);
    Q_INVOKABLE void pollNow();
    Q_INVOKABLE void chooseClientCertificate();
    Q_INVOKABLE bool submitClientCertificate();
    Q_INVOKABLE void clearClientCertificateInput();
    Q_INVOKABLE void retrySecureConnectionWithoutTls();
    Q_INVOKABLE void useClientCertificateForSecureConnection();

signals:
    void currentStepChanged();
    void serverUrlChanged();
    void serverUrlEditableChanged();
    void overrideServerSelectionChanged();
    void busyChanged();
    void authPollingChanged();
    void errorTextChanged();
    void loginUrlChanged();
    void authStatusTextChanged();
    void userDisplayNameChanged();
    void serverDisplayNameChanged();
    void avatarUrlChanged();
    void syncEverythingDescriptionChanged();
    void localSyncFolderChanged();
    void localSyncFolderErrorChanged();
    void localSyncFolderFreeSpaceChanged();
    void localSyncFolderRequiredChanged();
    void syncModeChanged();
    void canFinishChanged();
    void needsSyncOptionsChanged();
    void askBeforeLargeFoldersChanged();
    void largeFolderThresholdMbChanged();
    void askBeforeExternalStorageChanged();
    void proxySettingsChanged();
    void basicAuthChanged();
    void publicShareSetupChanged();
    void finished(int result);
    void advancedOptionsRequested();
    void proxySettingsRequested();
    void clientCertificateDialogRequested();
    void secureConnectionFailed(const QString &host, bool retryHttpOnly);
    void clientCertificateChanged();

private slots:
    void slotSystemProxyLookupDone(const QNetworkProxy &proxy);
    void slotFindServer();
    void slotFindServerBehindRedirect();
    void slotFoundServer(const QUrl &url, const QJsonObject &info);
    void slotNoServerFound(QNetworkReply *reply);
    void slotNoServerFoundTimeout(const QUrl &url);
    void slotDetermineAuthType();
    void slotFlow2AuthResult(OCC::Flow2Auth::Result result, const QString &errorString, const QString &user, const QString &appPassword);
    void slotFlow2StatusChanged(OCC::Flow2Auth::PollStatus status, int secondsLeft);
    void slotAuthError(QNetworkReply *reply);
    void slotRemoteFolderExists(QNetworkReply *reply);
    void slotCreateRemoteFolderFinished(QNetworkReply *reply);

private:
    void initialiseAccount();
    void ensureAccount();
    void initialiseOverrideServerChoices();
    void startServerCheck(const QUrl &serverUrl);
    void startFlow2Auth();
    void connectToAuthenticatedAccount(const QString &url, const QString &user, const QString &appPassword);
    void testOwnCloudConnect();
    void completeAuthentication();
    void fetchUserAvatar();
    void fetchRootFolderSize();
    AccountState *applyAccountChanges();
    void clearOneShotOverrides();
    void initialiseLocalSyncFolder();
    void setLocalSyncFolder(const QString &localSyncFolder, bool selectedByUser = false);
    void promptForInitialLocalSyncFolderIfNeeded();
    void validateLocalSyncFolder();
    [[nodiscard]] qint64 availableLocalSpace() const;
    [[nodiscard]] qint64 requiredLocalSpace() const;
    [[nodiscard]] bool ensureLocalSyncFolder();
    void startRemoteFolderCheck();
    void createRemoteFolder();
    void completeRemoteFolderCheck();
    [[nodiscard]] bool createSyncFolder(AccountState *accountState);
    [[nodiscard]] QString openLocalSyncFolderDialog(bool initialSelection) const;
    [[nodiscard]] QUrl localFolderServerUrl() const;
    [[nodiscard]] QString sanitizedRemoteFolderEntityPath() const;
    void setCurrentStep(Step step);
    void setBusy(bool busy);
    void setAuthPolling(bool authPolling);
    void setErrorText(const QString &errorText);
    void setLoginUrl(const QUrl &loginUrl);
    void setAuthStatusText(const QString &authStatusText);
    void setUserDisplayName(const QString &userDisplayName);
    void setServerDisplayName(const QString &serverDisplayName);
    void setAvatarUrl(const QString &avatarUrl);
    void setSyncEverythingDescription(const QString &syncEverythingDescription);
    void setNeedsSyncOptions(bool needsSyncOptions);
    void setPublicShareSetup(bool publicShareSetup);
    void setServerUrlEditable(bool editable);
    void emitProxySettingsChangedIfNeeded(bool previousValidity, bool previousLocalhostWarning);
    void discardFlow2Auth();
    [[nodiscard]] bool checkDowngradeAdvised(QNetworkReply *reply) const;
    [[nodiscard]] bool handleSecureConnectionFailure(QNetworkReply *reply, bool retryHttpOnly);

    AccountPtr _account;
    std::unique_ptr<Flow2Auth> _flow2Auth;
    QPointer<SelectiveSyncDialog> _selectiveSyncDialog;
    enum class ProxyAuthentication {
        AuthenticationRequired,
        NoAuthentication,
    };
    struct WizardProxySettings
    {
        QString _user;
        QString _password;
        QString _host;
        quint16 _port = 8080;
        ProxyAuthentication _needsAuth = ProxyAuthentication::NoAuthentication;
        QNetworkProxy::ProxyType _proxyType = QNetworkProxy::NoProxy;
    };
    Step _currentStep = ServerStep;
    QString _serverUrl;
    bool _serverUrlEditable = true;
    QStringList _overrideServerNames;
    QStringList _overrideServerUrls;
    int _overrideServerIndex = -1;
    bool _busy = false;
    bool _authPolling = false;
    QString _errorText;
    QUrl _loginUrl;
    QString _authStatusText;
    QString _userDisplayName;
    QString _serverDisplayName;
    QString _avatarUrl;
    QString _syncEverythingDescription;
    QString _localSyncFolder;
    QString _localSyncFolderError;
    QString _localSyncFolderFreeSpace;
    bool _localSyncFolderValid = false;
    bool _localSyncFolderSelected = false;
    bool _localSyncFolderOverride = false;
    bool _initialLocalSyncFolderPromptShown = false;
    bool _localSyncFolderPickerOpen = false;
    SyncMode _syncMode = SyncEverything;
    bool _needsSyncOptions = false;
    bool _askBeforeLargeFolders = true;
    int _largeFolderThresholdMb = 500;
    bool _askBeforeExternalStorage = true;
    qint64 _syncEverythingSize = -1;
    qint64 _selectiveSyncSize = -1;
    QString _remoteFolder;
    QStringList _selectiveSyncBlacklist;
    WizardProxySettings _proxySettings;
    QString _basicAuthUser;
    QString _basicAuthPassword;
    bool _publicShareSetup = false;
    QByteArray _clientCertBundle;
    QByteArray _clientCertPassword;
    QString _clientCertificatePath;
    QString _clientCertificatePassword;
    QString _clientCertificateError;
    QSslKey _clientSslKey;
    QSslCertificate _clientSslCertificate;
    QList<QSslCertificate> _clientSslCaCertificates;
    QUrl _secureConnectionFailedUrl;
};

} // namespace OCC

#endif // ACCOUNTWIZARDCONTROLLER_H
