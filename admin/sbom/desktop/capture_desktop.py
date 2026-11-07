# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Capture Desktop source identity and explicitly bundled third-party directories."""

import argparse
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import uuid

from common.sbom_utils import properties, reference, write_json


def capture(source_dir, output_dir, build_id):
    source = Path(source_dir).resolve()
    def git(*args):
        return subprocess.check_output(["git", "-C", str(source), *args])
    commit = git("rev-parse", "HEAD").decode().strip()
    status = git("status", "--porcelain", "--untracked-files=normal").decode()
    version = git("describe", "--tags", "--always", "--dirty").decode().strip()
    paths = git("ls-files", "--cached", "--others", "--exclude-standard", "-z").decode().split("\0")
    groups = {}
    files = []
    for name in sorted(set(paths) - {""}):
        parts = Path(name).parts
        markers = [i for i, part in enumerate(parts) if part in ("3rdparty", "third_party", "vendor")]
        if not markers:
            continue
        index = markers[0]
        group = "/".join(parts[:min(index + 2, len(parts))])
        path = source / name
        if path.is_symlink() or not path.is_file():
            continue
        data = path.read_bytes()
        licenses = sorted(set(re.findall(r"SPDX-License-Identifier:\s*([^\r\n]+)", data.decode("utf-8", errors="replace"))))
        record = {"path": name, "sha256": hashlib.sha256(data).hexdigest(), "license_expressions": licenses}
        files.append(record)
        groups.setdefault(group, []).append(record)
    components = []
    for group, records in sorted(groups.items()):
        components.append({"type": "library", "bom-ref": reference("bundled", group),
                           "name": Path(group).name,
                           "description": "Bundled source copy; upstream version is not established.",
                           "properties": properties({"source-path": group, "desktop-commit": commit,
                               "file-license-expressions": "\n".join(sorted({lic for r in records for lic in r["license_expressions"]}))}),
                           "evidence": {"occurrences": [{"location": r["path"]} for r in records]}})
    metadata = {"build_id": build_id, "captured_at": datetime.now(timezone.utc).isoformat(),
                "source_commit": commit, "source_version": version, "dirty": bool(status),
                "git_status": status, "files": files,
                "scope": "Source checkout observation, including platform-specific and build-only bundled code; directory-based discovery is incomplete."}
    bom = {"$schema": "http://cyclonedx.org/schema/bom-1.6.schema.json", "bomFormat": "CycloneDX",
           "specVersion": "1.6", "version": 1, "serialNumber": "urn:uuid:" + str(uuid.uuid4()),
           "metadata": {"timestamp": metadata["captured_at"], "component": {
               "type": "application", "bom-ref": "desktop-source", "name": "Desktop source only",
               "version": version, "description": metadata["scope"],
               "properties": properties({"source-commit": commit, "dirty": str(bool(status)).lower(), "build-id": build_id})}},
           "components": components, "compositions": [{"aggregate": "incomplete", "assemblies": ["desktop-source"]}]}
    output = Path(output_dir)
    output.mkdir(parents=True, exist_ok=False)
    try:
        write_json(output / "capture.json", metadata)
        write_json(output / "desktop.cdx.json", bom)
    except Exception:
        shutil.rmtree(output)
        raise
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()
    capture(args.source_dir, args.output_dir, args.build_id)
    print(f"Desktop source inventory saved to {args.output_dir}")
