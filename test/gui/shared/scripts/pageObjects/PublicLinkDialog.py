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
    UPDATE_PASSWORD_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ShareLinkWidget_OCC_ShareLinkWidget,
        "name": "pushButton_setPassword",
        "type": "QPushButton",
        "visible": 1,
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

    @staticmethod
    def openPublicLinkTab():
        squish.mouseClick(
            squish.waitForObject(PublicLinkDialog.PUBLIC_LINKS_TAB),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )

    @staticmethod
    def createPublicLink(password='', permissions='', expireDate='', linkName=''):
        radioObjectName = ''
        if permissions:
            radioObjectName = PublicLinkDialog.getRadioObjectForPermssion(permissions)

        if radioObjectName:
            squish.clickButton(squish.waitForObject(radioObjectName))

        if password:
            PublicLinkDialog.setPassword(password)

        if expireDate:
            PublicLinkDialog.setExpirationDate(expireDate)

        if linkName:
            PublicLinkDialog.setLinkName(linkName)

        squish.clickButton(squish.waitForObject(PublicLinkDialog.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.waitForObject(PublicLinkDialog.PUBLIC_LINK_NAME).displayText
                == "Public link"
            )
        )

    @staticmethod
    def togglePassword():
        squish.clickButton(squish.waitForObject(PublicLinkDialog.PASSWORD_CHECKBOX))

    @staticmethod
    def setPassword(password):
        enabled = squish.waitForObjectExists(PublicLinkDialog.PASSWORD_CHECKBOX).checked
        if not enabled:
            PublicLinkDialog.togglePassword()

        squish.mouseClick(
            squish.waitForObject(PublicLinkDialog.PASSWORD_INPUT_FIELD),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        squish.type(
            squish.waitForObject(PublicLinkDialog.PASSWORD_INPUT_FIELD),
            password,
        )

    @staticmethod
    def toggleExpirationDate():
        squish.clickButton(squish.waitForObject(PublicLinkDialog.EXPIRYDATE_CHECKBOX))

    @staticmethod
    def setExpirationDate(expireDate):
        enabled = squish.waitForObjectExists(
            PublicLinkDialog.EXPIRYDATE_CHECKBOX
        ).checked
        if not enabled:
            PublicLinkDialog.toggleExpirationDate()

        if not expireDate == "default":
            expDate = datetime.strptime(expireDate, '%Y-%m-%d')
            expYear = expDate.year - 2000
            squish.mouseClick(
                squish.waitForObject(PublicLinkDialog.EXPIRATION_DATE_FIELD),
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
                squish.waitForObjectExists(
                    PublicLinkDialog.EXPIRATION_DATE_FIELD
                ).displayText
            )
            expectedDate = f"{expDate.month}/{expDate.day}/{expYear}"
            if not actualDate == expectedDate:
                test.log(
                    f"Expected date: {expectedDate} is not same as that of Actual date: {actualDate}, trying workaround"
                )
                # retry with workaround
                PublicLinkDialog.setExpirationDateWithWorkaround(
                    expYear, expDate.month, expDate.day
                )
                actualDate = str(
                    squish.waitForObjectExists(
                        PublicLinkDialog.EXPIRATION_DATE_FIELD
                    ).displayText
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
                squish.waitForObjectExists(
                    PublicLinkDialog.EXPIRATION_DATE_FIELD
                ).displayText
            )
            PublicLinkDialog.setDefaultExpiryDate(defaultDate)

    # This workaround is needed because the above function 'setExpirationDate' can not set the month field sometime.
    # See for more details: https://github.com/owncloud/client/issues/9218
    @staticmethod
    def setExpirationDateWithWorkaround(year, month, day):
        squish.mouseClick(
            squish.waitForObject(PublicLinkDialog.EXPIRATION_DATE_FIELD),
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

    @staticmethod
    def getRadioObjectForPermssion(permissions):
        radioObjectName = ''
        if permissions == 'Download / View' or permissions == 'Viewer':
            radioObjectName = PublicLinkDialog.READ_ONLY_RADIO_BUTTON
        elif permissions == 'Download / View / Edit' or permissions == 'Editor':
            radioObjectName = PublicLinkDialog.READ_WRITE_RADIO_BUTTON
        elif permissions == 'Upload only (File Drop)' or permissions == 'Contributor':
            radioObjectName = PublicLinkDialog.UPLOAD_ONLY_RADIO_BUTTON
        else:
            raise Exception("No such radio object found for given permission")

        return radioObjectName

    @staticmethod
    def createPublicLinkWithRole(role):
        radioObjectName = PublicLinkDialog.getRadioObjectForPermssion(role)

        squish.clickButton(squish.waitForObject(radioObjectName))
        squish.clickButton(squish.waitForObject(PublicLinkDialog.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.findObject(PublicLinkDialog.PUBLIC_LINK_NAME).displayText
                == "Public link"
            )
        )

    @staticmethod
    def changePassword(password):
        PublicLinkDialog.setPassword(password)
        squish.clickButton(
            squish.waitForObject(PublicLinkDialog.UPDATE_PASSWORD_BUTTON)
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

    @staticmethod
    def getExpirationDate():
        defaultDate = str(
            squish.waitForObjectExists(
                PublicLinkDialog.EXPIRATION_DATE_FIELD
            ).displayText
        )
        return str(datetime.strptime(defaultDate, '%m/%d/%y')).split(' ')[0]
