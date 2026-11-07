# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Shared serialization and identity helpers for SBOM evidence."""

import hashlib
import json


def digest(path, algorithm="sha256"):
    checksum = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def reference(kind, value):
    return kind + ":" + hashlib.sha256(value.encode()).hexdigest()


def properties(values):
    return [{"name": "nextcloud:sbom:" + k, "value": str(v)}
            for k, v in sorted(values.items()) if v is not None]


def add_cpes(component, cpes):
    """Keep the first CPE in CycloneDX's field and additional CPEs readable by Syft/Grype."""
    cpes = list(dict.fromkeys(cpes))
    if cpes:
        component["cpe"] = cpes[0]
        component.setdefault("properties", []).extend(
            {"name": "syft:cpe23", "value": cpe} for cpe in cpes[1:])
