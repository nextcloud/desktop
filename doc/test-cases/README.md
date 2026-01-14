<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Test Process

This is the first step to improve our release process by introducing a formalized and documented test process as a part of it.
We keep it simple for now and rely on Markdown files with a fixed template.
This enables versioning tied directly to our project and also lowers the entry barrier to the whole topic while adding minimal overhead.

**These test cases and the testing as part of the release process is not binding!** We are testing the waters for now and adjust course along the way.

## Categories

- **Smoke Tests** (`/smoke`): Critical functionality that must work for a build to be viable.
- **Regression Tests** (`/regression`): Core features that should remain stable across releases.
- **Integration Tests** (`/integration`): Client-server interactions and external dependencies.

## Test Case Format

Please have a look at the already existing test cases.
We are still evaluating which format works best for us.