import names
import squish
import object  # pylint: disable=redefined-builtin


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
    DELETE_SHARE_BUTTON = {
        "container": names.sharingDialogUG_scrollArea_QScrollArea,
        "name": "deleteShareButton",
        "type": "QToolButton",
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

    @staticmethod
    def get_available_permission():
        edit_checked = squish.waitForObjectExists(
            SharingDialog.EDIT_PERMISSIONS_CHECKBOX
        ).checked
        share_checked = squish.waitForObjectExists(
            SharingDialog.SHARE_PERMISSIONS_CHECKBOX
        ).checked

        return edit_checked, share_checked

    @staticmethod
    def search_collaborator(collaborator):
        squish.mouseClick(
            squish.waitForObject(SharingDialog.SHARE_WITH_COLLABORATOR_INPUT_FIELD)
        )
        squish.type(
            squish.waitForObject(SharingDialog.SHARE_WITH_COLLABORATOR_INPUT_FIELD),
            collaborator,
        )

    @staticmethod
    def add_collaborator(receiver, permissions, is_group=False, collaborator_count=1):
        SharingDialog.select_collaborator(receiver, is_group)
        permissions_list = permissions.split(",")

        edit_checked, share_checked = SharingDialog.get_available_permission()

        if ("edit" in permissions_list and edit_checked is False) or (
            "edit" not in permissions_list and edit_checked is True
        ):
            squish.clickButton(
                squish.waitForObject(SharingDialog.EDIT_PERMISSIONS_CHECKBOX)
            )
        if ("share" in permissions_list and share_checked is False) or (
            "share" not in permissions_list and share_checked is True
        ):
            squish.clickButton(
                squish.waitForObject(SharingDialog.SHARE_PERMISSIONS_CHECKBOX)
            )

        # wait for share to complete
        # create the copy of the selector to avoid mutating the original
        collaborator_row_selector = SharingDialog.SHARING_DIALOG_CONTRIBUTOR_ROW.copy()
        collaborator_row_selector["occurrence"] = collaborator_count
        squish.waitForObjectExists(collaborator_row_selector)

    @staticmethod
    def get_sharing_dialog_message():
        return str(squish.waitForObjectExists(SharingDialog.SHARING_DIALOG).text)

    @staticmethod
    def get_error_text():
        return str(squish.waitForObjectExists(SharingDialog.SHARING_DIALOG_ERROR).text)

    @staticmethod
    def select_collaborator(receiver, is_group=False):
        suffix = ""
        if is_group:
            suffix = " (group)"

        SharingDialog.search_collaborator(receiver)

        # collaborator name with special characters contains escape characters '\\'
        # in the squish object
        # Example:
        # Actual collaborator name: Speci@l_Name-.+
        # Collaborator name in object: Speci@l\\_Name-\\.+
        escaped_receiver_name = receiver.replace("_", "\\_").replace(".", "\\.")
        squish.mouseClick(
            squish.waitForObjectItem(
                SharingDialog.SUGGESTED_COLLABORATOR,
                escaped_receiver_name + suffix,
            )
        )

    @staticmethod
    def remove_permissions(permissions):
        remove_permissions_list = permissions.split(",")
        (
            is_edit_permission_available,
            is_share_permission_available,
        ) = SharingDialog.get_available_permission()

        if "share" in remove_permissions_list and is_share_permission_available:
            squish.clickButton(
                squish.waitForObject(SharingDialog.SHARE_PERMISSIONS_CHECKBOX)
            )

        if "edit" in remove_permissions_list and is_edit_permission_available:
            squish.clickButton(
                squish.waitForObject(SharingDialog.EDIT_PERMISSIONS_CHECKBOX)
            )

    @staticmethod
    def unshare_with():
        squish.clickButton(squish.waitForObject(SharingDialog.DELETE_SHARE_BUTTON))

    @staticmethod
    def close_sharing_dialog():
        squish.clickButton(
            squish.waitForObject(SharingDialog.SHARING_DIALOG_CLOSE_BUTTON)
        )

    @staticmethod
    def get_collaborators():
        squish.waitForObject(SharingDialog.SHARING_DIALOG_CONTRIBUTOR_ROW)
        return squish.findAllObjects(SharingDialog.SHARING_DIALOG_CONTRIBUTOR_ROW)

    @staticmethod
    def has_edit_permission():
        if not object.exists(SharingDialog.EDIT_PERMISSIONS_CHECKBOX):
            return False
        return squish.waitForObjectExists(
            SharingDialog.EDIT_PERMISSIONS_CHECKBOX
        ).checked

    @staticmethod
    def has_share_permission():
        if not object.exists(SharingDialog.SHARE_PERMISSIONS_CHECKBOX):
            return False
        return squish.waitForObjectExists(
            SharingDialog.SHARE_PERMISSIONS_CHECKBOX
        ).checked

    @staticmethod
    def get_collaborator_name(occurrence=1):
        selector = SharingDialog.SHARING_DIALOG_CONTRIBUTOR_ROW.copy()
        selector["occurrence"] = occurrence
        return str(squish.waitForObjectExists(selector).text)

    @staticmethod
    def is_user_in_suggestion_list(user):
        exists = False
        try:
            squish.waitForObjectItem(SharingDialog.SUGGESTED_COLLABORATOR, user)
            exists = True
        except LookupError as e:
            pass
        return exists
