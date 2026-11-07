# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import tempfile
import unittest

from sparkle.capture_sparkle import capture
from common.sbom_utils import digest


class CaptureSparkleTests(unittest.TestCase):
    def test_capture_archive_identity_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "download"
            archive.write_bytes(b"archive fixture")
            output = root / "capture"
            metadata = capture(output, "build", archive, "2.9.1", "https://example.org/Sparkle.tar.xz")
            self.assertEqual((output / "Sparkle.tar.xz").read_bytes(), archive.read_bytes())
            self.assertEqual(metadata["archive_sha256"], digest(archive))
            bom = json.loads((output / "downloads.cdx.json").read_text())
            self.assertEqual(bom["components"][0]["purl"], "pkg:github/sparkle-project/sparkle@2.9.1")
            self.assertNotIn("hashes", bom["components"][0])
            with self.assertRaises(FileExistsError):
                capture(output, "other")
            self.assertTrue(json.loads((output / "capture.json").read_text())["enabled"])

    def test_disabled_and_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "disabled"
            self.assertFalse(capture(output, "build")["enabled"])
            self.assertEqual(json.loads((output / "downloads.cdx.json").read_text())["components"], [])
            with self.assertRaises(ValueError):
                capture(root / "invalid", "build", version="2.9.1")
            self.assertFalse((root / "invalid").exists())
            with self.assertRaises(FileNotFoundError):
                capture(root / "missing", "build", root / "absent", "2.9.1", "https://example.org/archive")
            self.assertFalse((root / "missing").exists())
