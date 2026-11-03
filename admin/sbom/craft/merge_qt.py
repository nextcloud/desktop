# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Merge all packages from verified captured Qt supplier documents."""

from craft.qt_sbom import parse_spdx, supplier_component
from craft.dependency_scope import qt_build_dependencies, set_scope
from common.sbom_utils import digest, properties


def merge_qt(bom, capture_dir, installed_files, packages, scopes=None):
    known_owners = {p["packageId"] for p in packages}
    owner_scopes = {p["packageId"]: (scopes or {}).get(p.get("packagePath"), "unknown") for p in packages}
    namespaces = set()
    imported = []
    issues = []
    directory = capture_dir / "qt"
    if directory.is_symlink():
        raise ValueError("Captured Qt directory must not be a symlink")
    for path in sorted(directory.glob("qt*.spdx")):
        if path.is_symlink() or not path.is_file():
            raise ValueError("Captured Qt documents must be regular files")
        checksum = digest(path)
        owners = {row["packageId"] for row in installed_files
                  if row["filename"] == "sbom/" + path.name
                  and row.get("fileHash") == "[SHA256]" + checksum
                  and row["packageId"] in known_owners}
        if len(owners) != 1:
            issues.append(f"Skipped unverified supplier document: {path.name}")
            continue
        data = parse_spdx(path.read_text(encoding="utf-8"))
        namespace = data["document"]["DocumentNamespace"][0]
        if namespace in namespaces:
            raise ValueError("Duplicate Qt supplier namespace")
        namespaces.add(namespace)
        build_dependencies = qt_build_dependencies(data)
        for identifier, record in sorted(data["packages"].items()):
            component = supplier_component(namespace, identifier, record, "qt/" + path.name)
            component["properties"].extend(properties({
                "craft-package-id": next(iter(owners)),
                "supplier-document-sha256": checksum,
            }))
            if owner_scopes[next(iter(owners))] == "build":
                set_scope(component, "build", "Owning Craft package is build-only: craft-dependencies.json")
            elif identifier in build_dependencies:
                set_scope(component, "build", "Supplier SPDX BUILD_DEPENDENCY_OF: qt/" + path.name)
            else:
                set_scope(component, "unknown", "Supplier package has no proven build-only role")
            imported.append(component)
    bom["components"].extend(imported)
    bom["metadata"]["properties"].extend(properties({
        "qt-supplier-documents": len(namespaces),
        "qt-supplier-components": len(imported),
        "qt-import-issues": "\n".join(issues) if issues else None,
    }))
