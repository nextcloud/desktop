import names
import squish


from helpers.WebUIHelper import authorize_via_webui
from helpers.ConfigHelper import get_config


class EnterPassword:
    LOGIN_CONTAINER = {
        "name": "LoginRequiredDialog",
        "type": "OCC::LoginRequiredDialog",
        "visible": 1,
    }
    LOGIN_USER_LABEL = {
        "container": names.groupBox_OCC_QmlUtils_OCQuickWidget,
        "type": "Label",
        "visible": True,
    }
    USERNAME_BOX = {
        "name": "usernameLineEdit",
        "type": "QLineEdit",
        "visible": 1,
        "window": LOGIN_CONTAINER,
    }
    PASSWORD_BOX = {
        "container": names.groupBox_OCC_QmlUtils_OCQuickWidget,
        "id": "passwordField",
        "type": "TextField",
        "visible": True,
    }
    LOGIN_BUTTON = {
        "container": names.groupBox_OCC_QmlUtils_OCQuickWidget,
        "id": "loginButton",
        "type": "Button",
        "visible": True,
    }
    LOGOUT_BUTTON = {
        "container": names.groupBox_OCC_QmlUtils_OCQuickWidget,
        "id": "logOutButton",
        "type": "Button",
        "visible": True,
    }
    COPY_URL_TO_CLIPBOARD_BUTTON = {
        "container": names.groupBox_OCC_QmlUtils_OCQuickWidget,
        "id": "copyToClipboardButton",
        "type": "Button",
        "visible": True,
    }
    TLS_CERT_WINDOW = {
        "name": "OCC__TlsErrorDialog",
        "type": "OCC::TlsErrorDialog",
        "visible": 1,
    }
    ACCEPT_CERTIFICATE_YES = {
        "text": "Yes",
        "type": "QPushButton",
        "visible": 1,
        "window": TLS_CERT_WINDOW,
    }

    def __init__(self, occurrence=1):
        if occurrence > 1 and get_config("ocis"):
            self.TLS_CERT_WINDOW.update({"occurrence": occurrence})

    def get_username(self):
        # Parse username from the login label:
        label = str(squish.waitForObjectExists(self.LOGIN_USER_LABEL).text)
        username = label.split(" ", maxsplit=2)[1]
        return username.capitalize()

    def enterPassword(self, password):
        squish.waitForObjectExists(
            self.PASSWORD_BOX, get_config("maxSyncTimeout") * 1000
        )
        squish.mouseClick(squish.waitForObjectExists(self.PASSWORD_BOX))
        squish.nativeType(password)
        squish.mouseClick(squish.waitForObjectExists(self.LOGIN_BUTTON))

    def oidcReLogin(self, username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.mouseClick(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))
        authorize_via_webui(username, password)

    def oauthReLogin(self, username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.mouseClick(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))
        authorize_via_webui(username, password, "oauth")

    def reLogin(self, username, password, oauth=False):
        if get_config("ocis"):
            self.oidcReLogin(username, password)
        elif oauth:
            self.oauthReLogin(username, password)
        else:
            self.enterPassword(password)

    def loginAfterSetup(self, username, password):
        if get_config("ocis"):
            self.oidcReLogin(username, password)
        else:
            self.enterPassword(password)

    def logout(self):
        squish.mouseClick(squish.waitForObject(self.LOGOUT_BUTTON))

    def accept_certificate(self):
        squish.clickButton(squish.waitForObject(self.ACCEPT_CERTIFICATE_YES))
