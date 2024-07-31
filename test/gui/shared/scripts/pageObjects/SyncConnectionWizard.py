import names
import squish
import object
from os import path
from helpers.SetupClientHelper import (
    getCurrentUserSyncPath,
    setCurrentUserSyncPath,
)
from helpers.ConfigHelper import get_config


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
    BACK_BUTTON = {
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
        "type": "QPushButton",
        "text": "< &Back",
        "visible": 1,
    }
    NEXT_BUTTON = {
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
        "type": "QPushButton",
        "text": "&Next >",
        "visible": 1,
    }
    SELECTIVE_SYNC_ROOT_FOLDER = {
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
    VFS_CHECKBOX = {
        "text": "Use virtual files instead of downloading content immediately",
        "type": "QCheckBox",
        "unnamed": 1,
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    SELECTIVE_SYNC_TREE_HEADER = {
        "container": names.add_Folder_Sync_Connection_Deselect_remote_folders_you_do_not_wish_to_synchronize_QTreeWidget,
        "orientation": 1,
        "type": "QHeaderView",
        "unnamed": 1,
        "visible": 1,
    }
    CANCEL_FOLDER_SYNC_CONNECTION_WIZARD = {
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
        "name": "qt_wizard_cancel",
        "type": "QPushButton",
        "visible": 1,
    }
    SPACE_NAME_SELECTOR = {
        "container": names.quickWidget_scrollView_ScrollView,
        "type": "Label",
        "visible": True,
    }
    CREATE_REMOTE_FOLDER_BUTTON = {
        "container": names.add_Folder_Sync_Connection_groupBox_QGroupBox,
        "name": "addFolderButton",
        "type": "QPushButton",
        "visible": 1,
    }
    CREATE_REMOTE_FOLDER_INPUT = {
        "buddy": names.create_Remote_Folder_Enter_the_name_of_the_new_folder_to_be_created_below_QLabel,
        "type": "QLineEdit",
        "unnamed": 1,
        "visible": 1,
    }
    CREATE_REMOTE_FOLDER_CONFIRM_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.create_Remote_Folder_QInputDialog,
    }
    REFRESH_BUTTON = {
        "container": names.add_Folder_Sync_Connection_groupBox_QGroupBox,
        "name": "refreshButton",
        "type": "QPushButton",
        "visible": 1,
    }
    REMOTE_FOLDER_SELECTION_INPUT = {
        "name": "folderEntry",
        "type": "QLineEdit",
        "visible": 1,
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
    }
    ADD_FOLDER_SYNC_BUTTON = {
        "checkable": False,
        "container": names.stackedWidget_quickWidget_OCC_QmlUtils_OCQuickWidget,
        "id": "addSyncButton",
        "type": "Button",
        "unnamed": 1,
        "visible": True,
    }
    WARN_LABEL = {
        "window": names.add_Folder_Sync_Connection_OCC_FolderWizard,
        "name": "warnLabel",
        "type": "QLabel",
        "visible": 1,
    }

    @staticmethod
    def setSyncPathInSyncConnectionWizardOc10(sync_path=''):
        squish.waitForObject(SyncConnectionWizard.ADD_FOLDER_SYNC_CONNECTION_WIZARD)
        squish.waitForObject(SyncConnectionWizard.CHOOSE_LOCAL_SYNC_FOLDER).setText("")
        if sync_path:
            squish.type(SyncConnectionWizard.CHOOSE_LOCAL_SYNC_FOLDER, sync_path)
            setCurrentUserSyncPath(sync_path)
        else:
            squish.type(
                SyncConnectionWizard.CHOOSE_LOCAL_SYNC_FOLDER,
                getCurrentUserSyncPath(),
            )
        SyncConnectionWizard.nextStep()

    @staticmethod
    def setSyncPathInSyncConnectionWizard(sync_path=''):
        if get_config('ocis'):
            SyncConnectionWizard.setSyncPathInSyncConnectionWizardOcis(sync_path)
        else:
            SyncConnectionWizard.setSyncPathInSyncConnectionWizardOc10(sync_path)

    @staticmethod
    def nextStep():
        squish.clickButton(squish.waitForObject(SyncConnectionWizard.NEXT_BUTTON))

    @staticmethod
    def back():
        squish.clickButton(squish.waitForObject(SyncConnectionWizard.BACK_BUTTON))

    @staticmethod
    def selectRemoteDestinationFolder(folder):
        squish.mouseClick(
            squish.waitForObjectItem(SyncConnectionWizard.REMOTE_FOLDER_TREE, folder)
        )
        SyncConnectionWizard.nextStep()

    @staticmethod
    def deselectAllRemoteFolders():
        # NOTE: checkbox does not have separate object
        # click on (11,11) which is a checkbox
        squish.mouseClick(
            squish.waitForObject(SyncConnectionWizard.SELECTIVE_SYNC_ROOT_FOLDER),
            11,
            11,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )

    @staticmethod
    def enableOrDisableVfsSupport(action='disable'):
        if not action in ['enable', 'disable']:
            raise Exception("Invalid action: " + action)

        checked = squish.waitForObjectExists(SyncConnectionWizard.VFS_CHECKBOX).checked
        is_enable = action == 'enable'
        if is_enable == checked:
            return
        squish.clickButton(squish.waitForObject(SyncConnectionWizard.VFS_CHECKBOX))

    @staticmethod
    def selectFoldersToSync(folders):
        # first deselect all
        SyncConnectionWizard.deselectAllRemoteFolders()
        for folder in folders:
            folder_levels = folder.strip("/").split("/")
            parent_selector = None
            for sub_folder in folder_levels:
                if not parent_selector:
                    SyncConnectionWizard.SYNC_DIALOG_FOLDER_TREE['text'] = sub_folder
                    parent_selector = SyncConnectionWizard.SYNC_DIALOG_FOLDER_TREE
                    selector = parent_selector
                else:
                    selector = {
                        "column": '0',
                        "container": parent_selector,
                        "text": sub_folder,
                        "type": 'QModelIndex',
                    }
                if (
                    len(folder_levels) == 1
                    or folder_levels.index(sub_folder) == len(folder_levels) - 1
                ):
                    # NOTE: checkbox does not have separate object
                    # click on (11,11) which is a checkbox to unselect the folder
                    squish.mouseClick(
                        squish.waitForObject(selector),
                        11,
                        11,
                        squish.Qt.NoModifier,
                        squish.Qt.LeftButton,
                    )
                else:
                    squish.doubleClick(squish.waitForObject(selector))

    @staticmethod
    def sortBy(headerText):
        squish.mouseClick(
            squish.waitForObject(
                {
                    "container": SyncConnectionWizard.SELECTIVE_SYNC_TREE_HEADER,
                    "text": headerText,
                    "type": "HeaderViewItem",
                    "visible": True,
                }
            )
        )

    @staticmethod
    def addSyncConnection():
        squish.clickButton(
            squish.waitForObject(SyncConnectionWizard.ADD_SYNC_CONNECTION_BUTTON)
        )

    @staticmethod
    def getItemNameFromRow(row_index):
        FOLDER_ROW = {
            "row": row_index,
            "container": SyncConnectionWizard.SELECTIVE_SYNC_ROOT_FOLDER,
            "type": "QModelIndex",
        }
        return str(squish.waitForObjectExists(FOLDER_ROW).displayText)

    @staticmethod
    def isRootFolderChecked():
        state = squish.waitForObject(SyncConnectionWizard.SELECTIVE_SYNC_ROOT_FOLDER)[
            "checkState"
        ]
        return state == "checked"

    @staticmethod
    def cancelFolderSyncConnectionWizard():
        squish.clickButton(
            squish.waitForObject(
                SyncConnectionWizard.CANCEL_FOLDER_SYNC_CONNECTION_WIZARD
            )
        )

    @staticmethod
    def selectSpaceToSync(spaceName):
        selector = SyncConnectionWizard.SPACE_NAME_SELECTOR.copy()
        selector["text"] = spaceName
        squish.mouseClick(squish.waitForObject(selector))

    @staticmethod
    def setSyncPathInSyncConnectionWizardOcis(sync_path):
        if not sync_path:
            sync_path = path.join(
                getCurrentUserSyncPath(), get_config("syncConnectionName")
            )
        squish.type(
            squish.waitForObject(SyncConnectionWizard.CHOOSE_LOCAL_SYNC_FOLDER),
            "<Ctrl+A>",
        )
        squish.type(
            SyncConnectionWizard.CHOOSE_LOCAL_SYNC_FOLDER,
            sync_path,
        )
        SyncConnectionWizard.nextStep()

    @staticmethod
    def syncSpace(spaceName):
        SyncConnectionWizard.selectSpaceToSync(spaceName)
        SyncConnectionWizard.nextStep()
        SyncConnectionWizard.setSyncPathInSyncConnectionWizard(
            path.join(getCurrentUserSyncPath(), spaceName)
        )
        SyncConnectionWizard.addSyncConnection()

    @staticmethod
    def create_folder_in_remote_destination(folder_name):
        squish.clickButton(
            squish.waitForObject(SyncConnectionWizard.CREATE_REMOTE_FOLDER_BUTTON)
        )
        squish.type(
            squish.waitForObject(SyncConnectionWizard.CREATE_REMOTE_FOLDER_INPUT),
            folder_name,
        )
        squish.clickButton(
            squish.waitForObject(
                SyncConnectionWizard.CREATE_REMOTE_FOLDER_CONFIRM_BUTTON
            )
        )

    @staticmethod
    def refresh_remote():
        squish.clickButton(squish.waitForObject(SyncConnectionWizard.REFRESH_BUTTON))

    @staticmethod
    def generate_remote_folder_selector(folder_name, parent_container=None):
        if not parent_container:
            parent_container = {
                "container": names.groupBox_folderTreeWidget_QTreeWidget,
                "text": "ownCloud",
                "type": "QModelIndex",
            }
        return {
            "container": parent_container,
            "text": folder_name,
            "type": "QModelIndex",
        }

    @staticmethod
    def has_remote_folder(folder_name):
        folder_tree = folder_name.strip("/").split("/")
        parent_container = None

        for folder in folder_tree:
            folder_selector = SyncConnectionWizard.generate_remote_folder_selector(
                folder, parent_container
            )
            try:
                if parent_container:
                    squish.doubleClick(parent_container)

                squish.waitForObject(folder_selector)

                parent_container = folder_selector
            except:
                return False, None
        return True, parent_container

    @staticmethod
    def is_remote_folder_selected(folder_selector):
        return squish.waitForObjectExists(folder_selector).selected

    @staticmethod
    def select_space_to_sync(space_name):
        SyncConnectionWizard.selectSpaceToSync(space_name)
        SyncConnectionWizard.nextStep()

    @staticmethod
    def open_sync_connection_wizard():
        squish.mouseClick(
            squish.waitForObject(SyncConnectionWizard.ADD_FOLDER_SYNC_BUTTON)
        )

    @staticmethod
    def get_local_sync_path():
        return str(
            squish.waitForObjectExists(
                SyncConnectionWizard.CHOOSE_LOCAL_SYNC_FOLDER
            ).displayText
        )

    @staticmethod
    def get_warn_label():
        return str(squish.waitForObjectExists(SyncConnectionWizard.WARN_LABEL).text)

    @staticmethod
    def is_add_sync_folder_button_enabled():
        return squish.waitForObjectExists(
            SyncConnectionWizard.ADD_FOLDER_SYNC_BUTTON
        ).enabled
