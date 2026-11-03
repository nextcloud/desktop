# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import tempfile
import unittest

from common.sbom_utils import digest
from craft.craft_sbom import make_bom
from craft.dependency_scope import load_scopes, qt_build_dependencies


class DependencyScopeTests(unittest.TestCase):
    def test_captured_roles_and_unknown_packages(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "craft-dependencies.json"
            path.write_text(json.dumps({"version": 1, "target_package": "client",
                                       "runtime": ["client", "libs/shared"], "build_only": ["libs/tool-library"]}))
            metadata = {"build_id": "build", "captured_at": "2026-11-03T12:00:00Z", "sha256": "a" * 64,
                        "dependencies_sha256": digest(path)}
            scopes = load_scopes(root, metadata)
            packages = [{"packageId": i, "packagePath": name, "version": "1.0"}
                        for i, name in enumerate(("libs/shared", "libs/tool-library", "libs/unknown"))]
            bom = make_bom(packages, metadata, scopes)
            roles = [next(p["value"] for p in c["properties"] if p["name"].endswith(":dependency-scope"))
                     for c in bom["components"]]
            self.assertEqual(roles, ["runtime", "build", "unknown"])
            self.assertEqual(load_scopes(root, {}), {})
            path.write_text(path.read_text() + "\n")
            with self.assertRaisesRegex(ValueError, "checksum"):
                load_scopes(root, metadata)

    def test_invalid_captured_roles_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "craft-dependencies.json"
            for data in (
                {"version": 2},
                {"version": 1, "runtime": "client", "build_only": []},
                {"version": 1, "target_package": "client", "runtime": [], "build_only": []},
                {"version": 1, "target_package": "client", "runtime": ["client"], "build_only": ["client"]},
            ):
                path.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    load_scopes(root, {"dependencies_sha256": digest(path)})

    def test_qt_build_edges_keep_shared_runtime_dependencies(self):
        data = {"packages": dict.fromkeys(("root", "compiler", "tool-library", "shared", "runtime")),
                "relationships": [("compiler", "BUILD_DEPENDENCY_OF", "root"),
                                  ("tool-library", "BUILD_DEPENDENCY_OF", "root"),
                                  ("shared", "BUILD_DEPENDENCY_OF", "root"),
                                  ("runtime", "DEPENDS_ON", "shared"),
                                  ("shared", "DEPENDS_ON", "tool-library"),
                                  ("tool-library", "DEPENDS_ON", "shared")]}
        self.assertEqual(qt_build_dependencies(data), {"compiler"})
        self.assertEqual(qt_build_dependencies({"packages": {}, "relationships": []}), set())
