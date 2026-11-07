# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Import the installed Qt SPDX 2.3 tag/value documents as supplier evidence."""

from collections import defaultdict

from common.sbom_utils import add_cpes, properties, reference


def parse_spdx(text):
    """Read Qt's package/file/license records, preserving repeated fields and text blocks."""
    records = []
    current = defaultdict(list)
    records.append(current)
    lines = iter(text.splitlines())
    for line in lines:
        if not line.strip() or line.startswith("#"):
            continue
        if ": " not in line:
            raise ValueError(f"Unsupported SPDX line: {line[:100]}")
        key, value = line.split(": ", 1)
        if key in ("PackageName", "FileName", "LicenseID"):
            current = defaultdict(list)
            records.append(current)
        if value.startswith("<text>"):
            while "</text>" not in value:
                try:
                    value += "\n" + next(lines)
                except StopIteration as error:
                    raise ValueError("Unterminated SPDX text block") from error
            if not value.endswith("</text>"):
                raise ValueError("Unsupported content after SPDX text block")
            value = value[6:-7]
        current[key].append(value)
    document = dict(records[0])
    if document.get("SPDXVersion") != ["SPDX-2.3"] or len(document.get("DocumentNamespace", [])) != 1:
        raise ValueError("Expected an SPDX 2.3 document with a namespace")
    packages, files, licenses, relationships = {}, {}, {}, []
    identifiers = set()
    for record in records:
        if "SPDXID" in record:
            if len(record["SPDXID"]) != 1 or record["SPDXID"][0] in identifiers:
                raise ValueError("Duplicate SPDX identifier")
            identifiers.add(record["SPDXID"][0])
        if "PackageName" in record:
            packages[record["SPDXID"][0]] = dict(record)
        elif "FileName" in record:
            files[record["SPDXID"][0]] = dict(record)
        elif "LicenseID" in record:
            licenses[record["LicenseID"][0]] = dict(record)
        for relationship in record.get("Relationship", []):
            parts = relationship.split()
            if len(parts) != 3:
                raise ValueError(f"Malformed SPDX relationship: {relationship}")
            relationships.append(parts)
    return {"document": document, "packages": packages, "files": files,
            "licenses": licenses, "relationships": relationships}


def supplier_component(namespace, identifier, record, evidence_path):
    name = record["PackageName"][0]
    values = {"supplier-document": evidence_path, "spdx-id": identifier,
              "evidence-method": "supplier-package-inventory", "supplier-namespace": namespace}
    component = {"type": "application" if record.get("PrimaryPackagePurpose") == ["APPLICATION"] else "library",
                 "bom-ref": reference("qt", namespace + "#" + identifier), "name": name}
    version = record.get("PackageVersion", [None])[0]
    if version and version.lower() not in ("unknown", "noassertion", "none"):
        component["version"] = version
    purls = [value.split(" ", 2)[2] for value in record.get("ExternalRef", [])
             if value.startswith("PACKAGE-MANAGER purl pkg:")]
    if purls:
        component["purl"] = purls[0]
        values["supplier-purls"] = "\n".join(dict.fromkeys(purls))
    for field, acknowledgement in (("PackageLicenseDeclared", "declared"), ("PackageLicenseConcluded", "concluded")):
        expression = record.get(field, [None])[0]
        if expression and expression not in ("NOASSERTION", "NONE"):
            component["licenses"] = [{"expression": expression, "acknowledgement": acknowledgement}]
            values["license-assertion-source"] = "supplier"
            break
    supplier = record.get("PackageSupplier", [""])[0]
    if supplier.startswith("Organization: "):
        component["supplier"] = {"name": supplier.removeprefix("Organization: ")}
    copyright_text = record.get("PackageCopyrightText", [None])[0]
    if copyright_text and copyright_text != "NOASSERTION":
        component["copyright"] = copyright_text
    if "PackageComment" in record:
        values["supplier-comment"] = "\n".join(record["PackageComment"])
    component["properties"] = properties(values)
    add_cpes(component, [value.split(None, 2)[2] for value in record.get("ExternalRef", [])
                        if value.startswith(("SECURITY cpe23Type ", "SECURITY cpe22Type "))])
    return component
