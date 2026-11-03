# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Explicit upstream identities for Craft packages; see README.md for mapping sources."""

import re
from urllib.parse import quote

from common.sbom_utils import add_cpes, properties


# CPE vendor/product pairs refer to upstream projects, not distribution repackagings.
# Empty pairs mean no CPE mapping is established; a generic PURL still records identity.
NATIVE_IDENTITIES = {
    "libs/libb2": ("libb2", ()),
    "libs/libbzip2": ("bzip2", ("bzip:bzip2", "bzip2_project:bzip2")),
    "libs/expat": ("expat", ("libexpat_project:libexpat",)),
    "libs/zlib": ("zlib", ("zlib:zlib",)),
    "libs/openssl": ("openssl", ("openssl:openssl",)),
    "libs/liblzma": ("xz", ("tukaani:xz", "xz_project:xz")),
    "libs/icu": ("icu", ("unicode:international_components_for_unicode", "icu-project:international_components_for_unicode")),
    "libs/libxml2": ("libxml2", ("xmlsoft:libxml2", "libxml2:libxml2")),
    "libs/libunistring": ("libunistring", ()),
    "libs/gettext": ("gettext", ("gnu:gettext",)),
    "libs/sqlite": ("sqlite", ("sqlite:sqlite",)),
    "libs/libffi": ("libffi", ("libffi_project:libffi",)),
    "libs/python": ("python", ("python:python", "python_software_foundation:python")),
    "libs/brotli": ("brotli", ("google:brotli",)),
    "libs/libzstd": ("zstd", ("facebook:zstandard",)),
    "libs/libpng": ("libpng", ("libpng:libpng", "png_reference_library:libpng", "greg_roelofs:libpng")),
    "libs/libjpeg-turbo": ("libjpeg-turbo", ("libjpeg-turbo:libjpeg-turbo", "d.r.commander:libjpeg-turbo")),
    "libs/freetype": ("freetype", ("freetype:freetype",)),
    "libs/dbus": ("dbus", ("freedesktop:dbus",)),
    "libs/fontconfig": ("fontconfig", ("fontconfig_project:fontconfig",)),
    "libs/pixman": ("pixman", ("pixman:pixman",)),
    "libs/pcre2": ("pcre2", ("pcre:pcre2",)),
    "libs/glib": ("glib", ("gnome:glib",)),
    "libs/cairo": ("cairo", ("cairographics:cairo",)),
    "libs/harfbuzz": ("harfbuzz", ("harfbuzz_project:harfbuzz",)),
    "libs/llvm": ("llvm", ("llvm:llvm",)),
    "libs/libp11": ("libp11", ()),
    "libs/kdsingleapplication": ("kdsingleapplication", ()),
    "qt-libs/qtkeychain": ("qtkeychain", ()),
    "libs/libarchive": ("libarchive", ("libarchive:libarchive", "freebsd:libarchive")),
    "kde/frameworks/extra-cmake-modules": ("extra-cmake-modules", ()),
    "kde/frameworks/tier1/karchive": ("karchive", ()),
    "dev-utils/7zip-base": ("7-zip", ("7-zip:7-zip", "igor_pavlov:7-zip")),
    "dev-utils/cmake-base": ("cmake", ("cmake_project:cmake",)),
    "dev-utils/m4": ("m4", ("gnu:m4",)),
    "dev-utils/sed": ("sed", ("gnu:sed",)),
    "dev-utils/perl": ("perl", ("perl:perl", "larry_wall:perl")),
    "dev-utils/autoconf": ("autoconf", ()),
    "dev-utils/automake": ("automake", ("gnu:automake",)),
    "dev-utils/libtool": ("libtool", ("gnu:libtool",)),
    "dev-utils/ninja": ("ninja", ()),
    "dev-utils/pkgconf": ("pkgconf", ("pkgconf:pkgconf",)),
    "dev-utils/nasm": ("nasm", ("nasm:netwide_assembler",)),
    "dev-utils/gperf": ("gperf", ()),
    "dev-utils/flex": ("flex", ("westes:flex", "will_estes:flex")),
    "dev-utils/texinfo": ("texinfo", ("gnu:texinfo",)),
    "dev-utils/help2man": ("help2man", ()),
    "dev-utils/bison": ("bison", ("gnu:bison",)),
}
for module in ("qtbase", "qtshadertools", "qtsvg", "qtlanguageserver", "qttools",
               "qtdeclarative", "qtwebsockets", "qt5compat", "qttranslations"):
    # Qt CPEs come from the captured supplier documents, including module-specific CPEs.
    NATIVE_IDENTITIES["libs/qt6/" + module] = (module, ())

PYTHON_IDENTITIES = {"python-modules/" + name: name for name in (
    "pip", "flit-core", "setuptools", "dmgbuild", "packaging", "pathspec",
    "pluggy", "trove-classifiers", "hatchling", "meson")}


def normalize(component, package_path, installed_version):
    """Normalize known release targets, retaining unknown identities and versions as evidence."""
    identity = NATIVE_IDENTITIES.get(package_path)
    python_name = PYTHON_IDENTITIES.get(package_path)
    version = installed_version
    if package_path == "libs/libp11":
        version = version.removeprefix("libp11-")
    # CraftBase.version appends its numeric patch level to the upstream build target.
    # Only known numeric release targets qualify; do not reinterpret arbitrary prereleases.
    release = re.fullmatch(r"(\d+(?:\.\d+)+[a-z]?)(?:-(\d+))?", version)
    if not identity and not python_name:
        status = "unmapped-package"
    elif not release:
        status = "unresolved-version"
    else:
        name, cpe_pairs = identity if identity else (python_name, ())
        version = release[1]
        component.update(name=name, version=version)
        component["purl"] = "pkg:" + ("pypi/" if python_name else "generic/") + quote(name, safe="") + "@" + quote(version, safe="")
        add_cpes(component, [f"cpe:2.3:a:{pair}:{version}:*:*:*:*:*:*:*" for pair in cpe_pairs])
        status = "pypi" if python_name else ("cpe" if cpe_pairs else "generic-purl-only")
    component["properties"].extend(properties({
        "identity-status": status,
        "version-kind": "upstream-release" if status in ("pypi", "cpe", "generic-purl-only") else "craft-installed",
    }))
