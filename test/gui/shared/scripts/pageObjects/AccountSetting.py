import names
import squish


class AccountSetting:
    MANAGE_ACCOUNT_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "name": "manageAccountButton",
        "type": "QToolButton",
        "visible": 1,
    }
    ACCOUNT_MENU = {
        "type": "QMenu",
        "unnamed": 1,
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    REMOVE_CONNECTION_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "text": "Remove connection",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }
    ACCOUNT_CONNECTION_LABEL = {
        "container": names.settings_stack_QStackedWidget,
        "name": "connectionStatusLabel",
        "type": "QLabel",
        "visible": 1,
    }
    LOG_BROWSER_WINDOW = {
        "name": "OCC__LogBrowser",
        "type": "OCC::LogBrowser",
        "visible": 1,
    }
    ACCOUNT_LOADING = {
        "window": names.settings_OCC_SettingsDialog,
        "name": "loadingPage",
        "type": "QWidget",
        "visible": 0,
    }
    DIALOG_STACK = {
        "name": "dialogStack",
        "type": "QStackedWidget",
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    CONFIRMATION_YES_BUTTON = {"text": "Yes", "type": "QPushButton", "visible": 1}

    @staticmethod
    def accountAction(action):
        squish.clickButton(squish.waitForObject(AccountSetting.MANAGE_ACCOUNT_BUTTON))
        squish.activateItem(
            squish.waitForObjectItem(AccountSetting.ACCOUNT_MENU, action)
        )

    @staticmethod
    def removeAccountConnection():
        AccountSetting.accountAction("Remove")
        squish.clickButton(
            squish.waitForObject(AccountSetting.REMOVE_CONNECTION_BUTTON)
        )

    @staticmethod
    def logout():
        AccountSetting.accountAction("Log out")

    @staticmethod
    def login():
        AccountSetting.accountAction("Log in")

    @staticmethod
    def getAccountConnectionLabel():
        return str(
            squish.waitForObjectExists(AccountSetting.ACCOUNT_CONNECTION_LABEL).text
        )

    @staticmethod
    def isConnecting():
        return "Connecting to" in AccountSetting.getAccountConnectionLabel()

    @staticmethod
    def isUserSignedOut(displayname, server):
        return 'Signed out' in AccountSetting.getAccountConnectionLabel()

    @staticmethod
    def isUserSignedIn(displayname, server):
        return 'Connected' in AccountSetting.getAccountConnectionLabel()

    @staticmethod
    def waitUntilConnectionIsConfigured(timeout=5000):
        result = squish.waitFor(
            lambda: AccountSetting.isConnecting(),
            timeout,
        )

        if not result:
            raise Exception(
                "Timeout waiting for connection to be configured for "
                + str(timeout)
                + " milliseconds"
            )

    @staticmethod
    def waitUntilAccountIsConnected(displayname, server, timeout=5000):
        result = squish.waitFor(
            lambda: AccountSetting.isUserSignedIn(displayname, server),
            timeout,
        )

        if not result:
            raise TimeoutError(
                "Timeout waiting for the account to be connected for "
                + str(timeout)
                + " milliseconds"
            )
        return result

    @staticmethod
    def wait_until_sync_folder_is_configured(timeout=5000):
        result = squish.waitFor(
            lambda: not squish.waitForObjectExists(
                AccountSetting.ACCOUNT_LOADING
            ).visible,
            timeout,
        )

        if not result:
            raise TimeoutError(
                "Timeout waiting for sync folder to be connected for "
                + str(timeout)
                + " milliseconds"
            )
        return result

    @staticmethod
    def pressKey(key):
        key = "<%s>" % key.replace('"', "")
        squish.nativeType(key)

    @staticmethod
    def isLogDialogVisible():
        visible = False
        try:
            visible = squish.waitForObjectExists(
                AccountSetting.LOG_BROWSER_WINDOW
            ).visible
        except:
            pass
        return visible
