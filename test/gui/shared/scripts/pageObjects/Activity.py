import names
import squish
from objectmaphelper import RegularExpression

from helpers.FilesHelper import build_conflicted_regex
from helpers.ConfigHelper import get_config


class Activity:
    TAB_CONTAINER = {
        "container": names.settings_dialogStack_QStackedWidget,
        "type": "QTabWidget",
        "visible": 1,
    }
    SUBTAB_CONTAINER = {
        "container": names.settings_dialogStack_QStackedWidget,
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
    def get_tab_object(tab_index):
        return {
            "container": Activity.SUBTAB_CONTAINER,
            "index": tab_index,
            "type": "TabItem",
        }

    @staticmethod
    def get_tab_text(tab_index):
        return squish.waitForObjectExists(Activity.get_tab_object(tab_index)).text

    @staticmethod
    def get_not_synced_file_selector(resource):
        return {
            "column": 1,
            "container": Activity.NOT_SYNCED_TABLE,
            "text": resource,
            "type": "QModelIndex",
        }

    @staticmethod
    def get_not_synced_status(row):
        return squish.waitForObjectExists(
            {
                "column": 6,
                "row": row,
                "container": Activity.NOT_SYNCED_TABLE,
                "type": "QModelIndex",
            }
        ).text

    @staticmethod
    def click_tab(tab_name):
        tab_found = False

        # NOTE: Some activity tabs are loaded dynamically
        # and the tab index changes after all the tabs are loaded properly
        # So wait for a second to let the UI render the tabs properly
        # before trying to click the tab
        squish.snooze(get_config("lowestSyncTimeout"))

        # Selecting tab by name fails for "Not Synced" when there are no unsynced files
        # Because files count will be appended like "Not Synced (2)"
        # So to overcome this the following approach has been implemented
        tab_count = squish.waitForObjectExists(Activity.SUBTAB_CONTAINER).count
        tabs = []
        for index in range(tab_count):
            tab_text = Activity.get_tab_text(index)
            tabs.append(tab_text)

            if tab_name in tab_text:
                tab_found = True
                # click_tab becomes flaky with "Not Synced" tab
                # because the tab text changes. e.g. "Not Synced (2)"
                # squish.click_tab(Activity.TAB_CONTAINER, tab_text)

                # NOTE: If only the objectOrName is specified,
                # the object is clicked in the middle by the Qt::LeftButton button
                # and with no keyboard modifiers pressed.
                squish.mouseClick(
                    squish.waitForObjectExists(Activity.get_tab_object(index))
                )
                break

        if not tab_found:
            raise LookupError(
                "Tab not found: "
                + tab_name
                + " in "
                + str(tabs)
                + ". Tabs count: "
                + str(tab_count)
            )

    @staticmethod
    def check_file_exist(filename):
        squish.waitForObjectExists(
            Activity.get_not_synced_file_selector(
                RegularExpression(build_conflicted_regex(filename))
            )
        )

    @staticmethod
    def is_resource_blacklisted(filename):
        result = squish.waitFor(
            lambda: Activity.has_sync_status(filename, "Blacklisted"),
            get_config("maxSyncTimeout") * 1000,
        )
        return result

    @staticmethod
    def is_resource_ignored(filename):
        result = squish.waitFor(
            lambda: Activity.has_sync_status(filename, "File Ignored"),
            get_config("maxSyncTimeout") * 1000,
        )
        return result

    @staticmethod
    def is_resource_excluded(filename):
        result = squish.waitFor(
            lambda: Activity.has_sync_status(filename, "Excluded"),
            get_config("maxSyncTimeout") * 1000,
        )
        return result

    @staticmethod
    def has_sync_status(filename, status):
        try:
            file_row = squish.waitForObject(
                Activity.get_not_synced_file_selector(filename),
                get_config("lowestSyncTimeout") * 1000,
            )["row"]
            if Activity.get_not_synced_status(file_row) == status:
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
            file_row = squish.waitForObject(
                Activity.get_synced_file_selector(resource),
                get_config("lowestSyncTimeout") * 1000,
            )["row"]
            squish.waitForObjectExists(
                {
                    "column": Activity.get_synced_table_column_number_by_name("Action"),
                    "row": file_row,
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
                    "row": file_row,
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
            file_row = squish.waitForObject(
                Activity.get_not_synced_file_selector(resource),
                get_config("lowestSyncTimeout") * 1000,
            )["row"]
            squish.waitForObjectExists(
                {
                    "column": Activity.get_not_synced_table_column_number_by_name(
                        "Status"
                    ),
                    "row": file_row,
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
                    "row": file_row,
                    "container": Activity.NOT_SYNCED_TABLE,
                    "text": account,
                    "type": "QModelIndex",
                }
            )
            return True
        except:
            return False
