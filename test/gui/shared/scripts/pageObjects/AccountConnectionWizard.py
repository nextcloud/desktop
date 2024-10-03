import test
import names
import squish

from pageObjects.EnterPassword import EnterPassword

from helpers.WebUIHelper import authorize_via_webui
from helpers.ConfigHelper import get_config
from helpers.SetupClientHelper import (
    create_user_sync_path,
    get_temp_resource_path,
    set_current_user_sync_path,
)
from helpers.SyncHelper import listen_sync_status_for_item


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
    def add_server(server_url):
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)
        )
        squish.type(
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX),
            server_url,
        )
        AccountConnectionWizard.next_step()

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
    def accept_certificate():
        squish.clickButton(squish.waitForObject(EnterPassword.ACCEPT_CERTIFICATE_YES))

    @staticmethod
    def add_user_credentials(username, password, oauth=False):
        if get_config("ocis"):
            AccountConnectionWizard.oidc_login(username, password)
        elif oauth:
            AccountConnectionWizard.oauth_login(username, password)
        else:
            AccountConnectionWizard.basic_login(username, password)

    @staticmethod
    def basic_login(username, password):
        squish.mouseClick(squish.waitForObject(AccountConnectionWizard.USERNAME_BOX))
        squish.nativeType(username)
        squish.mouseClick(squish.waitForObject(AccountConnectionWizard.PASSWORD_BOX))
        squish.nativeType(password)
        AccountConnectionWizard.next_step()

    @staticmethod
    def oidc_login(username, password):
        AccountConnectionWizard.browser_login(username, password, "oidc")

    @staticmethod
    def oauth_login(username, password):
        AccountConnectionWizard.browser_login(username, password, "oauth")

    @staticmethod
    def browser_login(username, password, login_type=None):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.COPY_URL_TO_CLIPBOARD_BUTTON)
        )
        authorize_via_webui(username, password, login_type)

    @staticmethod
    def next_step():
        squish.clickButton(
            squish.waitForObjectExists(AccountConnectionWizard.NEXT_BUTTON)
        )

    @staticmethod
    def select_sync_folder(user):
        # create sync folder for user
        sync_path = create_user_sync_path(user)

        AccountConnectionWizard.select_advanced_config()
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
        sync_path = get_temp_resource_path(folder_name)

        # clear the current path
        squish.mouseClick(
            squish.waitForObject(AccountConnectionWizard.SELECT_LOCAL_FOLDER)
        )

        squish.waitForObject(AccountConnectionWizard.SELECT_LOCAL_FOLDER).setText("")

        squish.type(
            squish.waitForObject(AccountConnectionWizard.SELECT_LOCAL_FOLDER),
            sync_path,
        )
        set_current_user_sync_path(sync_path)
        return sync_path

    @staticmethod
    def add_account(account_details):
        AccountConnectionWizard.add_account_information(account_details)
        AccountConnectionWizard.next_step()

    @staticmethod
    def add_account_information(account_details):
        if account_details["server"]:
            AccountConnectionWizard.add_server(account_details["server"])
            if get_config("ocis"):
                AccountConnectionWizard.accept_certificate()
        if account_details["user"]:
            AccountConnectionWizard.add_user_credentials(
                account_details["user"],
                account_details["password"],
                account_details["oauth"],
            )
        sync_path = ""
        if account_details["sync_folder"]:
            AccountConnectionWizard.select_advanced_config()
            sync_path = AccountConnectionWizard.set_temp_folder_as_sync_folder(
                account_details["sync_folder"]
            )
        elif account_details["user"]:
            sync_path = AccountConnectionWizard.select_sync_folder(
                account_details["user"]
            )
        if sync_path:
            # listen for sync status
            listen_sync_status_for_item(sync_path)

    @staticmethod
    def select_manual_sync_folder_option():
        squish.clickButton(
            squish.waitForObject(
                AccountConnectionWizard.CONF_SYNC_MANUALLY_RADIO_BUTTON
            )
        )

    @staticmethod
    def select_vfs_option():
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.VIRTUAL_FILE_RADIO_BUTTON)
        )

    @staticmethod
    def select_download_everything_option():
        squish.clickButton(
            squish.waitForObject(AccountConnectionWizard.SYNC_EVERYTHING_RADIO_BUTTON)
        )

    @staticmethod
    def get_error_message():
        return str(squish.waitForObjectExists(AccountConnectionWizard.ERROR_LABEL).text)

    @staticmethod
    def is_new_connection_window_visible():
        visible = False
        try:
            squish.waitForObject(AccountConnectionWizard.SERVER_ADDRESS_BOX)
            visible = True
        except:
            pass
        return visible

    @staticmethod
    def is_credential_window_visible():
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
    def select_advanced_config():
        squish.waitForObject(
            AccountConnectionWizard.ADVANCED_CONFIGURATION_CHECKBOX
        ).setChecked(True)

    @staticmethod
    def can_change_local_sync_dir():
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
    def is_sync_everything_option_checked():
        return squish.waitForObjectExists(
            AccountConnectionWizard.SYNC_EVERYTHING_RADIO_BUTTON
        ).checked

    @staticmethod
    def is_vfs_option_checked():
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
