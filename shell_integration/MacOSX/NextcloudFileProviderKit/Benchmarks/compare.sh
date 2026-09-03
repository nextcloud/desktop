#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Run the benchmark scenarios on an unmodified baseline and on the current checkout, against the
# same server, and print the comparison.
#
# Usage:
#   Benchmarks/compare.sh <base-ref> [scenario...]
#
# Example:
#   docker compose -f Benchmarks/docker-compose.yml up -d
#   Benchmarks/provision.sh
#   Benchmarks/compare.sh stable-34.0
#
# The baseline is built from a detached worktree at <base-ref> with this directory copied in, so the
# comparison is the production code of two revisions measured by one harness. Both are built in
# release: a debug build measures the optimiser, not the change.

set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <base-ref> [scenario...]" >&2
    exit 1
fi

BASE_REF="$1"
shift
SCENARIOS=("$@")

PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(git -C "$PACKAGE_DIR" rev-parse --show-toplevel)"
PACKAGE_SUBPATH="${PACKAGE_DIR#"$REPO_ROOT"/}"

if ! git -C "$REPO_ROOT" rev-parse --verify --quiet "$BASE_REF" >/dev/null; then
    echo "Unknown base ref '$BASE_REF'." >&2
    echo "Fetch it first, e.g.:" >&2
    echo "  git fetch https://github.com/nextcloud/desktop.git stable-34.0:refs/remotes/upstream/stable-34.0" >&2
    exit 1
fi

WORKTREE="$(mktemp -d -t nfpk-bench-base)"
cleanup() {
    git -C "$REPO_ROOT" worktree remove --force "$WORKTREE" >/dev/null 2>&1 || true
    rm -rf "$WORKTREE"
}
trap cleanup EXIT

echo "Preparing baseline worktree at $BASE_REF..."
git -C "$REPO_ROOT" worktree add --detach "$WORKTREE" "$BASE_REF" >/dev/null

BASE_PACKAGE="$WORKTREE/$PACKAGE_SUBPATH"
rm -rf "$BASE_PACKAGE/Benchmarks"
cp -R "$PACKAGE_DIR/Benchmarks" "$BASE_PACKAGE/Benchmarks"

# Declare the harness target in the baseline manifest without otherwise replacing it: the baseline
# must stay the baseline, and only the measuring instrument is carried across.
python3 - "$BASE_PACKAGE/Package.swift" <<'PY'
import re
import sys

path = sys.argv[1]
manifest = open(path).read()

if "NextcloudFileProviderKitBenchmarks" in manifest:
    sys.exit(0)

target = '''        .executableTarget(
            name: "NextcloudFileProviderKitBenchmarks",
            dependencies: [
                "NextcloudFileProviderKit",
                .product(name: "NextcloudKit", package: "NextcloudKit")
            ],
            path: "Benchmarks/Harness"
        )
'''

# Append as the last entry of the targets array.
index = manifest.rindex("    ]\n)")
manifest = manifest[:index].rstrip()
manifest = manifest + ",\n" + target + "    ]\n)\n"
open(path, "w").write(manifest)
PY

BASE_REVISION="$(git -C "$REPO_ROOT" rev-parse --short "$BASE_REF")"
HEAD_REVISION="$(git -C "$REPO_ROOT" rev-parse --short HEAD)"

BASE_JSON="$WORKTREE/base.json"
HEAD_JSON="$WORKTREE/head.json"

echo "Building baseline ($BASE_REVISION)..."
(cd "$BASE_PACKAGE" && swift build --configuration release --product NextcloudFileProviderKitBenchmarks >/dev/null)

echo "Building current checkout ($HEAD_REVISION)..."
(cd "$PACKAGE_DIR" && swift build --configuration release --product NextcloudFileProviderKitBenchmarks >/dev/null)

echo "Running baseline..."
(cd "$BASE_PACKAGE" && NFPK_BENCH_REVISION="$BASE_REVISION" \
    .build/release/NextcloudFileProviderKitBenchmarks "${SCENARIOS[@]}" --output "$BASE_JSON" >/dev/null)

echo "Running current checkout..."
(cd "$PACKAGE_DIR" && NFPK_BENCH_REVISION="$HEAD_REVISION" \
    .build/release/NextcloudFileProviderKitBenchmarks "${SCENARIOS[@]}" --output "$HEAD_JSON" >/dev/null)

python3 - "$BASE_JSON" "$HEAD_JSON" "$BASE_REVISION" "$HEAD_REVISION" <<'PY'
import json
import sys

base_path, head_path, base_revision, head_revision = sys.argv[1:5]

base = {result["scenario"]: result for result in json.load(open(base_path))}
head = {result["scenario"]: result for result in json.load(open(head_path))}


def render(value, unit):
    if unit == "s":
        return f"{value:.3f} s"
    if value == int(value):
        return f"{int(value)} {unit}"
    return f"{value:.2f} {unit}"


def change(before, after, lower_is_better):
    if before == after:
        return "unchanged"
    # A metric that reached zero has no ratio; say what happened instead of dividing.
    if after == 0:
        return "eliminated" if lower_is_better else "lost"
    if before == 0:
        return "introduced" if lower_is_better else "gained"
    if lower_is_better:
        return f"{before / after:.2f}x lower" if after < before else f"{after / before:.2f}x higher"
    return f"{after / before:.2f}x higher" if after > before else f"{before / after:.2f}x lower"


print()
for scenario, head_result in head.items():
    base_result = base.get(scenario)
    print(f"### {scenario}")
    print()
    print(f"| Metric | {base_revision} (baseline) | {head_revision} | Change |")
    print("| --- | --- | --- | --- |")

    base_metrics = {m["name"]: m for m in base_result["metrics"]} if base_result else {}

    for metric in head_result["metrics"]:
        name = metric["name"]
        after = metric["value"]
        unit = metric["unit"]
        baseline = base_metrics.get(name)

        if baseline is None:
            print(f"| {name} | not measured | {render(after, unit)} | n/a |")
            continue

        before = baseline["value"]
        print(
            f"| {name} | {render(before, unit)} | {render(after, unit)} "
            f"| {change(before, after, metric['lowerIsBetter'])} |"
        )

    print()
PY
