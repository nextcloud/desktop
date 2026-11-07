# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Capture the separately downloaded Sparkle archive and its CycloneDX identity."""

import argparse
from datetime import datetime, timezone
from pathlib import Path
import shutil
from urllib.parse import quote
import uuid

from common.sbom_utils import digest, properties, write_json


def capture(output_dir, build_id, archive=None, version=None, download_url=None):
    enabled = archive is not None
    if enabled and (not version or not download_url):
        raise ValueError("An archive requires a version and download URL")
    if not enabled and (version or download_url):
        raise ValueError("Disabled capture cannot declare a release")
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=False)
    try:
        metadata = {"build_id": build_id, "captured_at": datetime.now(timezone.utc).isoformat(),
                    "enabled": enabled, "version": version, "download_url": download_url}
        components = []
        if enabled:
            target = output_dir / "Sparkle.tar.xz"
            shutil.copyfile(archive, target)
            metadata["archive_sha256"] = digest(target)
            components.append({
                "type": "library", "bom-ref": "sparkle", "name": "Sparkle", "version": version,
                "purl": "pkg:github/sparkle-project/sparkle@" + quote(version, safe=""),
                "externalReferences": [{"type": "distribution", "url": download_url,
                    "hashes": [{"alg": "SHA-256", "content": metadata["archive_sha256"]}]}],
            })
        bom = {
            "$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json",
            "bomFormat": "CycloneDX", "specVersion": "1.6", "version": 1,
            "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
            "metadata": {"timestamp": metadata["captured_at"], "component": {
                "type": "application", "bom-ref": "downloads", "name": "Separate downloads only",
                "version": build_id,
                "description": "Sparkle download inventory; not a complete client SBOM or proof of shipped code. Dependencies inside Sparkle are not enumerated."},
                "properties": properties({"sparkle-enabled": str(enabled).lower()})},
            "components": components,
            "compositions": [{"aggregate": "incomplete", "assemblies": ["downloads"]}],
        }
        write_json(output_dir / "capture.json", metadata)
        write_json(output_dir / "downloads.cdx.json", bom)
    except Exception:
        shutil.rmtree(output_dir)
        raise
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--download-url")
    args = parser.parse_args()
    capture(args.output_dir, args.build_id, args.archive, args.version, args.download_url)
    print(f"Separate download inventory saved to {args.output_dir}")
