# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Coverage for the Qt SPDX parser retained by the modular capture."""

import unittest

from craft.qt_sbom import parse_spdx, supplier_component


class QtSbomTests(unittest.TestCase):
    def test_multiline_text_and_license_definitions(self):
        document = '''SPDXVersion: SPDX-2.3
SPDXID: SPDXRef-DOCUMENT
DocumentNamespace: https://example.org/qt
PackageName: QtCore
SPDXID: SPDXRef-Core
PackageComment: <text>First line
PackageName: still comment text
</text>
LicenseID: LicenseRef-Custom
ExtractedText: <text>Custom license text</text>
'''
        parsed = parse_spdx(document)
        self.assertIn("PackageName: still comment text", parsed["packages"]["SPDXRef-Core"]["PackageComment"][0])
        self.assertEqual(parsed["licenses"]["LicenseRef-Custom"]["ExtractedText"], ["Custom license text"])
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            parse_spdx(document + "PackageName: Duplicate\nSPDXID: SPDXRef-Core\n")
        with self.assertRaisesRegex(ValueError, "Unterminated"):
            parse_spdx(document + "PackageComment: <text>unfinished")

    def test_invalid_document(self):
        for document in ("", "invalid", "SPDXVersion: SPDX-2.2\nDocumentNamespace: https://example.org/qt"):
            with self.assertRaises(ValueError):
                parse_spdx(document)

    def test_cpe_without_purl_and_unknown_version(self):
        record = {"PackageName": ["QtCore"], "PackageVersion": ["NOASSERTION"],
                  "ExternalRef": ["SECURITY cpe23Type cpe:2.3:a:qt:qt:*:*:*:*:*:*:*:*"]}
        component = supplier_component("https://example.org/qt", "SPDXRef-Core", record, "qt.spdx")
        self.assertEqual(component["cpe"], "cpe:2.3:a:qt:qt:*:*:*:*:*:*:*:*")
        self.assertNotIn("purl", component)
        self.assertNotIn("version", component)
