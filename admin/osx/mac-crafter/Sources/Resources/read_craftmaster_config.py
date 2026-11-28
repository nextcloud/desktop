# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import importlib.util
import json
import sys
from pathlib import Path


def main():
    (
        path,
        target,
        blueprint,
        craftmaster_directory,
        kde_revision_key,
        client_revision_key,
        override_path,
        build_tests_override,
        blueprint_root_only,
    ) = sys.argv[1:]
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

    paths = [Path(path).absolute()]
    if override_path:
        paths.append(Path(override_path).absolute())
    config = ReadOnlyConfig(paths, variables=None)

    def blueprint_value(key):
        return config.get("BlueprintSettings", key, default=None, target=target) or None

    def target_setting(key, *, boolean=False):
        get_value = config.getBool if boolean else config.get
        if (target, key) in config:
            return get_value(target, key)
        if (f"{target}-GeneralSettings", key) in config or ("GeneralSettings", key) in config:
            return get_value("GeneralSettings", key, target=target)
        return None

    if blueprint_root_only == "true":
        blueprint_root = target_setting("Blueprints/BlueprintRoot")
        if blueprint_root is not None:
            blueprint_root = str(Path(blueprint_root).absolute())
        print(json.dumps({"blueprintRoot": blueprint_root}))
        return

    tests_key = f"{blueprint}.buildTests"
    build_tests = None
    if build_tests_override:
        build_tests = build_tests_override == "true"
    elif blueprint_value(tests_key) is not None:
        build_tests = config.getBool("BlueprintSettings", tests_key, target=target)

    print(json.dumps({
        "kdeBlueprintsRevision": blueprint_value(kde_revision_key),
        "clientBlueprintsRevision": blueprint_value(client_revision_key),
        "compileBuildType": target_setting("Compile/BuildType") or None,
        "blueprintBuildType": blueprint_value(f"{blueprint}.buildType"),
        "buildTests": build_tests,
        "workOffline": target_setting("General/WorkOffline", boolean=True),
    }))


if __name__ == "__main__":
    main()
