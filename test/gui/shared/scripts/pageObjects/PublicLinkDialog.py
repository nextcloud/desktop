import names
import squish
import object
import test
from datetime import datetime


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
    PASSWORD_INPUT_FIELD = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "lineEdit_password",
        "type": "QLineEdit",
        "visible": 1,
    }
    EXPIRYDATE_CHECKBOX = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "checkBox_expire",
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
    DELETE_LINK_BUTTON = {
        "container": names.oCC_ShareLinkWidget_linkShares_QTableWidget,
        "type": "QToolButton",
        "occurrence": 2,
    }
    CONFIRM_LINK_DELETE_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "text": "Delete",
        "type": "QPushButton",
    }

    # to store current default public link expiry date
    defaultExpiryDate = ''

    @staticmethod
    def setDefaultExpiryDate(defaultDate):
        defaultDate = datetime.strptime(defaultDate, '%m/%d/%y')
        PublicLinkDialog.defaultExpiryDate = (
            f"{defaultDate.year}-{defaultDate.month}-{defaultDate.day}"
        )

    @staticmethod
    def getDefaultExpiryDate():
        return PublicLinkDialog.defaultExpiryDate

    def openPublicLinkDialog(self):
        squish.mouseClick(
            squish.waitForObject(self.PUBLIC_LINKS_TAB),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )

    def createPublicLink(
        self, context, resource, password='', permissions='', expireDate='', linkName=''
    ):
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
            self.setPassword(password)

        if expireDate:
            self.setExpirationDate(expireDate)

        if linkName:
            self.setLinkName(linkName)

        squish.clickButton(squish.waitForObject(self.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.waitForObject(names.linkShares_0_0_QModelIndex).displayText
                == "Public link"
            )
        )

    def togglePassword(self):
        squish.clickButton(squish.waitForObject(self.PASSWORD_CHECKBOX))

    def setPassword(self, password):
        enabled = squish.waitForObjectExists(self.PASSWORD_CHECKBOX).checked
        if not enabled:
            self.togglePassword()

        squish.mouseClick(
            squish.waitForObject(self.PASSWORD_INPUT_FIELD),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        squish.type(
            squish.waitForObject(self.PASSWORD_INPUT_FIELD),
            password,
        )

    def toggleExpirationDate(self):
        squish.clickButton(squish.waitForObject(self.EXPIRYDATE_CHECKBOX))

    def setExpirationDate(self, expireDate):
        enabled = squish.waitForObjectExists(self.EXPIRYDATE_CHECKBOX).checked
        if not enabled:
            self.toggleExpirationDate()

        if not expireDate == "default":
            expDate = datetime.strptime(expireDate, '%Y-%m-%d')
            expYear = expDate.year - 2000
            squish.mouseClick(
                squish.waitForObject(self.EXPIRATION_DATE_FIELD),
                0,
                0,
                squish.Qt.NoModifier,
                squish.Qt.LeftButton,
            )
            # Move the cursor to year (last) field and enter the year
            squish.nativeType("<Ctrl+Right>")
            squish.nativeType("<Ctrl+Right>")
            squish.nativeType(expYear)

            # Move the cursor to day (middle) field and enter the day
            squish.nativeType("<Ctrl+Left>")
            squish.nativeType(expDate.day)

            # Move the cursor to month (first) field and enter the month
            # Backspace works most of the time, so we clear the data using backspace
            squish.nativeType("<Ctrl+Left>")
            squish.nativeType("<Ctrl+Left>")
            squish.nativeType("<Right>")
            squish.nativeType("<Backspace>")
            squish.nativeType("<Backspace>")
            squish.nativeType(expDate.month)
            squish.nativeType("<Return>")

            actualDate = str(
                squish.waitForObjectExists(self.EXPIRATION_DATE_FIELD).displayText
            )
            expectedDate = f"{expDate.month}/{expDate.day}/{expYear}"
            if not actualDate == expectedDate:
                test.log(
                    f"Expected date: {expectedDate} is not same as that of Actual date: {actualDate}, trying workaround"
                )
                # retry with workaround
                self.setExpirationDateWithWorkaround(
                    expYear, expDate.month, expDate.day
                )
                actualDate = str(
                    squish.waitForObjectExists(self.EXPIRATION_DATE_FIELD).displayText
                )
                if not actualDate == expectedDate:
                    test.fail(
                        f"workaround failed, actual date: {actualDate} is still not same as expected date: {expectedDate}"
                    )
            squish.waitFor(
                lambda: (test.vp("publicLinkExpirationProgressIndicatorInvisible"))
            )
        else:
            defaultDate = str(
                squish.waitForObjectExists(self.EXPIRATION_DATE_FIELD).displayText
            )
            PublicLinkDialog.setDefaultExpiryDate(defaultDate)

    # This workaround is needed because the above function 'setExpirationDate' can not set the month field sometime.
    # See for more details: https://github.com/owncloud/client/issues/9218
    def setExpirationDateWithWorkaround(self, year, month, day):
        squish.mouseClick(
            squish.waitForObject(self.EXPIRATION_DATE_FIELD),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )

        # date can only be set to future date. But sometimes it can not modify the month field in first attempt.
        # date format is 'm/d/yy', so we have to edit 'month' first
        # then 'day' and then 'year'.
        # Delete data from month field
        squish.nativeType("<Delete>")
        squish.nativeType("<Delete>")
        squish.nativeType(month)
        squish.nativeType(day)
        squish.nativeType(year)
        squish.nativeType("<Return>")

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

    def changePassword(self, password):
        self.setPassword(password)
        squish.clickButton(
            squish.waitForObject(
                names.oCC_ShareLinkWidget_pushButton_setPassword_QPushButton
            )
        )

    @staticmethod
    def deletePublicLink():
        squish.clickButton(squish.waitForObject(PublicLinkDialog.DELETE_LINK_BUTTON))
        squish.clickButton(
            squish.waitForObject(PublicLinkDialog.CONFIRM_LINK_DELETE_BUTTON)
        )
        squish.waitFor(
            lambda: (not object.exists(PublicLinkDialog.DELETE_LINK_BUTTON)),
        )

    def verifyPublicLinkName(self, publicLinkName):
        test.compare(
            str(squish.waitForObjectExists(self.PUBLIC_LINK_NAME).text),
            publicLinkName,
        )

    def verifyResource(self, resource):
        test.compare(
            str(squish.waitForObjectExists(self.ITEM_TO_SHARE).text),
            resource,
        )

    def verifyExpirationDate(self, expectedDate):
        expectedDate = datetime.strptime(expectedDate, '%Y-%m-%d')
        # date format in client UI is 'mm/dd/yy' e.g. '01/15/22'
        expYear = expectedDate.year - 2000
        expectedDate = f"{expectedDate.month}/{expectedDate.day}/{expYear}"

        test.compare(
            str(squish.waitForObjectExists(self.EXPIRATION_DATE_FIELD).displayText),
            str(expectedDate),
        )
