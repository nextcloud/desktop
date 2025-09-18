<!--
  - SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
# Agents.md
This `AGENTS.md` file provides guidelines for OpenAI Codex and other AI agents interacting with this codebase, including which directories are safe to read from or write to.

## Project Overview
The Nextcloud Desktop Client is a tool to synchronize files from Nextcloud Server with your computer.

## Project Structure: AI Agent Handling Guidelines

| Directory       | Description                                         | Agent Action         |
|-----------------|-----------------------------------------------------|----------------------|
| `/translations` | Translation files from Transifex.                   | Do not modify        |

## General Guidance

Every new file needs to get a SPDX header in the first rows according to this template. 
The year needs to be adjusted accordingly. The commenting signs need to be used depending on the file type.
```
SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
SPDX-License-Identifier: GPL-2.0-or-later
```

## Commit & PR Guidelines
- **Commits**: Follow Conventional Commits format. Use `feat: ...`, `fix: ...`, or `refactor: ...` as appropriate in the commit message prefix.
- Include a short summary of what changed. *Example:* `fix: prevent crash on empty todo title`.
- **Pull Request**: When the agent creates a PR, it should include a description summarizing the changes and why they were made. If a GitHub issue exists, reference it (e.g., “Closes #123”).
