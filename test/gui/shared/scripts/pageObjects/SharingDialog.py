import names
import squish
import test


class SharingDialog:

    ITEM_TO_SHARE = {
        "name": "label_name",
        "type": "QLabel",
        "visible": 1,
        "window": names.sharingDialog_OCC_ShareDialog,
    }
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
    SHARING_DIALOG = {
        "type": "QLabel",
        "unnamed": 1,
        "visible": 1,
        "window": names.sharingDialog_OCC_ShareDialog,
    }
    SHARING_DIALOG_ERROR = {"name": "errorLabel", "type": "QLabel", "visible": 1}
    SHARING_DIALOG_CONTRIBUTOR_ROW = {
        "container": names.sharingDialogUG_scrollArea_QScrollArea,
        "name": "sharedWith",
        "type": "QLabel",
        "visible": 1,
    }

    def getAvailablePermission(self):
        editChecked = squish.waitForObjectExists(self.EDIT_PERMISSIONS_CHECKBOX).checked
        shareChecked = squish.waitForObjectExists(
            self.SHARE_PERMISSIONS_CHECKBOX
        ).checked

        return editChecked, shareChecked

    def searchCollaborator(self, collaborator):
        squish.mouseClick(
            squish.waitForObject(self.SHARE_WITH_COLLABORATOR_INPUT_FIELD),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        squish.type(
            squish.waitForObject(self.SHARE_WITH_COLLABORATOR_INPUT_FIELD),
            collaborator,
        )

    def addCollaborator(
        self, receiver, permissions, isGroup=False, collaboratorCount=1
    ):
        self.selectCollaborator(receiver, isGroup)
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

        # wait for share to complete
        self.SHARING_DIALOG_CONTRIBUTOR_ROW["occurrence"] = collaboratorCount
        squish.waitForObjectExists(self.SHARING_DIALOG_CONTRIBUTOR_ROW)

    def getSharingDialogMessage(self):
        return str(squish.waitForObjectExists(self.SHARING_DIALOG).text)

    def getErrorText(self):
        return str(squish.waitForObjectExists(self.SHARING_DIALOG_ERROR).text)

    def selectCollaborator(self, receiver, isGroup=False):
        postFixInSuggestion = ""
        if isGroup:
            postFixInSuggestion = " (group)"

        self.searchCollaborator(receiver)

        # collaborator name with special characters contains escape characters '\\'
        # in the squish object
        # Example:
        # Actual collaborator name: Speci@l_Name-.+
        # Collaborator name in object: Speci@l\\_Name-\\.+
        escapedReceiverName = receiver.replace("_", "\\_").replace(".", "\\.")
        squish.mouseClick(
            squish.waitForObjectItem(
                self.SUGGESTED_COLLABORATOR, escapedReceiverName + postFixInSuggestion
            ),
            0,
            0,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )

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

    def verifyResource(self, resource):
        test.compare(
            str(squish.waitForObjectExists(self.ITEM_TO_SHARE).text),
            resource,
        )

    def closeSharingDialog(self):
        squish.clickButton(squish.waitForObject(self.SHARING_DIALOG_CLOSE_BUTTON))
