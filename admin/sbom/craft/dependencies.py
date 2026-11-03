# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

"""Resolve build/runtime roles using the configured local Craft blueprints."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def capture_dependencies(craft_root, output, target_package):
    """Isolate Craft imports and environment setup from the SBOM converter."""
    with tempfile.TemporaryDirectory(prefix="sbom-craft-") as temporary:
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1",
                   CRAFT_LOG_FILE=str(Path(temporary) / "craft.log"))
        subprocess.run([sys.executable, "-B", str(Path(__file__).resolve()),
                        "--craft-root", str(craft_root), "--target-package", target_package,
                        "--output", str(output)], env=env, check=True, timeout=120)


def resolve_dependencies(craft_root, target_package):
    root = Path(craft_root).resolve()
    os.environ["KDEROOT"] = str(root)
    os.environ["craftRoot"] = str(root / "craft")
    os.environ.pop("CRAFT_FORCE_RESET", None)
    sys.argv[0] = str(root / "craft/bin/craft.py")
    sys.path.insert(0, str(root / "craft/bin"))
    from CraftCore import CraftCore
    from Blueprints.CraftPackageObject import CraftPackageObject
    from Blueprints.CraftDependencyPackage import CraftDependencyPackage, DependencyType

    if CraftCore.compiler.isMacOS:
        os.environ.setdefault("MACOSX_DEPLOYMENT_TARGET", str(CraftCore.compiler.macOSDeploymentTarget))
    package = CraftPackageObject.get(target_package)
    if package is None:
        raise ValueError(f"Craft target package not found: {target_package}")
    runtime = {p.path for p in CraftDependencyPackage(package).getDependencies(depType=DependencyType.Runtime)}
    all_dependencies = {p.path for p in CraftDependencyPackage(package).getDependencies(depType=DependencyType.All)}
    if package.path not in runtime or not runtime <= all_dependencies:
        raise ValueError("Craft returned inconsistent runtime/build dependency sets")
    sources = {}
    for module in tuple(sys.modules.values()):
        filename = getattr(module, "__file__", None)
        if filename:
            path = Path(filename).resolve()
            if path.is_file() and path.suffix == ".py" and root in path.parents:
                sources[str(path.relative_to(root))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return {"version": 1, "target_package": package.path,
            "captured_at": datetime.now(timezone.utc).isoformat(),
            "method": "Configured local Craft Runtime and All dependency closures; no build actions.",
            "runtime": sorted(runtime), "build_only": sorted(all_dependencies - runtime),
            "source_sha256": dict(sorted(sources.items())),
            "settings_sha256": hashlib.sha256((root / "etc/CraftSettings.ini").read_bytes()).hexdigest()}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--craft-root", type=Path, required=True)
    parser.add_argument("--target-package", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = resolve_dependencies(args.craft_root, args.target_package)
    with args.output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(result, indent=2) + "\n")
