#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class AppImageIconTest(unittest.TestCase):
    repository = Path(__file__).resolve().parents[3]
    resolver = repository / "admin/linux/resolve-appimage-icon.sh"

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="appimage icon ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.share = self.root / "usr/share"
        self.desktop = self.share / "applications/nextcloud.desktop"
        self.desktop.parent.mkdir(parents=True)

    def create_icon(self, name="com.nextcloud.desktopclient.nextcloud", newline="\n"):
        icon = self.share / "icons/hicolor/512x512/apps" / f"{name}.png"
        icon.parent.mkdir(parents=True, exist_ok=True)
        icon.touch()
        contents = (
            "[Desktop Entry]\nName=Nextcloud\nIcon[fr]=localized-icon\n"
            f"Icon={name}\n[Desktop Action Quit]\nIcon=action-icon\n"
        )
        self.desktop.write_bytes(contents.replace("\n", newline).encode())
        return icon

    def resolve(self):
        return subprocess.run(
            ["bash", str(self.resolver), str(self.desktop)],
            capture_output=True, text=True, timeout=10,
        )

    def test_resolves_installed_main_icon(self):
        for name, newline in (
            ("com.nextcloud.desktopclient.nextcloud", "\n"),
            ("Acme Cloud", "\n"),
            ("com.example.Cloud", "\r\n"),
        ):
            with self.subTest(name=name, newline=repr(newline)):
                icon = self.create_icon(name, newline)
                result = self.resolve()
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), str(icon))

    def test_rejects_missing_main_icon(self):
        self.create_icon("action-icon")
        self.desktop.write_text("[Desktop Entry]\nName=Nextcloud\n[Desktop Action Quit]\nIcon=action-icon\n")
        result = self.resolve()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertTrue(result.stderr)

    def test_rejects_missing_installed_png(self):
        self.create_icon().unlink()
        result = self.resolve()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, "")
        self.assertTrue(result.stderr)

    def test_both_linuxdeploy_calls_use_installed_icon(self):
        icon = self.create_icon()
        deploy = self.root / "linuxdeploy-squashfs-root/AppRun"
        deploy.parent.mkdir()
        deploy.write_text(
            '#!/bin/bash\n'
            'for argument; do\n'
            '    case "$argument" in --icon-file=*) icon=${argument#*=} ;; esac\n'
            'done\n'
            '[[ "$icon" == "$EXPECTED_ICON" && -f "$icon" ]]\n'
        )
        deploy.chmod(0o755)
        lines = (self.repository / "admin/linux/build-appimage.sh").read_text().splitlines()
        commands = []
        for index, line in enumerate(lines):
            if line.startswith("./linuxdeploy-squashfs-root/AppRun "):
                command = line
                while lines[index].endswith("\\"):
                    index += 1
                    command += "\n" + lines[index]
                commands.append(command)
        self.assertEqual(len(commands), 2)
        environment = os.environ.copy()
        environment.update(
            DESKTOP_FILE=str(self.desktop), ICON_FILE=str(icon),
            EXPECTED_ICON=str(icon), EXECUTABLE_NAME="nextcloud",
            OPENVFS_VFS_PLUGIN="unused-openvfs.so",
        )
        for command in commands:
            with self.subTest(command=command.splitlines()[0]):
                result = subprocess.run(
                    ["bash", "-c", command], cwd=self.root, env=environment,
                    capture_output=True, text=True, timeout=10,
                )
                self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
