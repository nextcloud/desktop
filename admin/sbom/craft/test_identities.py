# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import unittest

from craft.craft_sbom import make_bom


class CraftIdentityTests(unittest.TestCase):
    def component(self, path, version):
        return make_bom([{"packageId": 1, "packagePath": path, "version": version}],
                        {"build_id": "build", "captured_at": "2026-11-03T12:00:00Z",
                         "sha256": "a" * 64})["components"][0]

    def test_upstream_names_versions_and_cpes(self):
        for path, version, name, upstream, pair in (
            ("libs/openssl", "3.6.4", "openssl", "3.6.4", "openssl:openssl"),
            ("libs/liblzma", "5.2.3-1", "xz", "5.2.3", "tukaani:xz"),
            ("libs/expat", "2.7.3", "expat", "2.7.3", "libexpat_project:libexpat"),
            ("libs/libxml2", "2.14.5", "libxml2", "2.14.5", "xmlsoft:libxml2"),
            ("libs/glib", "2.86.0-1", "glib", "2.86.0", "gnome:glib"),
            ("libs/openssl", "1.1.1w-2", "openssl", "1.1.1w", "openssl:openssl"),
        ):
            with self.subTest(path=path, version=version):
                component = self.component(path, version)
                self.assertEqual(component["name"], name)
                self.assertEqual(component["version"], upstream)
                self.assertEqual(component["purl"], f"pkg:generic/{name}@{upstream}")
                self.assertEqual(component["cpe"], f"cpe:2.3:a:{pair}:{upstream}:*:*:*:*:*:*:*")
                props = {p["name"]: p["value"] for p in component["properties"]}
                self.assertEqual(props["nextcloud:sbom:craft-package-path"], path)
                self.assertEqual(props["nextcloud:sbom:craft-installed-version"], version)
                self.assertEqual(props["nextcloud:sbom:version-kind"], "upstream-release")

    def test_alias_cpes_and_generic_identity_without_invented_cpe(self):
        component = self.component("libs/libxml2", "2.14.5")
        self.assertIn({"name": "syft:cpe23", "value": "cpe:2.3:a:libxml2:libxml2:2.14.5:*:*:*:*:*:*:*"}, component["properties"])
        component = self.component("libs/libp11", "libp11-0.4.21")
        self.assertEqual(component["name"], "libp11")
        self.assertEqual(component["version"], "0.4.21")
        self.assertEqual(component["purl"], "pkg:generic/libp11@0.4.21")
        self.assertNotIn("cpe", component)

    def test_python_ecosystem(self):
        component = self.component("python-modules/hatchling", "1.27.0-2")
        self.assertEqual(component["purl"], "pkg:pypi/hatchling@1.27.0")
        self.assertEqual(component["name"], "hatchling")
        self.assertEqual(component["version"], "1.27.0")
        self.assertNotIn("cpe", component)

    def test_unresolved_versions_and_unmapped_packages_remain_evidence(self):
        for path, version, status in (
            ("python-modules/setuptools", "master-2", "unresolved-version"),
            ("python-modules/flit-core", "latest-2", "unresolved-version"),
            ("libs/openssl", "3.0.0-beta1", "unresolved-version"),
            ("libs/unknown", "1.2.3-4", "unmapped-package"),
            ("virtual/base", "0.2", "unmapped-package"),
        ):
            with self.subTest(path=path, version=version):
                component = self.component(path, version)
                self.assertEqual(component["name"], path)
                self.assertEqual(component["version"], version)
                self.assertNotIn("purl", component)
                self.assertNotIn("cpe", component)
                self.assertIn({"name": "nextcloud:sbom:identity-status", "value": status}, component["properties"])
