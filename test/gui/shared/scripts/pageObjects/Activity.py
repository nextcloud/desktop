import names
import squish
from objectmaphelper import RegularExpression
from helpers.FilesHelper import buildConflictedRegex


class Activity:
    SUBTAB = {
        "container": names.settings_stack_QStackedWidget,
        "type": "QTabWidget",
        "unnamed": 1,
        "visible": 1,
    }

    def clickTab(self, tabName):
        # TODO: find some way to dynamically select the tab name
        # It might take some time for all files to sync except the expected number of unsynced files
        squish.snooze(10)
        squish.clickTab(squish.waitForObject(self.SUBTAB), tabName)

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
