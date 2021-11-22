import names
import squish
import test
import datetime


class PublicLinkDialog:
    PUBLIC_LINKS_TAB = {
        "container": names.sharingDialog_qt_tabwidget_tabbar_QTabBar,
        "text": "Public Links",
        "type": "TabItem",
    }
    ITEM_TO_SHARE = {
        "name": "label_name",
        "type": "QLabel",
        "visible": 1,
        "window": names.sharingDialog_OCC_ShareDialog,
    }
    PASSWORD_CHECKBOX = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "checkBox_password",
        "type": "QCheckBox",
        "visible": 1,
    }
    CREATE_SHARE_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "createShareButton",
        "type": "QPushButton",
        "visible": 1,
    }

    PUBLIC_LINK_NAME = {
        "column": 0,
        "container": names.oCC_ShareLinkWidget_linkShares_QTableWidget,
        "row": 0,
        "type": "QModelIndex",
    }
    EXPIRATION_DATE_FIELD = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "qt_spinbox_lineedit",
        "type": "QLineEdit",
        "visible": 1,
    }
    READ_ONLY_RADIO_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "radio_readOnly",
        "type": "QRadioButton",
        "visible": 1,
    }
    READ_WRITE_RADIO_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "radio_readWrite",
        "type": "QRadioButton",
        "visible": 1,
    }
    UPLOAD_ONLY_RADIO_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "radio_uploadOnly",
        "type": "QRadioButton",
        "visible": 1,
    }

    def openPublicLinkDialog(self):
        squish.mouseClick(
            squish.waitForObject(self.PUBLIC_LINKS_TAB),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )

    def createPublicLink(self, context, resource, password='', permissions=''):
        radioObjectName = ''
        if permissions:
            radioObjectName = self.getRadioObjectForPermssion(permissions)

        test.compare(
            str(squish.waitForObjectExists(self.ITEM_TO_SHARE).text),
            resource.replace(context.userData['currentUserSyncPath'], ''),
        )

        if radioObjectName:
            test.compare(
                str(squish.waitForObjectExists(radioObjectName).text), permissions
            )
            squish.clickButton(squish.waitForObject(radioObjectName))

        if password:
            squish.clickButton(squish.waitForObject(self.PASSWORD_CHECKBOX))
            squish.mouseClick(
                squish.waitForObject(self.PASSWORD_CHECKBOX),
                0,
                0,
                squish.Qt.NoModifier,
                squish.Qt.LeftButton,
            )
            squish.type(
                squish.waitForObject(self.PASSWORD_CHECKBOX),
                password,
            )

        squish.clickButton(squish.waitForObject(self.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.waitForObject(names.linkShares_0_0_QModelIndex).displayText
                == "Public link"
            )
        )

    def togglesPassword(self):
        squish.clickButton(squish.waitForObject(self.PASSWORD_CHECKBOX))

    def setExpirationDate(self, context, publicLinkName, resource):
        test.compare(
            str(squish.waitForObjectExists(self.ITEM_TO_SHARE).text),
            resource,
        )
        test.compare(
            str(squish.waitForObjectExists(self.PUBLIC_LINK_NAME).text),
            publicLinkName,
        )
        expDate = []
        for row in context.table:
            if row[0] == 'expireDate':
                expDate = datetime.datetime.strptime(row[1], '%Y-%m-%d')
        expYear = expDate.year - 2000
        squish.mouseClick(
            squish.waitForObject(self.EXPIRATION_DATE_FIELD),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        squish.nativeType("<Delete>")
        squish.nativeType("<Delete>")
        squish.nativeType(expDate.month)
        squish.nativeType(expDate.day)
        squish.nativeType(expYear)
        squish.nativeType("<Return>")
        squish.testSettings.silentVerifications = True
        squish.waitFor(
            lambda: (test.xvp("publicLinkExpirationProgressIndicatorInvisible"))
        )
        waitFor(lambda: (test.vp("publicLinkExpirationProgressIndicatorInvisible")))
        squish.testSettings.silentVerifications = False
        test.compare(
            str(squish.waitForObjectExists(self.EXPIRATION_DATE_FIELD).displayText),
            str(expDate.month) + "/" + str(expDate.day) + "/" + str(expYear),
        )

    def getRadioObjectForPermssion(self, permissions):
        radioObjectName = ''
        if permissions == 'Download / View' or permissions == 'Viewer':
            radioObjectName = self.READ_ONLY_RADIO_BUTTON
        elif permissions == 'Download / View / Edit' or permissions == 'Editor':
            radioObjectName = self.READ_WRITE_RADIO_BUTTON
        elif permissions == 'Upload only (File Drop)' or permissions == 'Contributor':
            radioObjectName = self.UPLOAD_ONLY_RADIO_BUTTON
        else:
            raise Exception("No such radio object found for given permission")

        return radioObjectName

    def createPublicLinkWithRole(self, role):
        radioObjectName = self.getRadioObjectForPermssion(role)

        squish.clickButton(squish.waitForObject(radioObjectName))
        squish.clickButton(squish.waitForObject(self.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.findObject(names.linkShares_0_0_QModelIndex).displayText
                == "Public link"
            )
        )

    def changePassword(self, publicLinkName, password):
        test.compare(
            str(squish.waitForObjectExists(self.PUBLIC_LINK_NAME).text),
            publicLinkName,
        )
        squish.mouseClick(
            squish.waitForObject(names.oCC_ShareLinkWidget_lineEdit_password_QLineEdit),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        squish.type(
            squish.waitForObject(names.oCC_ShareLinkWidget_lineEdit_password_QLineEdit),
            password,
        )
        squish.clickButton(
            squish.waitForObject(
                names.oCC_ShareLinkWidget_pushButton_setPassword_QPushButton
            )
        )
