# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Convert a captured Craft database to a partial CycloneDX inventory."""

import argparse
import json
from pathlib import Path
import sqlite3
import uuid

from craft.merge_qt import merge_qt
from craft.identities import normalize
from craft.dependency_scope import load_scopes, set_scope
from common.sbom_utils import digest, properties, reference


def make_bom(packages, metadata, scopes=None):
    components = []
    seen = set()
    for package in packages:
        name, version = package.get("packagePath"), package.get("version")
        if not all(isinstance(value, str) and value for value in (name, version)):
            raise ValueError("Each Craft package needs a packagePath and version")
        ref = reference("craft", json.dumps([name, version, package.get("packageId")]))
        if ref in seen:
            raise ValueError("Duplicate Craft package record")
        seen.add(ref)
        component = {
            "type": "library", "bom-ref": ref, "name": name, "version": version,
            "properties": properties({
                "craft-package-path": name,
                "craft-installed-version": version,
                "craft-package-id": package.get("packageId"),
                "craft-cache-version": package.get("cacheVersion"),
                "craft-revision": package.get("revision"),
            }),
        }
        normalize(component, name, version)
        set_scope(component, (scopes or {}).get(name, "unknown"),
                  "craft-dependencies.json" if scopes else "Dependency roles were not captured")
        components.append(component)
    return {
        "$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json",
        "bomFormat": "CycloneDX", "specVersion": "1.6", "version": 1,
        "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
        "metadata": {
            "timestamp": metadata["captured_at"],
            "component": {
                "type": "application", "bom-ref": "craft-inventory",
                "name": "Craft dependencies only", "version": metadata["build_id"],
                "description": "Installed Craft packages and verified Qt supplier package inventories, including build tools and unshipped packages. Not a complete client SBOM. Known Craft release identities are normalized with original records retained in properties; unresolved identities are labelled. Qt supplier identifiers are preserved.",
            },
            "properties": properties({"database-sha256": metadata["sha256"], "dependency-graph": "not captured"}),
        },
        "components": components,
        "compositions": [{"aggregate": "incomplete", "assemblies": ["craft-inventory"]}],
    }


def convert(capture_dir):
    capture_dir = Path(capture_dir).resolve()
    metadata = json.loads((capture_dir / "capture.json").read_text())
    database_path = capture_dir / "craft-install.db"
    if digest(database_path) != metadata["sha256"]:
        raise ValueError("Captured database checksum does not match capture.json")
    with sqlite3.connect(database_path.as_uri() + "?mode=ro", uri=True) as database:
        database.row_factory = sqlite3.Row
        packages = [dict(row) for row in database.execute("SELECT * FROM packageList ORDER BY packageId")]
    if len(packages) != metadata["package_count"]:
        raise ValueError("Captured package count does not match capture.json")
    scopes = load_scopes(capture_dir, metadata)
    bom = make_bom(packages, metadata, scopes)
    if (capture_dir / "qt").exists():
        with sqlite3.connect(database_path.as_uri() + "?mode=ro", uri=True) as database:
            database.row_factory = sqlite3.Row
            files = [dict(row) for row in database.execute("SELECT * FROM fileList")]
        merge_qt(bom, capture_dir, files, packages, scopes)
    output = capture_dir / "craft.cdx.json"
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(bom, indent=2) + "\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-dir", required=True, type=Path)
    print(convert(parser.parse_args().capture_dir))
