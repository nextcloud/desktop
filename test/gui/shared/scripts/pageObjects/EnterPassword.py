import names
import squish


class EnterPassword:
    PASSWORD_BOX = {
        "container": names.enter_Password_QInputDialog,
        "type": "QLineEdit",
        "unnamed": 1,
        "visible": 1,
    }
    OK_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.enter_Password_QInputDialog,
    }

    def __init__(self):
        pass

    def enterPassword(self, password):
        try:
            squish.waitForObject(self.PASSWORD_BOX, 10000)
            squish.type(squish.waitForObject(self.PASSWORD_BOX), password)
            squish.clickButton(squish.waitForObject(self.OK_BUTTON))
        except LookupError:
            pass
