#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# Create the benchmark fixture on the reference Nextcloud container.
#
# The files are written straight into the data directory and then indexed with `occ files:scan`,
# which is orders of magnitude faster than several thousand WebDAV PUTs and produces the same
# server-side state.
#
# Shape (override with environment variables):
#   nfpk-bench/tree/dir-NNN/file-NNN.txt   TREE_DIRS x FILES_PER_DIR   the many-container tree
#   nfpk-bench/wide/file-NNNN.txt          WIDE_FILES                  one wide, paginated listing

set -euo pipefail

CONTAINER="${NFPK_BENCH_CONTAINER:-nfpk-bench-nextcloud}"
USER_ID="${NFPK_BENCH_USER:-admin}"
TREE_DIRS="${TREE_DIRS:-200}"
FILES_PER_DIR="${FILES_PER_DIR:-15}"
WIDE_FILES="${WIDE_FILES:-500}"

if ! docker inspect "$CONTAINER" >/dev/null 2>&1; then
    echo "Container '$CONTAINER' is not running. Start it with:" >&2
    echo "  docker compose -f Benchmarks/docker-compose.yml up -d" >&2
    exit 1
fi

echo "Provisioning fixture in '$CONTAINER':"
echo "  tree: $TREE_DIRS directories x $FILES_PER_DIR files"
echo "  wide: $WIDE_FILES files"

docker exec -i -u www-data -e TREE_DIRS="$TREE_DIRS" -e FILES_PER_DIR="$FILES_PER_DIR" \
    -e WIDE_FILES="$WIDE_FILES" -e USER_ID="$USER_ID" "$CONTAINER" sh -s <<'INNER'
set -eu

ROOT="/var/www/html/data/${USER_ID}/files/nfpk-bench"
rm -rf "$ROOT"
mkdir -p "$ROOT/tree" "$ROOT/wide"

directory_index=0
while [ "$directory_index" -lt "$TREE_DIRS" ]; do
    directory=$(printf "%s/tree/dir-%03d" "$ROOT" "$directory_index")
    mkdir -p "$directory"

    file_index=0
    while [ "$file_index" -lt "$FILES_PER_DIR" ]; do
        printf 'benchmark fixture %03d/%03d\n' "$directory_index" "$file_index" \
            > "$(printf '%s/file-%03d.txt' "$directory" "$file_index")"
        file_index=$((file_index + 1))
    done

    directory_index=$((directory_index + 1))
done

file_index=0
while [ "$file_index" -lt "$WIDE_FILES" ]; do
    printf 'benchmark fixture wide %04d\n' "$file_index" \
        > "$(printf '%s/wide/file-%04d.txt' "$ROOT" "$file_index")"
    file_index=$((file_index + 1))
done
INNER

echo "Indexing..."
docker exec -u www-data "$CONTAINER" php occ files:scan "$USER_ID" --quiet

echo "Done. Fixture is at /nfpk-bench for user '$USER_ID'."
