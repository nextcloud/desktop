import names
import squish
from helpers.SetupClientHelper import getClientDetails, createUserSyncPath
from helpers.WebUIHelper import authorize_via_webui
from helpers.ConfigHelper import get_config
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

    @staticmethod
    def sanitizeFolderPath(folderPath):
        return folderPath.rstrip("/")

    @staticmethod
    def addServer(context):
        clientDetails = getClientDetails(context)
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX),
            clientDetails['server'],
        )
        AccountConnectionWizard.nextStep()

        if not get_config('ocis'):
            try:
                squish.clickButton(
                    squish.waitForObject(
                        AccountConnectionWizard.CONFIRM_INSECURE_CONNECTION_BUTTON, 1000
                    )
                )
            except:
                test.log(
                    "No insecure connection warning for server "
                    + clientDetails['server']
                )
                pass

    @staticmethod
    def acceptCertificate():
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.ACCEPT_CERTIFICATE_YES)
        )

    @staticmethod
    def addUserCreds(context):
        clientDetails = getClientDetails(context)

        if get_config('ocis'):
            AccountConnectionWizard.oidcLogin(
                clientDetails['user'], clientDetails['password']
            )
        else:
            AccountConnectionWizard.basicLogin(
                clientDetails['user'], clientDetails['password']
            )

    @staticmethod
    def basicLogin(username, password):
        squish.type(
            squish.waitForObject(AccountConnectionWizard.USERNAME_BOX),
            username,
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.USERNAME_BOX),
            "<Tab>",
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.PASSWORD_BOX),
            password,
        )
        AccountConnectionWizard.nextStep()

    @staticmethod
    def oidcLogin(username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.COPY_URL_TO_CLIPBOARD_BUTTON)
        )

        authorize_via_webui(username, password)

    @staticmethod
    def nextStep():
        squish.clickButton(squish.waitForObject(AccountConnectionWizard.NEXT_BUTTON))

    @staticmethod
    def selectSyncFolder(context):
        clientDetails = getClientDetails(context)
        # create sync folder for user
        syncPath = createUserSyncPath(context, clientDetails['user'])

        squish.waitForObject(
            AccountConnectionWizard.ADVANCED_CONFIGURATION_CHECKBOX
        ).setChecked(True)
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.DIRECTORY_NAME_BOX)
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.DIRECTORY_NAME_EDIT_BOX),
            syncPath,
        )
        squish.clickButton(squish.waitForObject(AccountConnectionWizard.CHOOSE_BUTTON))

    @staticmethod
    def addAccount(context):
        AccountConnectionWizard.addAccountCredential(context)
        AccountConnectionWizard.nextStep()

    @staticmethod
    def addAccountCredential(context):
        AccountConnectionWizard.addServer(context)
        if get_config('ocis'):
            AccountConnectionWizard.acceptCertificate()
        AccountConnectionWizard.addUserCreds(context)
        AccountConnectionWizard.selectSyncFolder(context)

    @staticmethod
    def selectManualSyncFolderOption():
        squish.clickButton(
            squish.waitForObject(
                AccountConnectionWizard.CONF_SYNC_MANUALLY_RADIO_BUTTON
            )
        )

    @staticmethod
    def selectVFSOption():
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.VIRTUAL_FILE_RADIO_BUTTON)
        )

    @staticmethod
    def confirmEnableExperimentalVFSOption():
        squish.clickButton(
            squish.waitForObject(
                AccountConnectionWizard.ENABLE_EXPERIMENTAL_FEATURE_BUTTON
            )
        )

    @staticmethod
    def cancelEnableExperimentalVFSOption():
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.STAY_SAFE_BUTTON)
        )

    @staticmethod
    def getErrorMessage():
        return str(squish.waitForObjectExists(AccountConnectionWizard.ERROR_LABEL).text)

    @staticmethod
    def isNewConnectionWindowVisible():
        visible = False
        try:
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)
            visible = True
        except:
            pass
        return visible

    @staticmethod
    def isCredentialWindowVisible(context):
        visible = False
        try:
            if get_config('ocis'):
                squish.waitForObject(AccountConnectionWizard.OAUTH_CREDENTIAL_PAGE)
            else:
                squish.waitForObject(AccountConnectionWizard.BASIC_CREDENTIAL_PAGE)
            visible = True
        except:
            pass
        return visible
