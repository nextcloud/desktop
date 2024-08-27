import squish, object, names
from helpers.SetupClientHelper import wait_until_app_killed
from helpers.ConfigHelper import get_config


class Toolbar:
    TOOLBAR_ROW = {
        "container": names.dialogStack_quickWidget_OCC_QmlUtils_OCQuickWidget,
        "type": "RowLayout",
        "visible": True,
    }
    ACCOUNT_BUTTON = {
        "checkable": False,
        "container": names.dialogStack_quickWidget_OCC_QmlUtils_OCQuickWidget,
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
        squish.mouseClick(squish.waitForObject(Toolbar.ACTIVITY_BUTTON))

    @staticmethod
    def openNewAccountSetup():
        squish.mouseClick(squish.waitForObject(Toolbar.ADD_ACCOUNT_BUTTON))

    @staticmethod
    def openAccount(displayname):
        _, selector = Toolbar.get_account(displayname)
        squish.mouseClick(squish.waitForObject(selector))

    @staticmethod
    def getDisplayedAccountText(displayname, host):
        return str(
            squish.waitForObjectExists(
                Toolbar.getItemSelector(displayname + "\n" + host)
            ).text
        )

    @staticmethod
    def open_settings_tab():
        squish.mouseClick(squish.waitForObject(Toolbar.SETTINGS_BUTTON))

    @staticmethod
    def quit_owncloud():
        squish.mouseClick(squish.waitForObject(Toolbar.QUIT_BUTTON))
        squish.clickButton(squish.waitForObject(Toolbar.CONFIRM_QUIT_BUTTON))
        for ctx in squish.applicationContextList():
            pid = ctx.pid
            ctx.detach()
            wait_until_app_killed(pid)

    @staticmethod
    def get_accounts():
        accounts = {}
        selectors = {}
        children_obj = object.children(squish.waitForObject(Toolbar.TOOLBAR_ROW))
        account_idx = 1
        for obj in children_obj:
            if hasattr(obj, "accountState"):
                account_info = {
                    'displayname': str(obj.accountState.account.davDisplayName),
                    'username': str(obj.accountState.account.davUser),
                    'hostname': str(obj.accountState.account.hostname),
                    'initials': str(obj.accountState.account.initials),
                    'current': obj.checked,
                }
                account_locator = Toolbar.ACCOUNT_BUTTON.copy()
                if account_idx > 1:
                    account_locator.update({"occurrence": account_idx})
                account_locator.update({"text": account_info['hostname']})

                accounts[account_info['displayname']] = account_info
                selectors[account_info['displayname']] = account_locator
                account_idx += 1
        return accounts, selectors

    @staticmethod
    def get_account(display_name):
        accounts, selectors = Toolbar.get_accounts()
        return accounts.get(display_name), selectors.get(display_name)

    @staticmethod
    def account_has_focus(display_name):
        account, selector = Toolbar.get_account(display_name)
        return account['current'] and squish.waitForObjectExists(selector).checked

    @staticmethod
    def account_exists(display_name):
        account, selector = Toolbar.get_account(display_name)
        if (
            account is None
            or selector is None
            and account['displayname'] != display_name
        ):
            raise LookupError(f'Account "{display_name}" does not exist')
        squish.waitForObjectExists(selector)
