#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create a user-local capture scheme without configuring or building the client."""

import getpass
from pathlib import Path
import platform
from xml.sax.saxutils import escape


def scheme_contents(template: str, build_directory: Path) -> str:
    executable = build_directory.resolve() / "bin" / "DocAssetsCapture"
    return template.replace("@CAPTURE_EXECUTABLE@", escape(str(executable), {'"': "&quot;"}))


def main() -> None:
    source = Path(__file__).resolve().parent
    repository = source.parent.parent
    build_directory = (
        repository / "build" / f"macos-clang-{platform.machine()}"
        / "build/nextcloud-client/work/build"
    )
    if not (build_directory / "CMakeCache.txt").is_file():
        raise SystemExit(f"An existing configured client build is required: {build_directory}")
    project = repository / "shell_integration/MacOSX/NextcloudIntegration/NextcloudIntegration.xcodeproj"
    schemes = project / "xcuserdata" / f"{getpass.getuser()}.xcuserdatad" / "xcschemes"
    schemes.mkdir(parents=True, exist_ok=True)
    destination = schemes / "Nextcloud Documentation Assets.xcscheme"
    destination.write_text(scheme_contents(source.joinpath("capture.xcscheme.in").read_text(), build_directory))
    print(f"Created local scheme: {destination}")
    print("Reopen the Xcode workspace if the scheme list has not refreshed.")


if __name__ == "__main__":
    main()
