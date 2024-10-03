import names
import squish


class AccountSetting:
    MANAGE_ACCOUNT_BUTTON = {
        "container": names.settings_dialogStack_QStackedWidget,
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
    CONFIRM_REMOVE_CONNECTION_BUTTON = {
        "container": names.settings_dialogStack_QStackedWidget,
        "text": "Remove connection",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }
    ACCOUNT_CONNECTION_LABEL = {
        "container": names.settings_dialogStack_QStackedWidget,
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
    def account_action(action):
        squish.clickButton(squish.waitForObject(AccountSetting.MANAGE_ACCOUNT_BUTTON))
        squish.activateItem(
            squish.waitForObjectItem(AccountSetting.ACCOUNT_MENU, action)
        )

    @staticmethod
    def remove_account_connection():
        AccountSetting.account_action("Remove")
        squish.clickButton(
            squish.waitForObject(AccountSetting.CONFIRM_REMOVE_CONNECTION_BUTTON)
        )

    @staticmethod
    def logout():
        AccountSetting.account_action("Log out")

    @staticmethod
    def login():
        AccountSetting.account_action("Log in")

    @staticmethod
    def get_account_connection_label():
        return str(
            squish.waitForObjectExists(AccountSetting.ACCOUNT_CONNECTION_LABEL).text
        )

    @staticmethod
    def is_connecting():
        return "Connecting to" in AccountSetting.get_account_connection_label()

    @staticmethod
    def is_user_signed_out():
        return "Signed out" in AccountSetting.get_account_connection_label()

    @staticmethod
    def is_user_signed_in():
        return "Connected" in AccountSetting.get_account_connection_label()

    @staticmethod
    def wait_until_connection_is_configured(timeout=5000):
        result = squish.waitFor(
            AccountSetting.is_connecting,
            timeout,
        )

        if not result:
            raise TimeoutError(
                "Timeout waiting for connection to be configured for "
                + str(timeout)
                + " milliseconds"
            )

    @staticmethod
    def wait_until_account_is_connected(timeout=5000):
        result = squish.waitFor(
            AccountSetting.is_user_signed_in,
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
    def press_key(key):
        key = key.replace('"', "")
        key = f"<{key}>"
        squish.nativeType(key)

    @staticmethod
    def is_log_dialog_visible():
        visible = False
        try:
            visible = squish.waitForObjectExists(
                AccountSetting.LOG_BROWSER_WINDOW
            ).visible
        except:
            pass
        return visible
