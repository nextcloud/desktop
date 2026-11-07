# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from desktop.capture_desktop import capture


class CaptureDesktopTests(unittest.TestCase):
    @patch("desktop.capture_desktop.subprocess.check_output")
    def test_bundled_identity_and_dirty_state(self, git):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            file = root / "src/3rdparty/example/code.h"
            file.parent.mkdir(parents=True)
            file.write_text("// SPDX-License-Identifier: MIT\n")
            output = root / "capture"
            responses = [b"commit\n", b" M source.cpp\n", b"v1-dirty\n", b"src/3rdparty/example/code.h\0"]
            git.side_effect = responses
            metadata = capture(root, output, "build")
            self.assertTrue(metadata["dirty"])
            self.assertEqual(metadata["files"][0]["license_expressions"], ["MIT"])
            bom = json.loads((output / "desktop.cdx.json").read_text())
            self.assertEqual(bom["components"][0]["name"], "example")
            self.assertNotIn("version", bom["components"][0])
            git.side_effect = responses
            with self.assertRaises(FileExistsError):
                capture(root, output, "other")

    @patch("desktop.capture_desktop.subprocess.check_output")
    def test_clean_empty_inventory_and_git_failure(self, git):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            git.side_effect = [b"commit\n", b"", b"v1\n", b""]
            self.assertFalse(capture(root, root / "capture", "build")["dirty"])
            git.side_effect = OSError("git unavailable")
            with self.assertRaises(OSError):
                capture(root, root / "failed", "build")
            self.assertFalse((root / "failed").exists())
