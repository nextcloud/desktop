import names
import squish
from helpers.WebUIHelper import authorize_via_webui
from helpers.ConfigHelper import get_config
from pageObjects.AccountConnectionWizard import AccountConnectionWizard


class EnterPassword:
    PASSWORD_BOX = {
        "container": names.loginRequiredDialog_contentWidget_QStackedWidget,
        "name": "passwordLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    LOGIN_BUTTON = {
        "text": "Log in",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.loginRequiredDialog_OCC_LoginRequiredDialog,
    }
    COPY_URL_TO_CLIPBOARD_BUTTON = {
        "container": names.loginRequiredDialog_contentWidget_QStackedWidget,
        "name": "copyUrlToClipboardButton",
        "type": "QPushButton",
        "visible": 1,
    }

    @staticmethod
    def enterPassword(password):
        squish.waitForObject(
            EnterPassword.PASSWORD_BOX, get_config('maxSyncTimeout') * 1000
        )
        squish.type(squish.waitForObject(EnterPassword.PASSWORD_BOX), password)
        squish.clickButton(squish.waitForObject(EnterPassword.LOGIN_BUTTON))

    @staticmethod
    def oidcReLogin(username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.clickButton(
            squish.waitForObject(EnterPassword.COPY_URL_TO_CLIPBOARD_BUTTON)
        )
        authorize_via_webui(username, password)

    @staticmethod
    def reLogin(username, password):
        if get_config('ocis'):
            EnterPassword.oidcReLogin(username, password)
        else:
            EnterPassword.enterPassword(password)

    @staticmethod
    def loginAfterSetup(username, password):
        if get_config('ocis'):
            AccountConnectionWizard.acceptCertificate()
            EnterPassword.oidcReLogin(username, password)
        else:
            EnterPassword.enterPassword(password)
