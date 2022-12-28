import names
import squish
from helpers.SetupClientHelper import getClientDetails, createUserSyncPath


class SyncConnectionWizard:
    ADD_FOLDER_SYNC_CONNECTION_WIZARD = {
        "name": "FolderWizardSourcePage",
        "type": "OCC::FolderWizardLocalPath",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    CHOOSE_LOCAL_SYNC_FOLDER = {
        "name": "localFolderLineEdit",
        "type": "QLineEdit",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    NEXT_BUTTON = {
        "name": "__qt__passive_wizardbutton1",
        "type": "QPushButton",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    SYNC_DIALOG_ROOT_FOLDER = {
        "column": 0,
        "container": names.add_Folder_Sync_Connection_Deselect_remote_folders_you_do_not_wish_to_synchronize_QTreeWidget,
        "text": "ownCloud",
        "type": "QModelIndex",
    }
    SYNC_DIALOG_FOLDER_TREE = {
        "column": 0,
        "container": names.deselect_remote_folders_you_do_not_wish_to_synchronize_ownCloud_QModelIndex,
        "type": "QModelIndex",
    }
    ADD_SYNC_CONNECTION_BUTTON = {
        "name": "qt_wizard_finish",
        "type": "QPushButton",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    REMOTE_FOLDER_TREE = {
        "container": names.add_Folder_Sync_Connection_groupBox_QGroupBox,
        "name": "folderTreeWidget",
        "type": "QTreeWidget",
        "visible": 1,
    }
    SELECTIVE_SYNC_FOLDER_TREE = {
        "container": names.add_Folder_Sync_Connection_Deselect_remote_folders_you_do_not_wish_to_synchronize_QTreeWidget,
        "orientation": 1,
        "type": "QHeaderView",
        "unnamed": 1,
        "visible": 1,
    }

    def setSyncPathInSyncConnectionWizard(self, context):
        squish.waitForObject(self.ADD_FOLDER_SYNC_CONNECTION_WIZARD)
        squish.type(
            self.CHOOSE_LOCAL_SYNC_FOLDER, context.userData['currentUserSyncPath']
        )
        self.nextStep()

    def nextStep(self):
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def selectRemoteDestinationFolder(self, folder):
        squish.mouseClick(
            squish.waitForObjectItem(SyncConnectionWizard.REMOTE_FOLDER_TREE, folder),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        self.nextStep()

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
                    "container": SyncConnectionWizard.SELECTIVE_SYNC_FOLDER_TREE,
                    "text": headerText,
                    "type": "HeaderViewItem",
                    "visible": True,
                }
            )
        )

    def addSyncConnection(self):
        squish.clickButton(squish.waitForObject(self.ADD_SYNC_CONNECTION_BUTTON))
