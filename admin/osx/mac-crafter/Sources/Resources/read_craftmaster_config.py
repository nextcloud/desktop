# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import importlib.util
import json
import sys
from pathlib import Path


def main():
    path, target, blueprint, craftmaster_directory, kde_revision_key, client_revision_key = sys.argv[1:]
    config_file = Path(craftmaster_directory).absolute() / "Config.py"
    if not config_file.is_file():
        raise FileNotFoundError(f"CraftMaster configuration module does not exist: {config_file}")

    sys.path.insert(0, str(config_file.parent))
    spec = importlib.util.spec_from_file_location("Config", config_file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    class ReadOnlyConfig(module.Config):
        def get(self, section, key, *args, **kwargs):
            # CraftMaster can write expanded settings during initialization.
            if (section, key) == ("General", "DumpConfig"):
                return False
            return super().get(section, key, *args, **kwargs)

    config = ReadOnlyConfig([Path(path).absolute()], variables=None)

    def blueprint_value(key):
        return config.get("BlueprintSettings", key, default=None, target=target) or None

    compile_key = "Compile/BuildType"
    if (target, compile_key) in config:
        compile_type = config.get(target, compile_key)
    else:
        compile_type = config.get("GeneralSettings", compile_key, default=None, target=target)

    tests_key = f"{blueprint}.buildTests"
    build_tests = (
        config.getBool("BlueprintSettings", tests_key, target=target)
        if blueprint_value(tests_key) is not None
        else None
    )

    print(json.dumps({
        "kdeBlueprintsRevision": blueprint_value(kde_revision_key),
        "clientBlueprintsRevision": blueprint_value(client_revision_key),
        "compileBuildType": compile_type or None,
        "blueprintBuildType": blueprint_value(f"{blueprint}.buildType"),
        "buildTests": build_tests,
    }))


if __name__ == "__main__":
    main()
