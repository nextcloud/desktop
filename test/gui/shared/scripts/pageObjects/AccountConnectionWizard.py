import test
import names
import squish

from pageObjects.EnterPassword import EnterPassword

from helpers.WebUIHelper import authorize_via_webui
from helpers.ConfigHelper import get_config
from helpers.SetupClientHelper import (
    createUserSyncPath,
    getTempResourcePath,
    setCurrentUserSyncPath,
)
from helpers.SyncHelper import listenSyncStatusForItem


class AccountConnectionWizard:
    SERVER_ADDRESS_BOX = {
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
        "name": "urlLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    NEXT_BUTTON = {
        "container": names.settings_dialogStack_QStackedWidget,
        "name": "nextButton",
        "type": "QPushButton",
        "visible": 1,
    }
    CONFIRM_INSECURE_CONNECTION_BUTTON = {
        "text": "Confirm",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.insecure_connection_QMessageBox,
    }
    USERNAME_BOX = {
        "container": names.contentWidget_OCC_QmlUtils_OCQuickWidget,
        "id": "userNameField",
        "type": "TextField",
        "visible": True,
    }
    PASSWORD_BOX = {
        "container": names.contentWidget_OCC_QmlUtils_OCQuickWidget,
        "id": "passwordField",
        "type": "TextField",
        "visible": True,
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
        "container": names.contentWidget_contentWidget_QStackedWidget,
        "type": "OCC::Wizard::BasicCredentialsSetupWizardPage",
        "visible": 1,
    }
    OAUTH_CREDENTIAL_PAGE = {
        "container": names.contentWidget_contentWidget_QStackedWidget,
        "type": "OCC::Wizard::OAuthCredentialsSetupWizardPage",
        "visible": 1,
    }
    COPY_URL_TO_CLIPBOARD_BUTTON = {
        "container": names.contentWidget_OCC_QmlUtils_OCQuickWidget,
        "id": "copyToClipboardButton",
        "type": "Button",
        "visible": True,
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
    SYNC_EVERYTHING_RADIO_BUTTON = {
        "container": names.advancedConfigGroupBox_syncModeGroupBox_QGroupBox,
        "name": "syncEverythingRadioButton",
        "type": "QRadioButton",
        "visible": 1,
    }

    @staticmethod
    def sanitizeFolderPath(folderPath):
        return folderPath.rstrip("/")

    @staticmethod
    def addServer(server_url):
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX),
            server_url,
        )
        AccountConnectionWizard.nextStep()

        if not get_config("ocis"):
            try:
                squish.clickButton(
                    squish.waitForObject(
                        AccountConnectionWizard.CONFIRM_INSECURE_CONNECTION_BUTTON, 1000
                    )
                )
            except:
                test.log("No insecure connection warning for server " + server_url)

    @staticmethod
    def acceptCertificate():
        squish.clickButton(squish.waitForObject(EnterPassword.ACCEPT_CERTIFICATE_YES))

    @staticmethod
    def addUserCreds(username, password, oauth=False):
        if get_config("ocis"):
            AccountConnectionWizard.oidcLogin(username, password)
        elif oauth:
            AccountConnectionWizard.oauthLogin(username, password)
        else:
            AccountConnectionWizard.basicLogin(username, password)

    @staticmethod
    def basicLogin(username, password):
        squish.mouseClick(squish.waitForObject(AccountConnectionWizard.USERNAME_BOX))
        squish.nativeType(username)
        squish.mouseClick(squish.waitForObject(AccountConnectionWizard.PASSWORD_BOX))
        squish.nativeType(password)
        AccountConnectionWizard.nextStep()

    @staticmethod
    def oidcLogin(username, password):
        AccountConnectionWizard.browserLogin(username, password, "oidc")

    @staticmethod
    def oauthLogin(username, password):
        AccountConnectionWizard.browserLogin(username, password, "oauth")

    @staticmethod
    def browserLogin(username, password, login_type=None):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.COPY_URL_TO_CLIPBOARD_BUTTON)
        )
        authorize_via_webui(username, password, login_type)

    @staticmethod
    def nextStep():
        squish.clickButton(
            squish.waitForObjectExists(AccountConnectionWizard.NEXT_BUTTON)
        )

    @staticmethod
    def selectSyncFolder(user):
        # create sync folder for user
        sync_path = createUserSyncPath(user)

        AccountConnectionWizard.selectAdvancedConfig()
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.DIRECTORY_NAME_BOX)
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.DIRECTORY_NAME_EDIT_BOX),
            sync_path,
        )
        squish.clickButton(squish.waitForObject(AccountConnectionWizard.CHOOSE_BUTTON))
        return sync_path

    @staticmethod
    def set_temp_folder_as_sync_folder(folder_name):
        sync_path = getTempResourcePath(folder_name)

        # clear the current path
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.SELECT_LOCAL_FOLDER)
        )

        squish.waitForObject(AccountConnectionWizard.SELECT_LOCAL_FOLDER).setText("")

        squish.type(
            squish.waitForObject(AccountConnectionWizard.SELECT_LOCAL_FOLDER),
            sync_path,
        )
        setCurrentUserSyncPath(sync_path)
        return sync_path

    @staticmethod
    def addAccount(account_details):
        AccountConnectionWizard.addAccountInformation(account_details)
        AccountConnectionWizard.nextStep()

    @staticmethod
    def addAccountInformation(account_details):
        if account_details["server"]:
            AccountConnectionWizard.addServer(account_details["server"])
            if get_config("ocis"):
                AccountConnectionWizard.acceptCertificate()
        if account_details["user"]:
            AccountConnectionWizard.addUserCreds(
                account_details["user"],
                account_details["password"],
                account_details["oauth"],
            )
        sync_path = ""
        if account_details["sync_folder"]:
            AccountConnectionWizard.selectAdvancedConfig()
            sync_path = AccountConnectionWizard.set_temp_folder_as_sync_folder(
                account_details["sync_folder"]
            )
        elif account_details["user"]:
            sync_path = AccountConnectionWizard.selectSyncFolder(
                account_details["user"]
            )
        if sync_path:
            # listen for sync status
            listenSyncStatusForItem(sync_path)

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
    def selectDownloadEverythingOption():
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.SYNC_EVERYTHING_RADIO_BUTTON)
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
    def isCredentialWindowVisible():
        visible = False
        try:
            if get_config("ocis"):
                squish.waitForObject(AccountConnectionWizard.OAUTH_CREDENTIAL_PAGE)
            else:
                squish.waitForObject(AccountConnectionWizard.BASIC_CREDENTIAL_PAGE)
            visible = True
        except:
            pass
        return visible

    @staticmethod
    def selectAdvancedConfig():
        squish.waitForObject(
            AccountConnectionWizard.ADVANCED_CONFIGURATION_CHECKBOX
        ).setChecked(True)

    @staticmethod
    def canChangeLocalSyncDir():
        can_change = False
        try:
            squish.waitForObjectExists(AccountConnectionWizard.SELECT_LOCAL_FOLDER)
            squish.clickButton(
                squish.waitForObject(AccountConnectionWizard.DIRECTORY_NAME_BOX)
            )
            squish.waitForObjectExists(AccountConnectionWizard.CHOOSE_BUTTON)
            can_change = True
        except:
            pass
        return can_change

    @staticmethod
    def isSyncEverythingOptionChecked():
        return squish.waitForObjectExists(
            AccountConnectionWizard.SYNC_EVERYTHING_RADIO_BUTTON
        ).checked

    @staticmethod
    def isVFSOptionChecked():
        return squish.waitForObjectExists(
            AccountConnectionWizard.VIRTUAL_FILE_RADIO_BUTTON
        ).checked

    @staticmethod
    def get_local_sync_path():
        return str(
            squish.waitForObjectExists(
                AccountConnectionWizard.SELECT_LOCAL_FOLDER
            ).displayText
        )
