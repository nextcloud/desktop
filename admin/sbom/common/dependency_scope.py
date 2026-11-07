# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Exclude proven build-only components while preserving unknown usage."""


def partition_components(components):
    included, excluded = [], []
    for component in components:
        roles = {p["value"] for p in component.get("properties", [])
                 if p["name"] == "nextcloud:sbom:dependency-scope"}
        (excluded if roles == {"build"} else included).append(component)
    return included, excluded


def prune_references(records, excluded, reference_lists):
    result = []
    for record in records:
        if record.get("ref") in excluded:
            continue
        updated = dict(record)
        for field in reference_lists:
            if field in updated:
                updated[field] = [ref for ref in updated[field] if ref not in excluded]
        result.append(updated)
    return result
