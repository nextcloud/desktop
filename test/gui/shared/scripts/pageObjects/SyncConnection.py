import names
import squish
import object


class SyncConnection:
    FOLDER_SYNC_CONNECTION_LIST = {
        "container": names.quickWidget_scrollView_ScrollView,
        "type": "ListView",
        "visible": True,
    }
    FOLDER_SYNC_CONNECTION = {
        "container": names.settings_stack_QStackedWidget,
        "name": "_folderList",
        "type": "QListView",
        "visible": 1,
    }
    FOLDER_SYNC_CONNECTION_MENU_BUTTON = {
        "checkable": False,
        "container": names.quickWidget_scrollView_ScrollView,
        "type": "Button",
        "unnamed": 1,
        "visible": True,
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
    SELECTIVE_SYNC_APPLY_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "name": "selectiveSyncApply",
        "type": "QPushButton",
        "visible": 1,
    }

    @staticmethod
    def openMenu():
        menu_button = squish.waitForObject(
            SyncConnection.FOLDER_SYNC_CONNECTION_MENU_BUTTON
        )
        squish.mouseClick(menu_button)

    @staticmethod
    def performAction(action):
        SyncConnection.openMenu()
        squish.activateItem(squish.waitForObjectItem(SyncConnection.MENU, action))

    @staticmethod
    def forceSync():
        SyncConnection.performAction("Force sync now")

    @staticmethod
    def pauseSync():
        SyncConnection.performAction("Pause sync")

    @staticmethod
    def resumeSync():
        SyncConnection.performAction("Resume sync")

    @staticmethod
    def enableVFS():
        SyncConnection.performAction("Enable virtual file support")

    @staticmethod
    def disableVFS():
        SyncConnection.performAction("Disable virtual file support")
        squish.clickButton(
            squish.waitForObject(SyncConnection.DISABLE_VFS_CONFIRMATION_BUTTON)
        )

    @staticmethod
    def hasMenuItem(item):
        return squish.waitForObjectItem(SyncConnection.MENU, item)

    @staticmethod
    def menu_item_exists(menuItem):
        obj = SyncConnection.MENU.copy()
        obj.update({"type": "QAction", "text": menuItem})
        return object.exists(obj)

    @staticmethod
    def choose_what_to_sync():
        SyncConnection.openMenu()
        SyncConnection.performAction("Choose what to sync")

    @staticmethod
    def unselect_folder_in_selective_sync(folder_name):
        sync_folders = object.children(
            squish.waitForObject(SyncConnection.FOLDER_SYNC_CONNECTION)
        )
        for sync_folder in sync_folders:
            # TODO: allow selective sync in other sync folders as well
            if hasattr(sync_folder, "text") and sync_folder.text == "Personal":
                items = object.children(sync_folder)
                for item in items:
                    if hasattr(item, "text") and item.text:
                        # remove item size suffix
                        # example: folder1 (13 B) => folder1
                        item_name = item.text.rsplit(" ", 2)[0]
                        if item_name == folder_name:
                            # NOTE: checkbox does not have separate object
                            # click on (11,11) which is a checkbox to unselect the folder
                            squish.mouseClick(
                                item,
                                11,
                                11,
                                squish.Qt.NoModifier,
                                squish.Qt.LeftButton,
                            )
                            break
        squish.clickButton(
            squish.waitForObject(SyncConnection.SELECTIVE_SYNC_APPLY_BUTTON)
        )

    @staticmethod
    def get_folder_connection_count():
        return squish.waitForObject(SyncConnection.FOLDER_SYNC_CONNECTION_LIST).count
