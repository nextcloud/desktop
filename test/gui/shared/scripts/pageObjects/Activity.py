import names
import squish
from objectmaphelper import RegularExpression

from helpers.FilesHelper import buildConflictedRegex
from helpers.ConfigHelper import get_config


class Activity:
    TAB_CONTAINER = {
        "container": names.settings_stack_QStackedWidget,
        "type": "QTabWidget",
        "visible": 1,
    }
    SUBTAB_CONTAINER = {
        "container": names.settings_stack_QStackedWidget,
        "name": "qt_tabwidget_tabbar",
        "type": "QTabBar",
        "visible": 1,
    }
    NOT_SYNCED_TABLE = {
        "container": names.qt_tabwidget_stackedwidget_OCC_IssuesWidget_OCC_IssuesWidget,
        "name": "_tableView",
        "type": "QTableView",
        "visible": 1,
    }
    LOCAL_ACTIVITY_FILTER_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ProtocolWidget_OCC_ProtocolWidget,
        "name": "_filterButton",
        "type": "QPushButton",
        "visible": 1,
    }
    SYNCED_ACTIVITY_FILTER_OPTION_SELECTOR = {
        "type": "QMenu",
        "unnamed": 1,
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    SYNCED_ACTIVITY_TABLE = {
        "container": names.qt_tabwidget_stackedwidget_OCC_ProtocolWidget_OCC_ProtocolWidget,
        "name": "_tableView",
        "type": "QTableView",
        "visible": 1,
    }
    NOT_SYNCED_FILTER_BUTTON = {
        "container": names.qt_tabwidget_stackedwidget_OCC_IssuesWidget_OCC_IssuesWidget,
        "name": "_filterButton",
        "type": "QPushButton",
        "visible": 1,
    }
    NOT_SYNCED_FILTER_OPTION_SELECTOR = {
        "type": "QMenu",
        "unnamed": 1,
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    SYNCED_ACTIVITY_TABLE_HEADER_SELECTOR = {
        "container": names.oCC_ProtocolWidget_tableView_QTableView,
        "name": "ActivityListHeaderV2",
        "orientation": 1,
        "type": "OCC::ExpandingHeaderView",
        "visible": 1,
    }
    NOT_SYNCED_ACTIVITY_TABLE_HEADER_SELECTOR = {
        "container": names.oCC_IssuesWidget_tableView_QTableView,
        "name": "ActivityErrorListHeaderV2",
        "orientation": 1,
        "type": "OCC::ExpandingHeaderView",
        "visible": 1,
    }

    @staticmethod
    def getTabObject(tab_index):
        return {
            "container": Activity.SUBTAB_CONTAINER,
            "index": tab_index,
            "type": "TabItem",
        }

    @staticmethod
    def getTabText(tab_index):
        return squish.waitForObjectExists(Activity.getTabObject(tab_index)).text

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

        # NOTE: Some activity tabs are loaded dynamically
        # and the tab index changes after all the tabs are loaded properly
        # So wait for a second to let the UI render the tabs properly
        # before trying to click the tab
        squish.snooze(get_config("lowestSyncTimeout"))

        # Selecting tab by name fails for "Not Synced" when there are no unsynced files
        # Because files count will be appended like "Not Synced (2)"
        # So to overcome this the following approach has been implemented
        tabCount = squish.waitForObjectExists(Activity.SUBTAB_CONTAINER).count
        tabs = []
        for index in range(tabCount):
            tabText = Activity.getTabText(index)
            tabs.append(tabText)

            if tabName in tabText:
                tabFound = True
                # clickTab becomes flaky with "Not Synced" tab
                # because the tab text changes. e.g. "Not Synced (2)"
                # squish.clickTab(Activity.TAB_CONTAINER, tabText)

                # NOTE: If only the objectOrName is specified,
                # the object is clicked in the middle by the Qt::LeftButton button
                # and with no keyboard modifiers pressed.
                squish.mouseClick(
                    squish.waitForObjectExists(Activity.getTabObject(index))
                )
                break

        if not tabFound:
            raise LookupError(
                "Tab not found: "
                + tabName
                + " in "
                + str(tabs)
                + ". Tabs count: "
                + str(tabCount)
            )

    @staticmethod
    def checkFileExist(filename):
        squish.waitForObjectExists(
            Activity.getNotSyncedFileSelector(
                RegularExpression(buildConflictedRegex(filename))
            )
        )

    @staticmethod
    def resourceIsBlacklisted(filename):
        result = squish.waitFor(
            lambda: Activity.hasSyncStatus(filename, "Blacklisted"),
            get_config("maxSyncTimeout") * 1000,
        )
        return result

    @staticmethod
    def resourceIsIgnored(filename):
        result = squish.waitFor(
            lambda: Activity.hasSyncStatus(filename, "File Ignored"),
            get_config("maxSyncTimeout") * 1000,
        )
        return result

    @staticmethod
    def resource_is_excluded(filename):
        result = squish.waitFor(
            lambda: Activity.hasSyncStatus(filename, "Excluded"),
            get_config("maxSyncTimeout") * 1000,
        )
        return result

    @staticmethod
    def hasSyncStatus(filename, status):
        try:
            fileRow = squish.waitForObject(
                Activity.getNotSyncedFileSelector(filename),
                get_config("lowestSyncTimeout") * 1000,
            )["row"]
            if Activity.getNotSyncedStatus(fileRow) == status:
                return True
            return False
        except:
            return False

    @staticmethod
    def select_synced_filter(sync_filter):
        squish.clickButton(squish.waitForObject(Activity.LOCAL_ACTIVITY_FILTER_BUTTON))
        squish.activateItem(
            squish.waitForObjectItem(
                Activity.SYNCED_ACTIVITY_FILTER_OPTION_SELECTOR, sync_filter
            )
        )

    @staticmethod
    def get_synced_file_selector(resource):
        return {
            "column": Activity.get_synced_table_column_number_by_name("File"),
            "container": Activity.SYNCED_ACTIVITY_TABLE,
            "text": resource,
            "type": "QModelIndex",
        }

    @staticmethod
    def get_synced_table_column_number_by_name(column_name):
        return squish.waitForObject(
            {
                "container": Activity.SYNCED_ACTIVITY_TABLE_HEADER_SELECTOR,
                "text": column_name,
                "type": "HeaderViewItem",
                "visible": True,
            }
        )["section"]

    @staticmethod
    def check_synced_table(resource, action, account):
        try:
            fileRow = squish.waitForObject(
                Activity.get_synced_file_selector(resource),
                get_config("lowestSyncTimeout") * 1000,
            )["row"]
            squish.waitForObjectExists(
                {
                    "column": Activity.get_synced_table_column_number_by_name("Action"),
                    "row": fileRow,
                    "container": Activity.SYNCED_ACTIVITY_TABLE,
                    "text": action,
                    "type": "QModelIndex",
                }
            )
            squish.waitForObjectExists(
                {
                    "column": Activity.get_synced_table_column_number_by_name(
                        "Account"
                    ),
                    "row": fileRow,
                    "container": Activity.SYNCED_ACTIVITY_TABLE,
                    "text": account,
                    "type": "QModelIndex",
                }
            )
            return True
        except:
            return False

    @staticmethod
    def select_not_synced_filter(filter_option):
        squish.clickButton(squish.waitForObject(Activity.NOT_SYNCED_FILTER_BUTTON))
        squish.activateItem(
            squish.waitForObjectItem(
                Activity.NOT_SYNCED_FILTER_OPTION_SELECTOR, filter_option
            )
        )

    @staticmethod
    def get_not_synced_table_column_number_by_name(column_name):
        return squish.waitForObject(
            {
                "container": Activity.NOT_SYNCED_ACTIVITY_TABLE_HEADER_SELECTOR,
                "text": column_name,
                "type": "HeaderViewItem",
                "visible": True,
            }
        )["section"]

    @staticmethod
    def check_not_synced_table(resource, status, account):
        try:
            fileRow = squish.waitForObject(
                Activity.getNotSyncedFileSelector(resource),
                get_config("lowestSyncTimeout") * 1000,
            )["row"]
            squish.waitForObjectExists(
                {
                    "column": Activity.get_not_synced_table_column_number_by_name(
                        "Status"
                    ),
                    "row": fileRow,
                    "container": Activity.NOT_SYNCED_TABLE,
                    "text": status,
                    "type": "QModelIndex",
                }
            )
            squish.waitForObjectExists(
                {
                    "column": Activity.get_not_synced_table_column_number_by_name(
                        "Account"
                    ),
                    "row": fileRow,
                    "container": Activity.NOT_SYNCED_TABLE,
                    "text": account,
                    "type": "QModelIndex",
                }
            )
            return True
        except:
            return False
