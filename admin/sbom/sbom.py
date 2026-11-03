# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Entry point for dependency capture and conversion by topic."""

import argparse
import runpy
import sys

COMMANDS = {
    ("common", "combine"): "common.combine",
    ("desktop", "capture"): "desktop.capture_desktop",
    ("craft", "capture"): "craft.capture_craft",
    ("craft", "convert"): "craft.craft_sbom",
    ("swift", "capture"): "swift.capture_swift",
    ("swift", "convert"): "swift.swift_sbom",
    ("sparkle", "capture"): "sparkle.capture_sparkle",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("topic", choices=("craft", "swift", "sparkle", "desktop", "common"))
    parser.add_argument("action", choices=("capture", "convert", "combine"))
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    module = COMMANDS.get((args.topic, args.action))
    if module is None:
        parser.error("Conversion for this topic is included in capture; use capture")
    sys.argv = [sys.argv[0], *args.arguments]
    runpy.run_module(module, run_name="__main__")


if __name__ == "__main__":
    main()
