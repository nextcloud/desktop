import names
import squish
from helpers.SetupClientHelper import getClientDetails, createUserSyncPath
from helpers.WebUIHelper import authorize_via_webui
import test


class AccountConnectionWizard:
    SERVER_ADDRESS_BOX = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "urlLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    NEXT_BUTTON = {
        "name": "nextButton",
        "type": "QPushButton",
        "visible": 1,
        "window": names.setupWizardWindow_OCC_Wizard_SetupWizardWindow,
    }
    CONFIRM_INSECURE_CONNECTION_BUTTON = {
        "text": "Confirm",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.insecure_connection_QMessageBox,
    }
    USERNAME_BOX = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "usernameLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    PASSWORD_BOX = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "passwordLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    SELECT_LOCAL_FOLDER = {
        "container": names.advancedConfigGroupBox_localDirectoryGroupBox_QGroupBox,
        "name": "localDirectoryLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    DIRECTORY_NAME_BOX = {
        "container": names.advancedConfigGroupBox_localDirectoryGroupBox_QGroupBox,
        "name": "chooseLocalDirectoryButton",
        "type": "QToolButton",
        "visible": 1,
    }
    CHOOSE_BUTTON = {
        "text": "Choose",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.qFileDialog_QFileDialog,
    }
    ERROR_LABEL = {
        "name": "errorMessageLabel",
        "type": "QLabel",
        "visible": 1,
        "window": names.setupWizardWindow_OCC_Wizard_SetupWizardWindow,
    }
    BASIC_CREDENTIAL_PAGE = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "CredentialsSetupWizardPage",
        "type": "OCC::Wizard::BasicCredentialsSetupWizardPage",
        "visible": 1,
    }
    OAUTH_CREDENTIAL_PAGE = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "CredentialsSetupWizardPage",
        "type": "OCC::Wizard::OAuthCredentialsSetupWizardPage",
        "visible": 1,
    }
    ACCEPT_CERTIFICATE_YES = {
        "text": "Yes",
        "type": "QPushButton",
        "visible": 1,
        "window": names.oCC_TlsErrorDialog_OCC_TlsErrorDialog,
    }
    COPY_URL_TO_CLIPBOARD_BUTTON = {
        "container": names.contentWidget_contentWidget_QStackedWidget,
        "name": "copyUrlToClipboardButton",
        "type": "QPushButton",
        "visible": 1,
    }
    CONF_SYNC_MANUALLY_RADIO_BUTTON = {
        "container": names.advancedConfigGroupBox_syncModeGroupBox_QGroupBox,
        "name": "configureSyncManuallyRadioButton",
        "type": "QRadioButton",
        "visible": 1,
    }
    ADVANCED_CONFIGURATION_CHECKBOX = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "advancedConfigGroupBox",
        "type": "QGroupBox",
        "visible": 1,
    }
    DIRECTORY_NAME_EDIT_BOX = {
        "buddy": names.qFileDialog_fileNameLabel_QLabel,
        "name": "fileNameEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    VIRTUAL_FILE_RADIO_BUTTON = {
        "container": names.advancedConfigGroupBox_syncModeGroupBox_QGroupBox,
        "name": "useVfsRadioButton",
        "type": "QRadioButton",
        "visible": 1,
    }
    ENABLE_EXPERIMENTAL_FEATURE_BUTTON = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "text": "Enable experimental placeholder mode",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }
    STAY_SAFE_BUTTON = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "text": "Stay safe",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }

    def __init__(self):
        pass

    def sanitizeFolderPath(self, folderPath):
        return folderPath.rstrip("/")

    def addServer(self, context):
        clientDetails = getClientDetails(context)
        squish.mouseClick(squish.waitForObject(self.SERVER_ADDRESS_BOX))
        squish.type(
            squish.waitForObject(self.SERVER_ADDRESS_BOX),
            clientDetails['server'],
        )
        self.nextStep()

        if not context.userData['ocis']:
            try:
                squish.clickButton(
                    squish.waitForObject(self.CONFIRM_INSECURE_CONNECTION_BUTTON, 1000)
                )
            except:
                test.log(
                    "No insecure connection warning for server "
                    + clientDetails['server']
                )
                pass

    def acceptCertificate(self):
        squish.clickButton(squish.waitForObject(self.ACCEPT_CERTIFICATE_YES))

    def addUserCreds(self, context):
        clientDetails = getClientDetails(context)

        if context.userData['ocis']:
            self.oidcLogin(clientDetails['user'], clientDetails['password'])
        else:
            self.basicLogin(clientDetails['user'], clientDetails['password'])

    def basicLogin(self, username, password):
        squish.type(
            squish.waitForObject(self.USERNAME_BOX),
            username,
        )
        squish.type(
            squish.waitForObject(self.USERNAME_BOX),
            "<Tab>",
        )
        squish.type(
            squish.waitForObject(self.PASSWORD_BOX),
            password,
        )
        self.nextStep()

    def oidcLogin(self, username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.clickButton(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))

        authorize_via_webui(username, password)

    def nextStep(self):
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def selectSyncFolder(self, context):
        clientDetails = getClientDetails(context)
        # create sync folder for user
        syncPath = createUserSyncPath(context, clientDetails['user'])

        squish.waitForObject(self.ADVANCED_CONFIGURATION_CHECKBOX).setChecked(True)
        squish.mouseClick(squish.waitForObject(self.DIRECTORY_NAME_BOX))
        squish.type(squish.waitForObject(self.DIRECTORY_NAME_EDIT_BOX), syncPath)
        squish.clickButton(squish.waitForObject(self.CHOOSE_BUTTON))
        test.compare(
            str(squish.waitForObjectExists(self.SELECT_LOCAL_FOLDER).text),
            self.sanitizeFolderPath(syncPath),
        )

    def addAccount(self, context):
        self.addAccountCredential(context)
        self.nextStep()

    def addAccountCredential(self, context):
        self.addServer(context)
        if context.userData['ocis']:
            self.acceptCertificate()
        self.addUserCreds(context)
        self.selectSyncFolder(context)

    def selectManualSyncFolderOption(self):
        squish.clickButton(squish.waitForObject(self.CONF_SYNC_MANUALLY_RADIO_BUTTON))

    def selectVFSOption(self):
        squish.clickButton(squish.waitForObject(self.VIRTUAL_FILE_RADIO_BUTTON))

    def confirmEnableExperimentalVFSOption(self):
        squish.clickButton(
            squish.waitForObject(self.ENABLE_EXPERIMENTAL_FEATURE_BUTTON)
        )

    def cancelEnableExperimentalVFSOption(self):
        squish.clickButton(squish.waitForObject(self.STAY_SAFE_BUTTON))
