# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

from setup_xcode import scheme_contents


class SchemeTests(unittest.TestCase):
    def test_absolute_executable_and_only_capture_target(self):
        template = Path(__file__).with_name("capture.xcscheme.in").read_text()
        build_directory = Path('/tmp/build with spaces & "quotes"')
        scheme = ET.fromstring(scheme_contents(template, build_directory))
        runnable = scheme.find("LaunchAction/PathRunnable")
        self.assertEqual(runnable.attrib["FilePath"], str(build_directory.resolve() / "bin/DocAssetsCapture"))
        self.assertNotIn("$(", runnable.attrib["FilePath"])
        entries = scheme.findall("BuildAction/BuildActionEntries/BuildActionEntry/BuildableReference")
        self.assertEqual([entry.attrib["BlueprintName"] for entry in entries], ["DocAssetsCapture"])
        self.assertEqual(entries[0].attrib["BlueprintIdentifier"], "D0CA55012FBE000100000001")


if __name__ == "__main__":
    unittest.main()
