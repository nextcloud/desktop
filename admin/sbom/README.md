<!--
SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Dependency SBOMs: Craft, Swift, Sparkle and Desktop source

mac-crafter captures these inventories after successful Craft compilation:

```text
<build-path>/sbom/<architecture>-<unique-id>/
  craft/  craft.cdx.json, database, package list, settings, metadata, Qt SPDX files
  swift/  swift.cdx.json, Package.resolved, metadata
  downloads/  downloads.cdx.json, Sparkle archive, metadata
  desktop/  desktop.cdx.json, source and bundled-code metadata
  bom.cdx.json
  bom.cdx.excluded.json  # Created when build-only components are excluded
```

Keep this folder before cleaning the build directory. Existing captures are never
overwritten. Capture failures stop the command. Python 3.9+ and Git are required.

- **Craft:** reads `<craft-root>/etc/blueprints/install.db`. Copies Qt supplier
  documents from `<craft-root>/sbom/qt*.spdx` and imports all their package entries
  when ownership and checksums match. Unverified documents are retained, skipped
  during conversion and reported in `qt-supplier-sboms.json`.
  mac-crafter also captures `craft-dependencies.json` by resolving the target's
  runtime and complete dependency sets from the configured local Craft blueprints.
  This performs metadata resolution only, with no build or download actions.
- **Swift:** reads the client project's lockfile, relative to the source checkout:
  `shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved`.
  Includes every resolved package, including indirect dependencies.

The Craft and Swift outputs use CycloneDX 1.6, labelled “Craft dependencies only” or “Swift
dependencies only”. They may include build tools and unshipped packages. They
are not complete client SBOMs and do not include a dependency graph. Known Craft
packages receive upstream names, release versions and generic or PyPI PURLs.
Original Craft paths and installed versions remain in component properties.
Qt supplier identifiers and licenses are preserved, with original SPDX files kept
for custom license definitions. Swift repositories receive Swift PURLs including
the host and repository namespace, plus the matching CycloneDX group/name.
- **Sparkle:** saves the downloaded archive, selected release, download URL and
  archive SHA-256. `downloads.cdx.json` is labelled “Separate downloads only”.
  Disabling the updater records an empty inventory. Dependencies inside Sparkle
  are not yet enumerated.

**Desktop source:** `desktop/desktop.cdx.json` records the Git-described source
version, commit and dirty state. `capture.json` retains status and bundled-file
hashes/license notices. Discovery covers `3rdparty`, `third_party` and `vendor`
directories, including platform-specific code. Upstream versions are not guessed;
code outside these directories needs further review. This is not proof of shipped
code or the final branded application version. Brander archival remains separate.

## Script layout

```text
sbom.py    Command entry point used by mac-crafter
craft/     Craft capture, conversion and Qt supplier handling
swift/     Swift capture and conversion
sparkle/   Sparkle capture and conversion
desktop/   Desktop source and bundled-code inventory
common/    Shared helpers
```

Tests sit beside the code they cover in each topic folder.

## Manual capture or conversion

Run from the Desktop checkout. Use a new output directory for each capture:

```sh
python3 admin/sbom/sbom.py craft capture --craft-root build/macos-clang-arm64 --target-package nextcloud-client --output-dir /archive/build-id/craft --build-id build-id
python3 admin/sbom/sbom.py swift capture --source-dir . --output-dir /archive/build-id/swift --build-id build-id
```

For captures without a CycloneDX output yet:

```sh
python3 admin/sbom/sbom.py craft convert --capture-dir /archive/build-id/craft
python3 admin/sbom/sbom.py swift convert --capture-dir /archive/build-id/swift
```

Converters check captured input checksums and refuse to overwrite existing output.
Tests live in `test_*.py` beside the modules they cover.

## Vulnerability matching

