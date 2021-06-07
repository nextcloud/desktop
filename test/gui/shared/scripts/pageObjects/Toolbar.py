import names
import squish


class Toolbar:
    ACTIVITY_BUTTON = {
        "name": "settingsdialog_toolbutton_Activity",
        "type": "QToolButton",
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    ADD_ACCOUNT_BUTTON = {
        "name": "settingsdialog_toolbutton_Add account",
        "type": "QToolButton",
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }

    def clickActivity(self):
        squish.clickButton(squish.waitForObject(self.ACTIVITY_BUTTON))

    def clickAddAccount(self):
        squish.clickButton(squish.waitForObject(self.ADD_ACCOUNT_BUTTON))

    def getDisplayedAccountText(self, displayname, host):
        return str(
            squish.waitForObjectExists(
                {
                    "name": "settingsdialog_toolbutton_" + displayname + "@" + host,
                    "type": "QToolButton",
                    "visible": 1,
                }
            ).text
        )
