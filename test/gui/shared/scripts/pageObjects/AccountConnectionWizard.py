import names
import squish
from helpers.SetupClientHelper import getClientDetails, createUserSyncPath
from helpers.WebUIHelper import authorize_via_webui
import test


class AccountConnectionWizard:
    SERVER_ADDRESS_BOX = names.contentWidget_urlLineEdit_QLineEdit
    NEXT_BUTTON = names.setupWizardWindow_nextButton_QPushButton
    CONFIRM_INSECURE_CONNECTION_BUTTON = names.insecure_connection_Confirm_QPushButton
    USERNAME_BOX = names.contentWidget_usernameLineEdit_QLineEdit
    PASSWORD_BOX = names.contentWidget_passwordLineEdit_QLineEdit
    SELECT_LOCAL_FOLDER = names.localDirectoryGroupBox_localDirectoryLineEdit_QLineEdit
    DIRECTORY_NAME_BOX = (
        names.localDirectoryGroupBox_chooseLocalDirectoryButton_QToolButton
    )
    CHOOSE_BUTTON = names.qFileDialog_Choose_QPushButton
    FINISH_BUTTON = {
        "name": "qt_wizard_finish",
        "type": "QPushButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
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
    ADVANCE_SETUP_PAGE = {
        "name": "OwncloudAdvancedSetupPage",
        "type": "OCC::OwncloudAdvancedSetupPage",
        "visible": 1,
        "container": names.setupWizardWindow_contentWidget_QStackedWidget,
    }
    MANUAL_SYNC_FOLDER_OPTION = {
        "name": "rManualFolder",
        "type": "QRadioButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    CHOOSE_WHAT_TO_SYNC_BUTTON = {
        "name": "bSelectiveSync",
        "type": "QPushButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    CHOOSE_LOCAL_SYNC_FOLDER = {
        "name": "localFolderLineEdit",
        "type": "QLineEdit",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    SELECTIVE_SYNC_DIALOG = names.choose_What_to_Sync_OCC_SelectiveSyncDialog
    SELECT_REMOTE_DESTINATION_FOLDER_WIZARD = (
        names.add_Folder_Sync_Connection_groupBox_QGroupBox
    )
    ADD_FOLDER_SYNC_CONNECTION_NEXT_BUTTON = (
        names.add_Folder_Sync_Connection_qt_passive_wizardbutton1_QPushButton
    )
    CONF_SYNC_MANUALLY_RADIO_BUTTON = (
        names.syncModeGroupBox_configureSyncManuallyRadioButton_QRadioButton
    )
    ADD_FOLDER_SYNC_CONNECTION_WIZARD = (
        names.add_Folder_Sync_Connection_FolderWizardSourcePage_OCC_FolderWizardLocalPath
    )
    ADD_SYNC_CONNECTION_BUTTON = {
        "name": "qt_wizard_finish",
        "type": "QPushButton",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    SYNC_DIALOG_FOLDER_TREE = {
        "column": 0,
        "container": names.deselect_remote_folders_you_do_not_wish_to_synchronize_ownCloud_QModelIndex,
        "type": "QModelIndex",
    }
    SYNC_DIALOG_ROOT_FOLDER = {
        "column": 0,
        "container": names.add_Folder_Sync_Connection_Deselect_remote_folders_you_do_not_wish_to_synchronize_QTreeWidget,
        "text": "ownCloud",
        "type": "QModelIndex",
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
    VIRTUAL_FILE_RADIO_BUTTON = names.syncModeGroupBox_useVfsRadioButton_QRadioButton
    ENABLE_EXPERIMENTAL_FEATURE_BUTTON = (
        names.contentWidget_Enable_experimental_placeholder_mode_QPushButton
    )
    STAY_SAFE_BUTTON = names.contentWidget_Stay_safe_QPushButton

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
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

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
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def oidcLogin(self, username, password, relogin=False):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        if relogin:
            self.COPY_URL_TO_CLIPBOARD_BUTTON[
                "container"
            ] = names.loginRequiredDialog_contentWidget_QStackedWidget
            squish.clickButton(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))
        else:
            self.COPY_URL_TO_CLIPBOARD_BUTTON[
                "container"
            ] = names.contentWidget_contentWidget_QStackedWidget
            squish.clickButton(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))

        authorize_via_webui(username, password)

    def finishSetup(self):
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
        self.finishSetup()

    def addAccountCredential(self, context):
        self.addServer(context)
        if context.userData['ocis']:
            self.acceptCertificate()
        self.addUserCreds(context)
        self.selectSyncFolder(context)

    def selectManualSyncFolder(self):
        squish.clickButton(squish.waitForObject(self.CONF_SYNC_MANUALLY_RADIO_BUTTON))

    def selectFoldersToSync(self, context):
        # first deselect all
        squish.mouseClick(
            squish.waitForObject(self.SYNC_DIALOG_ROOT_FOLDER),
            11,
            11,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        for row in context.table[1:]:
            self.SYNC_DIALOG_FOLDER_TREE['text'] = row[
                0
            ]  # added a new key 'text' to dictionary SYNC_DIALOG_FOLDER_TREE
            squish.mouseClick(
                squish.waitForObject(self.SYNC_DIALOG_FOLDER_TREE),
                11,
                11,
                squish.Qt.NoModifier,
                squish.Qt.LeftButton,
            )

    def sortBy(self, headerText):
        squish.mouseClick(
            squish.waitForObject(
                {
                    "container": names.deselect_remote_folders_you_do_not_wish_to_synchronize_QHeaderView_2,
                    "text": headerText,
                    "type": "HeaderViewItem",
                    "visible": True,
                }
            )
        )

    def selectARootSyncDirectory(self, folderName):
        squish.mouseClick(
            squish.waitForObjectItem(
                names.groupBox_folderTreeWidget_QTreeWidget, folderName
            ),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
