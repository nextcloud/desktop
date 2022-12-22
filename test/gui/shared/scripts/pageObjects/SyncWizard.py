import names
import squish


class SyncWizard:
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
        item_text = "Personal" if context.userData['ocis'] else "ownCloud"
        squish.openContextMenu(
            squish.waitForObjectItem(SyncWizard.FOLDER_SYNC_CONNECTION, item_text),
            0,
            0,
            squish.Qt.NoModifier,
        )

    @staticmethod
    def performAction(context, action):
        SyncWizard.openMenu(context)
        squish.activateItem(squish.waitForObjectItem(SyncWizard.MENU, action))

    @staticmethod
    def pauseSync(context):
        SyncWizard.performAction(context, "Pause sync")

    @staticmethod
    def resumeSync(context):
        SyncWizard.performAction(context, "Resume sync")

    @staticmethod
    def enableVFS(context):
        SyncWizard.performAction(
            context, "Enable virtual file support (experimental)..."
        )
        squish.clickButton(
            squish.waitForObject(SyncWizard.ENABLE_VFS_CONFIRMATION_BUTTON)
        )

    @staticmethod
    def disableVFS(context):
        SyncWizard.performAction(context, "Disable virtual file support...")
        squish.clickButton(
            squish.waitForObject(SyncWizard.DISABLE_VFS_CONFIRMATION_BUTTON)
        )

    @staticmethod
    def hasMenuItem(item):
        return squish.waitForObjectItem(SyncWizard.MENU, item)
