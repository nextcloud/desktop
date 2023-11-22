import names
import squish


class AccountSetting:
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
    ACCOUNT_CONNECTION_LABEL = {
        "container": names.settings_stack_QStackedWidget,
        "name": "connectLabel",
        "type": "QLabel",
        "visible": 1,
    }
    LOG_BROWSER_WINDOW = {
        "name": "OCC__LogBrowser",
        "type": "OCC::LogBrowser",
        "visible": 1,
    }
    LOGIN_DIALOG_LOGOUT_BUTTON = {
        "text": "Log out",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.loginRequiredDialog_OCC_LoginRequiredDialog,
    }
    ACCOUNT_LOADING = {
        "window": names.settings_OCC_SettingsDialog,
        "name": "loadingPage",
        "type": "QWidget",
        "visible": 0,
    }

    @staticmethod
    def accountAction(action):
        squish.sendEvent(
            "QMouseEvent",
            squish.waitForObject(AccountSetting.ACCOUNT_BUTTON),
            squish.QEvent.MouseButtonPress,
            0,
            0,
            squish.Qt.LeftButton,
            0,
            0,
        )
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
    def logoutFromLoginRequiredDialog():
        squish.clickButton(
            squish.waitForObject(AccountSetting.LOGIN_DIALOG_LOGOUT_BUTTON)
        )

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
        signedout_text = 'Signed out from <a href="{server}">{server}</a>.'.format(
            server=server
        )
        return signedout_text == AccountSetting.getAccountConnectionLabel()

    @staticmethod
    def isUserSignedIn(displayname, server):
        signedin_text = 'Connected to <a href="{server}">{server}</a>.'.format(
            server=server
        )
        return signedin_text == AccountSetting.getAccountConnectionLabel()

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
    def confirmRemoveAllFiles():
        squish.clickButton(squish.waitForObject(AccountSetting.REMOVE_ALL_FILES))

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