`craft/identities.py` contains the explicit Craft-to-upstream mappings. Package
names follow the Craft blueprints, including `liblzma` → `xz`, `libbzip2` →
`bzip2`, and the `libp11-` release-tag prefix. Only known numeric release targets
are normalized. The trailing numeric Craft patch level is recorded in the original
installed version and removed from the upstream version, following
[CraftBase.version](https://invent.kde.org/packaging/craft/-/blob/master/bin/CraftBase.py).
Craft patches can affect vulnerability applicability; a match still requires review.

Mapped native CPE vendor/product pairs were checked against the NVD-derived CPE
records in the Grype database built on 2026-11-03. The registry includes historical
upstream aliases where applicable and excludes unrelated projects with similar names.
Do not infer a CPE vendor from a package name when extending the mapping. Qt CPEs
come directly from the checksum-verified supplier documents.

CycloneDX has one `cpe` field per component. Additional CPEs are retained as
`syft:cpe23` properties, the representation understood by
[Syft and Grype](https://github.com/anchore/syft/blob/v1.56.0/syft/format/internal/cyclonedxutil/helpers/cpe.go).
Swift PURLs use `pkg:swift/<host>/<namespace>/<repository>@<version>` rather than
the repository-hosting ecosystem, following
[Syft's Swift package identities](https://github.com/anchore/syft/blob/v1.56.0/syft/pkg/cataloger/swift/package.go).

Each Craft component has `nextcloud:sbom:identity-status`: `cpe`, `pypi`,
`generic-purl-only`, `unresolved-version`, or `unmapped-package`. Generic PURLs
alone do not ensure Grype matching. Versions such as `latest` and `master`, virtual
packages, and unrecognized Craft paths remain evidence without guessed identifiers.
Swift locations that cannot be resolved to repository identities are likewise labelled.
Review these coverage gaps even when a scanner reports no vulnerabilities.

To regenerate without building the client, copy a capture's `craft/` and `swift/`
evidence into a new directory, omitting their existing `.cdx.json` files. Run the
two `convert` commands above on those copies, retain the unchanged `desktop/` and
`downloads/` inventories, and combine the four module files. The capture build ID
must remain the original ID. Original captures and scanner reports need not change.

## Combined SBOM

mac-crafter writes `bom.cdx.json` beside the module folders after all captures.
Original inventories and evidence remain unchanged. The combined file lists captured
components under one Desktop application. Source
labels are component properties; original module metadata is retained in metadata
properties. Supplier defaults to Nextcloud GmbH, as declared in NEXTCLOUD.cmake;
branded builds can pass `--name` and `--supplier`. Repeated identical
input files are included once; similar package names are not assumed equivalent.
Coverage remains partial and may include build tools or unshipped code.

By default, the combined scan input excludes components whose captured
`nextcloud:sbom:dependency-scope` is `build`. Craft packages are build-only when
they occur in the target's complete dependency closure but not its runtime
closure. Runtime dependencies shared with build tools remain included. Packages
outside the resolved graph remain unknown and included. No package-name denylist
is used for this classification.

Qt supplier components inherit a build-only role when their owning Craft package
is build-only. Otherwise, explicit SPDX `BUILD_DEPENDENCY_OF` relationships identify
build tools, except when a non-build package also depends on them. Other supplier
components remain included; an `APPLICATION` purpose alone does not prove a tool
is absent from the shipped application.

Excluded components and their evidence are saved beside the output, for example
`bom.cdx.excluded.json`. Modular inventories and original captures retain all
components. Pass `--include-build-dependencies` to `common combine` for the full
inventory. Dependency edges referring to excluded components are removed from
the filtered document.

Older captures without dependency metadata cannot classify Craft roles. They
can still use explicit Qt supplier relationships. Enriching an old capture from
current local blueprints is a new observation, not proof of the historic build
configuration; retain its timestamp and source hashes. A complete shipped-product
SBOM would additionally need evidence from the packaged application.

Other platforms can pass any selection of module files:

```sh
python3 admin/sbom/sbom.py common combine --build-id build-id --input /archive/build-id/craft/craft.cdx.json --input /archive/build-id/desktop/desktop.cdx.json --output /archive/build-id/bom.cdx.json
```

The combiner accepts our modular CycloneDX 1.6 format, rejects unsupported sections
and unresolved references, and refuses to overwrite existing output.
