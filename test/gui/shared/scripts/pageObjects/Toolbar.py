import squish, object, names
from helpers.SetupClientHelper import wait_until_app_killed
from helpers.ObjectHelper import get_center_coordinates
from helpers.ConfigHelper import get_config


class Toolbar:
    TOOLBAR = {
        "container": names.settings_dialogStack_QStackedWidget,
        "name": "quickWidget",
        "type": "QQuickWidget",
        "visible": 1,
    }
    ACCOUNT_BUTTON = {
        "checkable": False,
        "container": names.dialogStack_quickWidget_QQuickWidget,
        "type": "AccountButton",
        "visible": True,
    }
    ADD_ACCOUNT_BUTTON = {
        "container": names.dialogStack_quickWidget_QQuickWidget,
        "id": "addAccountButton",
        "type": "AccountButton",
        "visible": True,
    }
    ACTIVITY_BUTTON = {
        "container": names.dialogStack_quickWidget_QQuickWidget,
        "id": "logButton",
        "type": "AccountButton",
        "visible": True,
    }
    SETTINGS_BUTTON = {
        "container": names.dialogStack_quickWidget_QQuickWidget,
        "id": "settingsButton",
        "type": "AccountButton",
        "visible": True,
    }
    QUIT_BUTTON = {
        "container": names.dialogStack_quickWidget_QQuickWidget,
        "id": "quitButton",
        "type": "AccountButton",
        "visible": True,
    }
    QUIT_CONFIRMATION_DIALOG = {
        "type": "QMessageBox",
        "unnamed": 1,
        "visible": 1,
        "windowTitle": "Quit ownCloud",
    }
    CONFIRM_QUIT_BUTTON = {
        "text": "Yes",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": QUIT_CONFIRMATION_DIALOG,
    }

    TOOLBAR_ITEMS = ["Add Account", "Activity", "Settings", "Quit"]

    @staticmethod
    def getItemSelector(item_name):
        return {
            "container": names.dialogStack_quickWidget_QQuickWidget,
            "text": item_name,
            "type": "Label",
            "visible": True,
        }

    @staticmethod
    def hasItem(item_name, timeout=get_config("minSyncTimeout") * 1000):
        try:
            squish.waitForObject(Toolbar.getItemSelector(item_name), timeout)
            return True
        except:
            return False

    @staticmethod
    def openActivity():
        x, y = get_center_coordinates(squish.waitForObject(Toolbar.ACTIVITY_BUTTON))
        squish.mouseClick(
            squish.waitForObject(Toolbar.ACTIVITY_BUTTON), x, y, squish.Qt.LeftButton
        )

    @staticmethod
    def openNewAccountSetup():
        x, y = get_center_coordinates(squish.waitForObject(Toolbar.ADD_ACCOUNT_BUTTON))
        squish.mouseClick(
            squish.waitForObject(Toolbar.ADD_ACCOUNT_BUTTON), x, y, squish.Qt.LeftButton
        )

    @staticmethod
    def openAccount(displayname, host):
        account_title = displayname + "\n" + host
        x, y = get_center_coordinates(
            squish.waitForObject(Toolbar.getItemSelector(account_title))
        )
        squish.mouseClick(
            squish.waitForObject(Toolbar.getItemSelector(account_title)),
            x,
            y,
            squish.Qt.LeftButton,
        )

    @staticmethod
    def getDisplayedAccountText(displayname, host):
        return str(
            squish.waitForObjectExists(
                Toolbar.getItemSelector(displayname + "\n" + host)
            ).text
        )

    @staticmethod
    def open_settings_tab():
        x, y = get_center_coordinates(squish.waitForObject(Toolbar.SETTINGS_BUTTON))
        squish.mouseClick(
            squish.waitForObject(Toolbar.SETTINGS_BUTTON), x, y, squish.Qt.LeftButton
        )

    @staticmethod
    def quit_owncloud():
        x, y = get_center_coordinates(squish.waitForObject(Toolbar.QUIT_BUTTON))
        squish.mouseClick(
            squish.waitForObject(Toolbar.QUIT_BUTTON), x, y, squish.Qt.LeftButton
        )
        squish.clickButton(squish.waitForObject(Toolbar.CONFIRM_QUIT_BUTTON))
        for ctx in squish.applicationContextList():
            pid = ctx.pid
            ctx.detach()
            wait_until_app_killed(pid)

    @staticmethod
    def get_accounts():
        accounts = []
        children_obj = object.children(squish.waitForObject(Toolbar.TOOLBAR))
        for obj in children_obj:
            if hasattr(obj, "objectName") and str(obj.objectName).startswith(
                "settingsdialog_toolbutton"
            ):
                if not obj.text in Toolbar.TOOLBAR_ITEMS:
                    accounts.append(str(obj.text))
        return accounts

    @staticmethod
    def account_has_focus(account):
        account_locator = Toolbar.ACCOUNT_BUTTON.copy()
        account_locator.update({"text": account})
        return squish.waitForObjectExists(account_locator).checked is True
