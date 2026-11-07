# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Convert a captured Swift lockfile to a partial CycloneDX inventory."""

import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit
import uuid

from common.sbom_utils import properties, reference


def repository_identity(location):
    if location.startswith("git@") and ":" in location:
        host, path = location[4:].split(":", 1)
        location = "ssh://" + host + "/" + path
    url = urlsplit(location)
    if url.scheme not in ("https", "http", "ssh", "git+ssh") or not url.hostname or url.query or url.fragment:
        return None
    parts = [unquote(part) for part in url.path.strip("/").removesuffix(".git").split("/")]
    if not all(part and part not in (".", "..") and "/" not in part for part in parts):
        return None
    if url.hostname == "github.com":
        parts = [part.lower() for part in parts]
    host = url.hostname + (":" + str(url.port) if url.port else "")
    return [host, *parts]


def make_bom(data, metadata):
    if hashlib.sha256(data).hexdigest() != metadata["sha256"]:
        raise ValueError("Captured lockfile checksum does not match capture.json")
    lock = json.loads(data)
    if lock.get("version") not in (2, 3) or not isinstance(lock.get("pins"), list):
        raise ValueError("Expected a version 2 or 3 Package.resolved with pins")
    components = []
    seen = set()
    for pin in lock["pins"]:
        identity, location, state = pin.get("identity"), pin.get("location"), pin.get("state", {})
        revision = state.get("revision")
        if not all(isinstance(value, str) and value for value in (identity, location, revision)):
            raise ValueError("Each package needs an identity, repository and revision")
        version = state.get("version") or revision
        if not isinstance(version, str):
            raise ValueError("Package version must be a string")
        ref = reference("swift", json.dumps([location, revision]))
        if ref in seen:
            raise ValueError("Duplicate resolved package")
        seen.add(ref)
        component = {
            "type": "library", "bom-ref": ref, "name": identity, "version": version,
            "externalReferences": [{"type": "vcs", "url": location}],
            "properties": properties({"revision": revision, "branch": state.get("branch")}),
        }
        parts = repository_identity(location)
        if parts:
            component["group"] = "/".join(parts[:-1])
            component["name"] = parts[-1]
            component["purl"] = "pkg:swift/" + "/".join(quote(part, safe="") for part in parts) + "@" + quote(version, safe="")
        component["properties"].extend(properties({"swift-identity": identity,
                                                   "identity-status": "swift" if parts else "unresolved-repository"}))
        components.append(component)
    return {
        "$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json",
        "bomFormat": "CycloneDX", "specVersion": "1.6", "version": 1,
        "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
        "metadata": {
            "timestamp": metadata["captured_at"],
            "component": {
                "type": "application", "bom-ref": "swift-inventory",
                "name": "Swift dependencies only", "version": metadata["build_id"],
                "description": "Resolved Swift inventory; may include build-only packages. Not a complete client SBOM or proof of shipped code.",
            },
            "properties": properties({"source-commit": metadata["source_commit"], "lockfile-sha256": metadata["sha256"], "dependency-graph": "not captured"}),
        },
        "components": components,
        "compositions": [{"aggregate": "incomplete", "assemblies": ["swift-inventory"]}],
    }


def convert(capture_dir):
    capture_dir = Path(capture_dir)
    bom = make_bom((capture_dir / "Package.resolved").read_bytes(),
                   json.loads((capture_dir / "capture.json").read_text()))
    output = capture_dir / "swift.cdx.json"
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(bom, indent=2) + "\n")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture-dir", required=True, type=Path)
    print(convert(parser.parse_args().capture_dir))
