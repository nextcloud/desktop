# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Retain Qt supplier documents without selecting shipped components."""

from pathlib import Path
import shutil

from common.sbom_utils import digest, write_json


def capture_qt(root, output, packages, installed_files):
    root, output = Path(root), Path(output)
    package_names = {p["packageId"]: p["packagePath"] for p in packages}
    rows = {}
    for row in installed_files:
        rows.setdefault(row["filename"], []).append(row)
    documents = []
    directory = root / "sbom"
    if directory.is_symlink():
        raise ValueError("Qt supplier SBOM directory must not be a symlink")
    for source in sorted(directory.glob("qt*.spdx")):
        if source.is_symlink() or not source.is_file():
            raise ValueError(f"Qt supplier document must be a regular file: {source}")
        relative = source.relative_to(root).as_posix()
        target = output / "qt" / source.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        checksum = digest(target)
        owners = sorted({row["packageId"] for row in rows.get(relative, [])
                         if row.get("fileHash") == "[SHA256]" + checksum
                         and row["packageId"] in package_names})
        documents.append({
            "source": relative, "path": "qt/" + source.name, "sha256": checksum,
            "ownership_verified": len(owners) == 1,
            "matching_packages": [package_names[owner] for owner in owners],
        })
    qt_packages = sorted(name for name in package_names.values() if name.startswith("libs/qt6/"))
    verified_packages = {name for doc in documents if doc["ownership_verified"] for name in doc["matching_packages"]}
    report = {
        "documents": documents,
        "status": "no-documents" if not documents else (
            "verified" if all(doc["ownership_verified"] for doc in documents) else "needs-review"),
        "qt_packages_without_verified_document": sorted(set(qt_packages) - verified_packages),
        "scope": "Original installed Qt supplier SBOMs; verified documents are imported into craft.cdx.json. Ownership checks do not establish shipped-component coverage.",
    }
    write_json(output / "qt-supplier-sboms.json", report)
    return report
