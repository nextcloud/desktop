/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml

// Test replacement for the production C++ controller; provides its enums and the API the wizard QML reads.
QtObject {
    enum Step {
        ServerStep,
        BrowserAuthStep,
        BasicAuthStep,
        SyncOptionsStep,
        CompletedStep
    }

    enum SyncMode {
        SyncEverything,
        SelectiveSync,
        VirtualFiles
    }

    property int currentStep: AccountWizardController.ServerStep
    property int syncMode: AccountWizardController.SelectiveSync
    property bool busy: false
    property bool authPolling: false
    property bool canFinish: true
    property bool canUseVirtualFiles: false
    property bool canUseClassicSync: true
    property bool isUsingFileProvider: false
    property bool hasAdvancedOptions: true
    property bool localSyncFolderRequired: true
    property bool proxySettingsAvailable: true
    property bool basicAuthValid: false
    property bool serverUrlEditable: true
    property bool overrideServerSelectionRequired: false
    property bool startLoginFlowAutomatically: false
    property bool publicShareSetup: false
    property bool showLargeFolderConfirmation: false
    property bool showExternalStorageConfirmation: false
    property bool askBeforeLargeFolders: false
    property bool askBeforeExternalStorage: false
    property int largeFolderThresholdMb: 500
    property var overrideServerNames: []
    property int overrideServerIndex: 0
    property string appName: "Nextcloud"
    property string serverUrl: ""
    property string serverUrlPlaceholder: ""
    property string errorText: ""
    property url loginUrl: ""
    property string authStatusText: ""
    property string userDisplayName: "Test User"
    property string serverDisplayName: "cloud.example.com"
    property string avatarUrl: ""
    property string syncEverythingDescription: ""
    property string localSyncFolderDisplay: "/Users/test/Nextcloud/"
    property string localSyncFolderError: ""
    property string localSyncFolderFreeSpace: ""
    property string basicAuthUser: ""
    property string basicAuthPassword: ""
    property int proxyMode: 0
    property int manualProxyType: 0
    property string proxyHost: ""
    property int proxyPort: 0
    property bool proxyAuthenticationRequired: false
    property string proxyUser: ""
    property string proxyPassword: ""
    property bool proxySettingsValid: true
    property bool showProxyLocalhostWarning: false
    property string clientCertificatePath: ""
    property string clientCertificatePassword: ""
    property string clientCertificateError: ""
    property bool clientCertificateValid: false

    signal finished(int result)
    signal advancedOptionsRequested()
    signal proxySettingsRequested()
    signal clientCertificateDialogRequested()
    signal secureConnectionFailed(string host, bool retryHttpOnly)

    function cancel() {}
    function goBack() {}
    function finish() {}
    function skipFolderConfiguration() {}
    function openAdvancedOptions() {}
    function openProxySettings() {}
    function openSignup() {}
    function openSelfHostedServerGuide() {}
    function openBrowserLogin() {}
    function openSelectiveSync() {}
    function copyLoginLink() {}
    function submitServerUrl() {}
    function submitBasicAuth() {}
    function submitClientCertificate() {}
    function chooseClientCertificate() {}
    function clearClientCertificateInput() {}
    function retrySecureConnectionWithoutTls() {}
    function useClientCertificateForSecureConnection() {}
    function chooseLocalSyncFolder() {}
    function setSyncMode(mode) {}
    function setAskBeforeLargeFolders(ask) {}
    function setLargeFolderThresholdMb(thresholdMb) {}
    function setAskBeforeExternalStorage(ask) {}
}
