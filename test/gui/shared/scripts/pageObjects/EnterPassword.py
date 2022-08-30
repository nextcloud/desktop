import names
import squish


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

    def __init__(self):
        pass

    def enterPassword(self, password):
        squish.waitForObject(self.PASSWORD_BOX, 10000)
        squish.type(squish.waitForObject(self.PASSWORD_BOX), password)
        squish.clickButton(squish.waitForObject(self.LOGIN_BUTTON))
