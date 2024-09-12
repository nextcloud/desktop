import names
import squish


class Settings:
    CHECKBOX_OPTION_ITEM = {
        "container": names.stack_scrollArea_QScrollArea,
        "type": "QCheckBox",
        "visible": 1,
    }
    NETWORK_OPTION_ITEM = {
        "container": names.stack_scrollArea_QScrollArea,
        "type": "QGroupBox",
        "visible": 1,
    }
    ABOUT_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "name": "about_pushButton",
        "type": "QPushButton",
        "visible": 1,
    }
    ABOUT_DIALOG = {
        "name": "OCC__AboutDialog",
        "type": "OCC::AboutDialog",
        "visible": 1,
    }
    ABOUT_DIALOG_OK_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": ABOUT_DIALOG,
    }

    GENERAL_OPTIONS_MAP = {
        "Start on Login": "autostartCheckBox",
        "Use Monochrome Icons in the system tray": "monoIconsCheckBox",
        "Language": "languageDropdown",
        "Show desktop Notifications": "desktopNotificationsCheckBox",
    }
    ADVANCED_OPTION_MAP = {
        "Sync hidden files": "syncHiddenFilesCheckBox",
        "Show crash reporter": "",
        "Edit ignored files": "ignoredFilesButton",
        "Log settings": "logSettingsButton",
        "Ask for confirmation before synchronizing folders larger than 500 MB": "newFolderLimitCheckBox",
        "Ask for confirmation before synchronizing external storages": "newExternalStorage",
    }
    NETWORK_OPTION_MAP = {
        "Proxy Settings": "proxyGroupBox",
        "Download Bandwidth": "downloadBox",
        "Upload Bandwidth": "uploadBox",
    }

    @staticmethod
    def get_checkbox_option_selector(name):
        selector = Settings.CHECKBOX_OPTION_ITEM.copy()
        selector.update({"name": name})
        if name == "languageDropdown":
            selector.update({"type": "QComboBox"})
        elif name in ("ignoredFilesButton", "logSettingsButton"):
            selector.update({"type": "QPushButton"})
        return selector

    @staticmethod
    def get_network_option_selector(name):
        selector = Settings.NETWORK_OPTION_ITEM.copy()
        selector.update({"name": name})
        return selector

    @staticmethod
    def check_general_option(option):
        selector = Settings.GENERAL_OPTIONS_MAP[option]
        squish.waitForObjectExists(Settings.get_checkbox_option_selector(selector))

    @staticmethod
    def check_advanced_option(option):
        selector = Settings.ADVANCED_OPTION_MAP[option]
        squish.waitForObjectExists(Settings.get_checkbox_option_selector(selector))

    @staticmethod
    def check_network_option(option):
        selector = Settings.NETWORK_OPTION_MAP[option]
        squish.waitForObjectExists(Settings.get_network_option_selector(selector))

    @staticmethod
    def open_about_button():
        squish.clickButton(squish.waitForObject(Settings.ABOUT_BUTTON))

    @staticmethod
    def wait_for_about_dialog_to_be_visible():
        squish.waitForObjectExists(Settings.ABOUT_DIALOG)

    @staticmethod
    def close_about_dialog():
        squish.clickButton(squish.waitForObjectExists(Settings.ABOUT_DIALOG_OK_BUTTON))
