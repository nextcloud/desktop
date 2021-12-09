import names
import squish
from helpers.SetupClientHelper import getClientDetails, createUserSyncPath
import test


class AccountConnectionWizard:
    SERVER_ADDRESS_BOX = names.leUrl_OCC_PostfixLineEdit
    NEXT_BUTTON = names.owncloudWizard_qt_passive_wizardbutton1_QPushButton
    USERNAME_BOX = names.leUsername_QLineEdit
    PASSWORD_BOX = names.lePassword_QLineEdit
    SELECT_LOCAL_FOLDER = names.pbSelectLocalFolder_QPushButton
    DIRECTORY_NAME_BOX = names.fileNameEdit_QLineEdit
    CHOOSE_BUTTON = names.qFileDialog_Choose_QPushButton
    FINISH_BUTTON = {
        "name": "qt_wizard_finish",
        "type": "QPushButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    ERROR_OK_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.error_QMessageBox,
    }
    ERROR_LABEL = {
        "name": "errorLabel",
        "type": "QLabel",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    CREDENTIAL_PAGE = {
        "name": "OwncloudHttpCredsPage",
        "type": "OCC::OwncloudHttpCredsPage",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    ADVANCE_SETUP_PAGE = {
        "name": "OwncloudAdvancedSetupPage",
        "type": "OCC::OwncloudAdvancedSetupPage",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
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
    SELECTIVE_SYNC_DIALOG = names.choose_What_to_Sync_OCC_SelectiveSyncDialog
    SYNC_DIALOG_FOLDER_TREE = names.choose_What_To_Synchronize_QTreeWidget
    SYNC_DIALOG_ROOT_FOLDER = {
        "column": 0,
        "container": SYNC_DIALOG_FOLDER_TREE,
        "text": "/",
        "type": "QModelIndex",
    }
    SYNC_DIALOG_OK_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": SELECTIVE_SYNC_DIALOG,
    }

    def __init__(self):
        pass

    def sanitizeFolderPath(self, folderPath):
        return folderPath.rstrip("/")

    def addServer(self, context):
        clientDetails = getClientDetails(context)
        squish.mouseClick(squish.waitForObject(self.SERVER_ADDRESS_BOX))
        squish.type(
            squish.waitForObject(self.SERVER_ADDRESS_BOX), clientDetails['server']
        )
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def addUserCreds(self, context):
        clientDetails = getClientDetails(context)

        squish.type(squish.waitForObject(self.USERNAME_BOX), clientDetails['user'])
        squish.type(squish.waitForObject(self.USERNAME_BOX), "<Tab>")
        squish.type(squish.waitForObject(self.PASSWORD_BOX), clientDetails['password'])
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def selectSyncFolder(self, context):
        clientDetails = getClientDetails(context)
        # create sync folder for user
        syncPath = createUserSyncPath(context, clientDetails['user'])

        try:
            oldwaitForObjectTimeout = squish.testSettings.waitForObjectTimeout
            squish.testSettings.waitForObjectTimeout = 1000
            squish.clickButton(squish.waitForObject(self.ERROR_OK_BUTTON))
        except LookupError:
            squish.testSettings.waitForObjectTimeout = oldwaitForObjectTimeout
            pass
        squish.clickButton(squish.waitForObject(self.SELECT_LOCAL_FOLDER))
        squish.mouseClick(squish.waitForObject(self.DIRECTORY_NAME_BOX))
        squish.type(squish.waitForObject(self.DIRECTORY_NAME_BOX), syncPath)
        squish.clickButton(squish.waitForObject(self.CHOOSE_BUTTON))
        test.compare(
            str(squish.waitForObjectExists(self.SELECT_LOCAL_FOLDER).text),
            self.sanitizeFolderPath(syncPath),
        )

    def connectAccount(self):
        squish.clickButton(squish.waitForObject(self.FINISH_BUTTON))

    def addAccount(self, context):
        self.addServer(context)
        self.addUserCreds(context)
        self.selectSyncFolder(context)
        self.connectAccount()

    def openSyncDialog(self):
        squish.clickButton(squish.waitForObject(self.CHOOSE_WHAT_TO_SYNC_BUTTON))

    def selectManualSyncFolder(self):
        squish.clickButton(squish.waitForObject(self.MANUAL_SYNC_FOLDER_OPTION))

    def selectFoldersToSync(self, context):
        self.openSyncDialog()

        # first deselect all
        squish.mouseClick(
            squish.waitForObject(self.SYNC_DIALOG_ROOT_FOLDER),
            11,
            11,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        for row in context.table[1:]:
            squish.mouseClick(
                squish.waitForObjectItem(self.SYNC_DIALOG_FOLDER_TREE, "/." + row[0]),
                11,
                11,
                squish.Qt.NoModifier,
                squish.Qt.LeftButton,
            )
        squish.clickButton(squish.waitForObject(self.SYNC_DIALOG_OK_BUTTON))

    def sortBy(self, headerText):
        squish.mouseClick(
            squish.waitForObject(
                {
                    "container": names.deselect_remote_folders_you_do_not_wish_to_synchronize_QHeaderView,
                    "text": headerText,
                    "type": "HeaderViewItem",
                    "visible": True,
                }
            )
        )
