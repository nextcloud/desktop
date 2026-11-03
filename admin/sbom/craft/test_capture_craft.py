# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest.mock import patch

from craft.capture_craft import capture
from common.sbom_utils import digest


class CaptureCraftTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "craft root"
        self.database = self.root / "etc/blueprints/install.db"
        self.database.parent.mkdir(parents=True)
        self.output = Path(self.temp.name) / "capture"

    def test_snapshot_preserves_versions_and_files_without_overwrite(self):
        with sqlite3.connect(self.database) as db:
            db.executescript("CREATE TABLE packageList(packageId INTEGER, packagePath TEXT, version TEXT);"
                             "CREATE TABLE fileList(fileId INTEGER, packageId INTEGER, filename TEXT);")
            db.execute("INSERT INTO packageList VALUES (1, 'libs/glib', '2.86.0-1')")
            db.execute("INSERT INTO fileList VALUES (1, 1, 'lib/libglib.dylib')")
        metadata = capture(self.root, self.output, "build-1")
        self.assertEqual(metadata["package_count"], 1)
        bom = json.loads((self.output / "craft.cdx.json").read_text())
        self.assertEqual(bom["metadata"]["component"]["name"], "Craft dependencies only")
        self.assertEqual(bom["components"][0]["version"], "2.86.0")
        self.assertEqual(metadata["sha256"], digest(self.output / "craft-install.db"))
        self.assertEqual(json.loads((self.output / "craft-packages.json").read_text())[0]["version"], "2.86.0-1")
        with sqlite3.connect(self.output / "craft-install.db") as db:
            self.assertEqual(db.execute("SELECT filename FROM fileList").fetchone()[0], "lib/libglib.dylib")
        with self.assertRaises(FileExistsError):
            capture(self.root, self.output, "build-2")
        self.assertEqual(json.loads((self.output / "capture.json").read_text())["build_id"], "build-1")

    def test_missing_and_invalid_database_leave_no_capture(self):
        with self.assertRaises(ValueError):
            capture(self.root, self.output, "build")
        self.assertFalse(self.output.exists())
        self.database.write_bytes(b"not a database")
        with self.assertRaises(sqlite3.DatabaseError):
            capture(self.root, self.output, "build")
        self.assertFalse(self.output.exists())

    def test_empty_inventory(self):
        with sqlite3.connect(self.database) as db:
            db.executescript("CREATE TABLE packageList(packageId INTEGER); CREATE TABLE fileList(fileId INTEGER);")
        self.assertEqual(capture(self.root, self.output, "empty")["package_count"], 0)

    def test_dependency_roles_are_captured_and_bound_to_the_inventory(self):
        with sqlite3.connect(self.database) as db:
            db.executescript("CREATE TABLE packageList(packageId INTEGER, packagePath TEXT, version TEXT);"
                             "CREATE TABLE fileList(fileId INTEGER, packageId INTEGER, filename TEXT);")
            db.execute("INSERT INTO packageList VALUES (1, 'libs/python', '3.11.11-1')")
        def write_dependencies(root, output, target):
            self.assertEqual(root, self.root)
            self.assertEqual(target, "client")
            output.write_text(json.dumps({"version": 1, "target_package": "client", "runtime": ["client"],
                                          "build_only": ["libs/python"]}))
        with patch("craft.capture_craft.capture_dependencies", side_effect=write_dependencies):
            metadata = capture(self.root, self.output, "build", target_package="client")
        self.assertEqual(metadata["dependencies_sha256"], digest(self.output / "craft-dependencies.json"))
        component = json.loads((self.output / "craft.cdx.json").read_text())["components"][0]
        self.assertIn({"name": "nextcloud:sbom:dependency-scope", "value": "build"}, component["properties"])
