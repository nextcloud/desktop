# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from swift.capture_swift import LOCKFILE, capture


class CaptureSwiftTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source with spaces"
        self.lock = self.source / LOCKFILE
        self.lock.parent.mkdir(parents=True)
        self.data = json.dumps({"version": 3, "pins": [
            {"identity": name, "location": "https://example.org/" + name,
             "state": {"revision": "abc", "version": "1.0.0"}}
            for name in ("direct", "indirect", "build-tool")
        ]}).encode()
        self.lock.write_bytes(self.data)
        self.output = self.root / "archive"

    @patch("swift.capture_swift.subprocess.check_output", return_value="commit\n")
    def test_preserves_all_packages_and_exact_bytes(self, git):
        result = capture(self.source, self.output, "build-1")
        self.assertEqual((self.output / "Package.resolved").read_bytes(), self.data)
        self.assertEqual(result["package_count"], 3)
        bom = json.loads((self.output / "swift.cdx.json").read_text())
        self.assertEqual({c["name"] for c in bom["components"]}, {"direct", "indirect", "build-tool"})
        self.assertEqual(result["sha256"], hashlib.sha256(self.data).hexdigest())
        self.assertEqual(result["source_commit"], "commit")
        self.assertEqual(json.loads((self.output / "capture.json").read_text()), result)
        with self.assertRaises(FileExistsError):
            capture(self.source, self.output, "build-2")
        self.assertEqual((self.output / "Package.resolved").read_bytes(), self.data)

    def test_missing_or_invalid_lock_does_not_create_archive(self):
        for data in (b"invalid", b'{}', b'{"version":3,"pins":[{}]}'):
            self.lock.write_bytes(data)
            with self.assertRaises(ValueError):
                capture(self.source, self.output, "build")
            self.assertFalse(self.output.exists())
        self.lock.unlink()
        with self.assertRaises(FileNotFoundError):
            capture(self.source, self.output, "build")
        self.assertFalse(self.output.exists())
