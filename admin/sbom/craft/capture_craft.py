# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Preserve installed Craft packages for later SBOM processing."""

import argparse
import configparser
import sqlite3
from datetime import datetime, timezone
from pathlib import Path
import shutil

from craft.capture_qt import capture_qt
from craft.dependencies import capture_dependencies
from craft.craft_sbom import convert
from common.sbom_utils import digest, write_json


def snapshot_craft(root, evidence):
    source = root / "etc/blueprints/install.db"
    if not source.is_file():
        raise ValueError(f"Craft installation database missing: {source}")
    target = evidence / "craft-install.db"
    with sqlite3.connect(source.as_uri() + "?mode=ro", uri=True) as database:
        with sqlite3.connect(target) as snapshot:
            database.backup(snapshot)
    with sqlite3.connect(target) as database:
        database.row_factory = sqlite3.Row
        packages = [dict(row) for row in database.execute("SELECT * FROM packageList ORDER BY packageId")]
        files = [dict(row) for row in database.execute("SELECT * FROM fileList ORDER BY fileId")]
    settings = configparser.ConfigParser(interpolation=None)
    settings.read(root / "etc/CraftSettings.ini")
    allowed = {"General": ("ABI", "MacDeploymentTarget"),
               "Packager": ("UseCache", "CreateCache", "CacheVersion", "RepositoryUrl")}
    selected = {section: {key: settings.get(section, key) for key in keys
                          if settings.has_option(section, key)} for section, keys in allowed.items()}
    write_json(evidence / "craft-settings.json", selected)
    write_json(evidence / "craft-packages.json", packages)
    return packages, files


def capture(craft_root, output_dir, build_id, target_package=None):
    craft_root = Path(craft_root).resolve()
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=False)
    try:
        packages, files = snapshot_craft(craft_root, output_dir)
        capture_qt(craft_root, output_dir, packages, files)
        metadata = {
            "build_id": build_id,
            "captured_at": datetime.now(timezone.utc).isoformat(),
            "craft_root": str(craft_root),
            "source_database": "etc/blueprints/install.db",
            "sha256": digest(output_dir / "craft-install.db"),
            "package_count": len(packages),
            "scope": "installed Craft packages; may include build tools and unshipped packages",
        }
        if target_package:
            dependency_file = output_dir / "craft-dependencies.json"
            capture_dependencies(craft_root, dependency_file, target_package)
            metadata["dependencies_sha256"] = digest(dependency_file)
        write_json(output_dir / "capture.json", metadata)
        convert(output_dir)
    except Exception:
        shutil.rmtree(output_dir)
        raise
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--craft-root", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--target-package", help="Resolve build/runtime roles from local Craft blueprints")
    args = parser.parse_args()
    capture(args.craft_root, args.output_dir, args.build_id, args.target_package)
    print(f"Craft dependency inventory saved to {args.output_dir}")
