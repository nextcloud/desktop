import names
import squish


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
    ENABLE_VFS_CONFIRMATION_BUTTON = {
        "text": "Enable experimental placeholder mode",
        "type": "QPushButton",
        "visible": 1,
        "window": names.enable_experimental_feature_QMessageBox,
    }
    DISABLE_VFS_CONFIRMATION_BUTTON = {
        "text": "Disable support",
        "type": "QPushButton",
        "visible": 1,
        "window": names.disable_virtual_file_support_QMessageBox,
    }

    @staticmethod
    def openMenu(context):
        squish.openContextMenu(
            squish.waitForObjectItem(
                SyncConnection.FOLDER_SYNC_CONNECTION,
                context.userData['syncConnectionName'],
            ),
            0,
            0,
            squish.Qt.NoModifier,
        )

    @staticmethod
    def performAction(context, action):
        SyncConnection.openMenu(context)
        squish.activateItem(squish.waitForObjectItem(SyncConnection.MENU, action))

    @staticmethod
    def pauseSync(context):
        SyncConnection.performAction(context, "Pause sync")

    @staticmethod
    def resumeSync(context):
        SyncConnection.performAction(context, "Resume sync")

    @staticmethod
    def enableVFS(context):
        SyncConnection.performAction(
            context, "Enable virtual file support (experimental)..."
        )
        squish.clickButton(
            squish.waitForObject(SyncConnection.ENABLE_VFS_CONFIRMATION_BUTTON)
        )

    @staticmethod
    def disableVFS(context):
        SyncConnection.performAction(context, "Disable virtual file support...")
        squish.clickButton(
            squish.waitForObject(SyncConnection.DISABLE_VFS_CONFIRMATION_BUTTON)
        )

    @staticmethod
    def hasMenuItem(item):
        return squish.waitForObjectItem(SyncConnection.MENU, item)
