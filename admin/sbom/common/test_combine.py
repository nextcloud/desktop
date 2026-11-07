# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import json
from pathlib import Path
import tempfile
import unittest

from common.combine import combine


class CombineTests(unittest.TestCase):
    def document(self, name):
        return {"bomFormat": "CycloneDX", "specVersion": "1.6", "metadata": {
            "component": {"type": "application", "bom-ref": "root", "name": name}},
            "components": [{"type": "library", "bom-ref": "lib", "name": "shared-name", "version": "1"}],
            "dependencies": [{"ref": "root", "dependsOn": ["lib"]}],
            "compositions": [{"aggregate": "incomplete", "assemblies": ["root"]}]}

    def test_groups_and_remaps_references_without_modifying_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            a, b = root / "a.json", root / "b.json"
            a.write_text(json.dumps(self.document("Craft")))
            b.write_text(json.dumps(self.document("Swift")))
            original = a.read_bytes()
            result = combine([a, b, a], root / "bom.json", "build")
            self.assertEqual(len(result["components"]), 2)
            refs = {g["bom-ref"] for g in result["components"]}
            self.assertEqual(len(refs), 2)
            self.assertEqual(result["metadata"]["supplier"]["name"], "Nextcloud GmbH")
            self.assertEqual(result["metadata"]["component"]["name"], "Nextcloud Desktop")
            self.assertTrue(all("components" not in c for c in result["components"]))
            self.assertEqual(result["dependencies"][0]["ref"], "build-inventory")
            self.assertEqual(set(result["dependencies"][0]["dependsOn"]), refs)
            labels = {p["value"] for c in result["components"] for p in c["properties"]
                      if p["name"] == "nextcloud:sbom:source-module"}
            self.assertEqual(labels, {"Craft", "Swift"})
            self.assertEqual(a.read_bytes(), original)
            with self.assertRaises(FileExistsError):
                combine([a], root / "bom.json", "build")

    def test_invalid_inputs_do_not_create_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "input.json"
            invalid = self.document("bad")
            invalid["dependencies"][0]["dependsOn"] = ["missing"]
            unsupported = self.document("bad")
            unsupported["services"] = []
            for doc in (invalid, unsupported, {"bomFormat": "SPDX"}):
                source.write_text(json.dumps(doc))
                with self.assertRaises(ValueError):
                    combine([source], root / "bom.json", "build")
                self.assertFalse((root / "bom.json").exists())
            with self.assertRaises(ValueError):
                combine([], root / "bom.json", "build")

    def test_scanner_identities_survive_combination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "input.json"
            document = self.document("Swift")
            component = document["components"][0]
            component.update(name="library", group="github.com/owner", version="1.2.3",
                             purl="pkg:swift/github.com/owner/library@1.2.3",
                             cpe="cpe:2.3:a:vendor:library:1.2.3:*:*:*:*:*:*:*")
            extra_cpe = {"name": "syft:cpe23", "value": "cpe:2.3:a:old_vendor:library:1.2.3:*:*:*:*:*:*:*"}
            component["properties"] = [extra_cpe]
            source.write_text(json.dumps(document))
            result = combine([source], root / "bom.json", "build")["components"][0]
            for field in ("name", "group", "version", "purl", "cpe"):
                self.assertEqual(result[field], component[field])
            self.assertIn(extra_cpe, result["properties"])

    def test_build_dependencies_are_excluded_with_audit_and_valid_references(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "input.json"
            document = self.document("Craft")
            document["components"].append({"type": "library", "bom-ref": "tool", "name": "compiler",
                                           "properties": [{"name": "nextcloud:sbom:dependency-scope", "value": "build"}]})
            document["dependencies"][0]["dependsOn"].append("tool")
            document["dependencies"].append({"ref": "tool", "dependsOn": ["lib"]})
            source.write_text(json.dumps(document))
            original = source.read_bytes()
            result = combine([source], root / "bom.json", "build")
            self.assertEqual([c["name"] for c in result["components"]], ["shared-name"])
            self.assertEqual(result["dependencies"], [{"ref": "build-inventory", "dependsOn": [result["components"][0]["bom-ref"]]}])
            audit = json.loads((root / "bom.excluded.json").read_text())
            self.assertEqual([c["name"] for c in audit["components"]], ["compiler"])
            self.assertEqual(source.read_bytes(), original)
            complete = combine([source], root / "complete.json", "build", include_build_dependencies=True)
            self.assertEqual(len(complete["components"]), 2)

    def test_conflicting_role_is_kept_and_existing_audit_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "input.json"
            document = self.document("Craft")
            document["components"][0]["properties"] = [
                {"name": "nextcloud:sbom:dependency-scope", "value": "build"},
                {"name": "nextcloud:sbom:dependency-scope", "value": "runtime"}]
            source.write_text(json.dumps(document))
            self.assertEqual(len(combine([source], root / "shared.json", "build")["components"]), 1)
            document["components"][0]["properties"].pop()
            source.write_text(json.dumps(document))
            audit = root / "bom.excluded.json"
            audit.write_text("original")
            with self.assertRaises(FileExistsError):
                combine([source], root / "bom.json", "build")
            self.assertEqual(audit.read_text(), "original")
            self.assertFalse((root / "bom.json").exists())
