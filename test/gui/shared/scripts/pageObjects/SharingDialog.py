import names
import squish
import test


class SharingDialog:

    SHARE_WITH_COLLABORATOR_INPUT_FIELD = {
        "container": names.qt_tabwidget_stackedwidget_SharingDialogUG_OCC_ShareUserGroupWidget,
        "name": "shareeLineEdit",
        "type": "QLineEdit",
        "visible": 1,
    }
    SUGGESTED_COLLABORATOR = {"type": "QListView", "unnamed": 1, "visible": 1}
    EDIT_PERMISSIONS_CHECKBOX = {
        "container": names.sharingDialogUG_scrollArea_QScrollArea,
        "name": "permissionsEdit",
        "type": "QCheckBox",
        "visible": 1,
    }
    SHARE_PERMISSIONS_CHECKBOX = {
        "container": names.sharingDialogUG_scrollArea_QScrollArea,
        "name": "permissionShare",
        "type": "QCheckBox",
        "visible": 1,
    }
    SHARING_DIALOG_CLOSE_BUTTON = {
        "text": "Close",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.sharingDialog_OCC_ShareDialog,
    }
    SHARED_WITH_ = {
        "container": names.sharingDialogUG_scrollArea_QScrollArea,
        "name": "sharedWith",
        "type": "QLabel",
        "visible": 1,
    }
    ERROR_SHOWN_ON_SHARING_DIALOG = {
        "type": "QLabel",
        "unnamed": 1,
        "visible": 1,
        "window": names.sharingDialog_OCC_ShareDialog,
    }

    def getAvailablePermission(self):

        editChecked = squish.waitForObjectExists(self.EDIT_PERMISSIONS_CHECKBOX).checked
        shareChecked = squish.waitForObjectExists(
            self.SHARE_PERMISSIONS_CHECKBOX
        ).checked

        return editChecked, shareChecked

    def addCollaborator(self, receiver, permissions):
        squish.mouseClick(
            squish.waitForObject(self.SHARE_WITH_COLLABORATOR_INPUT_FIELD),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        squish.type(
            squish.waitForObject(self.SHARE_WITH_COLLABORATOR_INPUT_FIELD),
            receiver,
        )
        squish.mouseClick(
            squish.waitForObjectItem(self.SUGGESTED_COLLABORATOR, receiver),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        permissionsList = permissions.split(",")

        editChecked, shareChecked = self.getAvailablePermission()

        if ('edit' in permissionsList and editChecked == False) or (
            'edit' not in permissionsList and editChecked == True
        ):
            squish.clickButton(squish.waitForObject(self.EDIT_PERMISSIONS_CHECKBOX))
        if ('share' in permissionsList and shareChecked == False) or (
            'share' not in permissionsList and shareChecked == True
        ):
            squish.clickButton(squish.waitForObject(self.SHARE_PERMISSIONS_CHECKBOX))

        squish.clickButton(squish.waitForObject(self.SHARING_DIALOG_CLOSE_BUTTON))

    def getErrorText(self):
        return str(squish.waitForObjectExists(self.ERROR_SHOWN_ON_SHARING_DIALOG).text)

    def removePermissions(self, permissions):
        removePermissionsList = permissions.split(",")
        (
            isEditPermissionAvailable,
            isSharePermissionAvailable,
        ) = self.getAvailablePermission()

        if 'share' in removePermissionsList and isSharePermissionAvailable:
            squish.clickButton(
                squish.waitForObject(names.scrollArea_permissionShare_QCheckBox)
            )

        if 'edit' in removePermissionsList and isEditPermissionAvailable:
            squish.clickButton(
                squish.waitForObject(names.scrollArea_permissionsEdit_QCheckBox)
            )
