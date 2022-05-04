import names
import squish
from objectmaphelper import RegularExpression
from helpers.FilesHelper import buildConflictedRegex


class Activity:
    SUBTAB_CONTAINER = {
        "container": names.settings_stack_QStackedWidget,
        "name": "qt_tabwidget_tabbar",
        "type": "QTabBar",
        "visible": 1,
    }

    SUBTAB = {
        "container": names.settings_stack_QStackedWidget,
        "type": "QTabWidget",
        "unnamed": 1,
        "visible": 1,
    }

    def clickTab(self, tabName):
        tabFound = False

        # Selecting tab by name fails for "Not Synced" when there are no unsynced files
        # Because files count will be appended like "Not Synced (2)"
        # So to overcome this the following approach has been implemented
        tabCount = squish.waitForObjectExists(self.SUBTAB_CONTAINER).count
        for index in range(tabCount):
            tabText = squish.waitForObjectExists(
                {
                    "container": names.stack_qt_tabwidget_tabbar_QTabBar,
                    "index": index,
                    "type": "TabItem",
                }
            ).text

            if tabName in tabText:
                tabFound = True
                squish.clickTab(squish.waitForObject(self.SUBTAB), tabText)
                break

        if not tabFound:
            raise Exception("Tab not found: " + tabName)

    def checkFileExist(self, filename):
        squish.waitForObject(names.settings_OCC_SettingsDialog)
        squish.waitForObjectExists(
            {
                "column": 1,
                "container": names.oCC_IssuesWidget_tableView_QTableView,
                "text": RegularExpression(buildConflictedRegex(filename)),
                "type": "QModelIndex",
            }
        )

    def checkBlackListedResourceExist(self, context, filename):
        squish.waitForObject(names.settings_OCC_SettingsDialog)

        result = squish.waitFor(
            lambda: self.isResourceBlackListed(context, filename),
            context.userData['maxSyncTimeout'] * 1000,
        )

        return result

    def isResourceBlackListed(self, context, filename):
        try:
            # The blacklisted file does not have text like (conflicted copy) appended to it in the not synced table.
            fileRow = squish.waitForObject(
                {
                    "column": 1,
                    "container": names.oCC_IssuesWidget_tableView_QTableView,
                    "text": filename,
                    "type": "QModelIndex",
                },
                context.userData['lowestSyncTimeout'] * 1000,
            )["row"]
            squish.waitForObjectExists(
                {
                    "column": 6,
                    "row": fileRow,
                    "container": names.oCC_IssuesWidget_tableView_QTableView,
                    "text": "Blacklisted",
                    "type": "QModelIndex",
                },
                context.userData['lowestSyncTimeout'] * 1000,
            )

            return True
        except:
            return False
