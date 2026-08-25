<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Writing style

This guide applies to comments, documentation, commit messages, and other technical writing in the repository. Write for the next human reader: make the point quickly, use ordinary words, and include only the context needed to understand the code.

Use [`doc/terminology.md`](terminology.md) when naming recurring concepts. It defines the shared vocabulary and calls out terms that differ between the standard sync engine and the File Provider engine.

## Keep it concise

- One sentence is the default for a code comment.
- Use two or three sentences only when they explain a real constraint, lifecycle boundary, or external API behavior.
- If the explanation needs a paragraph, put it in a design or user-facing document and link to it from the code.
- Remove repetition. Do not restate the function name, the next line of code, or a fact the type system already makes obvious.
- Prefer one clear point over a complete history of the implementation.

Prefer:

> Persist the change-delivery session in Realm so a new enumerator can drain the remaining batch after the current enumerator is invalidated.

Avoid:

> This state-management mechanism ensures robust continuation behavior across enumerator lifecycle transitions and related asynchronous callbacks.

For a small implementation detail, shorter is better:

> Keep `keepDownloaded` when replacing server metadata.

## Use plain, direct language

- Start with the point. Put the important action or constraint first.
- Prefer active voice: “Save the batch in Realm,” not “The batch is persisted by the session manager.”
- Name the concrete object, operation, and boundary when they matter.
- Prefer familiar verbs such as “save,” “read,” “send,” “remove,” and “keep.”
- Avoid abstract noun chains such as “state transition handling behavior.”
- Remove filler such as “in order to,” “as such,” “it should be noted,” “ensures that,” “leverages,” and “robustly.”
- Do not use impressive-sounding wording that you would not use when explaining the code to a colleague.

## Explain the reason or contract

Comments should explain why the code is necessary or describe a contract that is not obvious from the code. They should not narrate each statement.

Good reasons include:

- an ordering requirement;
- a lifecycle boundary;
- a platform or framework rule;
- a value that must survive an update;
- a failure case that looks surprising; or
- a deliberate compatibility behavior.

If the behavior is unclear enough that it cannot be described in one or two simple sentences, investigate the implementation before writing the comment. If it still cannot be stated confidently, leave the comment out or move the explanation into a design document.

## C++ and Objective-C++ comments

- Use Doxygen `/** @brief ... */` blocks for types and their members, trailing `//!< ...` for a single instance variable or field, and plain `//` for free helper functions and file-local statics.
- Document each type and each public member when its purpose or behavior is not obvious.
- For public properties, methods, and protocol callbacks, state what they do and what event triggers a callback block.
- Add `@param` entries only when the parameter's behavior is not obvious from its name, especially for `nil`, `NO`, an empty string, or `0`.
- When overloaded methods funnel into one designated implementation, document the designated method fully and identify the others as convenience overloads.
- Comment instance variables and file-local statics selectively. Leave self-explanatory fields, counters, and backing storage uncommented.
- In Objective-C++, document instance variables in the `@implementation` block rather than the header.

## Swift comments

- Use `///` documentation comments for public Swift types and members when documentation is needed.
- Use `//` for short implementation comments and `// MARK: -` for meaningful sections.
- Document the behavior and important contract of a public API, including callback or delegate conditions when they are not obvious.
- Do not use Doxygen tags or C++ comment conventions in Swift files.

## Check accuracy

Read the implementation before trusting a comment. Describe what the code actually guarantees, not what it appears intended to do.

Avoid absolute claims when the implementation is best effort. For example, do not say that a helper “keeps the window on screen” if it only clamps its position, or that it “loads a remote URL” if it only reads a local file.

AI-generated comments must be edited to the same standard as human-written comments. If a comment sounds like a paragraph of technical language, shorten it until a human can understand its point on the first read.

## Spelling and formatting

- Choose one spelling for new prose and use it consistently within the document. Preserve exact public API, platform, and framework names.
- Do not mass-rename existing symbols or comments as part of an unrelated documentation change.
- Use the terminology chosen in [`doc/terminology.md`](terminology.md), including the engine-specific terms.
- Keep normal Markdown paragraphs and list items as single logical lines so editors can wrap them naturally. Tables may use one row per line.
