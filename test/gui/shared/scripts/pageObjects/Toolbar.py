import names
import squish


class Toolbar:
    ACTIVITY_BUTTON = {
        "name": "settingsdialog_toolbutton_Activity",
        "type": "QToolButton",
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }

    def clickActivity(self):
        squish.clickButton(squish.waitForObject(self.ACTIVITY_BUTTON))
