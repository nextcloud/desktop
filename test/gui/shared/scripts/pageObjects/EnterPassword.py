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
        "name": "topLabel",
        "type": "QLabel",
        "visible": 1,
        "window": LOGIN_CONTAINER,
    }
    USERNAME_BOX = {
        "name": "usernameLineEdit",
        "type": "QLineEdit",
        "visible": 1,
        "window": LOGIN_CONTAINER,
    }
    PASSWORD_BOX = {
        "name": "passwordLineEdit",
        "type": "QLineEdit",
        "visible": 1,
        "window": LOGIN_CONTAINER,
    }
    LOGIN_BUTTON = {
        "text": "Log in",
        "type": "QPushButton",
        "visible": 1,
        "window": names.stack_stackedWidget_QStackedWidget,
    }
    LOGOUT_BUTTON = {
        "text": "Log out",
        "type": "QPushButton",
        "visible": 1,
        "window": names.stack_stackedWidget_QStackedWidget,
    }
    COPY_URL_TO_CLIPBOARD_BUTTON = {
        "name": "copyUrlToClipboardButton",
        "type": "QPushButton",
        "visible": 1,
        "window": LOGIN_CONTAINER,
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
        # Parse username from following label:
        #   Please enter your password to log in to the account Alice Hansen@localhost.
        #   The account Alice Hansen@localhost:9200 is currently logged out.
        label = str(squish.waitForObjectExists(self.LOGIN_USER_LABEL).text)
        label = label.split("@", maxsplit=1)[0].split(" ")
        label.reverse()
        return label[1].capitalize()

    def enterPassword(self, password):
        squish.waitForObject(self.PASSWORD_BOX, get_config("maxSyncTimeout") * 1000)
        squish.type(squish.waitForObject(self.PASSWORD_BOX), password)
        squish.clickButton(squish.waitForObject(self.LOGIN_BUTTON))

    def oidcReLogin(self, username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.clickButton(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))
        authorize_via_webui(username, password)

    def oauthReLogin(self, username, password):
        # wait 500ms for copy button to fully load
        squish.snooze(1 / 2)
        squish.clickButton(squish.waitForObject(self.COPY_URL_TO_CLIPBOARD_BUTTON))
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
        squish.clickButton(squish.waitForObject(self.LOGOUT_BUTTON))

    def accept_certificate(self):
        squish.clickButton(squish.waitForObject(self.ACCEPT_CERTIFICATE_YES))
