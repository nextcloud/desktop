# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import configparser


class Config:
    def __init__(self, files, variables):
        self.settings = configparser.ConfigParser(
            interpolation=configparser.ExtendedInterpolation(),
            allow_no_value=True,
        )
        self.settings.optionxform = str
        for path in files:
            with path.open(encoding="utf-8") as source:
                self.settings.read_file(source)

        if self.get("General", "DumpConfig", default=False):
            files[0].with_suffix(".ini.dump").touch()

    def __contains__(self, entry):
        if isinstance(entry, tuple):
            section, key = entry
            return self.settings.has_section(section) and self.settings.has_option(
                section,
                key,
            )
        return self.settings.has_section(entry)

    def get(self, section, key, default=None, target=None):
        target_section = f"{target}-{section}"
        if (target_section, key) in self:
            return self.settings.get(target_section, key)
        return self.settings.get(section, key, fallback=default)

    def getBool(self, section, key, default=False, target=None):
        value = self.get(section, key, default=str(default), target=target)
        return self.settings._convert_to_boolean(value)
