import names
import squish
from objectmaphelper import RegularExpression
from helpers.FilesHelper import buildConflictedRegex
from helpers.ConfigHelper import get_config


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
    NOT_SYNCED_TABLE = {
        "container": names.qt_tabwidget_stackedwidget_OCC_IssuesWidget_OCC_IssuesWidget,
        "name": "_tableView",
        "type": "QTableView",
        "visible": 1,
    }

    @staticmethod
    def getTabText(tab_index):
        return squish.waitForObjectExists(
            {
                "container": Activity.SUBTAB_CONTAINER,
                "index": tab_index,
                "type": "TabItem",
            }
        ).text

    @staticmethod
    def getNotSyncedFileSelector(resource):
        return {
            "column": 1,
            "container": Activity.NOT_SYNCED_TABLE,
            "text": resource,
            "type": "QModelIndex",
        }

    @staticmethod
    def getNotSyncedStatus(row):
        return squish.waitForObjectExists(
            {
                "column": 6,
                "row": row,
                "container": Activity.NOT_SYNCED_TABLE,
                "type": "QModelIndex",
            }
        ).text

    @staticmethod
    def clickTab(tabName):
        tabFound = False

        # Selecting tab by name fails for "Not Synced" when there are no unsynced files
        # Because files count will be appended like "Not Synced (2)"
        # So to overcome this the following approach has been implemented
        tabCount = squish.waitForObjectExists(Activity.SUBTAB_CONTAINER).count
        for index in range(tabCount):
            tabText = Activity.getTabText(index)

            if tabName in tabText:
                tabFound = True
                squish.clickTab(squish.waitForObject(Activity.SUBTAB), tabText)
                break

        if not tabFound:
            raise Exception("Tab not found: " + tabName)

    @staticmethod
    def checkFileExist(filename):
        squish.waitForObjectExists(
            Activity.getNotSyncedFileSelector(
                RegularExpression(buildConflictedRegex(filename))
            )
        )

    @staticmethod
    def checkBlackListedResourceExist(filename):

        result = squish.waitFor(
            lambda: Activity.isResourceBlackListed(filename),
            get_config('maxSyncTimeout') * 1000,
        )

        return result

    @staticmethod
    def isResourceBlackListed(filename):
        try:
            # The blacklisted file does not have text like (conflicted copy) appended to it in the not synced table.
            fileRow = squish.waitForObject(
                Activity.getNotSyncedFileSelector(filename),
                get_config('lowestSyncTimeout') * 1000,
            )["row"]
            if Activity.getNotSyncedStatus(fileRow) == "Blacklisted":
                return True
            return False
        except:
            return False
