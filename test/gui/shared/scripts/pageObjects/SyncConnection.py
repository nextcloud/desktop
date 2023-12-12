import names
import squish

from helpers.ConfigHelper import get_config


class SyncConnection:
    FOLDER_SYNC_CONNECTION = {
        "container": names.settings_stack_QStackedWidget,
        "name": "_folderList",
        "type": "QTreeView",
        "visible": 1,
    }
    MENU = {
        "type": "QMenu",
        "window": names.settings_OCC_SettingsDialog,
        "visible": 1,
    }
    DISABLE_VFS_CONFIRMATION_BUTTON = {
        "text": "Disable support",
        "type": "QPushButton",
        "visible": 1,
        "window": names.disable_virtual_file_support_QMessageBox,
    }

    @staticmethod
    def openMenu():
        squish.openContextMenu(
            squish.waitForObjectItem(
                SyncConnection.FOLDER_SYNC_CONNECTION,
                get_config('syncConnectionName'),
            ),
            0,
            0,
            squish.Qt.NoModifier,
        )

    @staticmethod
    def performAction(action):
        SyncConnection.openMenu()
        squish.activateItem(squish.waitForObjectItem(SyncConnection.MENU, action))

    @staticmethod
    def pauseSync():
        SyncConnection.performAction("Pause sync")

    @staticmethod
    def resumeSync():
        SyncConnection.performAction("Resume sync")

    @staticmethod
    def enableVFS():
        SyncConnection.performAction("Enable virtual file support...")

    @staticmethod
    def disableVFS():
        SyncConnection.performAction("Disable virtual file support...")
        squish.clickButton(
            squish.waitForObject(SyncConnection.DISABLE_VFS_CONFIRMATION_BUTTON)
        )

    @staticmethod
    def hasMenuItem(item):
        return squish.waitForObjectItem(SyncConnection.MENU, item)
