import names
import squish
import object
from urllib.parse import urlparse


class AccountStatus:
    ACCOUNT_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "name": "_accountToolbox",
        "type": "QToolButton",
        "visible": 1,
    }
    ACCOUNT_MENU = {
        "type": "QMenu",
        "unnamed": 1,
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    SIGNED_OUT_TEXT_BAR = {
        "container": names.settings_stack_QStackedWidget,
        "name": "connectLabel",
        "type": "QLabel",
        "visible": 1,
    }
    REMOVE_CONNECTION_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "text": "Remove connection",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }
    REMOVE_ALL_FILES = {
        "window": names.remove_All_Files_QMessageBox,
        "text": "Remove all files",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }
    FOLDER_SYNC_CONNECTION = {
        "column": 0,
        "container": names.stack_folderList_QTreeView,
        "text": "%s",
        "type": "QModelIndex",
    }

    settingsdialogToolbutton = None

    def __init__(self, context, displayname, host=None):
        if host == None:
            host = urlparse(context.userData['localBackendUrl']).netloc
        self.settingsdialogToolbutton = {
            "name": "settingsdialog_toolbutton_" + displayname + "@" + host,
            "type": "QToolButton",
            "visible": 1,
        }
        squish.clickButton(squish.waitForObject(self.settingsdialogToolbutton))

    def accountAction(self, action):
        squish.sendEvent(
            "QMouseEvent",
            squish.waitForObject(self.ACCOUNT_BUTTON),
            squish.QEvent.MouseButtonPress,
            0,
            0,
            squish.Qt.LeftButton,
            0,
            0,
        )
        squish.activateItem(squish.waitForObjectItem(self.ACCOUNT_MENU, action))

    def removeConnection(self):
        self.accountAction("Remove")
        squish.clickButton(squish.waitForObject(self.REMOVE_CONNECTION_BUTTON))
        squish.waitFor(
            lambda: (not object.exists(self.settingsdialogToolbutton)),
        )

    def getText(self):
        return str(squish.waitForObjectExists(self.settingsdialogToolbutton).text)

    @staticmethod
    def confirmRemoveAllFiles():
        squish.clickButton(squish.waitForObject(AccountStatus.REMOVE_ALL_FILES))

    @staticmethod
    def openAccountMenu(context):
        # The account menu does not have its unique identifier
        # So we are clicking at (718, 27) of "stack_folderList_QTreeView" object
        item_text = "Personal" if context.userData['ocis'] else "ownCloud"
        squish.mouseClick(
            squish.waitForObjectItem(names.stack_folderList_QTreeView, item_text),
            718,
            27,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
