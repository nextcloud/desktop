# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

from craft.craft_sbom import convert, make_bom
from common.sbom_utils import digest


class CraftSbomTests(unittest.TestCase):
    def metadata(self):
        return {"build_id": "build-1", "captured_at": "2026-11-03T12:00:00Z", "sha256": "a" * 64, "package_count": 1}

    def test_preserves_all_records_without_inventing_identifiers(self):
        packages = [{"packageId": 1, "packagePath": "libs/unknown", "version": "2.86.0-1", "cacheVersion": "cache-1"},
                    {"packageId": 2, "packagePath": "dev-utils/cmake", "version": "4.0.0"}]
        bom = make_bom(packages, self.metadata())
        self.assertEqual(bom["metadata"]["component"]["name"], "Craft dependencies only")
        self.assertEqual([c["version"] for c in bom["components"]], ["2.86.0-1", "4.0.0"])
        self.assertEqual(bom["compositions"][0]["aggregate"], "incomplete")
        self.assertNotIn("dependencies", bom)
        for component in bom["components"]:
            self.assertNotIn("purl", component)
            self.assertNotIn("licenses", component)
        self.assertEqual(make_bom([], self.metadata())["components"], [])
        with self.assertRaises(ValueError):
            make_bom([packages[0], packages[0]], self.metadata())
        for record in ({}, {"packagePath": "libs/glib", "version": None}):
            with self.assertRaises(ValueError):
                make_bom([record], self.metadata())

    def test_conversion_checksum_count_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            db_path = root / "craft-install.db"
            with sqlite3.connect(db_path) as database:
                database.execute("CREATE TABLE packageList(packageId INTEGER, packagePath TEXT, version TEXT)")
                database.execute("INSERT INTO packageList VALUES (1, 'libs/glib', '2.86.0-1')")
            metadata = self.metadata()
            metadata_path = root / "capture.json"
            metadata_path.write_text(json.dumps(metadata))
            with self.assertRaises(ValueError):
                convert(root)
            self.assertFalse((root / "craft.cdx.json").exists())
            metadata["sha256"] = digest(db_path)
            metadata["package_count"] = 2
            metadata_path.write_text(json.dumps(metadata))
            with self.assertRaises(ValueError):
                convert(root)
            metadata["package_count"] = 1
            metadata_path.write_text(json.dumps(metadata))
            output = convert(root)
            original = output.read_bytes()
            component = json.loads(original)["components"][0]
            self.assertEqual(component["name"], "glib")
            self.assertEqual(component["version"], "2.86.0")
            self.assertEqual(component["cpe"], "cpe:2.3:a:gnome:glib:2.86.0:*:*:*:*:*:*:*")
            self.assertIn({"name": "nextcloud:sbom:craft-installed-version", "value": "2.86.0-1"}, component["properties"])
            with self.assertRaises(FileExistsError):
                convert(root)
            self.assertEqual(output.read_bytes(), original)
