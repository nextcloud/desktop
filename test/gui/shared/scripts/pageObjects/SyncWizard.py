import names
import squish


class SyncWizard:
    ACTION_MENU = {
        "container": names.settings_stack_QStackedWidget,
        "name": "_folderList",
        "type": "QTreeView",
        "visible": 1,
    }

    def performAction(self, context, action):
        item_text = "Personal" if context.userData['ocis'] else "ownCloud"
        squish.openContextMenu(
            squish.waitForObjectItem(self.ACTION_MENU, item_text),
            0,
            0,
            squish.Qt.NoModifier,
        )
        squish.activateItem(squish.waitForObjectItem(names.settings_QMenu, action))
