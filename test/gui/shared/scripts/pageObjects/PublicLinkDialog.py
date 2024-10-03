from datetime import datetime
import test
import names
import squish
import object  # pylint: disable=redefined-builtin


class PublicLinkDialog:
    PUBLIC_LINKS_TAB = {
        "container": names.sharingDialog_qt_tabwidget_tabbar_QTabBar,
        "text": "Public Links",
        "type": "TabItem",
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

    DATE_FORMATS = [
        "%m/%d/%Y",
        "%d/%m/%Y",
        "%Y-%m-%d",
        "%d-%m-%Y",
        "%Y/%m/%d",
        "%m/%d/%y",
        "%d/%m/%y",
    ]
    # to store current default public link expiry date
    default_expiry_date = ""

    @staticmethod
    def parse_date(date):
        for date_format in PublicLinkDialog.DATE_FORMATS:
            try:
                date = str(datetime.strptime(date, date_format))
                date = date.split(" ", maxsplit=1)[0]
                break
            except:
                pass
        return date

    @staticmethod
    def set_default_expiry_date(default_date):
        PublicLinkDialog.default_expiry_date = PublicLinkDialog.parse_date(default_date)

    @staticmethod
    def get_default_expiry_date():
        return PublicLinkDialog.default_expiry_date

    @staticmethod
    def open_public_link_tab():
        squish.mouseClick(squish.waitForObject(PublicLinkDialog.PUBLIC_LINKS_TAB))

    @staticmethod
    def create_public_link(password="", permissions="", expire_date=""):
        permission_locator = ""
        if permissions:
            permission_locator = PublicLinkDialog.get_radio_object_for_permssion(
                permissions
            )

        if permission_locator:
            squish.clickButton(squish.waitForObject(permission_locator))

        if password:
            PublicLinkDialog.set_password(password)

        if expire_date:
            PublicLinkDialog.set_expiration_date(expire_date)

        squish.clickButton(squish.waitForObject(PublicLinkDialog.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.waitForObject(PublicLinkDialog.PUBLIC_LINK_NAME).displayText
                == "Public link"
            )
        )

    @staticmethod
    def toggle_password():
        squish.clickButton(squish.waitForObject(PublicLinkDialog.PASSWORD_CHECKBOX))

    @staticmethod
    def set_password(password):
        if not squish.waitForObjectExists(PublicLinkDialog.PASSWORD_CHECKBOX).checked:
            PublicLinkDialog.toggle_password()

        squish.mouseClick(squish.waitForObject(PublicLinkDialog.PASSWORD_INPUT_FIELD))
        squish.type(
            squish.waitForObject(PublicLinkDialog.PASSWORD_INPUT_FIELD),
            password,
        )

    @staticmethod
    def toggle_expiration_date():
        squish.clickButton(squish.waitForObject(PublicLinkDialog.EXPIRYDATE_CHECKBOX))

    @staticmethod
    def set_expiration_date(expire_date):
        enabled = squish.waitForObjectExists(
            PublicLinkDialog.EXPIRYDATE_CHECKBOX
        ).checked
        if not enabled:
            PublicLinkDialog.toggle_expiration_date()

        if not expire_date == "default":
            exp_date = datetime.strptime(expire_date, "%Y-%m-%d")
            exp_year = exp_date.year - 2000
            squish.mouseClick(
                squish.waitForObject(PublicLinkDialog.EXPIRATION_DATE_FIELD)
            )
            # Move the cursor to year (last) field and enter the year
            squish.nativeType("<Ctrl+Right>")
            squish.nativeType("<Ctrl+Right>")
            squish.nativeType(exp_year)

            # Move the cursor to day (middle) field and enter the day
            squish.nativeType("<Ctrl+Left>")
            squish.nativeType(exp_date.day)

            # Move the cursor to month (first) field and enter the month
            # Backspace works most of the time, so we clear the data using backspace
            squish.nativeType("<Ctrl+Left>")
            squish.nativeType("<Ctrl+Left>")
            squish.nativeType("<Right>")
            squish.nativeType("<Backspace>")
            squish.nativeType("<Backspace>")
            squish.nativeType(exp_date.month)
            squish.nativeType("<Return>")

            actual_date = str(
                squish.waitForObjectExists(
                    PublicLinkDialog.EXPIRATION_DATE_FIELD
                ).displayText
            )
            expected_date = f"{exp_date.month}/{exp_date.day}/{exp_year}"
            if not actual_date == expected_date:
                test.log(
                    f"Expected date: {expected_date} is not same as that of actual date: {actual_date}"
                    + ", trying workaround"
                )
                # retry with workaround
                PublicLinkDialog.set_expiration_date_with_workaround(
                    exp_year, exp_date.month, exp_date.day
                )
                actual_date = str(
                    squish.waitForObjectExists(
                        PublicLinkDialog.EXPIRATION_DATE_FIELD
                    ).displayText
                )
                if not actual_date == expected_date:
                    test.fail(
                        f"workaround failed:\nactual date: {actual_date}\nexpected date: {expected_date}"
                    )
        else:
            default_date = str(
                squish.waitForObjectExists(
                    PublicLinkDialog.EXPIRATION_DATE_FIELD
                ).displayText
            )
            PublicLinkDialog.set_default_expiry_date(default_date)

    # This workaround is needed because the above function 'set_expiration_date' can not set the month field sometime.
    # See for more details: https://github.com/owncloud/client/issues/9218
    @staticmethod
    def set_expiration_date_with_workaround(year, month, day):
        squish.mouseClick(squish.waitForObject(PublicLinkDialog.EXPIRATION_DATE_FIELD))

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
    def get_radio_object_for_permssion(permissions):
        permission_locator = None
        if permissions in ("Download / View", "Viewer"):
            permission_locator = PublicLinkDialog.READ_ONLY_RADIO_BUTTON
        elif permissions in ("Download / View / Edit", "Editor"):
            permission_locator = PublicLinkDialog.READ_WRITE_RADIO_BUTTON
        elif permissions in ("Upload only (File Drop)", "Contributor"):
            permission_locator = PublicLinkDialog.UPLOAD_ONLY_RADIO_BUTTON
        else:
            raise LookupError("No such radio object found for given permission")

        return permission_locator

    @staticmethod
    def create_public_link_with_role(role):
        permission_locator = PublicLinkDialog.get_radio_object_for_permssion(role)

        squish.clickButton(squish.waitForObject(permission_locator))
        squish.clickButton(squish.waitForObject(PublicLinkDialog.CREATE_SHARE_BUTTON))
        squish.waitFor(
            lambda: (
                squish.findObject(PublicLinkDialog.PUBLIC_LINK_NAME).displayText
                == "Public link"
            )
        )

    @staticmethod
    def change_password(password):
        PublicLinkDialog.set_password(password)
        squish.clickButton(
            squish.waitForObject(PublicLinkDialog.UPDATE_PASSWORD_BUTTON)
        )

    @staticmethod
    def delete_public_link():
        squish.clickButton(squish.waitForObject(PublicLinkDialog.DELETE_LINK_BUTTON))
        squish.clickButton(
            squish.waitForObject(PublicLinkDialog.CONFIRM_LINK_DELETE_BUTTON)
        )
        squish.waitFor(
            lambda: (not object.exists(PublicLinkDialog.DELETE_LINK_BUTTON)),
        )

    @staticmethod
    def get_expiration_date():
        default_date = str(
            squish.waitForObjectExists(
                PublicLinkDialog.EXPIRATION_DATE_FIELD
            ).displayText
        )
        return PublicLinkDialog.parse_date(default_date)
