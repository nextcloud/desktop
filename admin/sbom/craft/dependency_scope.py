# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Classify only roles established by captured dependency metadata."""

import json

from common.sbom_utils import digest, properties


def load_scopes(capture_dir, metadata):
    checksum = metadata.get("dependencies_sha256")
    if not checksum:
        return {}
    path = capture_dir / "craft-dependencies.json"
    if digest(path) != checksum:
        raise ValueError("Captured dependency metadata checksum does not match capture.json")
    data = json.loads(path.read_text())
    if data.get("version") != 1:
        raise ValueError("Unsupported Craft dependency metadata version")
    for key in ("runtime", "build_only"):
        if not isinstance(data.get(key), list) or not all(isinstance(p, str) and p for p in data[key]):
            raise ValueError("Invalid Craft dependency package list")
    runtime, build = set(data["runtime"]), set(data["build_only"])
    if runtime & build or data.get("target_package") not in runtime:
        raise ValueError("Inconsistent Craft dependency roles")
    return {**dict.fromkeys(runtime, "runtime"), **dict.fromkeys(build, "build")}


def set_scope(component, scope, evidence):
    component.setdefault("properties", []).extend(properties({
        "dependency-scope": scope, "dependency-scope-evidence": evidence}))


def qt_build_dependencies(data):
    candidates = {a for a, relation, b in data["relationships"]
                  if relation == "BUILD_DEPENDENCY_OF" and a in data["packages"] and b in data["packages"]}
    # A package also reached by a non-build package must remain included.
    runtime = set(data["packages"]) - candidates
    while True:
        reached = {b for a, relation, b in data["relationships"] if a in runtime and relation == "DEPENDS_ON"}
        if reached <= runtime:
            break
        runtime |= reached
    return candidates - runtime
