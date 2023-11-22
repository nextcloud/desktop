import squish
from helpers.SetupClientHelper import wait_until_app_killed


class Toolbar:
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

    @staticmethod
    def getItemSelector(item_name):
        return {
            "name": "settingsdialog_toolbutton_%s" % item_name,
            "type": "QToolButton",
            "visible": 1,
        }

    @staticmethod
    def openActivity():
        squish.clickButton(squish.waitForObject(Toolbar.getItemSelector("Activity")))

    @staticmethod
    def openNewAccountSetup():
        squish.clickButton(squish.waitForObject(Toolbar.getItemSelector("Add account")))

    @staticmethod
    def openAccount(displayname, host):
        squish.clickButton(
            squish.waitForObject(Toolbar.getItemSelector(displayname + "@" + host))
        )

    @staticmethod
    def getDisplayedAccountText(displayname, host):
        return str(
            squish.waitForObjectExists(
                Toolbar.getItemSelector(displayname + "@" + host)
            ).text
        )

    @staticmethod
    def open_settings_tab():
        squish.clickButton(squish.waitForObject(Toolbar.getItemSelector("Settings")))

    @staticmethod
    def quit_owncloud():
        squish.clickButton(
            squish.waitForObject(Toolbar.getItemSelector("Quit ownCloud"))
        )
        squish.clickButton(squish.waitForObject(Toolbar.CONFIRM_QUIT_BUTTON))
        for ctx in squish.applicationContextList():
            pid = ctx.pid
            ctx.detach()
            wait_until_app_killed(pid)
