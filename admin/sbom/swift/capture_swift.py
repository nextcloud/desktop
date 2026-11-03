# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Preserve the client project's resolved Swift inventory without resolving packages."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

from swift.swift_sbom import convert

LOCKFILE = Path("shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved")


def capture(source_dir, output_dir, build_id):
    source_dir = Path(source_dir).resolve()
    data = (source_dir / LOCKFILE).read_bytes()
    lock = json.loads(data)
    if lock.get("version") not in (2, 3) or not isinstance(lock.get("pins"), list):
        raise ValueError("Expected a version 2 or 3 Package.resolved with pins")
    for pin in lock["pins"]:
        if not pin.get("identity") or not pin.get("location") or not pin.get("state", {}).get("revision"):
            raise ValueError("Each resolved package must have an identity, location and revision")
    commit = subprocess.check_output(
        ["git", "-C", str(source_dir), "rev-parse", "HEAD"], text=True
    ).strip()
    metadata = {
        "build_id": build_id,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "source_commit": commit,
        "source_lockfile": str(LOCKFILE),
        "sha256": hashlib.sha256(data).hexdigest(),
        "package_count": len(lock["pins"]),
        "scope": "resolved dependencies; may include packages not shipped",
    }
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=False)
    (output_dir / "Package.resolved").write_bytes(data)
    (output_dir / "capture.json").write_text(json.dumps(metadata, indent=2) + "\n")
    convert(output_dir)
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()
    capture(args.source_dir, args.output_dir, args.build_id)
    print(f"Swift dependency inventory saved to {args.output_dir}")


if __name__ == "__main__":
    main()
