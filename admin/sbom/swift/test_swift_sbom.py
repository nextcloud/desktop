# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from swift.swift_sbom import convert, make_bom


class SwiftSbomTests(unittest.TestCase):
    def inputs(self, pins, version=3):
        data = json.dumps({"version": version, "pins": pins}).encode()
        return data, {"sha256": hashlib.sha256(data).hexdigest(),
                      "captured_at": "2026-11-03T12:00:00+00:00",
                      "build_id": "build-1", "source_commit": "abc"}

    def pin(self, location="https://github.com/Owner/Library.git", version="1.2.3"):
        state = {"revision": "abcdef"}
        if version:
            state["version"] = version
        return {"identity": "library", "location": location, "state": state}

    def test_versions_identity_and_scope(self):
        for version in (2, 3):
            bom = make_bom(*self.inputs([self.pin()], version))
            self.assertEqual(bom["metadata"]["component"]["name"], "Swift dependencies only")
            self.assertEqual(bom["components"][0]["purl"], "pkg:swift/github.com/owner/library@1.2.3")
            self.assertEqual(bom["components"][0]["group"], "github.com/owner")
            self.assertEqual(bom["components"][0]["version"], "1.2.3")
            self.assertEqual(bom["compositions"][0]["aggregate"], "incomplete")
            self.assertNotIn("dependencies", bom)
            self.assertNotIn("licenses", bom["components"][0])

    def test_revision_only_and_non_github(self):
        bom = make_bom(*self.inputs([self.pin("https://example.org/library.git", None)]))
        component = bom["components"][0]
        self.assertEqual(component["version"], "abcdef")
        self.assertEqual(component["purl"], "pkg:swift/example.org/library@abcdef")
        self.assertEqual(component["externalReferences"][0]["url"], "https://example.org/library.git")
        self.assertEqual(make_bom(*self.inputs([]))["components"], [])

    def test_repository_namespace_and_encoding(self):
        for location, expected in (
            ("https://github.com/Owner/Library.git/", "github.com/owner/library"),
            ("ssh://git@github.com/Owner/Library.git", "github.com/owner/library"),
            ("git@github.com:Owner/Library.git", "github.com/owner/library"),
            ("https://git.example.org/group/subgroup/Library.git", "git.example.org/group/subgroup/Library"),
            ("https://example.org/My%20Org/Library.git", "example.org/My%20Org/Library"),
        ):
            with self.subTest(location=location):
                component = make_bom(*self.inputs([self.pin(location)]))["components"][0]
                self.assertEqual(component["purl"], "pkg:swift/" + expected + "@1.2.3")
                self.assertEqual(component["externalReferences"][0]["url"], location)
                self.assertIn({"name": "nextcloud:sbom:swift-identity", "value": "library"}, component["properties"])

    def test_unresolved_repository_is_not_assigned_a_purl(self):
        for location in ("../Library", "file:///Library", "https://example.org/", "https://example.org/a/%2Fbad"):
            with self.subTest(location=location):
                component = make_bom(*self.inputs([self.pin(location)]))["components"][0]
                self.assertNotIn("purl", component)
                self.assertEqual(component["name"], "library")
                self.assertIn({"name": "nextcloud:sbom:identity-status", "value": "unresolved-repository"}, component["properties"])

    def test_invalid_inputs(self):
        for pins, version in (([{}], 3), ([self.pin(), self.pin()], 3), ([], 1)):
            with self.assertRaises(ValueError):
                make_bom(*self.inputs(pins, version))
        data, metadata = self.inputs([])
        with self.assertRaises(ValueError):
            make_bom(data + b" ", metadata)

    def test_conversion_and_no_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data, metadata = self.inputs([self.pin()])
            (root / "Package.resolved").write_bytes(data)
            (root / "capture.json").write_text(json.dumps(metadata))
            output = convert(root)
            original = output.read_bytes()
            self.assertEqual(json.loads(original)["components"][0]["name"], "library")
            with self.assertRaises(FileExistsError):
                convert(root)
            self.assertEqual(output.read_bytes(), original)
