# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path
import tempfile
import unittest

from craft.capture_qt import capture_qt
from common.sbom_utils import digest


class CaptureQtTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "craft"
        self.output = Path(self.temp.name) / "capture"
        (self.root / "sbom").mkdir(parents=True)
        self.packages = [{"packageId": 1, "packagePath": "libs/qt6/qtbase"}]

    def test_exact_copy_and_owner_verification(self):
        source = self.root / "sbom/qtbase.spdx"
        source.write_bytes(b"SPDXVersion: SPDX-2.3\n")
        rows = [{"filename": "sbom/qtbase.spdx", "packageId": 1, "fileHash": "[SHA256]" + digest(source)}]
        report = capture_qt(self.root, self.output, self.packages, rows)
        self.assertEqual((self.output / "qt/qtbase.spdx").read_bytes(), source.read_bytes())
        self.assertEqual(report["status"], "verified")
        self.assertEqual(report["qt_packages_without_verified_document"], [])
        rows[0]["fileHash"] = "wrong"
        report = capture_qt(self.root, self.output, self.packages, rows)
        self.assertEqual(report["status"], "needs-review")
        self.assertFalse(report["documents"][0]["ownership_verified"])

    def test_missing_documents_and_symlinks(self):
        report = capture_qt(self.root, self.output, self.packages, [])
        self.assertEqual(report["status"], "no-documents")
        self.assertEqual(report["qt_packages_without_verified_document"], ["libs/qt6/qtbase"])
        (self.root / "sbom/qtbase.spdx").symlink_to("missing")
        with self.assertRaises(ValueError):
            capture_qt(self.root, self.output, self.packages, [])

    def test_ambiguous_owners(self):
        source = self.root / "sbom/qtbase.spdx"
        source.write_text("SPDXVersion: SPDX-2.3\n")
        packages = self.packages + [{"packageId": 2, "packagePath": "libs/qt6/other"}]
        rows = [{"filename": "sbom/qtbase.spdx", "packageId": i, "fileHash": "[SHA256]" + digest(source)} for i in (1, 2)]
        report = capture_qt(self.root, self.output, packages, rows)
        self.assertFalse(report["documents"][0]["ownership_verified"])
