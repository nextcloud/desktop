# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Combine modular CycloneDX inventories without guessing package equivalence."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import uuid

from common.sbom_utils import properties
from common.dependency_scope import partition_components, prune_references


def combine(inputs, output, build_id, name="Nextcloud Desktop", supplier="Nextcloud GmbH", include_build_dependencies=False):
    components, compositions, dependencies, seen = [], [], [], set()
    source_properties = []
    for source in inputs:
        source = Path(source)
        data = source.read_bytes()
        checksum = hashlib.sha256(data).hexdigest()
        if checksum in seen:
            continue
        seen.add(checksum)
        bom = json.loads(data)
        if bom.get("bomFormat") != "CycloneDX" or bom.get("specVersion") != "1.6":
            raise ValueError(f"Expected CycloneDX 1.6: {source}")
        allowed = {"$schema", "bomFormat", "specVersion", "serialNumber", "version", "metadata", "components", "compositions", "dependencies"}
        if set(bom) - allowed:
            raise ValueError(f"Unsupported SBOM sections in {source}; refusing to discard them")
        metadata = bom.get("metadata", {})
        if set(metadata) - {"timestamp", "component", "properties"}:
            raise ValueError(f"Unsupported metadata in {source}")
        if "component" not in metadata or not isinstance(bom.get("components"), list):
            raise ValueError(f"Missing module identity or components: {source}")
        references = {}
        def collect(value):
            if isinstance(value, dict):
                if "bom-ref" in value:
                    ref = value["bom-ref"]
                    if ref in references:
                        raise ValueError(f"Duplicate component reference in {source}: {ref}")
                    references[ref] = "module-" + checksum + ":" + ref
                for child in value.values():
                    collect(child)
            elif isinstance(value, list):
                for child in value:
                    collect(child)
        collect(metadata["component"])
        collect(bom["components"])
        module_root = metadata["component"].get("bom-ref")
        if module_root:
            references[module_root] = "build-inventory"
        def mapped(ref):
            if ref not in references:
                raise ValueError(f"Unresolved reference in {source}: {ref}")
            return references[ref]
        def rewrite(value):
            if isinstance(value, list):
                return [rewrite(child) for child in value]
            if not isinstance(value, dict):
                return value
            result = {}
            for key, child in value.items():
                if key in ("bom-ref", "ref"):
                    result[key] = mapped(child)
                elif key in ("dependsOn", "provides", "assemblies", "dependencies") and isinstance(child, list) and all(isinstance(x, str) for x in child):
                    result[key] = [mapped(x) for x in child]
                else:
                    result[key] = rewrite(child)
            return result
        label = metadata["component"]["name"]
        provenance = properties({"source-module": label, "input-sha256": checksum,
                                 "input-file": source.name, "input-timestamp": metadata.get("timestamp")})
        def flatten(items):
            for item in items:
                children = item.pop("components", [])
                item.setdefault("properties", []).extend(provenance)
                components.append(item)
                flatten(children)
        flatten(rewrite(bom["components"]))
        flatten(rewrite(metadata["component"].get("components", [])))
        source_properties.extend(properties({"input-metadata": json.dumps(metadata, sort_keys=True),
                                             "input-sha256": checksum}))
        compositions.extend(rewrite(bom.get("compositions", [])))
        dependencies.extend(rewrite(bom.get("dependencies", [])))
    if not seen:
        raise ValueError("At least one input SBOM is required")
    excluded = []
    if not include_build_dependencies:
        components, excluded = partition_components(components)
        excluded_refs = {c["bom-ref"] for c in excluded}
        dependencies = prune_references(dependencies, excluded_refs, ("dependsOn", "provides"))
        compositions = prune_references(compositions, excluded_refs, ("assemblies", "dependencies"))
    source_properties.extend(properties({
        "build-dependencies-included": str(include_build_dependencies).lower(),
        "excluded-build-components": len(excluded),
    }))
    result = {
        "$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json",
        "bomFormat": "CycloneDX", "specVersion": "1.6", "version": 1,
        "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
        "metadata": {"timestamp": datetime.now(timezone.utc).isoformat(),
                     "supplier": {"name": supplier}, "properties": source_properties, "component": {
            "type": "application", "bom-ref": "build-inventory", "name": name, "version": build_id,
            "supplier": {"name": supplier},
            "description": ("Combined build dependency inventory; includes build dependencies. Coverage remains partial."
                            if include_build_dependencies else
                            "Combined dependency inventory excluding confirmed build-only components; unknown roles remain included. Coverage remains partial.")}},
        "components": components,
        "compositions": [{"aggregate": "incomplete", "assemblies": ["build-inventory"]}, *compositions],
    }
    result["compositions"] = list({json.dumps(c, sort_keys=True): c for c in result["compositions"]}.values())
    if dependencies:
        merged = {}
        for edge in dependencies:
            target = merged.setdefault(edge["ref"], {"ref": edge["ref"]})
            for field in ("dependsOn", "provides"):
                if field in edge:
                    target[field] = sorted(set(target.get(field, [])) | set(edge[field]))
        result["dependencies"] = list(merged.values())
    output = Path(output)
    exclusion_path = output.with_name(output.stem + ".excluded.json")
    if excluded and exclusion_path.exists():
        raise FileExistsError(exclusion_path)
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
    if excluded:
        try:
            with exclusion_path.open("x", encoding="utf-8") as stream:
                stream.write(json.dumps({"build_id": build_id, "reason": "Confirmed build-only dependencies",
                                         "components": excluded}, indent=2) + "\n")
        except Exception:
            output.unlink()
            raise
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--name", default="Nextcloud Desktop")
    parser.add_argument("--supplier", default="Nextcloud GmbH")
    parser.add_argument("--include-build-dependencies", action="store_true")
    args = parser.parse_args()
    combine(args.input, args.output, args.build_id, args.name, args.supplier, args.include_build_dependencies)
    print(f"Combined SBOM saved to {args.output}")
