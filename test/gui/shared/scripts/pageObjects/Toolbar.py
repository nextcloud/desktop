import squish


class Toolbar:
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
