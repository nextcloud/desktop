# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path
import tempfile
import unittest

from craft.merge_qt import merge_qt
from common.sbom_utils import digest

DOCUMENT = '''SPDXVersion: SPDX-2.3
SPDXID: SPDXRef-DOCUMENT
DocumentNamespace: https://example.org/qt-test
PackageName: QtCore
SPDXID: SPDXRef-Qt
PackageVersion: 6.11.2
PackageName: bundled-library
SPDXID: SPDXRef-Bundled
PackageVersion: 1.2.3
ExternalRef: PACKAGE-MANAGER purl pkg:generic/bundled-library@1.2.3
ExternalRef: SECURITY cpe23Type cpe:2.3:a:vendor:bundled-library:1.2.3:*:*:*:*:*:*:*
ExternalRef: SECURITY cpe23Type cpe:2.3:a:old_vendor:bundled-library:1.2.3:*:*:*:*:*:*:*
ExternalRef: SECURITY cpe23Type cpe:2.3:a:vendor:bundled-library:1.2.3:*:*:*:*:*:*:*
PackageLicenseDeclared: MIT
'''


class MergeQtTests(unittest.TestCase):
    def test_import_and_unverified_exclusion(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "qt").mkdir()
            source = root / "qt/qtbase.spdx"
            source.write_text(DOCUMENT)
            rows = [{"filename": "sbom/qtbase.spdx", "packageId": 1, "fileHash": "[SHA256]" + digest(source)}]
            packages = [{"packageId": 1}]
            bom = {"components": [{"name": "Craft package"}], "metadata": {"properties": []}}
            merge_qt(bom, root, rows, packages)
            self.assertEqual(len(bom["components"]), 3)
            bundled = next(c for c in bom["components"] if c["name"] == "bundled-library")
            self.assertEqual(bundled["version"], "1.2.3")
            self.assertEqual(bundled["purl"], "pkg:generic/bundled-library@1.2.3")
            self.assertEqual(bundled["cpe"], "cpe:2.3:a:vendor:bundled-library:1.2.3:*:*:*:*:*:*:*")
            self.assertEqual([p["value"] for p in bundled["properties"] if p["name"] == "syft:cpe23"],
                             ["cpe:2.3:a:old_vendor:bundled-library:1.2.3:*:*:*:*:*:*:*"])
            self.assertNotIn("cpe", next(c for c in bom["components"] if c["name"] == "QtCore"))
            self.assertEqual(bundled["licenses"][0]["expression"], "MIT")
            self.assertNotIn("dependencies", bom)
            source.write_text(DOCUMENT + "\n")
            bom = {"components": [], "metadata": {"properties": []}}
            merge_qt(bom, root, rows, packages)
            self.assertEqual(bom["components"], [])
            self.assertTrue(any(p["name"].endswith("qt-import-issues") for p in bom["metadata"]["properties"]))

    def test_no_documents(self):
        with tempfile.TemporaryDirectory() as directory:
            bom = {"components": [], "metadata": {"properties": []}}
            merge_qt(bom, Path(directory), [], [])
            self.assertEqual(bom["components"], [])

    def test_build_only_owner_classifies_supplier_components(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "qt").mkdir()
            source = root / "qt/qttools.spdx"
            source.write_text(DOCUMENT)
            rows = [{"filename": "sbom/qttools.spdx", "packageId": 1, "fileHash": "[SHA256]" + digest(source)}]
            bom = {"components": [], "metadata": {"properties": []}}
            merge_qt(bom, root, rows, [{"packageId": 1, "packagePath": "libs/qt6/qttools"}],
                     {"libs/qt6/qttools": "build"})
            for component in bom["components"]:
                self.assertIn({"name": "nextcloud:sbom:dependency-scope", "value": "build"}, component["properties"])

    def test_malformed_verified_document(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "qt").mkdir()
            source = root / "qt/qtbase.spdx"
            source.write_text("invalid")
            rows = [{"filename": "sbom/qtbase.spdx", "packageId": 1, "fileHash": "[SHA256]" + digest(source)}]
            with self.assertRaises(ValueError):
                merge_qt({"components": [], "metadata": {"properties": []}}, root, rows, [{"packageId": 1}])
